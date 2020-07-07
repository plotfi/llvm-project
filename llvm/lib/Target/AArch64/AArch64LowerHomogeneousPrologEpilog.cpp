//===- AArch64LowerHomogeneousPrologEpilog.cpp ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a pass that lowers homogeneous prolog/epilog instructions.
//
//===----------------------------------------------------------------------===//

#include "AArch64InstrInfo.h"
#include "AArch64Subtarget.h"
#include "MCTargetDesc/AArch64InstPrinter.h"
#include "Utils/AArch64BaseInfo.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <sstream>

using namespace llvm;

#define AARCH64_LOWER_HOMOGENEOUS_PROLOG_EPILOG_NAME                           \
  "AArch64 homogeneous prolog/epilog lowering pass"

cl::opt<int> FrameHelperSizeThreshold(
    "frame-helper-size-threshold", cl::init(2), cl::Hidden,
    cl::desc("The minimum number of instructions that are outlined in a frame "
             "helper (default = 2)"));

namespace {

class AArch64LowerHomogeneousPE {
public:
  const AArch64InstrInfo *TII;

  AArch64LowerHomogeneousPE(Module *M, MachineModuleInfo *MMI)
      : M(M), MMI(MMI) {}

  bool run();
  bool runOnMachineFunction(MachineFunction &Fn);

private:
  Module *M;
  MachineModuleInfo *MMI;

  bool runOnMBB(MachineBasicBlock &MBB);
  bool runOnMI(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
               MachineBasicBlock::iterator &NextMBBI);

  /// Lower a HOM_Prolog pseudo instruction into a helper call
  /// or a sequence of homogeneous stores.
  /// When a a fp setup follows, it can be optimized.
  bool lowerHOM_Prolog(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                       MachineBasicBlock::iterator &NextMBBI);
  /// Lower a HOM_Epilog pseudo instruction into a helper call
  /// or a sequence of homogeneous loads.
  /// When a return follow, it can be optimized.
  bool lowerHOM_Epilog(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                       MachineBasicBlock::iterator &NextMBBI);
};

class AArch64LowerHomogeneousPrologEpilog : public ModulePass {
public:
  static char ID;

  AArch64LowerHomogeneousPrologEpilog() : ModulePass(ID) {
    initializeAArch64LowerHomogeneousPrologEpilogPass(
        *PassRegistry::getPassRegistry());
  }
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineModuleInfoWrapperPass>();
    AU.addPreserved<MachineModuleInfoWrapperPass>();
    AU.setPreservesAll();
    ModulePass::getAnalysisUsage(AU);
  }
  bool runOnModule(Module &M) override;

  StringRef getPassName() const override {
    return AARCH64_LOWER_HOMOGENEOUS_PROLOG_EPILOG_NAME;
  }
};

} // end anonymous namespace

char AArch64LowerHomogeneousPrologEpilog::ID = 0;

INITIALIZE_PASS(AArch64LowerHomogeneousPrologEpilog,
                "aarch64-lower-homogeneous-prolog-epilog",
                AARCH64_LOWER_HOMOGENEOUS_PROLOG_EPILOG_NAME, false, false)

bool AArch64LowerHomogeneousPrologEpilog::runOnModule(Module &M) {
  if (skipModule(M))
    return false;

  MachineModuleInfo *MMI =
      &getAnalysis<MachineModuleInfoWrapperPass>().getMMI();
  return AArch64LowerHomogeneousPE(&M, MMI).run();
}

bool AArch64LowerHomogeneousPE::run() {
  bool Changed = false;
  for (auto &F : *M) {
    if (F.empty())
      continue;

    MachineFunction *MF = MMI->getMachineFunction(F);
    if (!MF)
      continue;
    Changed |= runOnMachineFunction(*MF);
  }

  return Changed;
}
enum FrameHelperType { Prolog, PrologFrame, Epilog, EpilogTail };

