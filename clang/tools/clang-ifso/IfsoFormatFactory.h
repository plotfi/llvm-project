//===------------------- clang-ifso/IfsoFormatFactory.h  ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// This file implementes an anstract factor for different ifso targets. The
/// idea is that when we have collected lexical data in visible functions, we
/// want a seemless way to generate whatever interface library format we need
/// regardless if we are targering tbd files on Darwin, or if we are generating
/// hollowed out Elf .so files on Linux.
///
//===----------------------------------------------------------------------===//

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Support/raw_ostream.h"

#include <memory>
#include <string>
#include <vector>

#ifndef _IFSO_FORMAT_FACTORY_
#define _IFSO_FORMAT_FACTORY_

class IfsoFormat {
protected:
  llvm::Triple T;
  std::vector<std::string> SymbolNames;
  IfsoFormat(llvm::Triple &T) : T(T) {}

public:
  IfsoFormat() = delete;
  virtual ~IfsoFormat() {}
  virtual void appendSymbolName(const std::string &Name) {
    SymbolNames.push_back(Name);
  }
  virtual void writeIfsoFile(std::string Filename) = 0;
};

struct YamlElfIfsoFormat : public IfsoFormat {
  YamlElfIfsoFormat(llvm::Triple &T) : IfsoFormat(T) {}
  YamlElfIfsoFormat() = delete;
  virtual ~YamlElfIfsoFormat() {}
  virtual void writeIfsoFile(std::string Filename) override {
    auto &OS = llvm::errs();

    llvm::StringRef MachineType =
        llvm::StringSwitch<llvm::StringRef>(T.getArchName())
            .Case("x86_64", "EM_X86_64")
            .Case("x86", "EM_386")
            .Case("i386", "EM_386")
            .Case("i686", "EM_386")
            .Case("aarch64", "EM_AARCH64")
            .Case("arm", "EM_ARM")
            .Default("EM_X86_64");
    OS << "--- !ELF\n";
    OS << "FileHeader:\n";
    OS << "  Class:           ELFCLASS";
    OS << (T.isArch64Bit() ? "64" : "32");
    OS << "\n";
    OS << "  Data:            ELFDATA2";
    OS << (T.isLittleEndian() ? "LSB" : "MSB");
    OS << "\n";
    OS << "  Type:            ET_DYN\n";
    OS << "  Machine:         ";
    OS << MachineType << "\n";
    OS << "Sections:\n";
    OS << "  - Name:            .text\n";
    OS << "    Type:            SHT_PROGBITS\n";
    OS << "Symbols:\n";
    OS << "  Local:\n";
    OS << "    - Name:            .dynsym\n";
    OS << "      Type:            STT_SECTION\n";
    OS << "      Section:         .dynsym\n";
    OS << "    - Name:            .dynstr\n";
    OS << "      Type:            STT_SECTION\n";
    OS << "      Section:         .dynstr\n";

    OS << "  Global:\n";
    for (auto Name : SymbolNames) {
      OS << "    - Name:            " << Name << "\n";
      OS << "      Type:            STT_FUNC\n";
      OS << "      Section:         .text\n";
    }

    OS << "DynamicSymbols:\n";

    OS << "  Global:\n";
    for (auto Name : SymbolNames) {
      OS << "    - Name:            " << Name << "\n";
      OS << "      Type:            STT_FUNC\n";
      OS << "      Section:         .text\n";
    }

    OS << "...\n";

  }
};

class IfsoFormatFactory {
  IfsoFormatFactory() {}

public:
  static IfsoFormatFactory &getInstance() {
    static IfsoFormatFactory instance;
    return instance;
  }

  std::unique_ptr<IfsoFormat> createIfsoFormat(llvm::Triple &T) {
    return llvm::make_unique<YamlElfIfsoFormat>(T);
  }
};

#endif // _IFSO_FORMAT_FACTORY_
