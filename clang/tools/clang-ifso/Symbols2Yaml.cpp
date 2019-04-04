#include <string>
#include <vector>

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Support/raw_ostream.h"

void Symbols2Yaml(const llvm::Triple &T,
                  const std::vector<std::string> &SymbolNames,
                  llvm::raw_ostream &OS) {
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
  OS << "  - Name:            .dynsym\n";
  OS << "    Type:            STT_SECTION\n";
  OS << "    Section:         .dynsym\n";
  OS << "  - Name:            .dynstr\n";
  OS << "    Type:            STT_SECTION\n";
  OS << "    Section:         .dynstr\n";

  for (auto Name : SymbolNames) {
    OS << "  - Name:            " << Name << "\n";
    OS << "    Type:            STT_FUNC\n";
    OS << "    Section:         .text\n";
    OS << "    Binding:         STB_GLOBAL\n";
  }

  OS << "DynamicSymbols:\n";
  for (auto Name : SymbolNames) {
    OS << "  - Name:            " << Name << "\n";
    OS << "    Type:            STT_FUNC\n";
    OS << "    Section:         .text\n";
    OS << "    Binding:         STB_GLOBAL\n";
  }
  OS << "...\n";
  OS.flush();
}
