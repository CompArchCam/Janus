#ifndef Janus_EXECUTABLE_H
#define Janus_EXECUTABLE_H

#include "janus.h"
#include <set>
#include <string>
#include <vector>

#include "elf.h"

class JanusContext;

namespace janus
{
// ----------------------------------------------------------
//      Definition of a Janus executable
// ----------------------------------------------------------

enum SectionType {
    SECT_NONE,
    SECT_EXE,
    SECT_DATA,
    SECT_SYMTAB,
    SECT_STRTAB,
    SECT_RELA,
    SECT_NOTE,
    SECT_BSS
};

enum SymbolType {
    SYM_NONE = 0,
    SYM_OBJECT,
    SYM_FUNC,
    SYM_SECTION,
    SYM_OTHER,
    SYM_RELA
};

enum ExecutableType {
    UNRECOGNISED,
    BINARY_ELF,
    BINARY_MACHO_LE,
    BINARY_MACHO_BE,
    BINARY_COFF,
    BINARY_LIBRARY
};

struct Symbol;

struct Section {
    std::string name;
    SectionType type;
    PCAddress startAddr; // virtual addresses
    PCAddress endAddr;
    uint8_t *contents;
    uint32_t size;
    std::set<Symbol *> symbols;

    Section(std::string name, PCAddress startAddr, uint8_t *contents,
            uint32_t size)
        : name(name), startAddr(startAddr), contents(contents), size(size){};
};

struct Symbol {
    std::string name;
    SymbolType type;
    PCAddress startAddr; // virtual addresses
    Section *section;
    Symbol(std::string name, PCAddress startAddr, Section *section,
           SymbolType type)
        : name(name), startAddr(startAddr), section(section), type(type){};
};

// sorting for the symbol structure
bool operator<(const Symbol &a, const Symbol &b);

/** \brief Executable includes all the low level data loaded from binary
 * executables
 *
 */
class Executable
{
  public:
    Executable(){};
    ~Executable();
    uint64_t capstoneHandle;
    bool isExecutable;
    bool hasStaticSymbolTable;
    bool hasDynamicSymbolTable;
    int wordSize;

    std::vector<Section> sections;
    std::multiset<Symbol> symbols;
    std::set<PCAddress> crossSectionRef;
    ExecutableType type;

    uint32_t fileSize;
    /// load into the executable based on the file path
    void open(JanusContext *jc, const char *filename);
    /// lift to disassembly and functions
    void disassemble(JanusContext *jc);
    void printSection();

  protected:
    uint8_t *buffer; // executable storage
    uint32_t bufferSize;

  private:
    void parseHeader();
    void parseFlat();
    // create Function classes from the symbols
    void liftSymbolToFunction(JanusContext *jc);
    void retrieveHiddenSymbol(Section &section);
    void parseELF64();
};

} // end namespace janus

#define CAST(CLASS, buffer, offset) (*(CLASS *)((buffer) + (offset)))

// Class for interpreting and dumping ELF files. Has templates for 32 and 64 bit
// version

#endif
