#ifndef _JANUS_X86_DISASSEMBLE_
#define _JANUS_X86_DISASSEMBLE_

#include "Instruction.h"
#include "Function.h"
#include "Variable.h"
#include <vector>

///Disassemble all functions contained in jc
//void disassembleAll(JanusContext *jc);
void disassembleAll(uint64_t& capstoneHandle, janus::Function** fmainRet, std::vector<janus::Function>& functions, std::map<PCAddress, janus::Function *>&  functionMap,
		std::map<PCAddress, janus::Function *>& externalFunctions);
///Disassemble for the given function
//void disassemble(janus::Function *function);
void disassemble(janus::Function *function, uint64_t& handle);
///Given the raw machine instruction, return the opcode of the architecture independent Instruction
janus::Instruction::Opcode
liftOpcode(janus::MachineInstruction *minstr);
///Lift the machine instruction to abstract instructions
int liftInstructions(janus::Function *function);
///Fill the variable input of the instruction, include implicit variables
void getInstructionInputs(janus::MachineInstruction *minstr, std::vector<janus::Variable> &v);
#endif
