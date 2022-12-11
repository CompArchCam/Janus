#ifndef _OFF_ANALYSIS_
#define _OFF_ANALYSIS_
#include "JanusContext.h"
#include "IO.h"
#include "Arch.h"
using namespace janus;
bool traceToAlloc(VarState  *vs);
PCAddress traceToAllocPC(VarState  *vs);
bool traceToInstr(VarState  *vs, Instruction *instr);
int isAllocCall(Instruction *instr);
void get_range_expr(MemoryLocation *mloc, ExpandedExpr* range_expr, Iterator* loopIter);
#endif
