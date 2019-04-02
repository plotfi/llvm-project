#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/WithColor.h"
#include "llvm/MC/StringTableBuilder.h"
#include "llvm/Object/ELFTypes.h"

#include <string>
#include <vector>


#ifndef _ELF_IFSO_WRITER_
#define _ELF_IFSO_WRITER_


// Used to keep track of section and symbol names, so that in the YAML file
// sections and symbols can be referenced by name instead of by index.
namespace {
class NameToIdxMap {
  llvm::StringMap<int> Map;
public:
  /// \returns true if name is already present in the map.
  bool addName(llvm::StringRef Name, unsigned i) {
    return !Map.insert(std::make_pair(Name, (int)i)).second;
  }
  /// \returns true if name is not present in the map
  bool lookup(llvm::StringRef Name, unsigned &Idx) const {
    llvm::StringMap<int>::const_iterator I = Map.find(Name);
    if (I == Map.end())
      return true;
    Idx = I->getValue();
    return false;
  }
  /// asserts if name is not present in the map
  unsigned get(llvm::StringRef Name) const {
    unsigned Idx = 0;
    auto missing = lookup(Name, Idx);
    (void)missing;
    assert(!missing && "Expected section not found in index");
    return Idx;
  }
  unsigned size() const { return Map.size(); }
};
} // end anonymous namespace

/*
bool hasDynamicSymbols() {
  return !Doc.DynamicSymbols.Global.empty() ||
         !Doc.DynamicSymbols.Weak.empty() ||
         !Doc.DynamicSymbols.Local.empty() ||
         !Doc.DynamicSymbols.GNUUnique.empty();
}
*/

llvm::SmallVector<const char *, 5> implicitSectionNames() {
  // if (!hasDynamicSymbols())
  //   return {".symtab", ".strtab", ".shstrtab"};
  return {".symtab", ".strtab", ".shstrtab", ".dynsym", ".dynstr"};
}

template <typename ELFT> bool buildSectionIndex(
  const std::vector<llvm::StringRef> &SectionNames,
  NameToIdxMap &SN2I,
  llvm::StringTableBuilder &DotShStrtab) {
  for (unsigned i = 0, e = SectionNames.size(); i != e; ++i) {
    llvm::StringRef Name = SectionNames[i];
    DotShStrtab.add(Name);
    // "+ 1" to take into account the SHT_NULL entry.
    if (SN2I.addName(Name, i + 1)) {
      llvm::WithColor::error() << "Repeated section name: '" << Name
                         << "' at YAML section number " << i << ".\n";
      return false;
    }
  }

  auto SecNo = 1 + SectionNames.size();
  // Add special sections after input sections, if necessary.
  for (const auto &Name : implicitSectionNames())
    if (!SN2I.addName(Name, SecNo)) {
      // Account for this section, since it wasn't in the Doc
      ++SecNo;
      DotShStrtab.add(Name);
    }

  DotShStrtab.finalize();
  return true;
}

template <typename ELFT>
bool buildSymbolIndex(const std::vector<std::string> &Global,
                      const std::vector<std::string> &Local,
                      const std::vector<std::string> &Weak,
                      const std::vector<std::string> &GNUUnique,
                      NameToIdxMap &SymN2I) {
  std::size_t I = 0;
  std::vector<std::vector<std::string>> Sections = {Global, Local, Weak,
                                                    GNUUnique};

  for (const std::vector<std::string> &V : Sections) {
    for (const auto &Sym : V) {
      ++I;
      if (Sym.empty())
        continue;
      if (SymN2I.addName(Sym, I)) {
        llvm::WithColor::error() << "Repeated symbol name: '" << Sym << "'.\n";
        return false;
      }
    }
  }
  return true;
}

template <typename ELFT>
void initELFHeader(typename ELFT::Ehdr &Header) {
  /*
  using namespace llvm::ELF;
  zero(Header);
  Header.e_ident[EI_MAG0] = 0x7f;
  Header.e_ident[EI_MAG1] = 'E';
  Header.e_ident[EI_MAG2] = 'L';
  Header.e_ident[EI_MAG3] = 'F';
  Header.e_ident[EI_CLASS] = ELFT::Is64Bits ? ELFCLASS64 : ELFCLASS32;
  Header.e_ident[EI_DATA] = Doc.Header.Data;
  Header.e_ident[EI_VERSION] = EV_CURRENT;
  Header.e_ident[EI_OSABI] = Doc.Header.OSABI;
  Header.e_ident[EI_ABIVERSION] = Doc.Header.ABIVersion;
  Header.e_type = Doc.Header.Type;
  Header.e_machine = Doc.Header.Machine;
  Header.e_version = EV_CURRENT;
  Header.e_entry = Doc.Header.Entry;
  Header.e_phoff = sizeof(Header);
  Header.e_flags = Doc.Header.Flags;
  Header.e_ehsize = sizeof(Elf_Ehdr);
  Header.e_phentsize = sizeof(Elf_Phdr);
  Header.e_phnum = Doc.ProgramHeaders.size();
  Header.e_shentsize = sizeof(Elf_Shdr);
  // Immediately following the ELF header and program headers.
  Header.e_shoff =
      sizeof(Header) + sizeof(Elf_Phdr) * Doc.ProgramHeaders.size();
  Header.e_shnum = getSectionCount();
  Header.e_shstrndx = getDotShStrTabSecNo();
  */
}

