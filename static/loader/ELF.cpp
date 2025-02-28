#include "ParseELF.h"          // ELF files structure

#include "Executable.h"
#include <iostream>
#include <cstring>
#include <cassert>
#include "IO.h"

using namespace std;
using namespace janus;

bool janus::operator<(const Symbol &a, const Symbol &b)
{
    return a.startAddr < b.startAddr;
}


void Executable::parseELF64()
{
    vector<Elf64_Shdr> sectionHeaders;
    char *sectionStringTable;
    uint32_t sectionStringTableSize;
    int symbolTableSectionIndex = -1;
    int dynamicSymbolTableSectionIndex = -1;

    Elf64_Ehdr fileHeader = *(Elf64_Ehdr *)(buffer);
    //get number of sections
    uint32_t nSections = fileHeader.e_shnum;
    //get the pointer to section header
    uint32_t readSectPtr = (uint32_t)(fileHeader.e_shoff);

    uint32_t sectonHeaderSize = fileHeader.e_shentsize;
    GASSERT(sectonHeaderSize>0, "section header not recognised");

    //parse string table for sections index
    uint32_t stringTableIndex = fileHeader.e_shstrndx;
    GASSERT(sectonHeaderSize>0, "string table not loaded");

    //parse sections and find section string table.
    for(uint32_t i=0; i<nSections; i++)
    {
        Elf64_Shdr sectHeader = CAST(Elf64_Shdr, buffer, readSectPtr);

        sectionHeaders.push_back(sectHeader);
        readSectPtr += sectonHeaderSize;

        //if the current section is string table
        if(i == stringTableIndex) {
            sectionStringTable = (char *)buffer + (uint32_t)(sectHeader.sh_offset);
            sectionStringTableSize = sectHeader.sh_size;
        }
    }

    //parse sections to find symbol tables
    for(uint32_t i=0; i<nSections; i++)
    {
        auto &sectHeader = sectionHeaders[i];
        uint32_t sectionNameIndex = sectHeader.sh_name;

        GASSERT(sectionNameIndex < sectionStringTableSize, "Section name string not found");

        string sectionName = string(sectionStringTable + sectionNameIndex);

        Section section(
            sectionName,
            sectHeader.sh_addr, 
            buffer + (uint32_t)(sectHeader.sh_offset),
            sectHeader.sh_size
        );
        /* Insert fake-symbols at end of each section
         * to prevent disassemble going unbound */
        Symbol symbol(string("section_end"), sectHeader.sh_addr + sectHeader.sh_size,
                       &section, SYM_NONE);
        symbols.insert(symbol);

        switch(sectHeader.sh_type) {
            case SHT_PROGBITS: section.type = SECT_EXE; break;
            case SHT_SYMTAB: 
                section.type = SECT_SYMTAB; 
                symbolTableSectionIndex = i;
                break;
            case SHT_DYNSYM:
                section.type = SECT_SYMTAB; 
                dynamicSymbolTableSectionIndex = i;
                break;
            case SHT_RELA:
            case SHT_REL:
                section.type = SECT_RELA;
                break;
            case SHT_STRTAB: section.type = SECT_STRTAB; break;
            default: section.type = SECT_NONE; break;
        }

        /* This is only specific to GNU ELF */
        if(sectionName == string(".plt") || sectionName == string("_plt"))
            pltSectionIndex = i;
        if(sectionName == string(".plt.sec"))
            pltsecSectionIndex = i;
        if(sectionName == string(".plt.got"))
            pltGOTSectionIndex = i;

        sections.push_back(section);
        //to scan for got offset addresses for pic binaries.
        if(sectionName == string(".got.plt") || sectionName == string(".got")){
            GOTAddress = section.startAddr;
        }
    }

    //complete end address information
    Section *lastSection = NULL;
    for(auto &sect: sections)
    {
        if(lastSection) {
            if(sect.startAddr)
                lastSection->endAddr = sect.startAddr;
            else 
                lastSection->endAddr = lastSection->startAddr + lastSection->size;
        }
        lastSection = &sect;
    }
    //TODO resolve the last section end address
    if(lastSection) lastSection->endAddr = lastSection->startAddr;

    if(symbolTableSectionIndex > 0) {
        //parse static symbols
        Elf64_Shdr &symtabSection = sectionHeaders[symbolTableSectionIndex];

        GASSERT(symtabSection.sh_link < nSections, "Symbol table section link corrupted");

        char *symtabStringTable = (char *)buffer + sectionHeaders[symtabSection.sh_link].sh_offset;

        uint32_t symtabSize = uint32_t(symtabSection.sh_size);

        char *symtab = (char *)buffer + symtabSection.sh_offset;

        char *symtabEnd = symtab + symtabSize;

        uint32_t symtabEntrySize = symtabSection.sh_entsize;

        for(uint32_t isym=0; symtab < symtabEnd; symtab += symtabEntrySize, isym++)
        {
            Elf64_Sym sym = *(Elf64_Sym *)symtab;

            int type = sym.st_type;
            if(type == STT_GNU_IFUNC) type = SYM_FUNC;
            else if(type>STT_FILE) type = SYM_OTHER;
            int binding = sym.st_bind;
            string symbolName = string(symtabStringTable + sym.st_name);

            if (int16_t(sym.st_shndx) > 0){
                Section &mySection = sections[sym.st_shndx];
                if (type != STT_SECTION) {
                    Symbol symbol(symbolName, sym.st_value, &mySection, (SymbolType)type,sym.st_size);
                    symbols.insert(symbol);
                } else {
                    Symbol symbol(symbolName, sym.st_value, &mySection, SYM_NONE,sym.st_size);
                    symbols.insert(symbol);
                }     
            } 
        }
        hasStaticSymbolTable = true;
    } else hasStaticSymbolTable = false;
    
    if(dynamicSymbolTableSectionIndex > 0) {
        //parse dynamic symbols
        Elf64_Shdr &dynSymtabSection = sectionHeaders[dynamicSymbolTableSectionIndex];

        GASSERT(dynSymtabSection.sh_link < nSections, "Dynamic symbol table section link corrupted");

        char *dynSymtabStringTable = (char *)buffer + sectionHeaders[dynSymtabSection.sh_link].sh_offset;

        uint32_t dynSymtabSize = uint32_t(dynSymtabSection.sh_size);

        uint32_t dynSymtabEntrySize = dynSymtabSection.sh_entsize;
        
        char *dynSymtab = (char *)buffer + dynSymtabSection.sh_offset;
        char *dynSymtabEnd = dynSymtab + dynSymtabSize;

        Elf64_Sym *relaTab = (Elf64_Sym *)dynSymtab;

        for(uint32_t isym=0; dynSymtab < dynSymtabEnd; dynSymtab += dynSymtabEntrySize, isym++)
        {
            Elf64_Sym sym = *(Elf64_Sym *)dynSymtab;

            int type = sym.st_type;
            if(type == STT_GNU_IFUNC) type = SYM_FUNC;
            else if(type>STT_FILE) type = SYM_OTHER;
            int binding = sym.st_bind;

            string symbolName = string(dynSymtabStringTable + sym.st_name);
            if (int16_t(sym.st_shndx) > 0)
            {
                Section &mySection = sections[sym.st_shndx];
                Symbol symbol(symbolName, sym.st_value, &mySection, (SymbolType)type);
                symbols.insert(symbol);              
            } 
        }
        hasDynamicSymbolTable = true;
        pltAvailable = false;
        int index = -1;
        //.plt section
        if(pltSectionIndex > 0){
            Section &plt = sections[pltSectionIndex];
            symbols.insert(Symbol(plt.name, plt.startAddr, &plt, SYM_FUNC));
            pltSectionStartAddr = plt.startAddr;
            pltAvailable = true;
        }
        //.plt.sec section 
        if(pltsecSectionIndex > 0){
            Section &plt = sections[pltsecSectionIndex];
            symbols.insert(Symbol(plt.name, plt.startAddr, &plt, SYM_FUNC));
            pltsecSectionStartAddr = plt.startAddr;
            pltAvailable = true;
        }
        //.plt.got section
        if(pltGOTSectionIndex > 0){
            Section &plt = sections[pltGOTSectionIndex];
            symbols.insert(Symbol(plt.name, plt.startAddr, &plt, SYM_FUNC));
            pltGOTSectionStartAddr = plt.startAddr;
        }
        //HACK: some binaries will have both .plt and .plt.sec sections, with relocation symbols associated with .plt.sec. in that case, if the number of entries in those section and ones in relocation table do not match, we associate symbols with .plt.sec, and make fake symbols for .plt and no relocated address either.

        if(pltAvailable){
            //relocation symbols
            //since there is no plt symbols, we need to create it by ourselfs
            //borrows ideas from _bfd_get_synthetic_symbol
            if(pltsecSectionIndex > 0){
                index = pltsecSectionIndex;
                pltAlternative = true;
            }
            else if(pltSectionIndex > 0){
                index = pltSectionIndex;
                pltAlternative = false;
            }
            Section &plt = sections[index]; //it.first is the index
            for(uint32_t i=0; i<nSections; i++)
            {
                Elf64_Shdr &sectHeader = sectionHeaders[i];

                string sectionName = string(sectionStringTable+sectHeader.sh_name);
                //parse relocations
                if(sectHeader.sh_type == SHT_REL || sectHeader.sh_type == SHT_RELA) {
                    char * reltab = (char *)buffer + uint32_t(sectHeader.sh_offset);
                    char * reltabend = reltab + uint32_t(sectHeader.sh_size);
                    uint32_t entrysize = sectHeader.sh_entsize;
                    uint32_t expectedEntrySize = sectHeader.sh_type == SHT_RELA ? 
                       sizeof(Elf64_Rela) :              // Elf64_Rela, Elf64_Rela
                       sizeof(Elf64_Rela) - wordSize/8;  // Elf64_Rel,  Elf64_Rel

                    if (entrysize < expectedEntrySize) {entrysize = expectedEntrySize;}
                    uint64_t addr = plt.startAddr;
                    // Loop through entries
                    for (; reltab < reltabend; reltab += entrysize) {
                        Elf64_Rela rel;  rel.r_addend = 0;
                        memcpy(&rel, reltab, entrysize);
#ifdef JANUS_X86
                        if(rel.r_type != R_386_JMP_SLOT) continue;
#elif JANUS_AARCH64
                        //R_AARCH64_JUMP_SLOT 1026
                        if(rel.r_type != 1026) continue;
#endif
                        string symbolName = string(dynSymtabStringTable + relaTab[rel.r_sym].st_name);
                        
                        PCAddress relocatedAddress = rel.r_offset;
                        //a hack to include functions from plt into external function set: for security code
                        externalFuncNames.insert(symbolName+"@plt");
                        Symbol symbol(symbolName,relocatedAddress, &plt, SYM_RELA); 
                        symbols.insert(symbol);   
                    }
                }
            }
        }
    } else hasDynamicSymbolTable = false;
}
void Executable::parseELF32()
{
    vector<Elf32_Shdr> sectionHeaders;
    char *sectionStringTable;
    uint32_t sectionStringTableSize;
    int symbolTableSectionIndex = -1;
    int dynamicSymbolTableSectionIndex = -1;

    Elf32_Ehdr fileHeader = *(Elf32_Ehdr *)(buffer);
    //get number of sections
    uint32_t nSections = fileHeader.e_shnum;
    //get the pointer to section header
    uint32_t readSectPtr = (uint32_t)(fileHeader.e_shoff);

    uint32_t sectonHeaderSize = fileHeader.e_shentsize;
    GASSERT(sectonHeaderSize>0, "section header not recognised");

    //parse string table for sections index
    uint32_t stringTableIndex = fileHeader.e_shstrndx;
    GASSERT(sectonHeaderSize>0, "string table not loaded");
    //parse sections and find section string table.
    for(uint32_t i=0; i<nSections; i++)
    {
        Elf32_Shdr sectHeader = CAST(Elf32_Shdr, buffer, readSectPtr);

        sectionHeaders.push_back(sectHeader);
        readSectPtr += sectonHeaderSize;

        //if the current section is string table
        if(i == stringTableIndex) {
            sectionStringTable = (char *)buffer + (uint32_t)(sectHeader.sh_offset);
            sectionStringTableSize = sectHeader.sh_size;
        }
    }

    //parse sections to find symbol tables
    for(uint32_t i=0; i<nSections; i++)
    {
        auto &sectHeader = sectionHeaders[i];
        uint32_t sectionNameIndex = sectHeader.sh_name;

        GASSERT(sectionNameIndex < sectionStringTableSize, "Section name string not found");

        string sectionName = string(sectionStringTable + sectionNameIndex);

        Section section(
            sectionName,
            sectHeader.sh_addr, 
            buffer + (uint32_t)(sectHeader.sh_offset),
            sectHeader.sh_size
        );

        /* Insert fake-symbols at end of each section
         * to prevent disassemble going unbound */
        Symbol symbol(string("section_end"), sectHeader.sh_addr + sectHeader.sh_size,
                       &section, SYM_NONE);
        symbols.insert(symbol);

        switch(sectHeader.sh_type) {
            case SHT_PROGBITS: section.type = SECT_EXE; break;
            case SHT_SYMTAB: 
                section.type = SECT_SYMTAB; 
                symbolTableSectionIndex = i;
                break;
            case SHT_DYNSYM:
                section.type = SECT_SYMTAB; 
                dynamicSymbolTableSectionIndex = i;
                break;
            case SHT_RELA:
            case SHT_REL:
                section.type = SECT_REL;
                break;
            case SHT_STRTAB: section.type = SECT_STRTAB; break;
            default: section.type = SECT_NONE; break;
        }

        /* This is only specific to GNU ELF */
        if(sectionName == string(".plt") || sectionName == string("_plt"))
            pltSectionIndex = i;
        if(sectionName == string(".plt.sec"))
            pltsecSectionIndex = i;
        if(sectionName == string(".plt.got"))
            pltGOTSectionIndex = i;

        //to scan for got offset addresses for pic binaries.
        if(sectionName == string(".got.plt") || sectionName == string(".got")){
            GOTAddress = section.startAddr;
            cout<<sectionName<<" addr:"<<hex<<GOTAddress<<endl;
        }
        if(sectionName == string(".init") || sectionName == string(".fini")){
            exportedSymbols[section.startAddr] = sectionName;
        }
        if(sectionName == string(".rodata")){
            RODATA_offset = section.startAddr;
            RODATA_size   = section.size;
        }

        sections.push_back(section);
    }
    //complete end address information
    Section *lastSection = NULL;
    for(auto &sect: sections)
    {
        if(lastSection) {
            if(sect.startAddr)
                lastSection->endAddr = sect.startAddr;
            else 
                lastSection->endAddr = lastSection->startAddr + lastSection->size;
        }
        lastSection = &sect;
    }
    //TODO resolve the last section end address
    if(lastSection) lastSection->endAddr = lastSection->startAddr;

    if(symbolTableSectionIndex > 0) {
        //parse static symbols
        Elf32_Shdr &symtabSection = sectionHeaders[symbolTableSectionIndex];

        GASSERT(symtabSection.sh_link < nSections, "Symbol table section link corrupted");

        char *symtabStringTable = (char *)buffer + sectionHeaders[symtabSection.sh_link].sh_offset;

        uint32_t symtabSize = uint32_t(symtabSection.sh_size);

        char *symtab = (char *)buffer + symtabSection.sh_offset;

        char *symtabEnd = symtab + symtabSize;

        uint32_t symtabEntrySize = symtabSection.sh_entsize;

        for(uint32_t isym=0; symtab < symtabEnd; symtab += symtabEntrySize, isym++)
        {
            Elf32_Sym sym = *(Elf32_Sym *)symtab;

            int type = sym.st_type;
            if(type == STT_GNU_IFUNC) type = SYM_FUNC;
            else if(type>STT_FILE) type = SYM_OTHER;
            int binding = sym.st_bind;
            string symbolName = string(symtabStringTable + sym.st_name);

            if (int16_t(sym.st_shndx) > 0){
                Section &mySection = sections[sym.st_shndx];
                if (type != STT_SECTION) {
                    if((binding == STB_GLOBAL || binding == STB_WEAK)&& (sym.st_type == STT_FUNC || sym.st_type == STT_GNU_IFUNC)) 
                    {
                       //cout<<"exported symbol : "<<symbolName<<endl;
                       exportedSymbols[sym.st_value] = symbolName;
                    }
                    //Symbol symbol(symbolName, sym.st_value, &mySection, (SymbolType)type);
                    Symbol symbol(symbolName, sym.st_value, &mySection, (SymbolType)type,sym.st_size);
                    symbols.insert(symbol);
                } else {
                    //Symbol symbol(symbolName, sym.st_value, &mySection, SYM_NONE);
                    Symbol symbol(symbolName, sym.st_value, &mySection, SYM_NONE,sym.st_size);
                    symbols.insert(symbol);
                }     
            }
            else{ //undefined section for the symbol. imported symbols, value (address where it is define) of imported symbols is zero as address is not known
                  //weak binding for symbols in .plt.got that may have type NOTYPE such as __gmon_start
                  if ((sym.st_type == STT_FUNC || sym.st_type == STT_GNU_IFUNC || sym.st_type == STT_NOTYPE) && (binding == STB_GLOBAL || binding  == STB_WEAK)){
                       //cout<<"imported symbol : "<<symbolName<<endl;
                       importedSymbols.insert(symbolName);
                  }
            }
        }
        hasStaticSymbolTable = true;
    } else hasStaticSymbolTable = false;
    
    if(dynamicSymbolTableSectionIndex > 0) {
        //parse dynamic symbols
        Elf32_Shdr &dynSymtabSection = sectionHeaders[dynamicSymbolTableSectionIndex];

        GASSERT(dynSymtabSection.sh_link < nSections, "Dynamic symbol table section link corrupted");

        char *dynSymtabStringTable = (char *)buffer + sectionHeaders[dynSymtabSection.sh_link].sh_offset;

        uint32_t dynSymtabSize = uint32_t(dynSymtabSection.sh_size);

        uint32_t dynSymtabEntrySize = dynSymtabSection.sh_entsize;
        
        char *dynSymtab = (char *)buffer + dynSymtabSection.sh_offset;
        char *dynSymtabEnd = dynSymtab + dynSymtabSize;

        Elf32_Sym *relaTab = (Elf32_Sym *)dynSymtab;

        for(uint32_t isym=0; dynSymtab < dynSymtabEnd; dynSymtab += dynSymtabEntrySize, isym++)
        {
            Elf32_Sym sym = *(Elf32_Sym *)dynSymtab;

            int type = sym.st_type;
            if(type == STT_GNU_IFUNC) type = SYM_FUNC;
            else if(type>STT_FILE) type = SYM_OTHER;
            int binding = sym.st_bind;

            string symbolName = string(dynSymtabStringTable + sym.st_name);

            if (int16_t(sym.st_shndx) > 0)
            {
                if((binding == STB_GLOBAL || binding == STB_WEAK) && (sym.st_type == STT_FUNC || sym.st_type == STT_GNU_IFUNC)){ 
                       exportedSymbols[sym.st_value] = symbolName;
                }
                Section &mySection = sections[sym.st_shndx];
                Symbol symbol(symbolName, sym.st_value, &mySection, (SymbolType)type);
                symbols.insert(symbol);              
            } 
            else{ //undefined section for the symbol. imported symbols, value (address where it is define) of imported symbols is zero as address is not known
                  //weak binding for symbols in .plt.got that may have type NOTYPE such as __gmon_start
                  if ((sym.st_type == STT_FUNC || sym.st_type == STT_GNU_IFUNC || sym.st_type == STT_NOTYPE) && (binding == STB_GLOBAL || binding  == STB_WEAK)){
                       //cout<<"imported symbol : "<<symbolName<<endl;
                       importedSymbols.insert(symbolName);
                  }
            }
        }
        hasDynamicSymbolTable = true;
        int index = -1;
        pltAvailable = false;
        //.plt section
        if(pltSectionIndex > 0){
            Section &plt = sections[pltSectionIndex];
            symbols.insert(Symbol(plt.name, plt.startAddr, &plt, SYM_FUNC));
            pltSectionStartAddr = plt.startAddr;
            pltAvailable = true;
        }
        //.plt.sec section 
        if(pltsecSectionIndex > 0){
            Section &plt = sections[pltsecSectionIndex];
            symbols.insert(Symbol(plt.name, plt.startAddr, &plt, SYM_FUNC));
            pltsecSectionStartAddr = plt.startAddr;
            pltAvailable = true;
        }
        //.plt.got section
        if(pltGOTSectionIndex > 0){
            Section &plt = sections[pltGOTSectionIndex];
            symbols.insert(Symbol(plt.name, plt.startAddr, &plt, SYM_FUNC));
            pltGOTSectionStartAddr = plt.startAddr;
        }
        if(pltAvailable){
            //relocation symbols
            //since there is no plt symbols, we need to create it by ourselfs
            //borrows ideas from _bfd_get_synthetic_symbol
            if(pltsecSectionIndex > 0){
                index = pltsecSectionIndex;
                pltAlternative = true;
            }
            else if(pltSectionIndex > 0){
                index = pltSectionIndex;
                pltAlternative = false;
            }
            Section &plt = sections[index]; 
            for(uint32_t i=0; i<nSections; i++)
            {
                Elf32_Shdr &sectHeader = sectionHeaders[i];

                string sectionName = string(sectionStringTable+sectHeader.sh_name);
                //parse relocations
                if(sectHeader.sh_type == SHT_REL || sectHeader.sh_type == SHT_RELA) {
                    char * reltab = (char *)buffer + uint32_t(sectHeader.sh_offset);
                    char * reltabend = reltab + uint32_t(sectHeader.sh_size);
                    uint32_t entrysize = sectHeader.sh_entsize;
                    /*TODO: check Elf32_Rela and Rel 
                    */
                    uint32_t expectedEntrySize = sectHeader.sh_type == SHT_REL ? 
                    sizeof(Elf32_Rela) - wordSize/8 :              // Elf32_Rel, Elf32_Rel
                       sizeof(Elf32_Rela);  // Elf32_Rel,  Elf32_Rel

                     if (entrysize < expectedEntrySize) {entrysize = expectedEntrySize;}

                    // Loop through entries
                     for (; reltab < reltabend; reltab += entrysize) {
                        Elf32_Rel rel; 
                        memcpy(&rel, reltab, entrysize);
#ifdef JANUS_X86
                        if(rel.r_type != R_386_JMP_SLOT) continue;
#elif JANUS_AARCH64
                        //R_AARCH64_JUMP_SLOT 1026
                        if(rel.r_type != 1026) continue;
#endif
                        string symbolName = string(dynSymtabStringTable + relaTab[rel.r_sym].st_name);
                        
                        PCAddress relocatedAddress = rel.r_offset;
                        //a hack to include functions from plt into external function set: for security code
                        externalFuncNames.insert(symbolName+"@plt");

                        //Symbol symbol(symbolName,relocatedAddress, &plt, SYM_RELA); 
                        Symbol symbol(symbolName,relocatedAddress, &plt, SYM_REL); 
                        symbols.insert(symbol);   
                    }
                }
            }
        }
    }
    else hasDynamicSymbolTable = false;

}