/// Return a frame helper name with the given CSRs and the helper type.
/// For instance, a prolog helper that saves x19 and x20 is named as
/// OUTLINED_FUNCTION_PROLOG_x19x20.
static std::string getFrameHelperName(SmallVectorImpl<unsigned> &Regs,
                                      FrameHelperType Type, unsigned FpOffset) {
  std::ostringstream RegStream;
  switch (Type) {
  case FrameHelperType::Prolog:
    RegStream << "OUTLINED_FUNCTION_PROLOG_";
    break;
  case FrameHelperType::PrologFrame:
    RegStream << "OUTLINED_FUNCTION_PROLOG_FRAME" << FpOffset << "_";
    break;
  case FrameHelperType::Epilog:
    RegStream << "OUTLINED_FUNCTION_EPILOG_";
    break;
  case FrameHelperType::EpilogTail:
    RegStream << "OUTLINED_FUNCTION_EPILOG_TAIL_";
    break;
  }

  for (auto Reg : Regs)
    RegStream << AArch64InstPrinter::getRegisterName(Reg);

  return RegStream.str();
}

/// Create a Function for the unique frame helper with the given name.
/// Return a newly created MachineFunction with an empty MachineBasicBlock.
static MachineFunction &createFrameHelperMachineFunction(Module *M,
                                                         MachineModuleInfo *MMI,
                                                         StringRef Name) {
  LLVMContext &C = M->getContext();
  Function *F = M->getFunction(Name);
  assert(F == nullptr && "Function has been created before");
  F = Function::Create(FunctionType::get(Type::getVoidTy(C), false),
                       Function::ExternalLinkage, Name, M);
  assert(F && "Function was null!");

  // Use ODR linkage to avoid duplication.
  F->setLinkage(GlobalValue::LinkOnceODRLinkage);
  F->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);

  // Set no-opt/minsize, so we don't insert padding between outlined
  // functions.
  F->addFnAttr(Attribute::OptimizeNone);
  F->addFnAttr(Attribute::NoInline);
  F->addFnAttr(Attribute::MinSize);
  F->addFnAttr(Attribute::Naked);

  MachineFunction &MF = MMI->getOrCreateMachineFunction(*F);
  // Remove unnecessary register liveness and set NoVRegs.
  MF.getProperties().reset(MachineFunctionProperties::Property::TracksLiveness);
  MF.getProperties().reset(MachineFunctionProperties::Property::IsSSA);
  MF.getProperties().set(MachineFunctionProperties::Property::NoVRegs);
  MF.getRegInfo().freezeReservedRegs(MF);

  // Create entry block.
  BasicBlock *EntryBB = BasicBlock::Create(C, "entry", F);
  IRBuilder<> Builder(EntryBB);
  Builder.CreateRetVoid();

  // Insert the new block into the function.
  MachineBasicBlock *MBB = MF.CreateMachineBasicBlock();
  MF.insert(MF.begin(), MBB);

  return MF;
}

/// Emit a homogeneous store-pair instruction for frame-setup.
static void emitHomogeneousStore(MachineFunction &MF, MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator Pos,
                                 const TargetInstrInfo &TII, unsigned Reg1,
                                 unsigned Reg2) {
  bool IsFloat = AArch64::FPR64RegClass.contains(Reg1);
  assert(!(IsFloat ^ AArch64::FPR64RegClass.contains(Reg2)));
  int Opc = IsFloat ? AArch64::STPDpre : AArch64::STPXpre;
  MachineInstrBuilder MIB = BuildMI(MBB, Pos, DebugLoc(), TII.get(Opc));
  MIB.addDef(AArch64::SP)
      .addReg(Reg2)
      .addReg(Reg1)
      .addReg(AArch64::SP)
      .addImm(-2)
      .addMemOperand(
          MF.getMachineMemOperand(MachinePointerInfo::getUnknownStack(MF),
                                  MachineMemOperand::MOStore, 8, Align(8)))
      .addMemOperand(
          MF.getMachineMemOperand(MachinePointerInfo::getUnknownStack(MF),
                                  MachineMemOperand::MOStore, 8, Align(8)))
      .setMIFlag(MachineInstr::FrameSetup);
}

