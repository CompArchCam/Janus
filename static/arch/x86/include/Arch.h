/* x86 specific analysis
 * please implement each API if porting on other systems */
#ifndef _JANUS_ARCH_STATIC_
#define _JANUS_ARCH_STATIC_

#include "Function.h"
#include "janus_arch.h"
#include <map>
#include <set>

/// Direct translation matrix for capstone register sets
extern uint32_t csToJanus[];

/// Decode the register bit set into a set of registers
void decodeRegSet(uint64_t bits, std::set<uint32_t> &regSet);
/// Return the next register from the current register (for iterator of reg set)
int decodeNextReg(uint64_t bits, int reg);
/// Return the set of all general purpose registers (no SIMD)
void getAllGPRegisters(std::set<uint32_t> &regSet);
/// Return the set of all SIMD purpose registers
void getAllSIMDRegisters(std::set<uint32_t> &regSet);
/// get a vector of standard calling convention input arguments
void getInputCallConvention(std::vector<janus::Variable> &v);
/// get a vector of standard calling convention output arguments
void getOutputCallConvention(std::vector<janus::Variable> &v);
/// Analyse the stack frame size of the given function
void analyseStack(janus::Function *function);
/// Create additional architecture specific edges in the SSA graph
void linkArchSpecSSANodes(
    janus::Function &function,
    std::map<BlockID, std::map<janus::Variable, janus::VarState *> *>
        &globalDefs);
#endif