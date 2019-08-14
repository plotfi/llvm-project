//===- llvm-ifs.cpp -------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===-----------------------------------------------------------------------===/

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/ADT/Triple.h"
#include "llvm/ObjectYAML/yaml2obj.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Errc.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileOutputBuffer.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/WithColor.h"
#include "llvm/Support/YAMLTraits.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TextAPI/ELF/ELFStub.h"
#include <string>

using namespace llvm;
using namespace llvm::elfabi;

static cl::opt<std::string> Action("a", cl::desc("<llvm-ifs action>"),
                                   cl::Required,
                                   cl::value_desc("write-ifs | write-bin"));

static cl::list<std::string> InputFilenames(cl::Positional,
                                            cl::desc("<input ifs files>"),
                                            cl::ZeroOrMore);

static cl::opt<std::string> OutputFilename("o", cl::desc("<output file>"),
                                           cl::value_desc("path"));

/// YAML traits for ELFSymbolType.
template <> struct llvm::yaml::ScalarEnumerationTraits<ELFSymbolType> {
  static void enumeration(IO &IO, ELFSymbolType &SymbolType) {
    IO.enumCase(SymbolType, "NoType", ELFSymbolType::NoType);
    IO.enumCase(SymbolType, "Func", ELFSymbolType::Func);
    IO.enumCase(SymbolType, "Object", ELFSymbolType::Object);
    IO.enumCase(SymbolType, "TLS", ELFSymbolType::TLS);
    IO.enumCase(SymbolType, "Unknown", ELFSymbolType::Unknown);
    // Treat other symbol types as noise, and map to Unknown.
    if (!IO.outputting() && IO.matchEnumFallback())
      SymbolType = ELFSymbolType::Unknown;
  }
};

const VersionTuple IFSVersionCurrent(1, 0);
template <> struct llvm::yaml::ScalarTraits<VersionTuple> {
  static void output(const VersionTuple &Value, void *,
                     llvm::raw_ostream &Out) {
    Out << Value.getAsString();
  }

  static StringRef input(StringRef Scalar, void *, VersionTuple &Value) {
    if (Value.tryParse(Scalar))
      return StringRef("Can't parse version: invalid version format.");

    if (Value > IFSVersionCurrent)
      return StringRef("Unsupported IFS version.");

    // Returning empty StringRef indicates successful parse.
    return StringRef();
  }

  // Don't place quotation marks around version value.
  static QuotingType mustQuote(StringRef) { return QuotingType::None; }
};

/// YAML traits for ELFSymbol.
template <> struct llvm::yaml::MappingTraits<ELFSymbol> {
  static void mapping(IO &IO, ELFSymbol &Symbol) {
    IO.mapRequired("Type", Symbol.Type);
    // The need for symbol size depends on the symbol type.
    if (Symbol.Type == ELFSymbolType::NoType) {
      IO.mapOptional("Size", Symbol.Size, (uint64_t)0);
    } else if (Symbol.Type == ELFSymbolType::Func) {
      Symbol.Size = 0;
    } else {
      IO.mapRequired("Size", Symbol.Size);
    }
    IO.mapOptional("Undefined", Symbol.Undefined, false);
    IO.mapOptional("Weak", Symbol.Weak, false);
    IO.mapOptional("Warning", Symbol.Warning);
  }

  // Compacts symbol information into a single line.
  static const bool flow = true;
};

/// YAML traits for set of ELFSymbols.
template <> struct llvm::yaml::CustomMappingTraits<std::set<ELFSymbol>> {
  static void inputOne(IO &IO, StringRef Key, std::set<ELFSymbol> &Set) {
    ELFSymbol Sym(Key.str());
    IO.mapRequired(Key.str().c_str(), Sym);
    Set.insert(Sym);
  }

  static void output(IO &IO, std::set<ELFSymbol> &Set) {
    for (auto &Sym : Set)
      IO.mapRequired(Sym.Name.c_str(), const_cast<ELFSymbol &>(Sym));
  }
};

