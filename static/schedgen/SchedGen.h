#ifndef _Janus_HINTGEN_
#define _Janus_HINTGEN_

#include "janus.h"

class JanusContext;

namespace janus {
class BasicBlock;

/* Basic elements for a static rule */
class RewriteRule {
public:
    PCAddress                blockAddr;
    PCAddress                ruleAddr;
    
    union {
        uint64_t             reg0;
        RuleReg              ureg0;
    };

    union {
        uint64_t             reg1;
        RuleReg              ureg1;
    };

    uint32_t                 id;
    RuleOp                   opcode;

    RewriteRule(){};
    RewriteRule(RuleOp op, PCAddress blockAddr, PCAddress ruleAddr, uint32_t instrId);
    RewriteRule(RuleOp op, BasicBlock *block, bool pre_insert);
    RewriteRule(RuleOp op, BasicBlock *block, uint32_t position);

    void                    print(void *outputStream);

    //convert to dynamic compatible rule binary code
    RRule                   toRRule(uint32_t channel);
};
}

void
generateRules(JanusContext *gc);

/* Generic rule insertion API from outside rule generation module */
void
insertRule(uint32_t channel, janus::RewriteRule rule, janus::BasicBlock *block);
void
replaceRule(uint32_t channel, janus::RewriteRule rule, janus::BasicBlock *block);

void
removeRule(uint32_t channel, janus::RewriteRule rule, janus::BasicBlock *block);
/** \brief Encode the current JVar to rewrite rule (wrapper) */
void encodeJVar(JVar var, janus::RewriteRule &rule);
/** \brief Decode the the rewrite rule and get JVar */
JVar decodeJVar(janus::RewriteRule &rule);
#endif
