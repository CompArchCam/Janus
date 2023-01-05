#include "CoverageRule.h"
#include "JanusContext.h"
#include "IO.h"

using namespace janus;
using namespace std;
//void generateLoopCoverageProfilingRules(JanusContext *gc) {
void generateLoopCoverageProfilingRules(std::vector<janus::Loop>& loops, std::string name) {
    /* Get the array of CFG */
    BasicBlock *entry;
    /* Place holder for a static rule for copy */
    RewriteRule rule;

    /* File to map loopID to function name*/
    FILE *mapFile;
    //mapFile = fopen(string((gc->name)+"_map.out").c_str(), "w");
    mapFile = fopen(string((name)+"_map.out").c_str(), "w");
    
    /* Generate rules for each loops */
    //for(auto &loop: gc->loops)
    for(auto &loop: loops)
    {
        entry = loop.parent->entry;
	
	/* Get information of parent function name*/
	std::string parent_name= loop.parent->name;
        fprintf(mapFile, "%s\n", parent_name.c_str());
	
    /* Insert rules for manually split over-sized blocks */
    Function *parent = loop.parent;
    for (auto bset : parent->blockSplitInstrs) {
        BasicBlock *bb = entry + bset.first;
        for (auto sid : bset.second) {
            rule = RewriteRule(APP_SPLIT_BLOCK, bb->instrs->pc, parent->instrs[sid].pc, sid);
            insertRule(loop.id, rule, bb);
        }
    }
	
	/* PROF_LOOP_START is inserted at all init blocks */
        for(auto bb: loop.init)
        {
            BasicBlock *initBlock = entry+bb;
            rule = RewriteRule(PROF_LOOP_START, initBlock, POST_INSERT);
            rule.reg0 = loop.id;
            if (!initBlock->fake && initBlock->lastInstr()->isConditionalJump()){
                //Check whether the rule will be attached to a conditional jump instruction
                Instruction *trigger = initBlock->lastInstr();
                if (trigger->minstr->getTargetAddress() == loop.start->instrs->pc){
                    //Check whether jumping goes to the loop
                    rule.reg1 = 1;
                }
                else{
                    //or if not jumping goes to loop
                    rule.reg1 = 2;
                }

            }
            insertRule(loop.id, rule, entry + bb);
        }

        /* PROF_LOOP_ITER is inserted at loop start */
        rule = RewriteRule(PROF_LOOP_ITER, loop.start, PRE_INSERT);
        rule.reg0 = loop.id;
        insertRule(loop.id, rule, loop.start);

        /* PROF_LOOP_FINISH is inserted at all exit blocks */
        for(auto bb: loop.exit)
        {
            rule = RewriteRule(PROF_LOOP_FINISH, entry + bb, PRE_INSERT);
            rule.reg0 = loop.id;
            insertRule(loop.id, rule, entry + bb);
        }
    }
    fclose(mapFile); /* close the file*/
}

//void generateFunctionCoverageProfilingRules(JanusContext *gc) {
void generateFunctionCoverageProfilingRules(std::vector<janus::Function>& functions) {
    /* Get the array of CFG */
    BasicBlock *entry;
    /* Place holder for a static rule for copy */
    RewriteRule rule;
    /* Generate rules for each function */
    //for (auto &func: gc->functions) {
    for (auto &func: functions) {
        if (!func.entry || !func.minstrs.size()) continue;
        /* CTIMER_FUNC_START is inserted at all entry blocks */
        rule = RewriteRule(CTIMER_FUNC_START, func.entry, PRE_INSERT);
        rule.reg0 = func.fid;
        insertRule(0, rule, func.entry);
        /* CTIMER_LOOP_END is inserted at all exit blocks */
        for (auto retID:func.terminations) {
            BasicBlock &bb = func.blocks[retID];
            if (bb.lastInstr()->opcode == Instruction::Return) {
                rule = RewriteRule(CTIMER_FUNC_END, &bb, PRE_INSERT);
                rule.reg0 = func.fid;
                insertRule(0, rule, &bb);
            }
        }

        /* CTRACE_CALL_{START,END} is inserted {before,after} each call */
        for(auto call: func.calls) {
            Instruction &instr = func.instrs[call.first];
            BasicBlock *block = instr.block;
            if (!block) continue;
            /* CTRACE_CALL_START */
            rule = RewriteRule(CTRACE_CALL_START, block, POST_INSERT);
            rule.reg0 = call.second->fid;
            insertRule(0, rule, block);

            if(block && block->succ1)
                block = block->succ1;
            /* CTRACE_CALL_END */
            if(block) {
                rule = RewriteRule(CTRACE_CALL_END, block, PRE_INSERT);
                rule.reg0 = call.second->fid;
                insertRule(0, rule, block);
            }
        }
    }
}
