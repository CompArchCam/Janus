#include "JanusContext.h"
#include <set>

using namespace std;
using namespace janus;

void
setSupportedVectorOpcodes(set<InstOp> &supported_opcodes, set<InstOp> &singles, set<InstOp> &doubles);

bool
isVectorRuntimeCompatible(Loop &loop, set<InstOp> &supported_opcode, set<InstOp> &singles, set<InstOp> &doubles);

void
selectVectorisableLoop(JanusContext *gc, set<Loop *> &selected_loops, set<InstOp> &supported_opcode, set<InstOp> &singles, set<InstOp> &doubles);

void
printVectorisableLoops(set<Loop *> &selected_loops);