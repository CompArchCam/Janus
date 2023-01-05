#ifndef _DSL_GEN_UTIL_
#define _DSL_GEN_UTIL_

#include <iostream>
#include <fstream>
#include <string>
using namespace janus;
using namespace std;

#define get_fstartAddr(F)       F.startAddress
#define f.open(var)             f.open(var, ios::in | ios::out)
#define get_id(A) A.id

bool is_type(VarState* elem, _jvar_type type){
    if(elem->type == type)
        return true;
    else
        return false;
}

void writeToFile(fstream &file, uintptr_t val ){
    file<<val<<endl;
}
void writeToFile(fstream &file, std::string val ){
    file<<val<<endl;
}
void writeToFile(fstream &file, char* val ){
    file<<val<<endl;
}

std::string 
get_target_name(Instruction &instr){
    std::string trgname;
    if(instr.opcode == Instruction::Call){
        Function *targetFunc = instr.block->parentFunction->calls[instr.id];
        if(targetFunc != NULL){
            trgname = targetFunc->name;
        }
    }
    return trgname;
}

// TODO: seems to be dead code.
/*int get_num_loops(JanusContext &jc){
   return jc.loops.size();
}*/

int 
get_opcode(Instruction &instr){

    if(instr.isMemoryAccess()){
        MemoryInstruction mm(&instr);
        if(mm.type = MemoryInstruction::Read){
            return Instruction::Load;
        }
        else if(mm.type = MemoryInstruction::Write){
            return Instruction::Store;
        }
    }
    return instr.opcode;
}
int get_opcode(Instruction *instr){
    if(instr->isMemoryAccess()){
        MemoryInstruction mm(instr);
        if(mm.type = MemoryInstruction::Read){
            return Instruction::Load;
        }
        else if(mm.type = MemoryInstruction::Write){
            return Instruction::Store;
        }

    }
    return instr->opcode;
}

VarState*
get_input(Instruction &instr, int index){
    return instr.inputs[index];
}
VarState* 
get_output(Instruction &instr, int index){
    return instr.outputs[index];
}
VarState*
get_src1(Instruction &instr){
    return instr.inputs[0];
}
VarState*
get_src2(Instruction &instr){
    return instr.inputs[1];
}
VarState*
get_src3(Instruction &instr){
    return instr.inputs[2];
}
VarState*
get_src4(Instruction &instr){
    return instr.inputs[3];
}
VarState*
get_dest(Instruction &instr){
    return instr.outputs[0];
}
#endif