/// Emit a homogeneous load-pair instruction for frame-destroy.
static void emitHomogeneousLoad(MachineFunction &MF, MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator Pos,
                                const TargetInstrInfo &TII, unsigned Reg1,
                                unsigned Reg2) {
  bool IsFloat = AArch64::FPR64RegClass.contains(Reg1);
  assert(!(IsFloat ^ AArch64::FPR64RegClass.contains(Reg2)));
  int Opc = IsFloat ? AArch64::LDPDpost : AArch64::LDPXpost;
  MachineInstrBuilder MIB = BuildMI(MBB, Pos, DebugLoc(), TII.get(Opc));
  MIB.addDef(AArch64::SP)
      .addReg(Reg2)
      .addReg(Reg1)
      .addReg(AArch64::SP)
      .addImm(2)
      .addMemOperand(
          MF.getMachineMemOperand(MachinePointerInfo::getUnknownStack(MF),
                                  MachineMemOperand::MOLoad, 8, Align(8)))
      .addMemOperand(
          MF.getMachineMemOperand(MachinePointerInfo::getUnknownStack(MF),
                                  MachineMemOperand::MOLoad, 8, Align(8)))
      .setMIFlag(MachineInstr::FrameDestroy);
}

/// Return a unique function if a helper can be formed with the given Regs
/// and frame type.
/// 1) _OUTLINED_FUNCTION_PROLOG_x19x20x21x22:
///    stp x20, x19, [sp, #-16]!
///    stp x22, x21, [sp, #-16]!
///    ret
///
/// 2) _OUTLINED_FUNCTION_PROLOG_x19x20x30x29x21x22:
///    mov x16, x30
///    ldp x29, x30, [sp], #16      ; Restore x29/x30 stored at the caller
///    stp x20, x19, [sp, #-16]!
///    stp x29, x30, [sp, #-16]!    ; Save x29/30 (NeedSaveLR = true)
///    stp x22, x21, [sp, #-16]!
///    br x16
///
/// 3) _OUTLINED_FUNCTION_PROLOG_FRAME32_x19x20x21x22:
///    stp x20, x19, [sp, #-16]!
///    stp x22, x21, [sp, #-16]!
///    add fp, sp, #32
///    ret
///
/// 4) _OUTLINED_FUNCTION_PROLOG_FRAME0_x19x20x30x29x21x22:
///    mov x16, x30
///    ldp x29, x30, [sp], #16      ; Restore x29/x30 stored at the caller
///    stp x20, x19, [sp, #-16]!
///    stp x29, x30, [sp, #-16]!    ; Save x29/30 (NeedSaveLR = true)
///    stp x22, x21, [sp, #-16]!
///    mov fp, sp
///    br x16
///
/// 5) _OUTLINED_FUNCTION_EPILOG_x30x29x19x20x21x22:
///    mov x16, x30
///    ldp x22, x21, [sp], #16
///    ldp x20, x19, [sp], #16
///    ldp x29, x30, [sp], #16
///    br x16
///
/// 6) _OUTLINED_FUNCTION_EPILOG_TAIL_x30x29x19x20x21x22:
///    ldp x22, x21, [sp], #16
///    ldp x20, x19, [sp], #16
///    ldp x29, x30, [sp], #16
///    ret
/// @param M module
/// @param MMI machine module info
/// @param Regs callee save regs that the helper will handle
/// @param Type frame helper type
/// @return a helper function
static Function *getOrCreateFrameHelper(Module *M, MachineModuleInfo *MMI,
                                        SmallVectorImpl<unsigned> &Regs,
                                        FrameHelperType Type,
                                        unsigned FpOffset = 0) {
  assert(Regs.size() >= 2);
  bool NeedSaveLR = false;
  if (Type == FrameHelperType::Prolog || Type == FrameHelperType::PrologFrame) {
    // When FP/LR is the first pair, it has been already saved in the caller.
    NeedSaveLR = Regs[0] != AArch64::LR;
    if (!NeedSaveLR) {
      // Prolog helpers do not need to store FP/LR
      Regs.erase(Regs.begin());
      Regs.erase(Regs.begin());
    }
  }

  auto Name = getFrameHelperName(Regs, Type, FpOffset);
  auto F = M->getFunction(Name);
  if (F)
    return F;

  auto &MF = createFrameHelperMachineFunction(M, MMI, Name);
  MachineBasicBlock &MBB = *MF.begin();
  const TargetSubtargetInfo &STI = MF.getSubtarget();
  const TargetInstrInfo &TII = *STI.getInstrInfo();

  if (NeedSaveLR) {
    // Stash LR to X16
    BuildMI(MBB, MBB.end(), DebugLoc(), TII.get(AArch64::ORRXrs))
        .addDef(AArch64::X16)
        .addReg(AArch64::XZR)
        .addUse(AArch64::LR)
        .addImm(0);
    // Restore FP/LR from the stack
    emitHomogeneousLoad(MF, MBB, MBB.end(), TII, AArch64::LR, AArch64::FP);
  }

  int Size = (int)Regs.size();
  switch (Type) {
  case FrameHelperType::Prolog:
    for (int I = 0; I < Size; I += 2)
      emitHomogeneousStore(MF, MBB, MBB.end(), TII, Regs[I], Regs[I + 1]);
    if (NeedSaveLR)
      BuildMI(MBB, MBB.end(), DebugLoc(), TII.get(AArch64::BR))
          .addUse(AArch64::X16);
    else
      BuildMI(MBB, MBB.end(), DebugLoc(), TII.get(AArch64::RET))
          .addReg(AArch64::LR, RegState::Undef);
    break;

  case FrameHelperType::PrologFrame:
    for (int I = 0; I < Size; I += 2)
      emitHomogeneousStore(MF, MBB, MBB.end(), TII, Regs[I], Regs[I + 1]);
    BuildMI(MBB, MBB.end(), DebugLoc(), TII.get(AArch64::ADDXri))
        .addDef(AArch64::FP)
        .addUse(AArch64::SP)
        .addImm(FpOffset)
        .addImm(0)
        .setMIFlag(MachineInstr::FrameSetup);
    if (NeedSaveLR)
      BuildMI(MBB, MBB.end(), DebugLoc(), TII.get(AArch64::BR))
          .addUse(AArch64::X16);
    else
      BuildMI(MBB, MBB.end(), DebugLoc(), TII.get(AArch64::RET))
          .addReg(AArch64::LR, RegState::Undef);
    break;

  case FrameHelperType::Epilog:
    // Stash LR to X16
    BuildMI(MBB, MBB.end(), DebugLoc(), TII.get(AArch64::ORRXrs))
        .addDef(AArch64::X16)
        .addReg(AArch64::XZR)
        .addUse(AArch64::LR)
        .addImm(0);
    // Restore CSRs in the reverse order
    for (int I = Size - 1; I >= 0; I -= 2)
      emitHomogeneousLoad(MF, MBB, MBB.end(), TII, Regs[I - 1], Regs[I]);
    // Branch on X16 not to trash LR.
    BuildMI(MBB, MBB.end(), DebugLoc(), TII.get(AArch64::BR))
        .addUse(AArch64::X16);
    break;

  case FrameHelperType::EpilogTail:
    // Restore CSRs in the reverse order
    for (int I = Size - 1; I >= 0; I -= 2)
      emitHomogeneousLoad(MF, MBB, MBB.end(), TII, Regs[I - 1], Regs[I]);
    BuildMI(MBB, MBB.end(), DebugLoc(), TII.get(AArch64::RET))
        .addReg(AArch64::LR, RegState::Undef);
    break;
  }

  return M->getFunction(Name);
}

