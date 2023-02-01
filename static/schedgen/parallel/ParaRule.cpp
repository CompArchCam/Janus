#include "ParaRule.h"
#include "JanusContext.h"
#include "Expression.h"
#include "LoopSelect.h"
#include "IO.h"
#include <vector>

#ifdef JANUS_X86
#include "janus_x86.h"
#endif

#ifdef JANUS_VECT_SUPPORT
#include "VECT_rule_structs.h"
#include "VectRule.h"
#include "janus_arch.h"
#endif

using namespace std;
using namespace janus;

//static void prepareLoopHeader(JanusContext *gc, Loop &loop);
static void prepareLoopHeader(Loop &loop);
///Generate rewrite rules for DOALL parallelisation in the given loop
//static void generateDOALLRules(JanusContext *gc, Loop &loop, std::map<PCAddress)
static void generateDOALLRules(JanusContext *gc, Loop &loop, std::map<PCAddress, janus::Function *> functionMap);
///Generate rewrite rules for the sub functions in the given loop
static void
generateSubFunctionRules(JanusContext *gc, Loop &loop, Function &func);
static int
getEncodedArrayIndex(Loop *loop, Expr var);

//void generateParallelRules(JanusContext *gc)
void generateParallelRules(JanusContext *gc, std::map<PCAddress, janus::Function *>& functionMap, std::vector<janus::Function>& functions,
		LoopAnalysisReport& loopAnalysisReport, janus::Function *fmain, std::vector<janus::Loop>& loops, std::string name)
{

    set<LoopID> selected_loop;

    /* Step 1: select DOALL loops for parallelisation */
    //selectDOALLLoops(gc, selected_loop);
    selectDOALLLoops(selected_loop, functions, functionMap, loops, loopAnalysisReport, fmain, name);

    //gc->passedLoop = selected_loop.size();
    loopAnalysisReport = LoopAnalysisReport(selected_loop.size(), true);

    /* Step 2: generate rewrite rules for each selected loop
     * assign a dynamic id */

    int dynamic_id = 0;

    for (auto loopID : selected_loop) {
        //Loop &loop = gc->loops[loopID-1];
    	Loop &loop = loops[loopID-1];
        loop.pass = true;
        /* Generate headers based on the type of the loop */
        //prepareLoopHeader(gc, loop);
        prepareLoopHeader(loop);
        /* Assign dynamic loop id */
        loop.header.id = dynamic_id++;
        /* Generate DOALL parallelisation rules for each loop */
        //generateDOALLRules(gc, gc->loops[loopID-1], functionMap);
        generateDOALLRules(gc, loops[loopID-1], functionMap);
    }

    /* Step 3: select and generate speculative loops */

    /* Step 4: generate thread create/exit rules on main function */
    //GASSERT(gc->main, "main function not found in this program");
    GASSERT(fmain, "main function not found in this program");
    Function *main = fmain;

    RewriteRule rule = RewriteRule(THREAD_CREATE, main->entry, FIRST_INSTRUCTION);
    //rule.reg0 = gc->functions.size();
    rule.reg0 = functions.size();
    rewriteRules[MAIN_CHANNEL].insert(rule);
    /* Thread create explicitly changes the basic block termination
     * The basic block structure has been changed */
    reshapeBlock = main->entry;

    for(auto t:main->terminations) {
        BasicBlock *end_block = main->entry + t;
        insertRule(MAIN_CHANNEL,RewriteRule(THREAD_EXIT,end_block,PRE_INSERT), end_block);
    }
}

