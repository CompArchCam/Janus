/*! \file ParaRule.h
 *  \brief Rule generation for automatic parallelisation
 *  
 *  This header defines the API to generate rules for parallelisation
 */
#ifndef _PARALLEL_HINT_GEN_
#define _PARALLEL_HINT_GEN_

#include "SchedGenInt.h"

/** \brief Generate parallel related rules */
//void generateParallelRules(JanusContext *gc);
void generateParallelRules(JanusContext *gc, std::map<PCAddress, janus::Function *> functionMap);

/** \brief Compile all generated rules to a rule file */
uint32_t
compileParallelRulesToFile(JanusContext *gc);
#endif
