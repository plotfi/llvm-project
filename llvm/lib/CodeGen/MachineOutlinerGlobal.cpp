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
#include "llvm/CodeGen/MachineOutlinerGlobal.h"
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

#define DEBUG_TYPE "global-outliner"

using namespace llvm;
using namespace ore;
using namespace outliner;

extern cl::opt<bool> UseSingletonMachineOutlinerHashTree;
extern cl::opt<std::string> OutlinerHashTreeMode;

cl::opt<bool>
    OutlineDeadCodeOnly("outline-dead-code-only", cl::init(false), cl::Hidden,
                        cl::desc("Outline dead code only identified from "
                                 "execution profiles (default = off)"));
cl::opt<bool>
    OutlineColdCodeOnly("outline-cold-code-only", cl::init(false), cl::Hidden,
                        cl::desc("Outline cold code only identified from "
                                 "execution profiles (default = off)"));

namespace llvm {
namespace outliner {
bool allowOutline(MachineFunction &MF) {
  // Allow outline for all code by default.
  if (!OutlineDeadCodeOnly && !OutlineColdCodeOnly)
    return true;
  if (OutlineDeadCodeOnly && MIRProfileSummary::isMachineFunctionDead(MF))
    return true;
  if (OutlineColdCodeOnly && MIRProfileSummary::isMachineFunctionCold(MF))
    return true;
  return false;
}

bool allowOutline(MachineBasicBlock &MBB) {
  // Allow outline for all code by default.
  if (!OutlineDeadCodeOnly && !OutlineColdCodeOnly)
    return true;
  if (OutlineDeadCodeOnly && MIRProfileSummary::isMachineBlockDead(MBB))
    return true;
  if (OutlineColdCodeOnly && MIRProfileSummary::isMachineBlockCold(MBB))
    return true;
  return false;
}
} // namespace outliner
} // namespace llvm

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

// We are going to scan all subsequences of instructions by walking the global
// prefix tree. Because of the short outlined sequence, the subsequences are
// either quickly matched or discarded. In practice, the average loop count in
// the inner loop is 2.5, which makes the average time complexity almost linear
// to the number of instructions per module.
static std::vector<MatchedEntry>
getMatchedEntries(SuffixTree &ST,
                  const std::vector<MachineBasicBlock::iterator> &InstrList,
                  const std::vector<unsigned> &UnsignedVec,
                  std::vector<OutlinedFunction> &FunctionList,
                  StableHashTree &OutlinerHashTree) {

  std::vector<MatchedEntry> MatchedEntries;
  auto Size = UnsignedVec.size();
  for (size_t I = 0; I < Size; I++) {
    if (UnsignedVec.at(I) >= Size)
      continue;

    const MachineInstr &MI = *InstrList.at(I);
    stable_hash StableHashI = stableHashValue(MI);
    if (!StableHashI)
      continue;

    const HashNode *LastNode =
        followHashNode(StableHashI, OutlinerHashTree.getHashTreeImpl());
    if (!LastNode)
      continue;

    size_t J = I + 1;
    for (; J < Size; J++) {
      // Break on invalid code
      if (UnsignedVec.at(J) >= Size)
        break;

      const MachineInstr &MJ = *InstrList.at(J);
      stable_hash StableHashJ = stableHashValue(MJ);
      // Break on invalid stable hash
      if (!StableHashJ)
        break;

      LastNode = followHashNode(StableHashJ, LastNode);
      if (!LastNode)
        break;

      if (LastNode->IsTerminal) {
        MatchedEntries.push_back({I, J - I + 1});
      }
    }

    // Update stats on iterations of the nested loop.
    size_t SubseqLen = J - I;
    if (SubseqLen >= 2) {
      TotalSubseqLen += SubseqLen;
      CountSubseqLen++;
    }
  }

  return MatchedEntries;
}

// When using a previously built hash tree, find additional candidates by
// scanning instructions, and matching them with nodes in the hash tree
void findGlobalCandidatesFromHashTree(
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
    OF.StableHashSequence =
        stableHashMachineInstrs(C.front(), std::next(C.back()));
    FunctionList.push_back(OF);
  }
}

namespace llvm {
namespace outliner {
HashTreeMode getMode() {
  if (StringRef(OutlinerHashTreeMode).lower() == "read")
    return HashTreeMode::UsingHashTree;
  if (StringRef(OutlinerHashTreeMode).lower() == "write")
    return HashTreeMode::BuildingHashTree;
  return HashTreeMode::None;
}

void beginBuildingHashTree() {
  UseSingletonMachineOutlinerHashTree = true;
  OutlinerHashTreeMode = "write";
}

void beginUsingHashTree() {
  UseSingletonMachineOutlinerHashTree = true;
  OutlinerHashTreeMode = "read";
}
} // namespace outliner
} // namespace llvm
