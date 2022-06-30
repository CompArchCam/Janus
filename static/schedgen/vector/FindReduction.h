#include "JanusContext.h"
#include "VECT_rule_structs.h"
#include "VectUtils.h"
#include <map>
#include <set>

using namespace std;
using namespace janus;

bool findReduction(Loop *loop, map<uint32_t, inductRegInfo *> &inductRegs,
                   set<uint32_t> &reductRegs, RegSet &freeRegs,
                   RegSet &freeVectRegs, set<VECT_RULE *> &rules);