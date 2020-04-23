//===------------- AArch64ObjCCallOpt.cpp - Object-C Call Opt -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// This pass replaces Object-C call and its arguments setup by a custom call
// with compactly encoded arguments for the size optimization.
//===----------------------------------------------------------------------===//

#include "AArch64.h"
#include "AArch64InstrInfo.h"
#include "AArch64Subtarget.h"
#include "MCTargetDesc/AArch64InstPrinter.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <iomanip>
#include <sstream>

using namespace llvm;

#define DEBUG_TYPE "aarch64-objc-call-opt"

cl::opt<bool>
    ObjCCallOpt("objc-call-opt", cl::init(false), cl::Hidden,
                cl::desc("Optimize Object-C call for size (default = off)"));

namespace {
typedef enum {
  Invalid,
  Release, // objc_release
  MsgSend, // objc_msgSend
  Retain,  // objc_retain
} ObjCCallType;

struct CallCandidate {
  MachineInstr *Call;
  ObjCCallType Type;
  SmallVector<MachineInstr *, 5> Args;
  CallCandidate(MachineInstr *C, ObjCCallType T) : Call(C), Type(T) {}
  CallCandidate(MachineInstr *C) : CallCandidate(C, ObjCCallType::Invalid) {}
};

struct AArch64ObjCCallOpt : public ModulePass {

  static char ID;

  StringRef getPassName() const override { return "AArch64 Object-C Call Opt"; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineModuleInfoWrapperPass>();
    AU.addPreserved<MachineModuleInfoWrapperPass>();
    AU.setPreservesAll();
    ModulePass::getAnalysisUsage(AU);
  }

  AArch64ObjCCallOpt() : ModulePass(ID) {
    initializeAArch64ObjCCallOptPass(*PassRegistry::getPassRegistry());
  }

  bool runOnModule(Module &M) override;

private:
  bool runOnFunction(MachineFunction *MF);
  bool runOnBlock(MachineBasicBlock &MBB);

  /// Find candidates for the block.
  void findCandidates(MachineBasicBlock &MBB,
                      SmallVectorImpl<CallCandidate> &Candidates);
  /// Optimize candidates for the block.
  bool optimizeCandidates(MachineBasicBlock &MBB,
                          SmallVectorImpl<CallCandidate> &Candidates);

  /// Create a custom function whose body has an empty block.
  MachineFunction &createCustomMachineFunction(StringRef Name);

  // Return a candidate for call instr and call type.
  // The corresponding argument instructions are identified for the candidate.
  // This may return an invalid candidate.
  CallCandidate createCandidate(MachineInstr &MI, ObjCCallType Type);

  /// Optimize objc_release with a reg-copy arg.
  /// This replaces them by a single helper call.
  void optimizeRelease(CallCandidate &Cand);

  StringRef getReleaseHelperPrefix() { return "OUTLINED_FUNCTION_RELEASE_"; }

  Module *Mod;
  MachineModuleInfo *MMI;
};

} // Anonymous namespace.

char AArch64ObjCCallOpt::ID = 0;

namespace llvm {
ModulePass *createAArch64ObjCCallOptPass() { return new AArch64ObjCCallOpt(); }
} // namespace llvm

INITIALIZE_PASS(AArch64ObjCCallOpt, DEBUG_TYPE, "AArch64 Object-C Call Opt",
                false, false)

MachineFunction &
AArch64ObjCCallOpt::createCustomMachineFunction(StringRef Name) {
  LLVMContext &C = Mod->getContext();
  auto FC = Mod->getOrInsertFunction(Name, Type::getVoidTy(C));
  Function *F = dyn_cast<Function>(FC.getCallee());
  assert(F && "Function was null!");

  // Use ODR linkage to avoid duplication.
  F->setLinkage(GlobalValue::LinkOnceODRLinkage);
  F->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);

  // Set optsize/minsize, so we don't insert padding between outlined
  // functions.
  F->addFnAttr(Attribute::OptimizeForSize);
  F->addFnAttr(Attribute::MinSize);
  F->addFnAttr(Attribute::Naked);
  F->addFnAttr(Attribute::NoInline);

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

