#ifdef _GELF_
#define _GELF_

#include "elf.h"          // ELF files structure

class Executable;

// Class for interpreting and dumping ELF files. Has templates for 32 and 64 bit version
template <class TELF_Header, class TELF_SectionHeader, class TELF_Symbol, class TELF_Relocation>
class ELFParser {
public:
    ELFParser(janus::Executable *exe); 
};

#endif