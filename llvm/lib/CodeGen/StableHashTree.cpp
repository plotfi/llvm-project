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
#include "llvm/Support/VirtualFileSystem.h"
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

namespace llvm {

void HashTree::walkGraph(
    std::function<void(const HashNode *, const HashNode *)> CallbackEdge,
    std::function<void(const HashNode *)> CallbackVertex) const {
  std::stack<const HashNode *> Stack;
  Stack.push(&HashTreeImpl);

  while (!Stack.empty()) {
    const auto *Current = Stack.top();
    Stack.pop();
    CallbackVertex(Current);
    for (const auto &P : Current->Successors) {
      CallbackEdge(Current, P.second.get());
      Stack.push(P.second.get());
    }
  }
}

void HashTree::walkVertices(
    std::function<void(const HashNode *)> Callback) const {
  walkGraph([](const HashNode *A, const HashNode *B) {}, Callback);
}

void HashTree::walkEdges(
    std::function<void(const HashNode *, const HashNode *)> Callback) const {
  walkGraph(Callback, [](const HashNode *A) {});
}

void HashTree::dump(
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
    OS.raw_ostream::write_hex(Entry.first->Data);
    OS << "\",\n";

    OS << "    \"isTerminal\" : "
       << "\"" << (Entry.first->IsTerminal ? "true" : "false") << "\",\n";

    // For debugging we want to provide a string representation of the hashing
    // source, such as a MachineInstr dump, etc. Not intended for production.
    auto MII = DebugMap.find(Entry.first->Data);
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

llvm::Error HashTree::writeHashTreeToFile(StringRef Filename) const {
  std::error_code EC;
  llvm::raw_fd_ostream OS(Filename, EC, llvm::sys::fs::OF_Text);
  if (EC)
    return llvm::createStringError(EC, "Unable to open JSON HashTree");
  dump(OS);
  OS.flush();
  return llvm::Error::success();
}

llvm::Error HashTree::readHashTreeFromFile(StringRef Filename) {
  llvm::SmallString<256> Filepath(Filename);
  auto FileOrError = llvm::vfs::getRealFileSystem()->getBufferForFile(Filepath);
  if (!FileOrError)
    return llvm::errorCodeToError(FileOrError.getError());

  auto Json = llvm::json::parse(FileOrError.get()->getBuffer());
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
  NodeMap[0] = &HashTreeImpl;

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
    CurrentSubtree->Data = Hash;

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

void HashTree::insertIntoHashTree(
    const std::vector<stable_hash> &StableHashSequence) {
  HashNode *Current = &HashTreeImpl;
  for (stable_hash StableHash : StableHashSequence) {
    auto I = Current->Successors.find(StableHash);
    if (I == Current->Successors.end()) {
      std::unique_ptr<HashNode> Next = std::make_unique<HashNode>();
      HashNode *NextPtr = Next.get();
      NextPtr->Data = StableHash;
      Current->Successors.emplace(StableHash, std::move(Next));
      Current = NextPtr;
      continue;
    }
    Current = I->second.get();
  }
  Current->IsTerminal = true;
}

bool HashTree::findInHashTree(
    const std::vector<stable_hash> &StableHashSequence) const {
  const HashNode *Current = &HashTreeImpl;
  for (stable_hash StableHash : StableHashSequence) {
    const auto I = Current->Successors.find(StableHash);
    if (I == Current->Successors.end())
      return false;
    Current = I->second.get();
  }
  // return Current->Successors.empty();
  return Current->IsTerminal;
}

void HashTree::insertIntoHashTree(
    const std::vector<std::vector<stable_hash>> &StableHashSequences) {
  for (const auto &StableHashSequence : StableHashSequences)
    insertIntoHashTree(StableHashSequence);
}

} // namespace llvm
