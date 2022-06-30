#ifndef _OPT_HINT_GEN_
#define _OPT_HINT_GEN_

#include "Loop.h"
#include "MachineInstruction.h"
#include "SchedGenInt.h"
#include "Utils.h"
#include "VariableState.h"
#include "capstone/x86.h"
#include "new_instr.h"
#include <map>
#include <vector>

/** \brief Finds the innermost loop in which vs depends on an induction variable
 * (possibly via load instructions).
 *
 * Uses DFS with memoization to find the innermost loop, such that we can
 * compute vs via instructions, which only have input variables that are either
 * constant or induction variables in that loop. This includes memory operands
 * such that the address satisfies these criteria.
 */
janus::Loop *
findPrefetch(janus::VariableState *vs,
             std::map<janus::VariableState *, janus::Loop *> &store);

int evaluate(janus::BasicBlock *bb, int index, janus::VariableState *vs,
             janus::Loop *loop, int offset, std::vector<new_instr *> &gen,
             int &varid);

/** \brief Generate prefetching rules to the dynamic part. */
void generateOptRules(JanusContext *gc);

#endif