// Clone old instr and insert new one to the end of block.
static MachineInstr *cloneMachineInstrForOutlining(MachineFunction &MF,
                                                   MachineBasicBlock &MBB,
                                                   MachineInstr *OldMI) {
  MachineInstr *NewMI = MF.CloneMachineInstr(OldMI);
  NewMI->dropMemRefs(MF);
  NewMI->setDebugLoc(DebugLoc());
  MBB.insert(MBB.end(), NewMI);

  return NewMI;
}

void AArch64ObjCCallOpt::optimizeRelease(CallCandidate &Cand) {
  assert(Cand.Type == ObjCCallType::Release);
  assert(Cand.Args.size() == 1);
  auto Arg = Cand.Args[0];
  auto Call = Cand.Call;
  assert(AArch64InstrInfo::isGPRCopy(*Arg));
  int Reg = AArch64InstrInfo::getGPRCopySrc(*Arg)->getReg();
  // On null arg to release, it is effecitvely no-opt.
  if (Reg != AArch64::XZR) {
    // Create a helper name by appending register number to the prefix.
    std::ostringstream NameStream;
    NameStream << std::string(getReleaseHelperPrefix());
    NameStream << AArch64InstPrinter::getRegisterName(Reg);
    std::string Name = NameStream.str();

    MachineBasicBlock *MBB = Call->getParent();
    MachineFunction &MF = *MBB->getParent();
    const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();

    // Try to get a helper symbol if one has been already made.
    const Function *Helper = Mod->getFunction(Name);
    if (Helper == nullptr) {
      MachineFunction &NMF = createCustomMachineFunction(Name);
      MachineBasicBlock &NMBB = *NMF.begin();

      // Clone Arg and Call for the new helper.
      cloneMachineInstrForOutlining(NMF, NMBB, Arg);
      auto NMI = cloneMachineInstrForOutlining(NMF, NMBB, Call);
      // Turn the call to a tail-call to save a return.
      NMI->setDesc(TII.get(AArch64::TCRETURNdi));

      Helper = Mod->getFunction(Name);
      LLVM_DEBUG(NMF.dump());
    }

    // Insert a call to the helper
    BuildMI(*MBB, Call, Call->getDebugLoc(), TII.get(AArch64::BL))
        .addGlobalAddress(Helper)
        .addReg(Reg, RegState::Implicit)
        .copyImplicitOps(*Call)
        .copyImplicitOps(*Arg);
  }

  // Remove unnecessary Arg and Call
  Arg->removeFromParent();
  Call->eraseFromParent();
}

static ObjCCallType getObjCCallType(MachineInstr &MI) {
  if (MI.getOpcode() != AArch64::BL)
    return ObjCCallType::Invalid;

  MachineOperand &Op = MI.getOperand(0);
  if (!Op.isGlobal())
    return ObjCCallType::Invalid;

  auto TargetName = Op.getGlobal()->getName();
  if (TargetName == "objc_release")
    return ObjCCallType::Release;

  // TODO: Add call types as we handle more.

  return ObjCCallType::Invalid;
}

