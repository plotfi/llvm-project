//===---  InterfaceStubs.cpp - Base InterfaceStubs Implementations C++  ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "InterfaceStubs.h"
#include "CommonArgs.h"
#include "clang/Config/config.h" // for GCC_INSTALL_PREFIX
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/DriverDiagnostic.h"
#include "clang/Driver/Options.h"
#include "clang/Driver/Tool.h"
#include "clang/Driver/ToolChain.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/TargetParser.h"
#include "llvm/Support/VirtualFileSystem.h"
#include <system_error>

using namespace clang::driver;
using namespace clang;
using namespace llvm::opt;

void tools::ifstool::Merger::ConstructJob(Compilation &C, const JobAction &JA,
                                          const InputInfo &Output,
                                          const InputInfoList &Inputs,
                                          const ArgList &Args,
                                          const char *LinkingOutput) const {
  SmallVector<const char *, 16> CmdArgs;
  const char *Exec = "/bin/echo";
  // CmdArgs.push_back("-o");
  CmdArgs.push_back(Output.getFilename());
  for (auto Input : Inputs)
    CmdArgs.push_back(Input.getFilename());
  C.addCommand(std::make_unique<Command>(JA, *this, Exec, CmdArgs, Inputs));
}
