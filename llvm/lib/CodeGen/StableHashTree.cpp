//===---- StableHashTree.cpp ------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/StableHashTree.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/StableHashing.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

#include <cstdlib>
#include <functional>
#include <iterator>
#include <set>

#include <ios>
#include <stack>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#define DEBUG_TYPE "stable-hash-tree"

using namespace llvm;

#ifdef __FACEBOOK__
#include "llvm/Support/CommandLine.h"
#include <memory>
#include <mutex>
cl::opt<bool> UseSingletonMachineOutlinerHashTree(
    "use-singleton-machine-outliner-hashtree", cl::init(false), cl::Hidden,
    cl::desc("This flag is used by llvm-lto to tell the Global MachineOutliner "
             "to use a Singleton HashTree. This is valuable for collecting "
             "outliner HashTree data for serialization and for two-pass LTO "
             "(default = off)"));

static std::mutex HashTreeMutex;
static HashNode HashTreeImpl;
#endif

namespace llvm {

namespace {
template <typename T> auto getHashTreeImpl(T &Root) {
#ifdef __FACEBOOK__
  if (UseSingletonMachineOutlinerHashTree)
    return static_cast<T *>(&HashTreeImpl);
#endif
  return &Root;
}
} // namespace

void StableHashTree::insert(const std::vector<StableHashSequence> &Sequences) {
  if (!Sequences.size())
    return;

#ifdef __FACEBOOK__
  // TODO: Instead of doing all this work here and blocking,
  // have a different worker thread that just processed queued
  // stable hash sequences.
  if (UseSingletonMachineOutlinerHashTree) {
    std::lock_guard<std::mutex> Guard(HashTreeMutex);

    for (const auto &Sequence : Sequences)
      insert(Sequence);
    return;
  }
#endif

  for (const auto &Sequence : Sequences)
    insert(Sequence);
}

HashNode *StableHashTree::getHashTreeImpl() {
  return llvm::getHashTreeImpl(HashTreeImpl);
}
const HashNode *StableHashTree::getHashTreeImpl() const {
  return llvm::getHashTreeImpl(HashTreeImpl);
}

void StableHashTree::walkGraph(EdgeCallbackFn CallbackEdge,
                               NodeCallbackFn CallbackNode) const {
  std::stack<const HashNode *> Stack;
  Stack.push(getHashTreeImpl());

  while (!Stack.empty()) {
    const auto *Current = Stack.top();
    Stack.pop();
    CallbackNode(Current);
    for (const auto &P : Current->Successors) {
      CallbackEdge(Current, P.second.get());
      Stack.push(P.second.get());
    }
  }
}

void StableHashTree::print(
    llvm::raw_ostream &OS,
    std::unordered_map<stable_hash, std::string> DebugMap) const {

  std::unordered_map<const HashNode *, unsigned> NodeMap;

  walkVertices([&NodeMap](const HashNode *Current) {
    size_t Index = NodeMap.size();
    NodeMap[Current] = Index;
    assert(Index = NodeMap.size() + 1 &&
                   "Expected size of ModeMap to increment by 1");
  });

  bool IsFirstEntry = true;
  OS << "{";
  for (const auto &Entry : NodeMap) {
    if (!IsFirstEntry)
      OS << ",";
    OS << "\n";
    IsFirstEntry = false;
    OS << "  \"" << Entry.second << "\" : {\n";
    OS << "    \"hash\" : \"";
    OS.raw_ostream::write_hex(Entry.first->Hash);
    OS << "\",\n";

    OS << "    \"isTerminal\" : "
       << "\"" << (Entry.first->IsTerminal ? "true" : "false") << "\",\n";

    // For debugging we want to provide a string representation of the hashing
    // source, such as a MachineInstr dump, etc. Not intended for production.
    auto MII = DebugMap.find(Entry.first->Hash);
    if (MII != DebugMap.end())
      OS << "    \"source\" : \"" << MII->second << "\",\n";

    OS << "    \"neighbors\" : [";

    bool IsFirst = true;
    for (const auto &Adj : Entry.first->Successors) {
      if (!IsFirst)
        OS << ",";
      IsFirst = false;
      OS << " \"";
      OS << NodeMap[Adj.second.get()];
      OS << "\" ";
    }

    OS << "]\n  }";
  }
  OS << "\n}\n";
  OS.flush();
}

llvm::Error StableHashTree::readFromBuffer(StringRef Buffer) {
#ifdef __FACEBOOK__
  if (UseSingletonMachineOutlinerHashTree)
    return llvm::Error::success();
#endif

  auto Json = llvm::json::parse(Buffer);
  if (!Json)
    return Json.takeError();

  const json::Object *JO = Json.get().getAsObject();
  if (!JO)
    return llvm::createStringError(std::error_code(), "Bad Json");

  std::unordered_map<unsigned, const llvm::json::Value *> JsonMap;
  for (const auto &E : *JO)
    JsonMap[std::stoul(E.first.str())] = &E.second;

  assert(JsonMap.find(0x0) != JsonMap.end() && "Expected a root HashTree node");

  // We have a JsonMap and a NodeMap. We walk the JSON form of the HashTree
  // using the JsonMap by using the stack of JSON IDs. As we walk we used the
  // IDs to get the currwent JSON Node and the current HashNode.
  std::unordered_map<unsigned, HashNode *> NodeMap;
  std::stack<unsigned> Stack;
  Stack.push(0);
  NodeMap[0] = getHashTreeImpl();

  while (!Stack.empty()) {
    unsigned Current = Stack.top();
    Stack.pop();

    HashNode *CurrentSubtree = NodeMap[Current];
    const auto *CurrentJson = JsonMap[Current]->getAsObject();

    std::vector<unsigned> Neighbors;
    llvm::transform(*CurrentJson->get("neighbors")->getAsArray(),
                    std::back_inserter(Neighbors),
                    [](const llvm::json::Value &S) {
                      return std::stoull(S.getAsString()->str());
                    });

    stable_hash Hash = std::stoull(
        CurrentJson->get("hash")->getAsString()->str(), nullptr, 16);
    CurrentSubtree->Hash = Hash;

    std::string IsTerminalStr =
        StringRef(CurrentJson->get("isTerminal")->getAsString()->str()).lower();
    CurrentSubtree->IsTerminal =
        IsTerminalStr == "true" || IsTerminalStr == "on";

    for (auto N : Neighbors) {
      auto I = JsonMap.find(N);
      if (I == JsonMap.end())
        return llvm::createStringError(std::error_code(),
                                       "Missing neighbor in JSON");

      std::unique_ptr<HashNode> Neighbor = std::make_unique<HashNode>();
      HashNode *NeighborPtr = Neighbor.get();
      stable_hash StableHash = std::stoull(
          I->second->getAsObject()->get("hash")->getAsString()->str(), nullptr,
          16);
      CurrentSubtree->Successors.emplace(StableHash, std::move(Neighbor));
      NodeMap[N] = NeighborPtr;

      Stack.push(I->first);
    }
  }

  return llvm::Error::success();
}

void StableHashTree::insert(const StableHashSequence &Sequence) {
  HashNode *Current = getHashTreeImpl();
  for (stable_hash StableHash : Sequence) {
    auto I = Current->Successors.find(StableHash);
    if (I == Current->Successors.end()) {
      std::unique_ptr<HashNode> Next = std::make_unique<HashNode>();
      HashNode *NextPtr = Next.get();
      NextPtr->Hash = StableHash;
      Current->Successors.emplace(StableHash, std::move(Next));
      Current = NextPtr;
      continue;
    }
    Current = I->second.get();
  }
  Current->IsTerminal = true;
}

bool StableHashTree::find(const StableHashSequence &Sequence) const {
  const HashNode *Current = &HashTreeImpl;
  for (stable_hash StableHash : Sequence) {
    const auto I = Current->Successors.find(StableHash);
    if (I == Current->Successors.end())
      return false;
    Current = I->second.get();
  }
  return Current->IsTerminal;
}

} // namespace llvm
