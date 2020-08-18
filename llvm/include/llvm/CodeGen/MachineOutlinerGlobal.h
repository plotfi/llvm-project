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

std::tuple<bool, bool, std::vector<stable_hash>>
getResidualCodeCosts(std::vector<Candidate> &CandidatesForRepeatedSeq,
                     StableHashTree &OutlinerHashTree);

void findSingletonCandidatesFromHashTree(
    SuffixTree &ST, std::vector<MachineBasicBlock::iterator> &InstrList,
    DenseMap<MachineBasicBlock *, unsigned> &MBBFlagsMap,
    std::vector<unsigned> &UnsignedVec,
    std::vector<OutlinedFunction> &FunctionList, StableHashTree &OutlinerHashTree);
#endif
