//===------- MachineOutlinerGlobal.h - Outliner data structures --*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Contains all data structures shared between the outliner implemented in
/// MachineOutlinerGlobal.cpp .
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_MACHINEOUTLINERGLOBAL_H
#define LLVM_MACHINEOUTLINERGLOBAL_H

#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/LiveRegUnits.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/Support/SuffixTree.h"

#include "MachineOutliner.h"
#include "StableHashTree.h"
#include "StableHashing.h"

using namespace llvm;
using namespace outliner;

void findGlobalCandidatesFromHashTree(
    SuffixTree &ST, std::vector<MachineBasicBlock::iterator> &InstrList,
    DenseMap<MachineBasicBlock *, unsigned> &MBBFlagsMap,
    std::vector<unsigned> &UnsignedVec,
    std::vector<OutlinedFunction> &FunctionList, StableHashTree &OutlinerHashTree);

// In ThinLTO, we might be building or using a hash tree.
enum class HashTreeMode {
  None,
  BuildingHashTree,
  UsingHashTree,
};

namespace llvm {
namespace outliner {
// Return true if outlining in MF is allowed given performance constraints.
bool allowOutline(MachineFunction &MF);

// Return true if outlining in MBB is allowed given performance constraints.
bool allowOutline(MachineBasicBlock &MBB);

HashTreeMode getMode();
void beginBuildingHashTree();
void beginUsingHashTree();

} // namespace outliner
} // namespace llvm
#endif