// A cumulative representation of ELF stubs.
// Both textual and binary stubs will read into and write from this object.
class IFSStub {
  // TODO: Add support for symbol versioning.
public:
  VersionTuple IfsVersion;
  std::string Triple;
  std::string ObjectFileFormat;
  Optional<std::string> SoName;
  std::vector<std::string> NeededLibs;
  std::set<ELFSymbol> Symbols;

  IFSStub() = default;
  IFSStub(const IFSStub &Stub)
      : IfsVersion(Stub.IfsVersion), Triple(Stub.Triple),
        ObjectFileFormat(Stub.ObjectFileFormat), SoName(Stub.SoName),
        NeededLibs(Stub.NeededLibs), Symbols(Stub.Symbols) {}
  IFSStub(IFSStub &&Stub)
      : IfsVersion(std::move(Stub.IfsVersion)), Triple(std::move(Stub.Triple)),
        ObjectFileFormat(std::move(Stub.ObjectFileFormat)),
        SoName(std::move(Stub.SoName)), NeededLibs(std::move(Stub.NeededLibs)),
        Symbols(std::move(Stub.Symbols)) {}
};

/// YAML traits for IFSStub objects.
template <> struct llvm::yaml::MappingTraits<IFSStub> {
  static void mapping(IO &IO, IFSStub &Stub) {
    if (!IO.mapTag("!experimental-ifs-v1", true))
      IO.setError("Not a .ifs YAML file.");
    IO.mapRequired("IfsVersion", Stub.IfsVersion);
    IO.mapOptional("Triple", Stub.Triple);
    IO.mapOptional("ObjectFileFormat", Stub.ObjectFileFormat);
    IO.mapOptional("SoName", Stub.SoName);
    IO.mapOptional("NeededLibs", Stub.NeededLibs);
    IO.mapRequired("Symbols", Stub.Symbols);
  }
};

static Expected<std::unique_ptr<IFSStub>> readInputFile(StringRef FilePath) {
  // Read in file.
  ErrorOr<std::unique_ptr<MemoryBuffer>> BufOrError =
      MemoryBuffer::getFile(FilePath);
  if (!BufOrError)
    return createStringError(BufOrError.getError(), "Could not open `%s`",
                             FilePath.data());

  std::unique_ptr<MemoryBuffer> FileReadBuffer = std::move(*BufOrError);
  yaml::Input YamlIn(FileReadBuffer->getBuffer());
  std::unique_ptr<IFSStub> Stub(new IFSStub());
  YamlIn >> *Stub;

  if (std::error_code Err = YamlIn.error())
    return createStringError(Err, "Failed reading Interface Stub File.");

  return std::move(Stub);
}

void writeIfoYaml(const llvm::Triple &T, const std::set<ELFSymbol> &Symbols,
                  const StringRef Format, raw_ostream &OS) {
  OS << "--- !" << Format << "\n";
  OS << "FileHeader:\n";
  OS << "  Class:           ELFCLASS";
  OS << (T.isArch64Bit() ? "64" : "32");
  OS << "\n";
  OS << "  Data:            ELFDATA2";
  OS << (T.isLittleEndian() ? "LSB" : "MSB");
  OS << "\n";
  OS << "  Type:            ET_DYN\n";
  OS << "  Machine:         "
     << llvm::StringSwitch<llvm::StringRef>(T.getArchName())
            .Case("x86_64", "EM_X86_64")
            .Case("i386", "EM_386")
            .Case("i686", "EM_386")
            .Case("aarch64", "EM_AARCH64")
            .Case("amdgcn", "EM_AMDGPU")
            .Case("r600", "EM_AMDGPU")
            .Case("arm", "EM_ARM")
            .Case("thumb", "EM_ARM")
            .Case("avr", "EM_AVR")
            .Case("mips", "EM_MIPS")
            .Case("mipsel", "EM_MIPS")
            .Case("mips64", "EM_MIPS")
            .Case("mips64el", "EM_MIPS")
            .Case("msp430", "EM_MSP430")
            .Case("ppc", "EM_PPC")
            .Case("ppc64", "EM_PPC64")
            .Case("ppc64le", "EM_PPC64")
            .Case("x86", T.isOSIAMCU() ? "EM_IAMCU" : "EM_386")
            .Case("x86_64", "EM_X86_64")
            .Default("EM_NONE")
     << "\nSymbols:\n";
  for (const auto &Symbol : Symbols) {
    OS << "  - Name:            " << Symbol.Name << "\n"
       << "    Type:            STT_";
    switch (Symbol.Type) {
    default:
    case llvm::elfabi::ELFSymbolType::NoType:
      OS << "NOTYPE";
      break;
    case llvm::elfabi::ELFSymbolType::Object:
      OS << "OBJECT";
      break;
    case llvm::elfabi::ELFSymbolType::Func:
      OS << "FUNC";
      break;
    }
    OS << "\n    Binding:         STB_" << (Symbol.Weak ? "WEAK" : "GLOBAL")
       << "\n";
  }
  OS << "...\n";
  OS.flush();
}

