#include "JanusContext.h"
#include "VECT_rule_structs.h"
#include "VectUtils.h"
#include <map>
#include <set>

using namespace std;
using namespace janus;

bool insertExtendRules(Loop *loop, set<InstOp> &singles, set<InstOp> &doubles,
                       BasicBlock *bb, set<MachineInstruction *> &inputs,
                       uint64_t &stride, uint32_t &vectorWordSize,
                       map<uint32_t, inductRegInfo *> &inductRegs,
                       set<uint32_t> &reductRegs, RegSet &freeVectRegs,
                       set<VECT_RULE *> &rules);