//===-------- MachineOutlinerState.h - Outliner data structures --*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Shim to MachineOutlinerGlobal.h
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_MACHINEOUTLINERSTATE_H
#define LLVM_MACHINEOUTLINERSTATE_H
#include "MachineOutlinerGlobal.h"

#include "llvm/CodeGen/MachineFunction.h"

namespace llvm {
namespace outliner {
// Return true if outlining in MF is allowed given performance constraints.
bool allowOutline(MachineFunction &MF);

// Return true if outlining in MBB is allowed given performance constraints.
bool allowOutline(MachineBasicBlock &MBB);

} // namespace outliner
} // namespace llvm

#endif
