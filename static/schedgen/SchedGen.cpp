#include "JanusContext.h"
#include "BasicBlock.h"
#include "IO.h"
#include "SchedGenInt.h"
//#include "OptRule.h"
#include "ParaRule.h"
#include "dllRule.h"
#include "CoverageRule.h"
#include "DSLGen.h"

#ifdef JANUS_X86
#include "VectRule.h"
#include "PrefetchRule.h"
#endif
#include "PlanRule.h"
#include <cstdlib>
#include <cstdio>

using namespace janus;
using namespace std;

/* Rules are firstly organised as channels */
vector<RuleCluster>                 rewriteRules;
/* If we wish to insert as application, the basic block
 * structure would be changed */
BasicBlock                          *reshapeBlock;

//static uint32_t compileRewriteRulesToFile(JanusContext *gc);
static uint32_t compileRewriteRulesToFile(JanusContext *gc, std::vector<janus::Function> functions, std::vector<janus::Loop> loops, std::string name);

static uint32_t
compileRewriteRuleDataToFile(JanusContext *gc);

//void generateRules(JanusContext *gc)
void generateRules(JanusContext *gc, std::map<PCAddress, janus::Function *> functionMap, std::vector<janus::Function>& functions, std::vector<janus::Loop> loops,
		LoopAnalysisReport& loopAnalysisReport, janus::Function *fmain, std::string name)
{
    uint32_t size;
    if (!gc) return;

    //uint32_t numLoops = gc->loops.size();
    uint32_t numLoops = loops.size();

    if (gc->mode != JFCOV && !numLoops) {
        GSTEP("No rules generated"<<endl);
        return;
    }

    /* Allocate a rule cluster for each loop */
    for(int i=0; i<=numLoops; i++) {
        rewriteRules.emplace_back(i);
    }

    GSTEP("Generating rewrite schedules: "<<endl);

    switch(gc->mode) {
    case JOPT:
        //generateOptRules(gc);
        break;
    case JPARALLEL:
    	//generateParallelRules(gc);
        generateParallelRules(gc, functionMap, functions, loopAnalysisReport, fmain, loops, name);
        break;
    case JVECTOR:
#ifdef JANUS_X86
        //generateVectorRules(gc);
    	generateVectorRules(gc, loops);
#endif
        break;
    case JLCOV:
        //generateLoopCoverageProfilingRules(gc);
        generateLoopCoverageProfilingRules(loops, name);
        break;
    case JFCOV:
        //generateFunctionCoverageProfilingRules(gc);
        generateFunctionCoverageProfilingRules(functions);
        break;
    case JPROF:
        //generateLoopPlannerRules(gc);
    	generateLoopPlannerRules(gc, functions, loops, name);
        break;
    //case JSECURE:
        //generateSecurityRule(gc);
        //break;
    case JFETCH:
        //generatePrefetchRules(gc);
    	generatePrefetchRules(gc, loops);
        break;
    case JCUSTOM:
        //generateCustomRules(gc);
    	generateCustomRules(functions);
        break;
    case JDLL:
        //generateDLLRules(gc);
        generateDLLRules(functions);
        break;
    default:
        break;
    }

    /* Now we generated all rules, compile the static rules
     * to a rule file */
    GSTEP("Writing rewrite schedules to file: "<<endl);
    if (gc->mode == JPARALLEL)
        //size = compileParallelRulesToFile(gc);
    	compileParallelRulesToFile(gc, name, loops, loopAnalysisReport, functions);
    else
        //size = compileRewriteRulesToFile(gc);
    	size = compileRewriteRulesToFile(gc, functions, loops, name);
    GSTEP("Rewrite schedule file: "<<gc->name<<".jrs generated, "<<size<<" bytes "<<endl);
}

/* We have *fake* basic blocks which
 * doesn't terminate with a branch instruction,
 * this is different to dynamoRIO's interpretation
 * In order to add rules to a place,
 * we need to recursively add rules for all fake blocks that contains it 
 * for example:
 * IN - IN - IN - OUT types 
 * separate rule needs to be generated for each IN entry */
