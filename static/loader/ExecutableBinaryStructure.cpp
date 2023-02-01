//#include "Executable.h"
#include "ExecutableBinaryStructure.h"

//#include "JanusContext.h"
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

janus::ExecutableBinaryStructure::ExecutableBinaryStructure(std::string executableName)
{
	this->executableName = executableName;
    open(this->executableName.c_str());
}

janus::ExecutableBinaryStructure::~ExecutableBinaryStructure()
{
    delete[] buffer;
}

//void Executable::open(JanusContext *jc, const char *filename)
void janus::ExecutableBinaryStructure::open(const char *filename)
{
    ifstream binFile(filename, ios::in|ios::ate|ios::binary);

    GASSERT(binFile.is_open(), "could not find file "<<filename);

    fileSize = binFile.tellg();

    GSTEP("Reading file \""<<filename<<"\" size: "<<fileSize<<" bytes."<<endl);

    buffer = new uint8_t[fileSize + 2048];

    binFile.seekg (0, ios::beg);
    binFile.read((char *)buffer,fileSize);
    binFile.close();

    //recognise executable headers
    parseHeader();
    GASSERT(type == BINARY_ELF, "Executable header not supported");

    if (!hasStaticSymbolTable) {
        parseFlat();
    }
}

//void Executable::disassemble(JanusContext *jc)
void janus::ExecutableBinaryStructure::disassemble(std::vector<janus::Function>& functions, Function* fmain, std::map<PCAddress, janus::Function *>&  functionMap, std::map<PCAddress, janus::Function *>& externalFunctions)
{
    //lift all the recognised symbols to function
    //liftSymbolToFunction(jc);

	liftSymbolToFunction(functions);
    //disassemble each identified functions
    //disassembleAll(jc);

	printf("		disassembleAll --- START --- \n");
	disassembleAll(capstoneHandle, fmain, functions, functionMap, externalFunctions);
	printf("		disassembleAll --- DONE --- \n");
	// So, here we could return every function
	//return functions;
}

void janus::ExecutableBinaryStructure::parseHeader()
{
    if(strncmp((char *)buffer,ELFMAG,4)==0) {
        type = BINARY_ELF;
        switch(buffer[EI_CLASS]) {
            case ELFCLASS32:                         
                wordSize = 32; 
                GASSERT_NOT_IMPLEMENTED(false,"ELF 32-bit header");
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
}

void janus::ExecutableBinaryStructure::parseFlat()
{
    GSTEP("This is a flat "<<(isExecutable?"executable":"library")<<\
          " , recovering hidden symbols"<<endl);
    /* We only parse .text and .plt section */
    for(auto &section: sections) {
        if(section.name == string(".text"))
            retrieveHiddenSymbol(section);
    }
    GSTEP("Found "<<symbols.size()<<" hidden symbols"<<endl);
}

//void Executable::liftSymbolToFunction(JanusContext *jc)
void janus::ExecutableBinaryStructure::liftSymbolToFunction(std::vector<janus::Function>& functions)
{
    uint32_t      fid = 0;
    //construct a vector of functions from the symbol table
    //Infer the symbol boundaries by looking at the next entry
    for (auto sit=symbols.begin(); sit != symbols.end(); sit++) {
        //cout <<(*sit).name<<" "<<hex<<(*sit).startAddr<<" type "<<(*sit).type<<" section start "<<(*sit).section->startAddr << " end " << (*sit).section->endAddr <<endl;
        if ((*sit).type == SYM_FUNC || (*sit).type == SYM_RELA) {
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
            //jc->functions.emplace_back(jc,fid,(*sit),size);
            functions.emplace_back(fid,(*sit),size);
            fid++;
        }
    }
    //return functions;
}

void janus::ExecutableBinaryStructure::retrieveHiddenSymbol(Section &section)
{
    /* Setup disassembly */
    csh cs_handle;
    uint8_t *flatBuffer = section.contents;
    size_t bufferSize = section.size;
    set<PCAddress> callTargets;
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
        /* JUMP targets could also be starts of function,
         * we will check this in building basic blocks */
    }

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

    cs_close(&cs_handle);
}

bool compareProc(Symbol &a, Symbol &b)
{
    return (a.startAddr) < (b.startAddr);
}

void janus::ExecutableBinaryStructure::printSection()
{
    for(auto s:sections) {
        cout <<s.name<<endl;
        for(auto sym: s.symbols)
            cout <<"    :"<<sym->name<<endl;
    }
}
