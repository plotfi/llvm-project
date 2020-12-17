//===- StableHashTreeTest.cpp ---------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/StableHashTree.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineStableHash.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/ModuleSlotTracker.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "gtest/gtest.h"

using namespace llvm;

namespace {
// Include helper functions to ease the manipulation of MachineFunctions.
#include "MFCommon.inc"

TEST(HashBasicBlock, StableHashTreeTest) {
  LLVMContext Ctx;
  Module Mod("Module", Ctx);
  auto MF = createMachineFunction(Ctx, Mod);

  MCInstrDesc MCID1 = {0, 0, 0, 0, 0, 0, 0, nullptr, nullptr, nullptr};
  MCInstrDesc MCID2 = {1, 0, 0, 0, 0, 0, 0, nullptr, nullptr, nullptr};
  MCInstrDesc MCID3 = {2, 0, 0, 0, 0, 0, 0, nullptr, nullptr, nullptr};
  MCInstrDesc MCID4 = {3, 0, 0, 0, 0, 0, 0, nullptr, nullptr, nullptr};
  MCInstrDesc MCID5 = {4, 0, 0, 0, 0, 0, 0, nullptr, nullptr, nullptr};

  std::vector<std::vector<MCInstrDesc>> InstrSequences = {
      {MCID1, MCID2, MCID4},
      {MCID1, MCID3, MCID4},
      {MCID1, MCID3, MCID4, MCID5},
  };

  // Populate a Stable Hash Tree with Machine Instructions. Because this is a
  // Hash Trie the series of instructions should overlap and result in a tree
  // that is of depth 4 and of size 7.
  bool IsFirst = true;
  StableHashTree Tree;
  for (auto &IS : InstrSequences) {
    auto *MBB = MF->CreateMachineBasicBlock();
    for (auto &MCID : IS) {
      auto *MI = MF->CreateMachineInstr(MCID, DebugLoc());
      MBB->insert(MBB->end(), MI);
    }

    auto BI = MBB->begin();
    std::vector<std::vector<stable_hash>> HashList = {
        llvm::stableHashMachineInstrs(BI, MBB->end())};
    Tree.insert(HashList);

    if (IsFirst) {
      IsFirst = false;
      ASSERT_TRUE(Tree.depth() == 3);
    }
  }

  // Check depth and size of this tree as expected above.
  ASSERT_TRUE(Tree.depth() == 4);
  ASSERT_TRUE(Tree.size() == 7);

  // Since the purpose of Stable Hash Tree is partly for serializing, test
  // print.
  std::string Str;
  raw_string_ostream Sstr(Str);
  Tree.print(Sstr);

  // Now test that `Tree` is deserialized to a string, serialize it into Tree2.
  StableHashTree Tree2;
  if (auto Err = Tree2.readFromBuffer(StringRef(Str)))
    consumeError(std::move(Err));

  // Because the HashNode uses an unordered_map as a successor as we walk to
  // compare `Tree` and `Tree2` we must insert the hash values into the sorted
  // std::map and std::set for proper comparison.
  std::map<stable_hash, std::set<stable_hash>> HashValueMap1;
  std::map<stable_hash, std::set<stable_hash>> HashValueMap2;

  Tree.walkVertices([&HashValueMap1](const HashNode *N) {
    for (const auto &Succ : N->Successors)
      HashValueMap1[N->Hash].insert(Succ.first);
  });

  Tree2.walkVertices([&HashValueMap2](const HashNode *N) {
    for (const auto &Succ : N->Successors)
      HashValueMap2[N->Hash].insert(Succ.first);
  });

  ASSERT_TRUE(std::equal(HashValueMap1.begin(), HashValueMap1.end(),
                         HashValueMap2.begin()));
}

} // end namespace