// New Interface Stubs Yaml Format:
// --- !experimental-ifs-v1
// IfsVersion:      1.0
// Triple:          <llvm triple>
// ObjectFileFormat: <ELF | others not yet supported>
// Symbols:
//   _ZSymbolName: { Type: <type> }
// ...

int main(int argc, char *argv[]) {
  // Parse arguments.
  cl::ParseCommandLineOptions(argc, argv);

  if (InputFilenames.empty())
    InputFilenames.push_back("-");

  IFSStub Stub;

  for (const std::string &InputFilePath : InputFilenames) {
    Expected<std::unique_ptr<IFSStub>> StubOrErr = readInputFile(InputFilePath);
    if (!StubOrErr) {
      WithColor::error() << StubOrErr.takeError() << "\n";
      return -1;
    }
    std::unique_ptr<IFSStub> TargetStub = std::move(StubOrErr.get());

    if (Stub.Triple.empty()) {
      Stub.IfsVersion = TargetStub->IfsVersion;
      Stub.Triple = TargetStub->Triple;
      Stub.ObjectFileFormat = TargetStub->ObjectFileFormat;
      Stub.SoName = TargetStub->SoName;
      Stub.NeededLibs = TargetStub->NeededLibs;
    } else if (Stub.IfsVersion != TargetStub->IfsVersion ||
               Stub.Triple != TargetStub->Triple ||
               Stub.ObjectFileFormat != TargetStub->ObjectFileFormat ||
               Stub.SoName != TargetStub->SoName ||
               Stub.NeededLibs != TargetStub->NeededLibs) {
      WithColor::error() << "Interface Stub Input Mismatch.\n";
      return -1;
    }

    for (auto Symbol : TargetStub->Symbols)
      Stub.Symbols.insert(Symbol);
  }

  std::error_code SysErr;

  // Open file for writing.
  raw_fd_ostream Out(OutputFilename, SysErr);
  if (SysErr) {
    WithColor::error() << "Couldn't open " << OutputFilename
                       << " for writing.\n";
    return -1;
  }

  if (Action == "write-ifs") {
    yaml::Output YamlOut(Out, NULL, /*WrapColumn =*/0);
    YamlOut << const_cast<IFSStub &>(Stub);
  } else {
    if (Stub.ObjectFileFormat != "ELF") {
      WithColor::error()
          << "Invalid ObjectFileFormat: Only ELF is supported.\n";
      return -1;
    }

    SmallString<0> Storage;
    Storage.clear();
    raw_svector_ostream OS(Storage);
    writeIfoYaml(llvm::Triple(Stub.Triple), Stub.Symbols, Stub.ObjectFileFormat,
                 OS);

    yaml::Input YIn(OS.str());
    Error E = llvm::yaml::convertYAML(YIn, Out);
    if (!E)
      return -1;
  }
}
