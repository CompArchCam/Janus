
#ifndef _SYMTAB_
#define _SYMTAB_
#include<iostream>
#include<set>
#include "elf.h"

void parseELF32(uint8_t * buffer);
void parseELF64(uint8_t * buffer);
typedef struct Symdata {
    std::map<uintptr_t, const char*> exportedSyms; //<sym addr, sym name>
    std::set<const char *> importedSyms;       //<sym name, 1>
} Symdata;
typedef struct elfdata{
    uintptr_t text_start;
    uintptr_t text_end;
    uintptr_t init_start;
    uintptr_t init_end;
    uintptr_t fini_start;
    uintptr_t fini_end;
    uintptr_t ro_start;
    uintptr_t ro_end;
    uintptr_t plt_start;
    uintptr_t plt_end;
    uintptr_t plt_sec_start;
    uintptr_t plt_sec_end;
    uintptr_t plt_got_start;
    uintptr_t plt_got_end;
    uintptr_t got_plt_start;
    uintptr_t got_plt_end;
    uintptr_t got_start;
    uintptr_t got_end;
    uintptr_t GOTAddress;
    std::set<uintptr_t> symbols;
    std::set<uintptr_t> exportedSymbols;
    bool hasStaticSymbolTable;
    bool hasDynamicSymbolTable;
     
} ElfData;

#define CAST(CLASS,buffer,offset) \
  (*(CLASS *)((buffer) + (offset)))
