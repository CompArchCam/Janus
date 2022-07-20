#include "dllRule.h"
#include "JanusContext.h"
using namespace std;
using namespace janus;

void generateDLLRules(JanusContext *jc)
{
    for (auto &f : jc->functions) {
        for (auto &bb : f.getCFG().blocks) {
            int local_inst_count = 0;
            for (auto &I : bb.minstrs) {
                if (I.type == MemoryInstruction::Read) {
                    local_inst_count++;
                }
            }
            if (local_inst_count > 0) {
                RewriteRule rule;
                rule = RewriteRule((RuleOp)1, &bb, PRE_INSERT);
                rule.reg0 = local_inst_count;
                insertRule(0, rule, &bb);
            }
        }
    }
}
