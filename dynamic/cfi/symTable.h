
#ifndef _SYMTAB_
#define _SYMTAB_

#include "elf.h"

void parseELF32(uint8_t * buffer);
void parseELF64(uint8_t * buffer);
typedef struct Symdata {
    std::map<uintptr_t, const char*> exportedSyms; //<sym addr, sym name>
    std::set<const char *> importedSyms;       //<sym name, 1>
} Symdata;

#endif