/// Get a valid non-negative adjustment to set fp from sp.
/// @param MBBI instruciton setting fp from sp.
/// @return a valid non-negative adjustment. Or -1 for any other case.
int getFpAdjustmentFromSp(MachineBasicBlock::iterator &MBBI) {
  MachineInstr &MI = *MBBI;
  if (!MI.getFlag(MachineInstr::FrameSetup))
    return -1;
  unsigned Opcode = MI.getOpcode();
  if (Opcode != AArch64::ADDXri && Opcode != AArch64::SUBXri)
    return -1;
  if (!MI.getOperand(0).isReg())
    return -1;
  if (MI.getOperand(0).getReg() != AArch64::FP)
    return -1;
  if (!MI.getOperand(1).isReg())
    return -1;
  if (MI.getOperand(1).getReg() != AArch64::SP)
    return -1;

  int Imm = MI.getOperand(2).getImm();
  if (Opcode == AArch64::ADDXri && Imm >= 0)
    return Imm;
  else if (Opcode == AArch64::SUBXri && Imm <= 0)
    return -Imm;

  return -1;
}

/// This function checks if a frame helper should be used for
/// HOM_Prolog/HOM_Epilog pseudo instruction expansion.
/// @param MBB machine basic block
/// @param NextMBBI  next instruction following HOM_Prolog/HOM_Epilog
/// @param Regs callee save registers that are saved or restored.
/// @param Type frame helper type
/// @return True if a use of helper is qualified.
static bool shouldUseFrameHelper(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator &NextMBBI,
                                 SmallVectorImpl<unsigned> &Regs,
                                 FrameHelperType Type) {
  int RegCount = (int)Regs.size();
  assert(RegCount > 0 && (RegCount % 2 == 0));
  // # of instructions that will be outlined.
  int InstCount = RegCount >> 1;

  // Do not use a helper call when not saving LR.
  if (std::find(Regs.begin(), Regs.end(), AArch64::LR) == Regs.end())
    return false;

  switch (Type) {
  case FrameHelperType::Prolog:
    // Prolog helper cannot save FP/LR.
    InstCount--;
    break;
  case FrameHelperType::PrologFrame: {
    // Prolog helper cannot save FP/LR.
    // Check if the following instruction is beneficial to be included.
    if (NextMBBI == MBB.end())
      return false;
    int FpAdjustment = getFpAdjustmentFromSp(NextMBBI);
    if (FpAdjustment == -1)
      return false;
    // Effecitvely no change in InstCount since FpAdjusment is included.
    break;
  }
  case FrameHelperType::Epilog:
    // No change in InstCount for the regular epilog case.
    break;
  case FrameHelperType::EpilogTail: {
    // EpilogTail helper includes the caller's return.
    if (NextMBBI == MBB.end())
      return false;
    if (NextMBBI->getOpcode() != AArch64::RET_ReallyLR)
      return false;
    InstCount++;
    break;
  }
  }

  return InstCount >= FrameHelperSizeThreshold;
}