//static void prepareLoopHeader(JanusContext *gc, Loop &loop)
static void prepareLoopHeader(Loop &loop)
{
    RSLoopHeader &header = loop.header;
    header.staticID = loop.id;
    header.loopStartAddr = loop.start->instrs->pc;
    header.registerToCopy = loop.registerToCopy.bits;
    header.registerToMerge = loop.registerToMerge.bits;
    header.registerToConditionalMerge = loop.registerToConditionalMerge.bits;
    /* Scratch register (4 provided) */
    header.scratchReg0 = loop.scratchRegs.regs.reg0;
    header.scratchReg1 = loop.scratchRegs.regs.reg1;
    header.scratchReg2 = loop.scratchRegs.regs.reg2;
    header.scratchReg3 = loop.scratchRegs.regs.reg3;

    if (loop.check.size() > 1){
        fprintf(stderr, "Loop has multiple check blocks, jumpInstructionOpcode probably not correct!\n");
    }
    for (auto checkID: loop.check) {
        BasicBlock &checkBlock = loop.parent->entry[checkID];
        //get the last conditional jump, a check block is always conditional jump
        Instruction *cjump = checkBlock.lastInstr();
        std::cout << *cjump << endl;
        header.jumpInstructionOpcode = cjump->minstr->opcode;
        header.jumpingGoesBack = (cjump->minstr->getTargetAddress() == loop.start->instrs->pc); //Look whether the jcc jumps to loop start
    }
#ifdef JANUS_X86
    //In dynamic/parallel/arch/x86 emit_init_induction_variable_block we currently require that s1 is not RAX or RDX
    if (header.scratchReg1 == JREG_RAX || header.scratchReg1 == JREG_RDX){
        if (header.scratchReg0 != JREG_RAX && header.scratchReg0 != JREG_RDX){ //Try swapping s0 and s1
            swap(header.scratchReg0, header.scratchReg1); 
        }
        else{ //If s1 and s0 are RAX and RDX, swap s1 and s2
            swap(header.scratchReg1, header.scratchReg2);
        }
    }
#endif 

    //if it has ancestors, it means the loop is an inner loop
    if (loop.ancestors.size())
        header.isInnerLoop = 1;
    else
        header.isInnerLoop = 0;

    //DOALL Block based parallelisation
    //TODO: intelligent determine the policy
    header.schedule = PARA_DOALL_BLOCK;
}

