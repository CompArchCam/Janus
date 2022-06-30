#include "JanusContext.h"
#include "VECT_rule_structs.h"
#include "VectUtils.h"
#include <map>
#include <set>

using namespace std;
using namespace janus;

bool findInit(MachineInstruction *instr, int64_t &init);

bool findInductionVariables(Loop *loop, uint64_t &stride,
                            map<uint32_t, inductRegInfo *> &inductRegs,
                            set<VECT_RULE *> &rules);