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
/// A StableHashTree is a Trie that contains sequences of hash values. This
/// data structure is generic, and is intended for use cases where something
/// similar to a trie is already in use. The upside to using a StableHashTree is
/// that it can be used to understand data collected across modules, or it can
/// be used to serialize data about a build to disk for use in a future build.
///
/// To use a StableHashTree you must already have a way to take some sequence of
/// data and use llvm::stable_hash to turn that sequence into a
/// std::vector<llvm::stable_hash> (ie StableHashSequence). Each of these hash
/// sequences can be inserted into a StableHashTree where the beginning of a
/// unique sequence starts from the root of the tree and ends at a Terminal
/// (IsTerminal) node.
///
/// This StableHashTree was originally implemented as part of the EuroLLVM 2020
/// talk "Global Machine Outliner for ThinLTO":
///
///   https://llvm.org/devmtg/2020-04/talks.html#TechTalk_58
///
/// This talk covers how a global stable hash tree is used to collect
/// information about valid MachineOutliner Candidates across modules, and used
/// to inform modules where matching candidates are encountered but occur in
/// less frequency and as a result are ignored by the MachineOutliner had there
/// not been a global stable hash tree in use (assuming FullLTO is disabled).
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_CODEGEN_STABLEHASHTREE_H
#define LLVM_CODEGEN_STABLEHASHTREE_H

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <vector>

#include "llvm/CodeGen/StableHashing.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/VirtualFileSystem.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {

/// \brief A HashNode is an entry in a StableHashTree that contains a value Hash
/// as well as a collection of Successors (which are other HashNodes that are
/// part of a sequence of llvm::stable_hashes). A HashNode might
/// be IsTerminal meaning that it represents the end of a stable_hash sequence.
struct HashNode {
  stable_hash Hash = 0LL;
  bool IsTerminal{false};
  std::unordered_map<stable_hash, std::unique_ptr<HashNode>> Successors;
};

struct StableHashTree {

  /// Graph traversal callback types.
  ///{
  using EdgeCallbackFn =
      std::function<void(const HashNode *, const HashNode *)>;
  using NodeCallbackFn = std::function<void(const HashNode *)>;
  ///}

  using StableHashSequence = std::vector<stable_hash>;

  /// Walks every edge and node in the StableHashTree and calls CallbackEdge
  /// for the edges and CallbackNode for the nodes with the stable_hash for
  /// the source and the stable_hash of the sink for an edge. These generic
  /// callbacks can be used to traverse a StableHashTree for the purpose of
  /// print debugging or serializing it.
  void walkGraph(EdgeCallbackFn CallbackEdge,
                 NodeCallbackFn CallbackNode) const;

  /// Walks the nodes of a StableHashTree using walkGraph.
  void walkVertices(NodeCallbackFn Callback) const {
    walkGraph([](const HashNode *A, const HashNode *B) {}, Callback);
  }

  /// Uses walkVertices to print a StableHashTree.
  /// If a \p DebugMap is provided, then it will be used to provide richer
  /// output.
  void print(raw_ostream &OS = llvm::errs(),
             std::unordered_map<stable_hash, std::string> DebugMap = {}) const;

  void dump() const { print(llvm::errs()); }

  /// Builds a StableHashTree from a \p Buffer.
  /// The serialization format here should be considered opaque, and may change.
  /// \returns llvm::Error::ErrorSuccess if successful, otherwise returns some
  /// other llvm::Error error kind.
  llvm::Error readFromBuffer(StringRef Buffer);

  /// Serializes a StableHashTree from a file at \p Filename.
  llvm::Error readFromFile(StringRef Filename) {
    llvm::SmallString<256> Filepath(Filename);
    auto FileOrError =
        llvm::vfs::getRealFileSystem()->getBufferForFile(Filepath);
    if (!FileOrError)
      return llvm::errorCodeToError(FileOrError.getError());
    return readFromBuffer(FileOrError.get()->getBuffer());
  }

  /// Serializes a StableHashTree to a file at \p Filename.
  llvm::Error writeToFile(StringRef Filename) const {
    std::error_code EC;
    llvm::raw_fd_ostream OS(Filename, EC, llvm::sys::fs::OF_Text);
    if (EC)
      return llvm::createStringError(EC, "Unable to open StableHashTree Data");
    // Note: For now the format is the same as the print output, but this can
    // change.
    print(OS);
    OS.flush();
    return llvm::Error::success();
  }

  void insert(const std::vector<StableHashSequence> &Sequences);

  /// \returns true if \p Sequence exists in a StableHashTree, false
  /// otherwise.
  bool find(const StableHashSequence &Sequence) const;

  /// \returns the size of a StableHashTree by traversing it. If
  /// \p GetTerminalCountOnly is true, it only counts the terminal nodes
  /// (meaning it returns the size of the number of hash sequences in a
  /// StableHashTree).
  size_t size(bool GetTerminalCountOnly = false) const {
    size_t Size = 0;
    walkVertices([&Size, GetTerminalCountOnly](const HashNode *N) {
      Size += (N && (!GetTerminalCountOnly || N->IsTerminal));
    });
    return Size;
  }

  size_t depth() const {
    size_t Size = 0;

    std::unordered_map<const HashNode *, size_t> DepthMap;

    walkGraph(
        [&DepthMap](const HashNode *Src, const HashNode *Dst) {
          size_t Depth = DepthMap[Src];
          DepthMap[Dst] = Depth + 1;
        },
        [&Size, &DepthMap](const HashNode *N) {
          Size = std::max(Size, DepthMap[N]);
        });

    return Size;
  }

  const HashNode *getHashTreeImpl() const;
  HashNode *getHashTreeImpl();

private:
  /// StableHashTree is a compact representation of a set of stable_hash
  /// sequences. It allows for for efficient walking of these sequences for
  /// matching purposes. HashTreeImpl is the root node of this tree. Its Hash
  /// value is 0, and its Successors are the beginning of StableHashSequences
  /// inserted into the StableHashTree.
  HashNode HashTreeImpl;

  /// Inserts a \p Sequence into a StableHashTree. The last node in the sequence
  /// will set IsTerminal to true in StableHashTree.
  void insert(const StableHashSequence &Sequence);
};

} // namespace llvm

#endif