//static void generateDOALLRules(JanusContext *gc, Loop &loop, std::map<PCAddress)
static void generateDOALLRules(JanusContext *gc, Loop &loop, std::map<PCAddress, janus::Function *> functionMap)
{
    Function *parent = loop.parent;
    /* Get entry block of the CFG */
    BasicBlock *entry = parent->entry;
    /* Get the array of instructions */
    Instruction *instrs = parent->instrs.data();
    RewriteRule rule;
    uint32_t id = loop.id;

    /* Insert rules for manually split over-sized blocks */
    for (auto bset : parent->blockSplitInstrs) {
        BasicBlock *bb = entry + bset.first;
        for (auto sid : bset.second) {
            rule = RewriteRule(APP_SPLIT_BLOCK, bb->instrs->pc, parent->instrs[sid].pc, sid);
            insertRule(id, rule, bb);
        }
    }

    /* If it is an inner loop, insert annotation in the outer loop */
    for (auto outer : loop.ancestors) {
        if (outer->level == 0) {
            //outermost loop init
            for (auto bid: outer->init) {
                BasicBlock *bb = entry + bid;
                rule = RewriteRule(PARA_OUTER_LOOP_INIT, bb, POST_INSERT);
                rule.reg0 = outer->header.id;
                insertRule(id, rule, bb);
            }

            //outermost loop finish
            for (auto bid: outer->exit) {
                BasicBlock *bb = entry + bid;
                rule = RewriteRule(PARA_OUTER_LOOP_END, bb, POST_INSERT);
                rule.reg0 = outer->header.id;
                insertRule(id, rule, bb);
            }
        }
    }

    // -----------------------------------------------------
    /* Loop start and finish */
    // -----------------------------------------------------
    for (auto bid: loop.init) {
        BasicBlock *bb = entry + bid;
        rule = RewriteRule(PARA_LOOP_INIT, bb, POST_INSERT);
        rule.reg0 = loop.header.id;
        if (!bb->fake && bb->lastInstr()->isConditionalJump()){
            //Check whether the rule will be attached to a conditional jump instruction
            Instruction *trigger = bb->lastInstr();
            if (trigger->minstr->getTargetAddress() == loop.start->instrs->pc){
                //Check whether jumping goes to the loop
                rule.reg1 = 1;
            }
            else{
                //or if not jumping goes to loop
                rule.reg1 = 2;
            }

        }
        insertRule(id, rule, bb);
    }

    for (auto bid: loop.exit)
    {
        rule = RewriteRule(PARA_LOOP_FINISH, entry + bid, PRE_INSERT);
        rule.reg0 = loop.header.id;
        rule.reg1 = entry[bid].instrs->pc;
        insertRule(id, rule, entry + bid);
    }
    // -----------------------------------------------------

    /* Loop iteration Start */
    rule = RewriteRule(PARA_LOOP_ITER, loop.start, PRE_INSERT);
    /* Only the modified register should be check-pointed */
    rule.reg0 = loop.header.id;
    insertRule(id, rule, loop.start);

    RegSet checkRegisters;            //We use these for MEM_RESTORE_CHECK_REG rule insertion
    std::set<Instruction*> cmpInstrs; //

    /* Update check conditions */
    for (auto checkID: loop.check) {
        BasicBlock &checkBlock = loop.parent->entry[checkID];
        //get the last conditional jump, a check block is always conditonal jump
        auto *cjump = checkBlock.lastInstr();
        //retrieve the phi node of the eflag
        Instruction *cmpInstr = NULL;
        for (auto vi : cjump->inputs) {
            if (vi->type == JVAR_CONTROLFLAG) {
                cmpInstr = vi->lastModified;
            }
        }

        cmpInstrs.insert(cmpInstr);

        if (!cmpInstr) {
            cerr<<"error in finding compare instruction for cjump "<<*cjump<<endl;
            continue;
        }

        Variable checkVar; //This is the check variable obtained from the induction variable
        Variable inductionVar; //This is the induction var in the cmp instruction
        Variable condition; //This is obtained as the non-induction var operand in the cmp instruction
        Iterator *iter = NULL;
        bool found = false;
        int inductionVarOpndIndex = -1; //This stores which operand in the cmp instruction is the induction variable
                                        //Used when handling PARA_LOOP_UPDATE_BOUNDS
        int i = 0;
        for (auto phi: cmpInstr->inputs) {
            if (phi->expr && phi->expr->expandedLoopForm) {
                iter =  phi->expr->expandedLoopForm->hasIterator(&loop);
                if (iter) {
                    //We retrieve checkVar from the main iterator
                    if (iter->checkState) checkVar = (Variable)*(iter->checkState);
                    inductionVar = (Variable)*(iter->vs);
                    inductionVarOpndIndex = i;
                    found = true;
                }
            }
            if (loop.isConstant(phi)) {
                //Is this branch redundant?
                condition = (Variable)*phi;
            }
            i++;
        }
        //JAN-59
        //There are scenarios like:
        //mov rax, 0x10
        //cmp inductionreg, rax
        //In this case, the static analysis will determine (with Loop::getAbsoluteStorage) that 
        //the check value is 0x10, not rax
        //So we use that, instead of the second operand of the instruction
        if (checkVar.type != JVAR_UNKOWN) condition = checkVar;
        if (condition.type == JVAR_REGISTER) checkRegisters.insert(condition.value);

        if (!found) {
            //cerr<<"Loop "<<loop.id<<" compare instruction does not contain induction variable, quit rule generation"<<endl;
            //we relax this condition since this would never be a constant bound
            break;
        }

        if (condition.type == JVAR_UNKOWN){
            cerr << "Loop condition variable unknown!" << endl;
        }
        //if condition is a constant or memory location, then emit a rewrite rule to alter the cmp instruction at runtime
        if (condition.type == JVAR_CONSTANT || condition.type == JVAR_MEMORY || condition.type == JVAR_ABSOLUTE) {
            rule = RewriteRule(PARA_UPDATE_LOOP_BOUND, cmpInstr->block->instrs->pc, cmpInstr->pc, cmpInstr->id);
            int var_index = 0;

            for (auto var: loop.encodedVariables) {
                Variable test = var.var;
                if (test == inductionVar)
                    break;
                var_index++;
            }

            rule.ureg0.down = var_index;
            rule.ureg0.up = inductionVarOpndIndex;
            rule.reg1 = loop.header.id;
            insertRule(id, rule, cmpInstr->block);
        }
    }

    /* Update stack redirections */
    bool useStack = false;
    for (auto bid: loop.body) {
        BasicBlock *bb = entry + bid;
        //Which of the conditional merge registers have been written to in this block
        uint64_t blockWrittenCondMergeRegs = 0;

        for (int i=0; i<bb->size; i++) {
            Instruction &instr = bb->instrs[i];
            //skip function call
            if (instr.opcode == Instruction::Call) continue;

            if (loop.spillRegs.bits & instr.regReads.bits ||
                loop.spillRegs.bits & instr.regWrites.bits) {
                /* Insert MEM_SCRATCH_REG */
                rule = RewriteRule(MEM_SCRATCH_REG, bb->instrs->pc, instr.pc, instr.id);
                rule.reg1 = loop.header.id;
                insertRule(id,rule,bb);
            }

            //If an instruction uses the loop check register and is not a cmp
            //we want the original check value, not our modified one
            //so we insert a MEM_RESTORE_CHECK_REG rule
            //If the same instruction also gets a MEM_SCRATCH_REG, dynamically it's processed before,
            //so that if the check reg is a scratch reg, the MEM_RESTORE_CHECK_REG value stays
            if ((checkRegisters.bits & instr.regReads.bits) && 
                instr.opcode != Instruction::Compare) {        
                rule = RewriteRule(MEM_RESTORE_CHECK_REG, bb->instrs->pc, instr.pc, instr.id);
                //reg0 contains the check register id that we need to spill
                rule.reg0 = (checkRegisters & instr.regReads).getNextLowest(JREG_RAX); //The regset should contain only one register
                rule.reg1 = loop.header.id;
                insertRule(id,rule,bb);
            }

            if (loop.registerToConditionalMerge.bits & instr.regWrites.bits){
                //reg0 contains the register id that is written to
                blockWrittenCondMergeRegs |= (loop.registerToConditionalMerge.bits & instr.regWrites.bits);
            }
        }
        if (blockWrittenCondMergeRegs){
            rule = RewriteRule(MEM_RECORD_REG_WRITE, bb->instrs->pc, bb->instrs->pc, bb->instrs->id);
            rule.reg0 = blockWrittenCondMergeRegs;
            rule.reg1 = loop.header.id;
            insertRule(id,rule,bb);
        }


        /* Generate rules for memory accesses */
        for (auto &memInstr: bb->minstrs) {
            if (!useStack && memInstr.mem->type == JVAR_STACK)
                useStack = true;

            //if the stack access is not private variable, then use shared stack
            if (memInstr.mem->type == JVAR_STACK) {
                Variable stackVar = *(Variable *)memInstr.mem;
                if (loop.privateVars.find(stackVar) == loop.privateVars.end() &&
                    loop.phiVars.find(stackVar)== loop.phiVars.end() &&
                    loop.checkVars.find(stackVar)== loop.checkVars.end()) {
                    //MEM_MAIN_STACK rule is inserted at every non-private stack accesses
                    //induction variable stored in private stacks are assumed to be private too
                    //loop bound are also privatised too
                    rule = RewriteRule(MEM_MAIN_STACK, bb->instrs->pc, memInstr.instr->pc, memInstr.instr->id);
                    rule.reg1 = loop.header.id;
                    insertRule(id, rule, entry + bid);
                }
            }
        }

        if (bb->lastInstr()->opcode == Instruction::Call) {
            /* PARA_SUBCALL_START is inserted before performing the function call */
            rule = RewriteRule(PARA_SUBCALL_START, bb, POST_INSERT);
            rule.reg0 = loop.header.id;
            insertRule(id, rule, bb);

            /* Generate rewrite rules for this function */
            //Function *func = bb->lastInstr()->getTargetFunction();
            Function *func = bb->lastInstr()->getTargetFunction(functionMap);
            if (func && !func->isExternal)
                generateSubFunctionRules(gc, loop, *func);
            else {
                //external functions, generate TX_START and TX_END
                rule = RewriteRule(TX_START, bb, POST_INSERT);
                rule.reg0 = loop.header.id;
                insertRule(id, rule, bb);
                if(bb->succ1) {
                    rule = RewriteRule(TX_FINISH, bb->succ1, PRE_INSERT);
                    rule.reg0 = loop.header.id;
                    insertRule(id, rule, bb->succ1);
                }
            }

            /* PARA_SUBCALL_FINISH is inserted at the block where the subcall is supposed to be returned */
            if(bb->succ1) {
                rule = RewriteRule(PARA_SUBCALL_FINISH, bb->succ1, PRE_INSERT);
                rule.reg0 = loop.header.id;
                insertRule(id, rule, bb->succ1);
            }
        }
    }

    if (useStack) loop.header.useStack = 1;
    else loop.header.useStack = 0;

    int stackFrameSize = parent->totalFrameSize;
    //align the stack size
    stackFrameSize = (stackFrameSize / 64 + 1) * 64;

    if (useStack) loop.header.stackFrameSize = stackFrameSize;
    else loop.header.stackFrameSize = 0;

    /* MEM_RECORD_BOUNDS is inserted where the array base runtime is generated
     * so that Janus can keep a record privately for its later runtime check */
    for (auto checkBase: loop.arrayToCheck) {
        if (checkBase.vs && checkBase.vs->lastModified) {
            Instruction *lastModified = checkBase.vs->lastModified;
            BasicBlock *bb = lastModified->block;
            if (lastModified->opcode == Instruction::Call) {
                //insert at the next basic block after the call
                bb = bb->succ1;
                rule = RewriteRule(MEM_RECORD_BOUNDS, bb, PRE_INSERT);
            } else {
                //insert at after the instruction
                Instruction &next = bb->instrs[lastModified->id - bb->startInstID + 1];
                rule = RewriteRule(MEM_RECORD_BOUNDS, bb->instrs->pc, next.pc, next.id);
            }
            rule.reg0 = getEncodedArrayIndex(&loop, checkBase);
            rule.reg1 = loop.header.id;
            insertRule(id, rule, bb);
        } else if (checkBase.vs) {
            //variable state is a function argument, then record it at function entry
            BasicBlock *bb = checkBase.vs->block;
            rule = RewriteRule(MEM_RECORD_BOUNDS, bb, PRE_INSERT);
            rule.reg0 = getEncodedArrayIndex(&loop, checkBase);
            rule.reg1 = loop.header.id;
            insertRule(id, rule, bb);
        }
    }

    /* MEM_BOUND_CHECK is inserted before the loop after all the inputs been recorded */
    for (auto checkBase: loop.arrayToCheck) {
        for (auto memBase: loop.arrayAccesses) {
            Expr checkBase2 = memBase.first;
            if (!(checkBase == checkBase2)) {
                for (auto bid: loop.init) {
                    BasicBlock *bb = entry + bid;
                    rule = RewriteRule(MEM_BOUNDS_CHECK, bb, PRE_INSERT);
                    rule.ureg0.up = getEncodedArrayIndex(&loop, checkBase);
                    rule.ureg0.down = getEncodedArrayIndex(&loop, checkBase2);
                    rule.reg1 = loop.header.id;
                    insertRule(id, rule, bb);
                }
            }
        }
    }
#ifdef JANUS_VECT_SUPPORT
    VECT_RULE *vrule;

    //step 1: insert broadcast rules
    set<uint32_t> copySet;
    loop.registerToCopy.toSTLSet(copySet);
    for (auto reg: copySet) {
        if (jreg_is_simd(reg)) {
            //get the definition
            Variable v((uint32_t)reg);
            VarState *vs = loop.start->alive(v);
            if (vs != NULL && vs->lastModified) {
                Instruction *instr = vs->lastModified;
                VECT_RULE *rule = new VECT_RULE(instr->block, instr->pc, instr->id, VECT_BROADCAST);
                insertRule(id, *rule->encode(), instr->block);
            }
        }
    }

    //step 1: retrieve the alignment information
    bool aligned = alignmentAnalysis(loop);
    //insert VECT_LOOP_PEEL if not aligned
    if (loop.peelDistance > 0) {
        //insert loop peeling after the loop
        for (auto init: loop.init) {
            BasicBlock *bb = entry + init;
            Instruction *instr = &(bb->instrs[bb->size-1]);
            VECT_LOOP_PEEL_rule *rule = new VECT_LOOP_PEEL_rule(bb, instr->pc, instr->id, (uint32_t)(loop.start->instrs->pc-instr->pc),
                (uint16_t)loop.scratchRegs.regs.reg0, (uint16_t)loop.scratchRegs.regs.reg1, (uint32_t)loop.staticIterCount);
            rule->vectorWordSize = loop.vectorWordSize;
            insertRule(id, *rule->encode(), bb);
        }
    }

    //step 2: examine each loop instructions
    for (auto bid: loop.body) {
        BasicBlock &bb = entry[bid];
        for (int i=0; i<bb.size; i++) {
            Instruction &instr = bb.instrs[i];
            if (instr.isVectorInstruction()) {
                //extend the existing vector instruction
                bool postIteratorUpdate = checkPostIteratorUpdate(instr);
                VECT_CONVERT_rule *rule = new VECT_CONVERT_rule(&bb, instr.pc, instr.id, 0, postIteratorUpdate, (uint16_t)loop.mainIterator->stride, 
                0, ALIGNED, 0, loop.staticIterCount);
                insertRule(id, *rule->encode(), &bb);
            }
        }
    }

    //update all strides
    for (auto iterEntry: loop.iterators) {
        Iterator &iter = iterEntry.second;

        if (iter.strideKind == Iterator::INTEGER) {
            //insert rewrite rule on the update instruction
            Instruction *instr = iter.getUpdateInstr();
            if (!instr) {
                cerr<<"Induction update instruction not found"<<endl;
            }
            rule = RewriteRule(VECT_INDUCTION_STRIDE_UPDATE, instr->block->instrs->pc, instr->pc, instr->id);
            rule.reg0 = 0;
            rule.reg1 = loop.vectorWordSize;
            insertRule(id,rule,instr->block);
        }
        else if (iter.strideKind == Iterator::SINGLE_VAR) {
            VarState *vs = iter.strideVar;
            if (vs->type == JVAR_REGISTER) {
                uint64_t reg = vs->value;
                //unroll the loop stride before the loop
                for (auto init: loop.init) {
                    BasicBlock *bb = entry + init;
                    rule = RewriteRule(VECT_INDUCTION_STRIDE_UPDATE, bb, POST_INSERT);
                    rule.reg0 = reg;
                    rule.reg1 = loop.vectorWordSize;
                    insertRule(id, rule, bb);
                }
                // Revert required for VECT_INDUCTION_STRIDE_UPDATE's multiplication of the reg value.
                for (auto bid: loop.exit) {
                    rule = RewriteRule(VECT_INDUCTION_STRIDE_RECOVER, entry + bid, PRE_INSERT);
                    rule.reg0 = reg;
                    rule.reg1 = loop.vectorWordSize;
                    insertRule(id, rule, entry + bid);
                }
            }
        } else continue;
    }
#endif
}

