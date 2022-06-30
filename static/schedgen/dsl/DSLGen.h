#ifndef _DSL_GEN_
#define _DSL_GEN_

#include "BasicBlock.h"
#include "Function.h"
#include "IO.h"
#include "Instruction.h"
#include "JanusContext.h"
#include "Loop.h"
#include "SchedGenInt.h"

void generateCustomRules(JanusContext *gc);

void ruleGenerationTemplate(JanusContext &gc);

template <typename T>
void insertCustomRule(int ruleID, T &comp, int trigger, bool attach_data,
                      int data);

#endif
