//===---- MachineOutlinerGlobal.cpp - Outline instructions --------*- C++ -===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// The machine outliner has special "global" outlining support for ThinLTO.
/// This kicks in when the -thinlto-two-codegen-rounds option is enabled in
/// ThinLTO. In this case, the machine outliner uses a new data structure to
/// exchange information between the two rounds: a tree of outlined instruction
/// sequence hashes. This data structure is provided by the new
/// MachineOutlinerState.* files.
///
/// During the first round, any instruction sequence that is beneficial to
/// outline in any particular module is hashed and entered as a chain of nodes
/// into the hash tree, with a terminal flag indicating whether a node
/// represents the end of an outlined instruction sequence. Hashing happens via
/// a set of "stable" hash functions for machine instructions, operand, and
/// whatever is relevant in an operand. All values are deep-hashed, e.g. going
/// through the characters of the names of external symbols. In practice, this
/// seems to give a very high precision in detecting exact duplicates across
/// modules.
///
/// During the second round, this hash tree is used in two ways:
/// - To boost candidate instruction sequences for outlining (i.e. instruction
///   sequences that occur at least twice within a module) that otherwise
///   wouldn't be considered as "beneficial" in terms of their frequency and the
///   cost of outlining.
/// - To discover additional "singleton" instruction sequences that only occur
///   once within a module, but still match a chain of nodes in the hash tree,
///   ending in a terminal node. In either case, we know that a corresponding
///   outlined function will get created in some module, so no additional cost
///   for outlining the function in other modules.
//===----------------------------------------------------------------------===//
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/Twine.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineOptimizationRemarkEmitter.h"
#include "llvm/CodeGen/MachineOutliner.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/MachineStableHash.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/StableHashTree.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Mangler.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/SuffixTree.h"
#include "llvm/Support/raw_ostream.h"
#include <functional>
#include <tuple>
#include <vector>

using namespace llvm;
using namespace ore;
using namespace outliner;

extern cl::opt<std::string> OutlinerHashTreeMode;

std::tuple<bool, bool, std::vector<stable_hash>>
getResidualCodeCosts(std::vector<Candidate> &CandidatesForRepeatedSeq,
                     StableHashTree &OutlinerHashTree) {
  // compute NoResidualCodeCost and StableHashSequence if desired
  bool NoResidualCodeCost = false;
  bool NoResidualCodeCostOverride = false;
  std::vector<stable_hash> StableHashSequence;
  if ((StringRef(OutlinerHashTreeMode).lower() == "write" ||
       StringRef(OutlinerHashTreeMode).lower() == "read") &&
      CandidatesForRepeatedSeq.size() >= 1) {
    auto &C = CandidatesForRepeatedSeq.front();
    StableHashSequence =
        stableHashMachineInstrs(C.front(), std::next(C.back()));
    if (StringRef(OutlinerHashTreeMode).lower() == "read" &&
        OutlinerHashTree.find(StableHashSequence)) {
      NoResidualCodeCost = true;
    }
  }

  // Should we override the next check because of NoResidualCodeCost?
  if (NoResidualCodeCost && CandidatesForRepeatedSeq.size() == 1)
    NoResidualCodeCostOverride = true;

  return std::tuple<bool, bool, std::vector<stable_hash>>(
      NoResidualCodeCost, NoResidualCodeCostOverride, StableHashSequence);
}

struct MatchedEntry {
  size_t StartIdx;
  size_t Length;
};

static const HashNode *followHashNode(stable_hash StableHash,
                                      const HashNode *Current) {
  auto I = Current->Successors.find(StableHash);
  return (I == Current->Successors.end()) ? nullptr : I->second.get();
}

