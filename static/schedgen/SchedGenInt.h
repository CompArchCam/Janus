#ifndef _HINT_GEN_INTERNAL_
#define _HINT_GEN_INTERNAL_

#include "SchedGen.h"

#include <map>
#include <set>
#include <vector>

namespace janus {
/* Data structures for storing rules */
class RuleCluster {
public:
    uint32_t                              channel;    //cluster channel
    std::map<PCAddress, std::set<RewriteRule>>      ruleMap;    //map for easy lookup
    RuleCluster(uint32_t channel):channel(channel){};
    void                    insert(RewriteRule &rule);
    void                    replace(RewriteRule &rule);
    void                    remove(RewriteRule &rule);
    void                    print(void *outputStream);
};


/* overloading operations for static rules */
bool
operator<(const RewriteRule &lhs, const RewriteRule &rhs);
bool
operator==(const RewriteRule &lhs, const RewriteRule &rhs);
}

#define PRE_INSERT true
#define POST_INSERT false
#define MAIN_CHANNEL (uint32_t)0
#define FIRST_INSTRUCTION (uint32_t)1

/* Main buffer to store all static rules */
extern std::vector<janus::RuleCluster> rewriteRules;
extern janus::BasicBlock *reshapeBlock;
#endif