/// Lower a HOM_Epilog pseudo instruction into a helper call while
/// creating the helper on demand. Or emit a sequence of homogeneous loads in
/// place when not using a helper call.
///
/// 1. With a helper including ret
///    HOM_Epilog x30, x29, x19, x20, x21, x22              ; MBBI
///    ret                                                  ; NextMBBI
///    =>
///    b _OUTLINED_FUNCTION_EPILOG_TAIL_x30x29x19x20x21x22
///    ...                                                  ; NextMBBI
///
/// 2. With a helper
///    HOM_Epilog x30, x29, x19, x20, x21, x22
///    =>
///    bl _OUTLINED_FUNCTION_EPILOG_x30x29x19x20x21x22
///
/// 3. Without a helper
///    HOM_Epilog x30, x29, x19, x20, x21, x22
///    =>
///    ldp x22, x21, [sp], #16
///    ldp x20, x19, [sp], #16
///    ldp x29, x30, [sp], #16
bool AArch64LowerHomogeneousPE::lowerHOM_Epilog(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI) {
  auto &MF = *MBB.getParent();
  MachineInstr &MI = *MBBI;

  DebugLoc DL = MI.getDebugLoc();
  SmallVector<unsigned, 8> Regs;
  for (auto &MO : MI.implicit_operands())
    if (MO.isReg())
      Regs.push_back(MO.getReg());
  int Size = (int)Regs.size();
  if (Size == 0)
    return false;
  // Registers are in pair.
  assert(Size % 2 == 0);
  assert(MI.getOpcode() == AArch64::HOM_Epilog);

  auto Return = NextMBBI;
  if (shouldUseFrameHelper(MBB, NextMBBI, Regs, FrameHelperType::EpilogTail)) {
    // When MBB ends with a return, emit a tail-call to the epilog helper
    auto EpilogTailHelper =
        getOrCreateFrameHelper(M, MMI, Regs, FrameHelperType::EpilogTail);
    BuildMI(MBB, MBBI, DL, TII->get(AArch64::TCRETURNdi))
        .addGlobalAddress(EpilogTailHelper)
        .addImm(0)
        .setMIFlag(MachineInstr::FrameDestroy)
        .copyImplicitOps(MI)
        .copyImplicitOps(*Return);
    NextMBBI = std::next(Return);
    Return->removeFromParent();
  } else if (shouldUseFrameHelper(MBB, NextMBBI, Regs,
                                  FrameHelperType::Epilog)) {
    // The default epilog helper case.
    auto EpilogHelper =
        getOrCreateFrameHelper(M, MMI, Regs, FrameHelperType::Epilog);
    BuildMI(MBB, MBBI, DL, TII->get(AArch64::BL))
        .addGlobalAddress(EpilogHelper)
        .setMIFlag(MachineInstr::FrameDestroy)
        .copyImplicitOps(MI);
  } else {
    // Fall back to no-helper.
    for (int I = Size - 1; I >= 0; I -= 2)
      emitHomogeneousLoad(MF, MBB, MBBI, *TII, Regs[I - 1], Regs[I]);
  }

  MBBI->removeFromParent();
  return true;
}