void parseELF32(uint8_t * buffer, ElfData* elf){
    vector<Elf32_Shdr> sectionHeaders;
    char *sectionStringTable;
    uint32_t sectionStringTableSize;
    int symbolTableSectionIndex = -1;
    int dynamicSymbolTableSectionIndex = -1;
    bool pltAvailable = false;

    Elf32_Ehdr fileHeader = *(Elf32_Ehdr *)(buffer);
    //get number of sections
    uint32_t nSections = fileHeader.e_shnum;
    //get the pointer to section header
    uint32_t readSectPtr = (uint32_t)(fileHeader.e_shoff);

    uint32_t sectonHeaderSize = fileHeader.e_shentsize;
    if(sectonHeaderSize <= 0) 
        cerr<<"section header not recognised"<<endl;

    //parse string table for sections index
    uint32_t stringTableIndex = fileHeader.e_shstrndx;
    if(sectonHeaderSize <= 0) 
         cerr<<"string table not loaded"<<endl;
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

        if(sectionNameIndex >= sectionStringTableSize)
            cerr<<"Section name string not found"<<endl;

        string sectionName = string(sectionStringTable + sectionNameIndex);
        if(sectionName.compare(".text") == 0){
           elf->text_start = sectHeader.sh_addr;
           elf->text_end = elf->text_start + sectHeader.sh_size;
        }
        if(sectionName.compare(".init") == 0){
           elf->init_start = sectHeader.sh_addr;
           elf->init_end = elf->init_start + sectHeader.sh_size;
           elf->exportedSymbols.insert(elf->init_start);
        }
        if(sectionName.compare(".fini") == 0){
           elf->fini_start = sectHeader.sh_addr;
           elf->fini_end = elf->fini_start + sectHeader.sh_size;
           elf->exportedSymbols.insert(elf->fini_start);
        }
        if(sectionName.compare(".plt") == 0){
           elf->plt_start = sectHeader.sh_addr;
           elf->plt_end = elf->plt_start + sectHeader.sh_size;
           pltAvailable = true;
        }
        if(sectionName.compare(".plt.got") == 0){
           elf->plt_got_start = sectHeader.sh_addr;
           elf->plt_got_end = elf->plt_got_start + sectHeader.sh_size;
           pltAvailable = true;
        }
        if(sectionName.compare(".plt.sec") == 0){
           elf->plt_sec_start = sectHeader.sh_addr;
           elf->plt_sec_end = elf->plt_sec_start + sectHeader.sh_size;
           pltAvailable = true;
        }
        if(sectionName.compare(".got") == 0){
           elf->got_start = sectHeader.sh_addr;
           elf->got_end = elf->got_start + sectHeader.sh_size;
            elf->GOTAddress = elf->got_start;
        }
        if(sectionName.compare(".got.plt") == 0){
           elf->got_plt_start = sectHeader.sh_addr;
           elf->got_plt_end = elf->got_plt_start + sectHeader.sh_size;
           elf->GOTAddress = elf->got_plt_start;
        }
        if(sectionName.compare(".rodata") == 0){
           elf->ro_start = sectHeader.sh_addr;
           elf->ro_end = elf->ro_start + sectHeader.sh_size;
        }
        switch(sectHeader.sh_type) {
            case SHT_SYMTAB: 
                symbolTableSectionIndex = i;
                break;
            case SHT_DYNSYM:
                dynamicSymbolTableSectionIndex = i;
                break;
            default:
                break;
        }
    }

    if(symbolTableSectionIndex > 0) {
        //parse static symbols
        Elf32_Shdr &symtabSection = sectionHeaders[symbolTableSectionIndex];

        if(symtabSection.sh_link >= nSections)
            cerr<<"Symbol table section link corrupted"<<endl;

        char *symtabStringTable = (char *)buffer + sectionHeaders[symtabSection.sh_link].sh_offset;

        uint32_t symtabSize = uint32_t(symtabSection.sh_size);

        char *symtab = (char *)buffer + symtabSection.sh_offset;

        char *symtabEnd = symtab + symtabSize;

        uint32_t symtabEntrySize = symtabSection.sh_entsize;

        for(uint32_t isym=0; symtab < symtabEnd; symtab += symtabEntrySize, isym++)
        {
            Elf32_Sym sym = *(Elf32_Sym *)symtab;

            int type = sym.st_type;
            if(type == STT_GNU_IFUNC || type == STT_FUNC){ 
                int binding = sym.st_bind;
                if (int16_t(sym.st_shndx) > 0){
                    if (type != STT_SECTION) {
                        if((binding == STB_GLOBAL || binding == STB_WEAK)&& (sym.st_type == STT_FUNC || sym.st_type == STT_GNU_IFUNC)) 
                        {
                           elf->exportedSymbols.insert(sym.st_value);
                        }
                        //Symbol symbol(symbolName, sym.st_value, &mySection, (SymbolType)type);
                        elf->symbols.insert(sym.st_value);
                    }
                }
                /*else{ //undefined section for the symbol. imported symbols, value (address where it is define) of imported symbols is zero as address is not known
                  //weak binding for symbols in .plt.got that may have type NOTYPE such as __gmon_start
                  if ((sym.st_type == STT_FUNC || sym.st_type == STT_GNU_IFUNC || sym.st_type == STT_NOTYPE) && (binding == STB_GLOBAL || binding  == STB_WEAK)){
                       cout<<"imported symbol"<<endl;
                       //   importedSymbols.insert(symbolName);
                   }
                }*/
            }
        }
        elf->hasStaticSymbolTable = true;
    } else elf->hasStaticSymbolTable = false;
    
    if(dynamicSymbolTableSectionIndex > 0) {
        //parse dynamic symbols
        Elf32_Shdr &dynSymtabSection = sectionHeaders[dynamicSymbolTableSectionIndex];

        if(dynSymtabSection.sh_link >= nSections)
            cerr<<"Dynamic symbol table section link corrupted"<<endl;

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
            if(type == STT_GNU_IFUNC || type == STT_FUNC) {
                int binding = sym.st_bind;
                if (int16_t(sym.st_shndx) > 0)
                {
                    if((binding == STB_GLOBAL || binding == STB_WEAK) && (sym.st_type == STT_FUNC || sym.st_type == STT_GNU_IFUNC)){ 
                           elf->exportedSymbols.insert(sym.st_value);
                    }
                    elf->symbols.insert(sym.st_value);              
                } 
                /*else{ //undefined section for the symbol. imported symbols, value (address where it is define) of imported symbols is zero as address is not known
                      //weak binding for symbols in .plt.got that may have type NOTYPE such as __gmon_start
                      if ((sym.st_type == STT_FUNC || sym.st_type == STT_GNU_IFUNC || sym.st_type == STT_NOTYPE) && (binding == STB_GLOBAL || binding  == STB_WEAK)){
                           cout<<"dyn - imported symbol"<<endl;
                           //importedSymbols.insert(symbolName);
                      }
                }*/
            }
        }
        elf->hasDynamicSymbolTable = true;
        int index = -1;
#ifdef PLT_RELOCATIONS
        if(pltAvailable){
            //relocation symbols
            //since there is no plt symbols, we need to create it by ourselfs
            //borrows ideas from _bfd_get_synthetic_symbol
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
                        if(rel.r_type != R_386_JMP_SLOT) continue;
                        
                        PCAddress relocatedAddress = rel.r_offset;
                        //Symbol symbol(symbolName,relocatedAddress, &plt, SYM_REL); 
                        symbols.insert(relocatedAddress);   
                    }
                }
            }
        }
#endif
    }
    else elf->hasDynamicSymbolTable = false;
     
}

#endif


