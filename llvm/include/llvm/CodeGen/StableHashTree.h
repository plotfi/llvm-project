//===-- StableHashTree.h ----------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Contains a stable hash tree implementation based on llvm::stable_hash.
/// Primarily used or Global Machine Outlining but is reusable.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_CODEGEN_STABLEHASHTREE_H
#define LLVM_CODEGEN_STABLEHASHTREE_H

#include <memory>
#include <unordered_map>
#include <vector>

#include "llvm/CodeGen/StableHashing.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {

// A node in the hash tree might be terminal, i.e. it represents the end
// of an stable instruction hash sequence that was outlined in some module.
// Each node may have several successor nodes that can be reached via
// different stable instruction hashes.
//
// Data is the Hash for the current node
// IsTerminal is true if this node is the last node in a hash sequence
struct HashNode {
  stable_hash Data = 0LL;
  bool IsTerminal{false};
  std::unordered_map<stable_hash, std::unique_ptr<HashNode>> Successors;
};

class HashTree {
public:
  /// Walks every edge and vertex in the HashTree and calls CallbackEdge for the
  /// edges and CallbackVertex for the vertices with the stable_hash for the
  /// source and the stable_hash of the sink for the edge. Using walkEdges it
  /// should be possible to traverse the HashTree and serialize it, compute its
  /// depth, compute the number of vertices, etc.
  void walkGraph(
      std::function<void(const HashNode *, const HashNode *)> CallbackEdge,
      std::function<void(const HashNode *)> CallbackVertex) const;

  /// Walks the edges of a HashTree using walkGraph.
  void walkEdges(
      std::function<void(const HashNode *, const HashNode *)> Callback) const;

  // Walks the vertices of a HashTree using walkGraph.
  void walkVertices(std::function<void(const HashNode *)> Callback) const;

  /// Uses HashTree::walkEdges to print the edges of the hash tree.
  /// If a DebugMap is provided, then it will be used to provide richer output.
  void dump(raw_ostream &OS = llvm::errs(),
            std::unordered_map<stable_hash, std::string> DebugMap = {}) const;

  /// Builds a HashTree from a JSON file. Same format as getJsonMap.
  llvm::Error readHashTreeFromFile(StringRef Filename);

  /// Writes a JSON file representing the current HashTree.
  llvm::Error writeHashTreeToFile(StringRef Filename) const;

  /// When building a hash tree, insert sequences of stable instruction hashes.
  void insertIntoHashTree(
      const std::vector<std::vector<stable_hash>> &StableHashSequences);

  // When using a hash tree, starting from the root, check whether a sequence
  // of stable instruction hashes ends up at a terminal node.
  bool findInHashTree(const std::vector<stable_hash> &StableHashSequence) const;

private:
  // The hash tree is a compact representation of the set of all outlined
  // instruction sequences across all modules.
  // This is not a suffix tree, but just represents a set of instruction
  // sequences, allowing for efficient walking of instruction sequence
  // prefixes for matching purposes.
  // We build the tree by inserting stable instruction sequences during the
  // first first ThinLTO codegen round, and we use the tree by following and
  // finding stable instruction hashes during the second ThinLTO codegen round.
  HashNode HashTreeImpl;

  void insertIntoHashTree(const std::vector<stable_hash> &StableHashSequence);
};

} // namespace llvm

#endif
