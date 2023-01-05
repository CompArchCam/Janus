#include "dllRule.h"
#include "JanusContext.h"
using namespace std;
using namespace janus;

//void generateDLLRules(JanusContext *jc) {
void generateDLLRules(std::vector<janus::Function>& functions) {
//    for(auto &f: jc->functions){
    for(auto &f: functions){
        for(auto &bb: f.blocks){
            int local_inst_count =0;
            for(auto &I: bb.minstrs){
                if(I.type == MemoryInstruction::Read  ){
                    local_inst_count++;
                }
            }
            if(local_inst_count>0){
                RewriteRule rule;
                rule = RewriteRule((RuleOp)1, &bb, PRE_INSERT);
                rule.reg0 = local_inst_count;
                insertRule(0, rule, &bb);
            }
        }
    }
}