static std::vector<MatchedEntry> getMatchedEntries(
    SuffixTree &ST, const std::vector<MachineBasicBlock::iterator> &InstrList,
    const std::vector<unsigned> &UnsignedVec,
    std::vector<OutlinedFunction> &FunctionList, StableHashTree &OutlinerHashTree) {
  // We are running in the second round of ThinLTO codegen, using a previously
  // built hash tree. Besides considering repeated instruction sequences, which
  // happened above, we are also going to consider "singleton" instruction
  // sequences which only occur once in this module, but, as we can learn
  // via the hash tree, are also going to get outlined in other modules.

  // To this end, we are going to scan the instructions (following the already
  // built data structures of the Mapper), keeping track of all matching
  // prefixes of stable instruction hashes in the hash tree, and creating a
  // new Candidate instance for each terminal node in the hash tree.

  struct TrackedEntry {
    size_t StartIdx;
    size_t Length;
    const HashNode *LastNode;
  };
  std::vector<TrackedEntry> TrackedEntries;
  std::vector<MatchedEntry> MatchedEntries;
  auto Size = UnsignedVec.size();
  for (size_t Idx = 0; Idx < Size; Idx++) {
    if (UnsignedVec.at(Idx) >= Size) {
      TrackedEntries.clear();
      continue;
    }
    const MachineInstr &MI = *InstrList.at(Idx);
    stable_hash StableHash = stableHashValue(MI);
    if (!StableHash) {
      TrackedEntries.clear();
      continue;
    }

    std::vector<TrackedEntry> NextTrackedEntries;
    auto Add = [&](const TrackedEntry &E) {
      NextTrackedEntries.push_back(E);
      if (E.LastNode && E.LastNode->IsTerminal) {
        assert(Idx == E.StartIdx + E.Length - 1);
        MatchedEntries.push_back({E.StartIdx, E.Length});
      }
    };
    const HashNode *HN =
        followHashNode(StableHash, OutlinerHashTree.getHashTreeImpl());
    if (HN)
      Add({Idx, 1, HN});
    for (const auto &E : TrackedEntries) {
      HN = followHashNode(StableHash, E.LastNode);
      if (HN)
        Add({E.StartIdx, E.Length + 1, HN});
    }
    TrackedEntries = NextTrackedEntries;
  }

  return MatchedEntries;
}

// When using a previously built hash tree, find additional candidates by
// scanning instructions, and matching them with nodes in the hash tree
void findSingletonCandidatesFromHashTree(
    SuffixTree &ST, std::vector<MachineBasicBlock::iterator> &InstrList,
    DenseMap<MachineBasicBlock *, unsigned> &MBBFlagsMap,
    std::vector<unsigned> &UnsignedVec,
    std::vector<OutlinedFunction> &FunctionList, StableHashTree &OutlinerHashTree) {

  std::vector<MatchedEntry> MatchedEntries = getMatchedEntries(
      ST, InstrList, UnsignedVec, FunctionList, OutlinerHashTree);

  std::vector<Candidate> CandidatesForRepeatedSeq;
  for (auto &ME : MatchedEntries) {
    CandidatesForRepeatedSeq.clear();
    MachineBasicBlock::iterator StartIt = InstrList[ME.StartIdx];
    MachineBasicBlock::iterator EndIt = InstrList[ME.StartIdx + ME.Length - 1];
    MachineBasicBlock *MBB = StartIt->getParent();
    Candidate C(ME.StartIdx, ME.Length, StartIt, EndIt, MBB,
                FunctionList.size(), MBBFlagsMap[MBB]);
    CandidatesForRepeatedSeq.push_back(C);
    const TargetInstrInfo *TII = C.getMF()->getSubtarget().getInstrInfo();
    OutlinedFunction OF =
        TII->getOutliningCandidateInfo(CandidatesForRepeatedSeq);
    if (OF.Candidates.size() == 0) {
      continue;
    }
    OF.NoResidualCodeCost = true;
    OF.NoResidualCodeCostOverride = true;
    OF.Singleton = true;
    OF.StableHashSequence =
        stableHashMachineInstrs(C.front(), std::next(C.back()));
    FunctionList.push_back(OF);
  }
}