template <typename ELFT> int writeELF(std::vector<std::string> SymbolNames,
                                      llvm::raw_ostream &OS) {
  std::vector<std::string> Global;
  std::vector<std::string> Local;
  std::vector<std::string> Weak;
  std::vector<std::string> GNUUnique;

  for (const auto &SymbolName : SymbolNames)
    Global.push_back(SymbolName);

  NameToIdxMap SN2I;
  NameToIdxMap SymN2I;

  /// The future ".strtab" section.
  llvm::StringTableBuilder DotStrtab{llvm::StringTableBuilder::ELF};

  /// The future ".shstrtab" section.
  llvm::StringTableBuilder DotShStrtab{llvm::StringTableBuilder::ELF};

  /// The future ".dynstr" section.
  llvm::StringTableBuilder DotDynstr{llvm::StringTableBuilder::ELF};

  // Finalize .strtab and .dynstr sections. We do that early because want to
  // finalize the string table builders before writing the content of the
  // sections that might want to use them.
  auto AddSymbols = [](const std::vector<std::string> &Global,
                       const std::vector<std::string> &Local,
                       const std::vector<std::string> &Weak,
                       const std::vector<std::string> &GNUUnique,
                       llvm::StringTableBuilder &StrTab) {
    for (const auto &Sym : Global)
      StrTab.add(Sym);
    for (const auto &Sym : Local)
      StrTab.add(Sym);
    for (const auto &Sym : Weak)
      StrTab.add(Sym);
    for (const auto &Sym : GNUUnique)
      StrTab.add(Sym);
  };

  AddSymbols(Global, Local, Weak, GNUUnique, DotStrtab);

  /*
  if (!buildSectionIndex<ELFT>(Global, Local, Weak, GNUUnique, SN2I))
    return 1;

  if (!buildSymbolIndex<ELFT>(Global, Local, Weak, GNUUnique, SymN2I))
    return 1;

  typename ELFT::Ehdr Header;
  State.initELFHeader(Header);

  // TODO: Flesh out section header support.

  std::vector<Elf_Phdr> PHeaders;
  State.initProgramHeaders(PHeaders);

  // XXX: This offset is tightly coupled with the order that we write
  // things to `OS`.
  const size_t SectionContentBeginOffset = Header.e_ehsize +
                                           Header.e_phentsize * Header.e_phnum +
                                           Header.e_shentsize * Header.e_shnum;
  ContiguousBlobAccumulator CBA(SectionContentBeginOffset);

  std::vector<Elf_Shdr> SHeaders;
  if(!State.initSectionHeaders(SHeaders, CBA))
    return 1;

  // Populate SHeaders with implicit sections not present in the Doc
  for (const auto &Name : State.implicitSectionNames())
    if (State.SN2I.get(Name) >= SHeaders.size())
      SHeaders.push_back({});

  // Initialize the implicit sections
  auto Index = State.SN2I.get(".symtab");
  State.initSymtabSectionHeader(SHeaders[Index], SymtabType::Static, CBA);
  Index = State.SN2I.get(".strtab");
  State.initStrtabSectionHeader(SHeaders[Index], ".strtab", State.DotStrtab, CBA);
  Index = State.SN2I.get(".shstrtab");
  State.initStrtabSectionHeader(SHeaders[Index], ".shstrtab", State.DotShStrtab, CBA);
  if (State.hasDynamicSymbols()) {
    Index = State.SN2I.get(".dynsym");
    State.initSymtabSectionHeader(SHeaders[Index], SymtabType::Dynamic, CBA);
    SHeaders[Index].sh_flags |= ELF::SHF_ALLOC;
    Index = State.SN2I.get(".dynstr");
    State.initStrtabSectionHeader(SHeaders[Index], ".dynstr", State.DotDynstr, CBA);
    SHeaders[Index].sh_flags |= ELF::SHF_ALLOC;
  }

  // Now we can decide segment offsets
  State.setProgramHeaderLayout(PHeaders, SHeaders);

  OS.write((const char *)&Header, sizeof(Header));
  writeArrayData(OS, makeArrayRef(PHeaders));
  writeArrayData(OS, makeArrayRef(SHeaders));
  CBA.writeBlobToStream(OS);
  */
  return 0;
}

#endif // _ELF_IFSO_WRITER_
