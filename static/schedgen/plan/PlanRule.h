#ifndef _LOOP_PLAN_RULE_GEN_
#define _LOOP_PLAN_RULE_GEN_

#include "SchedGenInt.h"

//void generateLoopPlannerRules(JanusContext *gc);
void generateLoopPlannerRules(JanusContext *gc, std::vector<janus::Function>& functions, std::vector<janus::Loop>& loops, std::string name);

///Generate a full report of all loops in the binary
void
generateLoopReport(JanusContext *gc);
#endif
