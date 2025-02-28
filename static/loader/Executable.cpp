#include "Executable.h"
#include "Function.h"
#include "JanusContext.h"
#include "ParseELF.h"
#include "Disassemble.h"
#include "IO.h"
#include "capstone/capstone.h"
#include <cstring>
#include <algorithm>
#include <set>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;
using namespace janus;
#define FILE_TYPE_OFFSET 16
Executable::~Executable()
{
    delete[] buffer;
}

void Executable::open(JanusContext *jc, const char *filename)
{
    ifstream binFile(filename, ios::in|ios::ate|ios::binary);

    GASSERT(binFile.is_open(), "could not find file "<<filename);

    fileSize = binFile.tellg();

    GSTEP("Reading file \""<<filename<<"\" size: "<<fileSize<<" bytes."<<endl);

    buffer = new uint8_t[fileSize + 2048];

    binFile.seekg (0, ios::beg);
    binFile.read((char *)buffer,fileSize);
    binFile.close();

    //plt section 
    pltSectionIndex = -1;
    pltsecSectionIndex = -1;
    pltGOTSectionIndex = -1;

    //recognise executable headers
    parseHeader();
    GASSERT(type == BINARY_ELF, "Executable header not supported");

    if (!hasStaticSymbolTable) {
        parseFlat();
    }
}

void Executable::disassemble(JanusContext *jc)
{
    //lift all the recognised symbols to functions
    liftSymbolToFunction(jc);
    //disassemble each identified functions
    disassembleAll(jc);
}

void Executable::parseHeader()
{
    if(strncmp((char *)buffer,ELFMAG,4)==0) {
        type = BINARY_ELF;
        switch(buffer[EI_CLASS]) {
            case ELFCLASS32:                         
                wordSize = 32; 
                //GASSERT_NOT_IMPLEMENTED(false,"ELF 32-bit header");
                parseELF32();
            break;
            case ELFCLASS64:                                                                                  
                wordSize = 64; 
                parseELF64();
            break;
        }
    }
    //will add more executable format later
    else { 
        type = UNRECOGNISED;
    }
    switch(buffer[FILE_TYPE_OFFSET]){
         case ET_DYN:
             binaryType = BINARY_PIC;
         break;
         case ET_EXEC:
             binaryType = BINARY_NONPIC;
         break;
         default:
              binaryType = BINARY_UNKNOWN;
         break;
    }
}

void Executable::parseFlat()
{
    GSTEP("This is a flat "<<(isExecutable?"executable":"library")<<\
          " , recovering hidden symbols"<<endl);
    /* We only parse .text and .plt section */
    for(auto &section: sections) {
        if(section.name == string(".text")){
            retrieveHiddenSymbol(section);
            codeStartAddr = section.startAddr;
            codeEndAddr = section.endAddr;
        }
    }
    GSTEP("Found "<<symbols.size()<<" hidden symbols"<<endl);
}

void Executable::liftSymbolToFunction(JanusContext *jc)
{
    uint32_t      fid = 0;
    //construct a vector of functions from the symbol table
    //Infer the symbol boundaries by looking at the next entry
    for (auto sit=symbols.begin(); sit != symbols.end(); sit++) {
        //cout <<(*sit).name<<" "<<hex<<(*sit).startAddr<<" type "<<(*sit).type<<" section start "<<(*sit).section->startAddr << " end " << (*sit).section->endAddr <<endl;
        int rel_type;
        rel_type = (wordSize == 64) ? SYM_RELA : SYM_REL;
        if ((*sit).type == SYM_FUNC || (*sit).type == SYM_RELA || (*sit).type == SYM_REL) {
            //since the symbols is already sorted, just look for the next different symbol
            auto sit_next = sit;
            sit_next++;
            uint32_t size;
            if ((sit_next != symbols.end())) {
                /* Skip different labels with the same address */
                while((*sit).startAddr == (*sit_next).startAddr) {
                    sit_next++;
                }
                size = (*sit_next).startAddr - (*sit).startAddr;
            }
            else
                size = (*sit).section->endAddr - (*sit).startAddr;
            
            //create new function and put it into the global vector
            jc->functions.emplace_back(jc,fid,(*sit),size);
            fid++;
        }
    }
}

void Executable::retrieveHiddenSymbol(Section &section)
{
    /* Setup disassembly */
    csh cs_handle;
    uint8_t *flatBuffer = section.contents;
    size_t bufferSize = section.size;
    set<PCAddress> callTargets;
    set<PCAddress> endBranchTargets;
    PCAddress pc = section.startAddr;
    /* Insert the start of the section */
    callTargets.insert(pc);
    /* Insert the end of the section */
    callTargets.insert(section.endAddr);

    cs_err err;
    err = cs_open(CS_ARCH_X86, CS_MODE_64, (csh *)(&cs_handle));

    // we need the details for each instruction
    cs_option(cs_handle, CS_OPT_DETAIL, CS_OPT_ON);
    //skip the padding between the instructions
    cs_option(cs_handle, CS_OPT_SKIPDATA, CS_OPT_ON);

    cs_insn *instr = cs_malloc(cs_handle);

    if (err) {
        printf("Failed on cs_open() in capstone with error returned: %u\n", err);
        exit(-1);
    }

    /* We perform a quick disassemble on control flow instructions */
    while(cs_disasm_iter(cs_handle, (const uint8_t **)(&flatBuffer), &bufferSize, (uint64_t *)&pc, instr)) {
        /* Call targets are very likely to be a start of function */
        if (instr->id == X86_INS_CALL) {
            cs_detail *detail = instr->detail;
            if (detail->x86.op_count == 1 &&
                detail->x86.operands[0].type == X86_OP_IMM)
            callTargets.insert(detail->x86.operands[0].imm);
        }
        //ADDED for those instructions that are not call targets but have endbranch. TODO: do detailed prologue analysis
        if(instr->id == X86_INS_ENDBR32 || instr->id == X86_INS_ENDBR64){
            cs_detail *detail = instr->detail;
            endBranchTargets.insert((uintptr_t)instr->address);
        }
        /* JUMP targets could also be starts of function,
         * we will check this in building basic blocks */
    }
    //TODO: also check for exported symbols. and retain the names of those addresses. in fact do this before anything else. also start of the .text section is also a function.

    cs_free(instr,1);

    stringstream ss;

    /* Process all call targets */
    int i=0;
    for(auto target: callTargets) {
        /* Check if this target is within range */
        if (target < section.startAddr || target >= section.endAddr) {
            crossSectionRef.insert(target);
            continue;
        }
        ss << "Function_"<<i;
        if (target == section.endAddr) {
            Symbol s(ss.str(), target, &section, SYM_NONE);
            symbols.insert(s);
        }
        else {
            Symbol s(ss.str(), target, &section, SYM_FUNC);
            symbols.insert(s);
        }
        ss.str(string());
        i++;
    }
    //TODO: for endbranch targets, check if they are already part of some function, if not add them to symbols.

    cs_close(&cs_handle);
}

bool compareProc(Symbol &a, Symbol &b)
{
    return (a.startAddr) < (b.startAddr);
}

void Executable::printSection()
{
    for(auto s:sections) {
        cout <<s.name<<endl;
        for(auto sym: s.symbols)
            cout <<"    :"<<sym->name<<endl;
    }
}
