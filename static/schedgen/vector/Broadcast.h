#include "JanusContext.h"
#include "VECT_rule_structs.h"
#include <set>

using namespace std;
using namespace janus;

void insertBroadcastRules(Loop *loop, set<MachineInstruction *> &inputs,
                          set<uint32_t> &reductRegs, set<VECT_RULE *> &rules);