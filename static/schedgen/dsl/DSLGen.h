#ifndef _DSL_GEN_
#define _DSL_GEN_

#include "SchedGenInt.h"
#include "JanusContext.h"
#include "Instruction.h"
#include "BasicBlock.h"
#include "Function.h"
#include "Loop.h"
#include "IO.h"

//void generateCustomRules(JanusContext *gc);
void generateCustomRules(std::vector<janus::Function>& functions);

//void ruleGenerationTemplate(JanusContext &gc);
void ruleGenerationTemplate(std::vector<janus::Function>& functions);

template<typename T> void
insertCustomRule(int ruleID, T &comp, int trigger, bool attach_data, int data);

#endif
