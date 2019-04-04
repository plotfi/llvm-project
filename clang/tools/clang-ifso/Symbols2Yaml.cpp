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
          .Default("EM_NONE");

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