/// Lower a HOM_Prolog pseudo instruction into a helper call while
/// creating the helper on demand. Or emit a sequence of homogeneous stores in
/// place when not using a helper call.
///
/// 1. With a helper including frame-setup
///    HOM_Prolog x30, x29, x19, x20, x21, x22      ; MBBI
///    add x29, x30, #32                            ; NextMBBI
///    =>
///    stp x29, x30, [sp, #-16]!
///    bl _OUTLINED_FUNCTION_PROLOG_FRAME32_x19x20x21x22
///    ...                                          ; NextMBBI
///
/// 2. With a helper
///    HOM_Prolog x30, x29, x19, x20, x21, x22
///    =>
///    stp x29, x30, [sp, #-16]!
///    bl _OUTLINED_FUNCTION_PROLOG_x19x20x21x22
///
/// 3. Without a helper
///    HOM_Prolog x30, x29, x19, x20, x21, x22
///    =>
///    stp x29, x30, [sp, #-16]!
///    stp x20, x19, [sp, #-16]!
///    stp x22, x21, [sp, #-16]!
bool AArch64LowerHomogeneousPE::lowerHOM_Prolog(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI) {
  auto &MF = *MBB.getParent();
  MachineInstr &MI = *MBBI;

  DebugLoc DL = MI.getDebugLoc();
  SmallVector<unsigned, 8> Regs;
  for (auto &MO : MI.implicit_operands())
    if (MO.isReg())
      Regs.push_back(MO.getReg());
  int Size = (int)Regs.size();
  if (Size == 0)
    return false;
  // Allow compact unwind case only for oww.
  assert(Size % 2 == 0);
  assert(MI.getOpcode() == AArch64::HOM_Prolog);

  auto FpAdjustment = NextMBBI;
  if (shouldUseFrameHelper(MBB, NextMBBI, Regs, FrameHelperType::PrologFrame)) {
    // FP/LR is stored at the top of stack before the prolog helper call.
    emitHomogeneousStore(MF, MBB, MBBI, *TII, AArch64::LR, AArch64::FP);
    auto FpOffset = getFpAdjustmentFromSp(NextMBBI);
    auto PrologFrameHelper = getOrCreateFrameHelper(
        M, MMI, Regs, FrameHelperType::PrologFrame, FpOffset);
    BuildMI(MBB, MBBI, DL, TII->get(AArch64::BL))
        .addGlobalAddress(PrologFrameHelper)
        .setMIFlag(MachineInstr::FrameSetup)
        .copyImplicitOps(MI)
        .copyImplicitOps(*FpAdjustment)
        .addReg(AArch64::FP, RegState::Implicit | RegState::Define)
        .addReg(AArch64::SP, RegState::Implicit);
    NextMBBI = std::next(FpAdjustment);
    FpAdjustment->removeFromParent();
  } else if (shouldUseFrameHelper(MBB, NextMBBI, Regs,
                                  FrameHelperType::Prolog)) {
    // FP/LR is stored at the top of stack before the prolog helper call.
    emitHomogeneousStore(MF, MBB, MBBI, *TII, AArch64::LR, AArch64::FP);
    auto PrologHelper =
        getOrCreateFrameHelper(M, MMI, Regs, FrameHelperType::Prolog);
    BuildMI(MBB, MBBI, DL, TII->get(AArch64::BL))
        .addGlobalAddress(PrologHelper)
        .setMIFlag(MachineInstr::FrameSetup)
        .copyImplicitOps(MI);
  } else {
    // Fall back to no-helper.
    for (int I = 0; I < Size; I += 2)
      emitHomogeneousStore(MF, MBB, MBBI, *TII, Regs[I], Regs[I + 1]);
  }

  MBBI->removeFromParent();
  return true;
}

