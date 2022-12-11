#ifndef _Janus_ANALYSIS_
#define _Janus_ANALYSIS_

#include "Function.h"
#include "Loop.h"
#include "Operand.h"
#include <set>

void
scanMemoryAccess(janus::Function *function);

void
scanDefUse(janus::Function *function);

void
livenessAnalysis(janus::Function *function);

void
flagsAnalysis(janus::Function *function);

void
reachingAnalysis(janus::Function *function);

void
analyseStack(janus::Function *function);

void
variableAnalysis(janus::Function *function);

/** \brief builds the dependence graph given function.
 *
 * Requires the SSA graph to be constructed. 
 */
void buildDDGGraph(janus::Function *function);

/** \brief Analyse all variables in the loop */
void
variableAnalysis(janus::Loop *loop);

/** \brief Retrieve scratch registers (at least four) */
void
scratchAnalysis(janus::Loop *loop);

/* Get all instructions which have reaching definations on the current instruction
 * the instruction must be in the basic block */
void
getReachingInstructionList(std::set<InstID> &reachingSet, janus::MachineInstruction &instr, janus::Variable var);

#endif