void insertRule(uint32_t channel, RewriteRule rule, BasicBlock *block)
{
    RuleCluster &cluster = rewriteRules[channel];

    //And check if the block has been modified
    if(reshapeBlock && (block->instrs->pc == reshapeBlock->instrs->pc))
        //change the block address to the next instruction pc
        rule.blockAddr = block->instrs[1].pc;

    //insert the rule into the corresponding cluster
    cluster.insert(rule);

    for(auto pred:block->pred) {
        if(pred->fake) {
            rule.blockAddr = pred->instrs->pc;
            insertRule(channel,rule,pred);
        }
    }
}

void replaceRule(uint32_t channel, RewriteRule rule, BasicBlock *block)
{
    RuleCluster &cluster = rewriteRules[channel];

    //insert the rule into the corresponding cluster
    cluster.replace(rule);

    for(auto pred:block->pred) {
        if(pred->fake) {
            rule.blockAddr = pred->instrs->pc;
            replaceRule(channel,rule,pred);
        }
    }
}

void removeRule(uint32_t channel, RewriteRule rule, BasicBlock *block)
{
    RuleCluster &cluster = rewriteRules[channel];

    //insert the rule into the corresponding cluster
    cluster.remove(rule);

    for(auto pred:block->pred) {
        if(pred->fake) {
            rule.blockAddr = pred->instrs->pc;
            removeRule(channel,rule,pred);
        }
    }
}

/* Emit all relevant info in the rule file */
//static uint32_t compileRewriteRulesToFile(JanusContext *gc)
static uint32_t compileRewriteRulesToFile(JanusContext *gc, std::vector<janus::Function> functions, std::vector<janus::Loop> loops, std::string name)
{
    //FILE *op = fopen(string((gc->name)+".jrs").c_str(),"w");
	FILE *op = fopen(string((name)+".jrs").c_str(),"w");
    fpos_t pos;
    RSchedHeader header;
    uint32_t numRules = 0;
    int offset = 0;

    header.ruleFileType = (uint32_t)(gc->mode);
    //uint32_t numLoops = gc->loops.size();
    uint32_t numLoops = loops.size();

    /* For single loop mode, simply append rules */
    header.numLoops = numLoops;
    //header.numFuncs = gc->functions.size();
    header.numFuncs = functions.size();
    header.multiMode = 0;

    for(int channel=0; channel<=numLoops; channel++) {
        for(auto set:(rewriteRules[channel].ruleMap)) {
            numRules += set.second.size();
        }
    }

    header.numRules = numRules;
    header.ruleDataOffset = 0;
    /* Emit rule header */
    fwrite(&header,sizeof(RSchedHeader),1,op);
    offset += sizeof(RSchedHeader);

    RRule rule;
    for(int channel=0; channel<=numLoops; channel++) {
        for(auto set:(rewriteRules[channel].ruleMap)) {
            for(auto srule:set.second) {
                rule = srule.toRRule(channel);
                fwrite(&rule,sizeof(RRule),1,op);
                offset += sizeof(RRule);
            }
        }
    }

    fclose(op);

    return numRules;
}

void
RuleCluster::insert(RewriteRule &rule)
{
    PCAddress addr = rule.blockAddr;
    ruleMap[addr].insert(rule);
}

void
RuleCluster::replace(RewriteRule &rule)
{
    PCAddress addr = rule.blockAddr;
    auto &ruleSet = ruleMap[addr];

    for (auto oRule : ruleSet) {
        if (oRule.ruleAddr == rule.ruleAddr && oRule.opcode == rule.opcode) {
            ruleSet.erase(oRule);
            break;
        }
    }

    ruleMap[addr].insert(rule);
}

