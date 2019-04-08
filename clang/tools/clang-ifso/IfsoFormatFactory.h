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

#include "llvm/ObjectYAML/ObjectYAML.h"
#include "llvm/Support/YAMLTraits.h"

#include <memory>
#include <string>
#include <vector>

#ifndef _IFSO_FORMAT_FACTORY_
#define _IFSO_FORMAT_FACTORY_


void Symbols2Yaml(const llvm::Triple &T,
                  const std::vector<std::string> &SymbolNames,
                  llvm::raw_ostream &OS);
int convertYAML(llvm::yaml::Input &YIn, llvm::raw_ostream &Out);

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
  virtual void writeIfsoFile(llvm::raw_ostream &OS) = 0;
};

struct YamlElfIfsoFormat : public IfsoFormat {
  YamlElfIfsoFormat(llvm::Triple &T) : IfsoFormat(T) {}
  YamlElfIfsoFormat() = delete;
  virtual ~YamlElfIfsoFormat() {}
  virtual void writeIfsoFile(llvm::raw_ostream &OS) override {
    Symbols2Yaml(T, SymbolNames, OS);
  }
};

struct ElfIfsoFormat : public IfsoFormat {
  ElfIfsoFormat(llvm::Triple &T) : IfsoFormat(T) {}
  ElfIfsoFormat() = delete;
  virtual ~ElfIfsoFormat() {}
  virtual void writeIfsoFile(llvm::raw_ostream &OS) override {
    std::string Yaml;
    llvm::raw_string_ostream YOS(Yaml);
    Symbols2Yaml(T, SymbolNames, YOS);

    llvm::StringRef Buffer(Yaml);
    llvm::yaml::Input YIn(Buffer);

    convertYAML(YIn, OS);
    OS.flush();
  }
};

class IfsoFormatFactory {
  IfsoFormatFactory() {}

public:
  static IfsoFormatFactory &getInstance() {
    static IfsoFormatFactory instance;
    return instance;
  }

  std::unique_ptr<IfsoFormat> createIfsoFormat(llvm::Triple &T, bool DebugMode = false) {
    if (DebugMode)
      return llvm::make_unique<YamlElfIfsoFormat>(T);
    else
      return llvm::make_unique<ElfIfsoFormat>(T);
  }
};

#endif // _IFSO_FORMAT_FACTORY_
