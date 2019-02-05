#ifndef _Janus_CONTROLFLOW_
#define _Janus_CONTROLFLOW_

//Control flow related analysis

#include "janus.h"
#include "Function.h"
#include "JanusContext.h"

/** \brief build the control flow graph*/
void buildCFG(janus::Function &function);

/** \brief build the control dependence graph (Better to be called after SSA).*/
void buildCDG(janus::Function &function);

void traverseCFG(janus::Function &function);

void buildCallGraphs(JanusContext *gc);

/** \brief builds the dominance frontier of each block the given function.

 * It can be used for control flow analysis and SSA construction
 */
void buildDominanceFrontiers(janus::Function &function);

/** \brief builds the post dominance frontier of each block the given function.

 * It can be used for building control dependence graph
 */
void buildPostDominanceFrontiers(janus::Function &function);
#endif