void
RuleCluster::remove(RewriteRule &rule)
{
    PCAddress addr = rule.blockAddr;
    auto &ruleSet = ruleMap[addr];

    for (auto oRuleIter = ruleSet.begin(); oRuleIter != ruleSet.end() ; ){
        if (oRuleIter->ruleAddr == rule.ruleAddr) {
            ruleSet.erase(oRuleIter++);
        }
        else ++oRuleIter;
    }
}


RewriteRule::RewriteRule(RuleOp op, PCAddress blockAddr, PCAddress ruleAddr, uint32_t instr_id)
:opcode(op),blockAddr(blockAddr),ruleAddr(ruleAddr),id(instr_id)
{
    reg0 = 0;
    reg1 = 0;
}

RewriteRule::RewriteRule(RuleOp op, BasicBlock *block, bool pre_insert)
:opcode(op)
{
    reg0 = 0;
    reg1 = 0;
    blockAddr = block->instrs->pc;

    if(pre_insert) {
        ruleAddr = blockAddr;
        id = block->startInstID; 
    }
    else {
        //for normal instructions, insert at beginning of the next block
        if(block->fake) {
            ruleAddr = block->succ1->instrs->pc;
            id = block->succ1->instrs->id;
        }
        //for post insert, if the instruction is a jump, then insert before the jump
        else {
            ruleAddr = block->lastInstr()->pc;
            id = block->lastInstr()->id;
        }
    }
}

RewriteRule::RewriteRule(RuleOp op, BasicBlock *block, uint32_t position)
:opcode(op)
{
    reg0 = 0;
    reg1 = 0;
    blockAddr = block->instrs->pc;
    ruleAddr = block->instrs[position].pc;
    id = block->instrs[position].id;
}

RRule
RewriteRule::toRRule(uint32_t channel)
{
    RRule rule;
    rule.channel = channel;
    rule.opcode = opcode;
    IF_VERBOSE(rule.id = id);
    rule.block_address = blockAddr;
    rule.pc = ruleAddr;
    rule.reg0 = reg0;
    rule.reg1 = reg1;
    rule.next = NULL;
    return rule;
}

void
RewriteRule::print(void *outputStream) {
    ostream &os = *((ostream*)outputStream);

    os << "HINT - " << opcode << " Block=" << blockAddr << ", Addr=" << ruleAddr << "(ID: " << ((uint16_t)ureg0.down) << "), r0=" << reg0 << ", r1=" << reg1 << endl;
}

/** \brief Encode the current JVar to rewrite rule (wrapper) */
void encodeJVar(JVar var, RewriteRule &rule)
{
    JVarPack vp;
    vp.var = var;
    rule.reg0 = vp.data1;
    rule.reg1 = vp.data2;
}

/** \brief Decode the the rewrite rule and get JVar */
JVar decodeJVar(RewriteRule &rule)
{
    JVarPack vp;
    vp.data1 = rule.reg0;
    vp.data2 = rule.reg1;
    return vp.var;
}

bool janus::operator<(const RewriteRule &lhs, const RewriteRule &rhs)
{
    if(lhs.id < rhs.id) return true;
    else if(lhs.id == rhs.id) {
        if(lhs.opcode < rhs.opcode)
            return true;
        else if (lhs.opcode > rhs.opcode)
                return false;
             else if (lhs.ruleAddr < rhs.ruleAddr) 
                     return true;
                  else if (lhs.ruleAddr > rhs.ruleAddr)
                          return false;
                       else if (lhs.reg0 < rhs.reg0)
                               return true;
                            else if (lhs.reg0 > rhs.reg0)
                                    return false;
                                 else return (lhs.reg1 < rhs.reg1);
    }
    else return false;
}

bool janus::operator==(const RewriteRule &lhs, const RewriteRule&rhs)
{
    return (lhs.id == rhs.id) && (lhs.opcode == rhs.opcode) && (lhs.ruleAddr == rhs.ruleAddr) && (lhs.reg0 == rhs.reg0) && (lhs.reg1 == rhs.reg1);
}
