#ifndef _COV_RULE_GEN_
#define _COV_RULE_GEN_

#include "SchedGenInt.h"

//void generateLoopCoverageProfilingRules(JanusContext *gc);
void generateLoopCoverageProfilingRules(std::vector<janus::Loop>& loops, std::string name);
//void generateFunctionCoverageProfilingRules(JanusContext *gc);
void generateFunctionCoverageProfilingRules(std::vector<janus::Function>& functions);
#endif
