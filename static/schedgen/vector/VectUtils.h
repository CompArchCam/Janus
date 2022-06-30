#ifndef _VECTOR_UTILS
#define _VECTOR_UTILS

#include "JanusContext.h"
#include "VECT_rule_structs.h"
#include <set>

using namespace std;
using namespace janus;

extern int currentLoopId;

class inductRegInfo
{
  public:
    int64_t init;
    uint64_t modifyingPC;

    inductRegInfo(int64_t _init, uint64_t _modifyingPC)
    {
        init = _init;
        modifyingPC = _modifyingPC;
    }
};

void printExpr(const Expr *expr);

void deleteMapValues(map<uint32_t, inductRegInfo *> &map);

void insertRegCheckRule(Loop *loop, uint8_t reg, uint32_t value,
                        set<VECT_RULE *> &rules);

void insertUnalignedRule(Loop *loop, RegSet &unusedRegs, RegSet &freeRegs,
                         set<VECT_RULE *> &rules);

void insertForceSSERule(Loop *loop, set<VECT_RULE *> &rules);

void removeLiveRegs(Loop *loop, RegSet &freeRegs);

bool checkDependingRegs(Loop *loop, map<uint32_t, inductRegInfo *> &inductRegs,
                        set<uint32_t> &reductRegs);

void updateRulesWithVectorWordSize(set<VECT_RULE *> &rules,
                                   uint32_t vectorWordSize);

#endif