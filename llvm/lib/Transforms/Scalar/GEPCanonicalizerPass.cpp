//===- GEPCanonicalizerPass.cpp - GEP Canonicalization --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
/// This transformation aims to alter patterns in address offset computation so
/// that it is more address computation code is outlined.
///
//===----------------------------------------------------------------------===//

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/ADT/GraphTraits.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Scalar/GepCanonicalization.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/FunctionComparator.h"
#include <algorithm>
#include <cassert>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

using namespace llvm;

#define DEBUG_TYPE "gepcanon"

cl::opt<unsigned> GEPGroupMinSize(
    "gep-canon-group-min-size", cl::Hidden, cl::init(2), cl::ZeroOrMore,
    cl::desc("For a group of GEPs with the same base pointer, skip "
             "canonicalizing if this threshold is not reached."));

cl::opt<unsigned>
    MinFunctionHashMatch("gep-canon-min-func-hash-match", cl::Hidden,
                         cl::init(1), cl::ZeroOrMore,
                         cl::desc("Only canonicalize GEPs for functions that "
                                  "have this many hash matches or more."));

namespace {

bool runImpl(Function &F) {
  bool Changed = false;

  LLVM_DEBUG(llvm::dbgs() << "GEP Canon Function: " << F.getName() << "\n";);

  // Create a groups of GEPs based on the base pointer they are taking the
  // offset for.
  std::map<Value *, std::vector<GetElementPtrInst *>> GEPGroups;

  for (auto &BB : F)
    for (auto &I : BB)
      if (auto *GEP = dyn_cast<GetElementPtrInst>(&I))
        GEPGroups[GEP->getOperand(0)].push_back(GEP);

  std::vector<Instruction *> BaseGeps;
  for (auto &GEPGroup : GEPGroups) {

    GetElementPtrInst *ShortGep = nullptr;
    for (auto *GEP : GEPGroup.second) {
      if (!ShortGep)
        ShortGep = GEP;
      if (GEP->getNumOperands() < ShortGep->getNumOperands())
        ShortGep = GEP;
    }

    unsigned MinMatchLength = ~0U;
    for (auto *GEP : GEPGroup.second) {
      unsigned MatchCount = 0;
      for (unsigned I = 1, E = ShortGep->getNumOperands(); I != E; ++I) {
        if (!isa<ConstantInt>(GEP->getOperand(I)) ||
            !isa<ConstantInt>(ShortGep->getOperand(I)))
          break;

        auto *CI1 = cast<ConstantInt>(GEP->getOperand(I));
        auto *CI2 = cast<ConstantInt>(ShortGep->getOperand(I));
        if (CI1->getZExtValue() != CI2->getZExtValue())
          break;
        MatchCount++;
      }
      MinMatchLength = std::min(MatchCount + 1, MinMatchLength);
    }

    std::vector<GetElementPtrInst *> GepsToRewrite;
    llvm::copy_if(GEPGroup.second, std::back_inserter(GepsToRewrite),
                  [&ShortGep](const GetElementPtrInst *GEP) {
                    if (GEP == ShortGep)
                      return false;

                    for (unsigned I = 1, E = ShortGep->getNumOperands(); I != E;
                         ++I) {

                      if (!isa<ConstantInt>(GEP->getOperand(I)) ||
                          !isa<ConstantInt>(ShortGep->getOperand(I)))
                        return false;

                      auto *CI1 = cast<ConstantInt>(GEP->getOperand(I));
                      auto *CI2 = cast<ConstantInt>(ShortGep->getOperand(I));
                      if (CI1->getZExtValue() != CI2->getZExtValue())
                        return false;
                    }

                    return true;
                  });

    // if all we have is a min match to `getelementptr %T, %T* %p, i64 0` then
    // it is not very useful to hoist: bail.
    if (MinMatchLength < 3)
      continue;

    // If we found too few GEPs that we could turn into dependent GEPs, bail.
    if (GEPGroupMinSize > 0 && GepsToRewrite.size() < GEPGroupMinSize - 1)
      continue;

    /// TODO: For now we only handle cases where the pointer operand domnates
    /// everything (ie function argument):
    if (isa<Instruction>(ShortGep->getPointerOperand()))
      continue;

    LLVM_DEBUG({
      const size_t GEPGroupSize = GEPGroup.second.size();
      llvm::dbgs() << "Found a hoistable GEP\n";
      llvm::dbgs() << "\tGroup size: " << GEPGroupSize << "\n";
      llvm::dbgs() << "\tFunction: " << F.getName() << "\n";
    });

    Changed = true;
    Instruction *CI = ShortGep->clone();
    CI->insertBefore(&*F.getEntryBlock().begin());
    ShortGep->replaceAllUsesWith(CI);
    ShortGep->eraseFromParent();
    BaseGeps.push_back(CI);

    for (auto *GEP : GepsToRewrite) {
      llvm::SmallVector<Value *, 6> Offsets;

      Offsets.push_back(
          ConstantInt::get(Type::getInt64Ty(F.getParent()->getContext()), 0));
      for (unsigned I = 0, E = GEP->getNumOperands(); I != E; ++I) {
        if (I < CI->getNumOperands())
          continue;
        Offsets.push_back(GEP->getOperand(I));
      }

      auto *NewGEP =
          GetElementPtrInst::Create(CI->getType()->getPointerElementType(), CI,
                                    Offsets, Twine(""), GEP /* InsertBefore */);

      GEP->replaceAllUsesWith(NewGEP);
      GEP->eraseFromParent();
    }
  }

  if (!BaseGeps.size())
    return Changed;

  // Easiest thing to do for outlining and calling the similar IR is to call
  // deleteBody on the original function. We will clone the base GEPs we want to
  // keep for later insertion.
  std::vector<Instruction *> BaseGepsClone;
  llvm::transform(BaseGeps, std::back_inserter(BaseGepsClone),
                  [](const Instruction *I) { return I->clone(); });

  ValueToValueMapTy VMap;
  std::vector<Type *> ArgTypes;
  for (auto II = F.arg_begin(), IE = F.arg_end(); II != IE; ++II)
    if (!VMap.count(&*II))
      ArgTypes.push_back(II->getType());

  llvm::transform(BaseGepsClone, std::back_inserter(ArgTypes),
                  [](const Value *V) { return V->getType(); });

  Function *NewF = Function::Create(
      FunctionType::get(F.getFunctionType()->getReturnType(), ArgTypes,
                        F.getFunctionType()->isVarArg()),
      F.getLinkage(), F.getAddressSpace(), F.getName().str() + "_GEPCANONED",
      F.getParent());

  Function::arg_iterator DestI = NewF->arg_begin();
  for (const Argument &I : F.args()) {
    if (VMap.count(&I) == 0) {
      DestI->setName(I.getName());
      VMap[&I] = &*DestI++;
    }
  }

  ClonedCodeInfo CodeInfo;
  SmallVector<ReturnInst *, 8> Returns; // Ignore returns cloned.
  CloneFunctionInto(NewF, &F, VMap, CloneFunctionChangeType::LocalChangesOnly,
                    Returns, "", &CodeInfo);

  std::vector<Value *> OldArgValues;
  std::vector<Value *> NewArgValues;
  for (auto II = F.arg_begin(), IE = F.arg_end(); II != IE; ++II)
    OldArgValues.push_back(II);
  for (auto II = NewF->arg_begin(), IE = NewF->arg_end(); II != IE; ++II)
    NewArgValues.push_back(II);

  std::vector<Value *> ArgsToMap;
  std::vector<Value *> GepsToMap;
  auto BBI = NewF->getEntryBlock().begin();
  for (unsigned I = OldArgValues.size(); I < NewArgValues.size(); ++I) {
    ArgsToMap.push_back(NewF->getArg(I));
    GepsToMap.push_back(&*BBI++);
  }

  // Order matters with what we are about to do here, and since we did did
  // BaseGeps.push_back() while we did llvm::Instruction::insertBefore, our
  // BaseGeps list is reversed from the Args and Instructions.
  std::reverse(GepsToMap.begin(), GepsToMap.end());
  assert(ArgsToMap.size() == GepsToMap.size() &&
         "Expected same number of GEPs and Args.");

  std::vector<std::tuple<Value *, Value *>> GEPReplacePairs;
  for (unsigned I = 0, E = ArgsToMap.size(); I != E; ++I)
    GEPReplacePairs.push_back({ArgsToMap[I], GepsToMap[I]});

  for (auto &GEPReplacePair : GEPReplacePairs) {
    auto *Arg = std::get<0>(GEPReplacePair);
    auto *GEP = cast<GetElementPtrInst>(std::get<1>(GEPReplacePair));
    GEP->replaceAllUsesWith(Arg);
    GEP->eraseFromParent();
  }

  NewF->copyAttributesFrom(&F);
  NewF->setDLLStorageClass(GlobalValue::DefaultStorageClass);
  NewF->setLinkage(llvm::GlobalValue::PrivateLinkage);
  NewF->addFnAttr("GEPCANONED");

  F.deleteBody();
  BasicBlock::Create(F.getContext())->insertInto(&F);

  SmallVector<Value *, 6> Args;
  for (auto II = F.arg_begin(), IE = F.arg_end(); II != IE; ++II)
    Args.push_back(II);
  llvm::copy(BaseGepsClone, std::back_inserter(Args));

  auto *CallI = CallInst::Create(NewF->getFunctionType(), NewF, Args, None, "",
                                 &F.getEntryBlock());
  CallI->setTailCall(true);
  CallI->setDebugLoc(BaseGepsClone.back()->getDebugLoc());

  if (F.getReturnType()->isVoidTy()) {
    ReturnInst::Create(F.getContext(), &F.getEntryBlock());
  } else {
    ReturnInst::Create(F.getContext(), CallI, &F.getEntryBlock());
  }

  for (auto *BaseGep : BaseGepsClone)
    BaseGep->insertBefore(CallI);

  return Changed;
}

/// Returns true if function \p F is eligible for merging.
static bool isEligibleFunction(Function *F) {
  if (F->isDeclaration())
    return false;

  if (F->hasAvailableExternallyLinkage())
    return false;

  if (F->getFunctionType()->isVarArg())
    return false;

  return true;
}

bool runImpl(Module &M) {

  // All functions in the module, ordered by hash. Functions with a unique
  // hash value are easily eliminated.
  std::unordered_map<FunctionComparator::FunctionHash, std::vector<Function *>>
      HashedFuncs;
  for (Function &Func : M)
    if (isEligibleFunction(&Func))
      HashedFuncs[FunctionComparator::functionHash(Func)].push_back(&Func);

  bool Changed = false;
  std::vector<Function *> OriginalFunctions;

  for (auto &E : HashedFuncs)
    if (E.second.size() >= MinFunctionHashMatch)
      for (auto *F : E.second)
        OriginalFunctions.push_back(F);

  for (auto *F : OriginalFunctions)
    Changed |= runImpl(*F);
  return Changed;
}

struct GepCanonicalizationLegacyPass : public ModulePass {
  static char ID;
  GepCanonicalizationLegacyPass() : ModulePass(ID) {
    initializeGepCanonicalizationLegacyPassPass(
        *PassRegistry::getPassRegistry());
  }

  bool runOnModule(Module &M) override { return runImpl(M); }

  StringRef getPassName() const override { return "gepcanon"; }
};
} // end anonymous namespace

char GepCanonicalizationLegacyPass::ID = 0;

INITIALIZE_PASS_BEGIN(GepCanonicalizationLegacyPass, "gepcanon",
                      "GEP Canonicalization", false, false)
INITIALIZE_PASS_END(GepCanonicalizationLegacyPass, "gepcanon",
                    "GEP Canonicalization", false, false)

PreservedAnalyses GepCanonicalizationPass::run(Module &M,
                                               ModuleAnalysisManager &MAM) {
  bool Changed = runImpl(M);
  if (Changed)
    return PreservedAnalyses::none();
  return PreservedAnalyses::all();
}
