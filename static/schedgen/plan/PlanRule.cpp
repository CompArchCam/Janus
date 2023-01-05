#include "PlanRule.h"
#include "JanusContext.h"
#include "IO.h"
#include <iostream>
#include <sstream>
#include <inttypes.h>

using namespace janus;
using namespace std;

//static void generateRulesForEachLoop(JanusContext *gc, Loop &loop)
static void generateRulesForEachLoop(JanusContext *gc, Loop &loop, std::vector<janus::Function>& functions)
{
    /* Get the array of CFG */
    BasicBlock *entry = loop.parent->entry;
    /* Place holder for a static rule for copy */
    RewriteRule rule;

    /* Insert rules for manually split over-sized blocks */
    for (auto bset : loop.parent->blockSplitInstrs) {
        BasicBlock *bb = entry + bset.first;
        for (auto sid : bset.second) {
            rule = RewriteRule(APP_SPLIT_BLOCK, bb->instrs->pc, loop.parent->minstrs[sid].pc, sid);
            insertRule(loop.id, rule, bb);
        }
    }

    /* PROF_LOOP_START is inserted at all init blocks */
    for(auto bb: loop.init)
    {
        rule = RewriteRule(PROF_LOOP_START, entry + bb, POST_INSERT);
        rule.reg0 = loop.id;
        rule.reg1 = loop.level;		//added by MA
        insertRule(loop.id, rule, entry + bb);
    }

    /* PROF_LOOP_ITER is inserted at the start block of the loop */
    rule = RewriteRule(PROF_LOOP_ITER, loop.start, PRE_INSERT);
    rule.reg0 = loop.id;
    rule.reg1 = loop.level;		//added by MA
    insertRule(loop.id, rule, loop.start);

    /* PROF_LOOP_FINISH is inserted at all exit blocks */
    for(auto bb: loop.exit)
    {
        rule = RewriteRule(PROF_LOOP_FINISH, entry + bb, POST_INSERT);
        rule.reg0 = loop.id;
        rule.reg1 = loop.level;		//added by MA
        insertRule(loop.id, rule, entry + bb);
    }

    for (auto bid: loop.body) {
        BasicBlock *bb = entry + bid;
        /* PROF_MEM_ACCESS is inserted before every memory access
         * TODO: reduce the number of checks using alias analysis */
        for (auto &memInstr: bb->minstrs) {
            bool instrUsesPrivateVar = false;
            for (auto &opnd: memInstr.instr->inputs){
                Variable stackVar = *(Variable *)memInstr.mem;
                if (loop.privateVars.find(stackVar) != loop.privateVars.end() ||
                    loop.phiVars.find(stackVar) != loop.phiVars.end() ||
                    loop.checkVars.find(stackVar) != loop.checkVars.end()) {
                    //If the instruction uses a private stack variable, we will use privatization to avoid the dependency
                    instrUsesPrivateVar = true;
                }
            }
            if (!instrUsesPrivateVar){
                rule = RewriteRule(PROF_MEM_ACCESS, bb->instrs->pc, memInstr.instr->pc, memInstr.instr->id);
                rule.reg0 = memInstr.instr->id;
                //rule.reg1 = loop.level;		//added by MA
                rule.reg1 = loop.id;		    //added by MA
                insertRule(loop.id, rule, bb);
            }
        }
    }

    /* Add PROF_MEM_ACCESS in sub calls */
    for (auto fid: loop.subCalls) {
        //Function &func = gc->functions[fid];
    	Function &func = functions[fid];
        for (auto &bb: func.blocks) {
            /* PROF_MEM_ACCESS is inserted before every memory access
            * TODO: reduce the number of checks using alias analysis */
            for (auto &mem: bb.minstrs) {
                rule = RewriteRule(PROF_MEM_ACCESS, bb.instrs->pc, mem.instr->pc, mem.instr->id); 
                rule.reg0 = mem.instr->id;
                rule.reg1 = loop.id;		//added by MA
                insertRule(loop.id, rule, &bb);
            }
        }
    }
    if(gc->sharedOn){
	/* PROF_CALL_{START,END} is inserted {before,after} each shared library call */
	for (auto it : loop.calls) {
	    BasicBlock *block =  entry + it.first;
	    //Function &func = gc->functions[it.second];
	    Function &func = functions[it.second];
	    if (func.isExternal){
		if (!block) continue;
		/* PROF_CALL_START */
		rule = RewriteRule(PROF_CALL_START, block, POST_INSERT);
		rule.reg0 = func.fid;
		insertRule(loop.id, rule, block);

		if(block && block->succ1)
		    block = block->succ1;
		/* PROF_CALL_FINISH */
		if(block) {
		    rule = RewriteRule(PROF_CALL_FINISH, block, PRE_INSERT);
		    rule.reg0 = func.fid;
		    insertRule(loop.id, rule, block);
		}
	    }
	}
    }
}
static bool
loopNeedProfiling(Loop &loop)
{
//loop has low coverage or low iteration count
if (loop.removed) return false;
//loop has unsafe features that Janus paralleliser couldn't handle
//Currently we profile more to understand this loop better
//if (loop.unsafe) return false;
//loop is fully decided by static alias analysis with runtime check
//if (loop.undecidedMemAccesses.size() == 0) return false;
return true;
}

//void generateLoopPlannerRules(JanusContext *gc) {
void generateLoopPlannerRules(JanusContext *gc, std::vector<janus::Function>& functions, std::vector<janus::Loop>& loops, std::string name)
{
/* Create a plan file */
ofstream planFile;
//planFile.open(gc->name + ".plan", ios::out);
planFile.open(name + ".plan", ios::out);

    /* Generate rules for each loops */
    //for(auto &loop: gc->loops)
	for(auto &loop: loops)
    {
        if (loopNeedProfiling(loop)) {
            planFile << loop.id<<" "<<loop.coverage<<" "<<loop.total_iteration_count/loop.invocation_count<<" "<<loop.parent->name<<endl;
            //generateRulesForEachLoop(gc, loop);
            generateRulesForEachLoop(gc, loop, functions);
        }
    }

    planFile.close();
}
