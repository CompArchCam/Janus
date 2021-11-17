/*! \file SSA.h
 *  \brief Single Static Assignment Related Analysis
 *  
 *  This header contains the Single Static Assignment related construction and analysis.
 */

#ifndef _Janus_SSA_
#define _Janus_SSA_

#include "janus.h"
#include "Function.h"
#include "Loop.h"

/** \brief After analysis is done, this function fills the functions
 *         [equalgroup](@ref janus::Function::equalgroup) field.
 */
void saveEqualGroups(janus::Function *function);

/** \brief builds the SSA graph of the given function.
 * 
 * A call to this function is required in order that the SSA graph consisting of   
 * *janus::MachineInstruction* and *janus::VariableState* objects is linked. It is automatically 
 * called from *janus::Function::translate*, which also makes sure prerequisites are satisfied.
 */
void buildSSAGraph(janus::Function &function);


/** \brief builds the SSA graph of the given loop.
 * 
 * This function requires the loop's parent function's SSA to be built as prerequisites.
 * It adds additional phi notes to loop specific variables including memories.
 */
void buildSSAGraph(janus::Loop &loop);

/** \brief Returns the phi node position of a given janus::Variable var.
 *  \param function Function for analysis
 *  \param var A variable for analysis
 *  \param[out] bbs Set of basic blocks that generates defination of this variable
 *  \param[out] phiblocks Set of dominance frontier (blocks) of the bbs.
 *
 *  It requires the inputs and outputs of each basic block is collected as prerequisites.
 */
void
buildDominanceFrontierClosure(janus::Function &function,
                              janus::Variable var,
                              std::set<janus::BasicBlock*> &bbs,
                              std::set<janus::BasicBlock*> &phiblocks);
#endif