static void
generateSubFunctionRules(JanusContext *gc, Loop &loop, Function &func)
{

}

/* Emit all relevant info in the rule file */
//uint32_t compileParallelRulesToFile(JanusContext *gc)
uint32_t compileParallelRulesToFile(JanusContext *gc, std::string name, std::vector<janus::Loop>& loops, LoopAnalysisReport& loopAnalysisReport, std::vector<janus::Function>& functions)
{
    //FILE *op = fopen(string((gc->name)+".jrs").c_str(),"w");
	FILE *op = fopen(string((name)+".jrs").c_str(),"w");
    fpos_t pos;
    RSchedHeader header;
    uint32_t numRules = 0;
    int offset = 0;
    int fileSize;
    //uint32_t numLoops = gc->loops.size();
    uint32_t numLoops = loops.size();

    int id = 0;
    //for (auto &loop: gc->loops) {
    for (auto &loop: loops) {
        if (loop.pass) {
            loop.header.id = id++;
        }
    }

    /* Step 1: prepare Rule Program  headers */
    header.ruleFileType = (uint32_t)(gc->mode);
    //header.numFuncs = gc->functions.size();
    header.numFuncs = functions.size();
    //header.numLoops = gc->passedLoop;
    header.numLoops = loopAnalysisReport.getPassedLoopId();
    header.multiMode = 1;
    /* Calculate the offset of loop and rule header */
    header.loopHeaderOffset = sizeof(RSchedHeader);
    //header.ruleInstOffset = sizeof(RSchedHeader) + (gc->passedLoop) * sizeof(RSLoopHeader);
    header.ruleInstOffset = sizeof(RSchedHeader) + (loopAnalysisReport.getPassedLoopId()) * sizeof(RSLoopHeader);
    /* Calculate the offset of individual loop header */
    /* Add rules for channel 0 */
    for(auto set:(rewriteRules[0].ruleMap)) {
        numRules += set.second.size();
    }
    offset = header.ruleInstOffset + (numRules * sizeof(RRule));
    //for (auto &loop : gc->loops) {
    for (auto &loop : loops) {
        if (loop.pass) {
            /* Get the number of static rules */
            int size = 0;
            for(auto set:(rewriteRules[loop.id].ruleMap)) {
                size += set.second.size();
            }
            loop.header.ruleInstOffset = offset;
            loop.header.ruleInstSize = size;
            offset += (size * sizeof(RRule));
            numRules += size;
        }
    }

    header.numRules = numRules;
    header.ruleDataOffset = header.ruleInstOffset + numRules * sizeof(RRule);

    offset = header.ruleDataOffset;
    //for (auto &loop : gc->loops) {
    for (auto &loop : loops) {
        if (loop.pass) {
            int size = loop.encodedVariables.size();
            loop.header.ruleDataOffset = offset;
            loop.header.ruleDataSize = size;
            offset += (size * sizeof(JVarProfile));
        }
    }
    fileSize = offset;

    /* Emit rule header */
    fwrite(&header,sizeof(RSchedHeader),1,op);
    offset += sizeof(RSchedHeader);

    /* Emit loop headers */
    //for (auto &loop : gc->loops) {
    for (auto &loop : loops) {
        if (loop.pass) {
            fwrite(&(loop.header),sizeof(RSLoopHeader),1,op);
        }
    }

    /* Emit rule instructions */
    RRule rule;
    for(int channel=0; channel<=numLoops; channel++) {
        //if (!channel || gc->loops[channel-1].pass) {
    	if (!channel || loops[channel-1].pass) {
            for(auto set:(rewriteRules[channel].ruleMap)) {
                for(auto srule:set.second) {
                    rule = srule.toRRule(channel);
                    fwrite(&rule,sizeof(RRule),1,op);
                    offset += sizeof(RRule);
                }
            }
        }
    }

    /* Emit rule induction variables */
    //for (auto &loop : gc->loops) {
    for (auto &loop : loops) {
        if (loop.pass) {
            for (auto &ind: loop.encodedVariables) {
                fwrite(&ind,sizeof(JVarProfile),1,op);
            }
        }
    }
    fclose(op);

    return fileSize;
}

static int
getEncodedArrayIndex(Loop *loop, Expr var)
{
    if (var.kind == Expr::EXPANDED) return -1;
    int index = 0;
    for (auto profile: loop->encodedVariables) {
        if (profile.type == ARRAY_PROFILE) {
            JVarPack vp;
            if (var.vs == NULL) return -1;
            vp.var = *var.vs;
            if (vp.var == profile.array.base && var.vs->version == profile.version)
                return index;
        }
        index++;
    }
    return -1;
}