/// Process each machine instruction
/// @param MBB machine basic block
/// @param MBBI current instruction iterator
/// @param NextMBBIT next instruction iterator which can be updated
/// @return True when IR is changed.
bool AArch64LowerHomogeneousPE::runOnMI(MachineBasicBlock &MBB,
                                        MachineBasicBlock::iterator MBBI,
                                        MachineBasicBlock::iterator &NextMBBI) {
  MachineInstr &MI = *MBBI;
  unsigned Opcode = MI.getOpcode();
  switch (Opcode) {
  default:
    break;
  case AArch64::HOM_Prolog:
    return lowerHOM_Prolog(MBB, MBBI, NextMBBI);
  case AArch64::HOM_Epilog:
    return lowerHOM_Epilog(MBB, MBBI, NextMBBI);
  }
  return false;
}

bool AArch64LowerHomogeneousPE::runOnMBB(MachineBasicBlock &MBB) {
  bool Modified = false;

  MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
  while (MBBI != E) {
    MachineBasicBlock::iterator NMBBI = std::next(MBBI);
    Modified |= runOnMI(MBB, MBBI, NMBBI);
    MBBI = NMBBI;
  }

  return Modified;
}

bool AArch64LowerHomogeneousPE::runOnMachineFunction(MachineFunction &MF) {
  TII = static_cast<const AArch64InstrInfo *>(MF.getSubtarget().getInstrInfo());

  bool Modified = false;
  for (auto &MBB : MF)
    Modified |= runOnMBB(MBB);
  return Modified;
}

ModulePass *llvm::createAArch64LowerHomogeneousPrologEpilogPass() {
  return new AArch64LowerHomogeneousPrologEpilog();
}
