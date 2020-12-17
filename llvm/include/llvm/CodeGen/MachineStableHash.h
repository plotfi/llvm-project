//===------------ MachineStableHash.h - MIR Stable Hashing Utilities ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Stable hashing for MachineInstr and MachineOperand. Useful or getting a
// hash across runs, modules, etc.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CODEGEN_MACHINESTABLEHASH_H
#define LLVM_CODEGEN_MACHINESTABLEHASH_H

#ifdef __FACEBOOK__
#include "llvm/CodeGen/MachineBasicBlock.h"
#endif // __FACEBOOK__

#include "llvm/CodeGen/StableHashing.h"

namespace llvm {
class MachineBasicBlock;
class MachineFunction;
class MachineInstr;
class MachineOperand;

stable_hash stableHashValue(const MachineOperand &MO);
stable_hash stableHashValue(const MachineInstr &MI, bool HashVRegs = false,
                            bool HashConstantPoolIndices = false,
                            bool HashMemOperands = false);
stable_hash stableHashValue(const MachineBasicBlock &MBB);
stable_hash stableHashValue(const MachineFunction &MF);

#ifdef __FACEBOOK__
/// \returns a collection of stable_hashes for each of the MachineInstrs of
/// a given MachineBasicBlock from iterator \p Begin to \p End.
std::vector<stable_hash>
stableHashMachineInstrs(const MachineBasicBlock::iterator &Begin,
                        const MachineBasicBlock::iterator &End);
#endif // __FACEBOOK__
} // namespace llvm

#endif