CallCandidate AArch64ObjCCallOpt::createCandidate(MachineInstr &MI,
                                                  ObjCCallType Type) {
  CallCandidate Cand(&MI);
  int NumArgs = 0;
  switch (Type) {
  case ObjCCallType::Release:
    NumArgs = 1;
    break;
  default:
    Type = ObjCCallType::Invalid;
    break;
  }
  if (NumArgs == 0)
    return Cand;
  Cand.Args.resize(NumArgs, nullptr);

  // TODO: Use a (def/use) graph to find args for the call candidate.
  MachineBasicBlock *MBB = MI.getParent();
  for (int ArgIdx = 0; ArgIdx < NumArgs; ++ArgIdx) {
    // TODO: The below logic does not consider sub-register case.
    int Reg = AArch64::X0 + ArgIdx;
    MachineBasicBlock::reverse_iterator It = &MI, Er = MBB->rend();
    for (++It; It != Er; ++It) {
      auto Def = &*It;
      if (Def->isDebugInstr())
        continue;
      if (Def->definesRegister(Reg)) {
        assert(!Cand.Args[ArgIdx]);
        Cand.Args[ArgIdx] = Def;
        break;
      }
    }
  }

  // Check if Arg is a valid one we can handle.
  auto TRI = MBB->getParent()->getSubtarget().getRegisterInfo();
  for (auto &Arg : Cand.Args) {
    // TODO: Handle a copy arg only for now.
    if (Arg == nullptr || !AArch64InstrInfo::isGPRCopy(*Arg)) {
      Type = ObjCCallType::Invalid;
      break;
    }
    // Ensure if the arg can be moved down without clobbering.
    int Reg = AArch64InstrInfo::getGPRCopySrc(*Arg)->getReg();
    MachineBasicBlock::iterator It = Arg, Er = &MI;
    for (++It; It != Er; ++It) {
      auto Def = &*It;
      if (Def->isDebugInstr())
        continue;
      if (Def->modifiesRegister(Reg, TRI)) {
        Type = ObjCCallType::Invalid;
        break;
      }
    }
  }

  // Update type to candidate.
  Cand.Type = Type;
  return Cand;
}

bool AArch64ObjCCallOpt::optimizeCandidates(
    MachineBasicBlock &MBB, SmallVectorImpl<CallCandidate> &Candidates) {
  bool Changed = false;
  for (auto &Cand : Candidates) {
    switch (Cand.Type) {
    case ObjCCallType::Release:
      optimizeRelease(Cand);
      Changed = true;
      break;
    default:;
    }
  }
  return Changed;
}

void AArch64ObjCCallOpt::findCandidates(
    MachineBasicBlock &MBB, SmallVectorImpl<CallCandidate> &Candidtes) {
  for (auto &MI : MBB) {
    if (MI.isDebugInstr() || !MI.isCall())
      continue;

    ObjCCallType Type = getObjCCallType(MI);
    if (Type == ObjCCallType::Invalid)
      continue;

    CallCandidate Cand = createCandidate(MI, Type);
    if (Cand.Type == ObjCCallType::Invalid)
      continue;

    Candidtes.push_back(Cand);
  }
}

bool AArch64ObjCCallOpt::runOnBlock(MachineBasicBlock &MBB) {
  // TODO: Build Def/Use Chain to avoid repeated IR traversals.

  // Find call candidates
  SmallVector<CallCandidate, 5> Candidates;
  findCandidates(MBB, Candidates);

  // Optimize candidates
  bool Changed = optimizeCandidates(MBB, Candidates);

  return Changed;
}

bool AArch64ObjCCallOpt::runOnFunction(MachineFunction *MF) {
  bool Changed = false;
  auto &F = MF->getFunction();
  if (F.empty())
    return false;
  if (!F.hasOptSize())
    return false;
  if (F.getName().startswith("OUTLINED_FUNCTION_"))
    return false;

  for (auto &MBB : *MF)
    Changed |= runOnBlock(MBB);
  return Changed;
}

bool AArch64ObjCCallOpt::runOnModule(Module &M) {
  if (M.empty())
    return false;

  bool Changed = false;
  Mod = &M;
  MMI = &getAnalysis<MachineModuleInfoWrapperPass>().getMMI();
  LLVM_DEBUG(dbgs() << "***** AArch64ObjCCallOpt *****\n");

  // TODO: Gather statistic for all callsites to automate custom call selection.
  // For now, a helper call is aggressively formed on demand.
  for (auto &F : M) {
    MachineFunction *MF = MMI->getMachineFunction(F);
    if (!MF)
      continue;
    Changed |= runOnFunction(MF);
  }

  return Changed;
}
