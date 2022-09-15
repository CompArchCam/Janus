#include "VectRule.h"
#include "Iterator.h"
#include "JanusContext.h"
//#include "janus_x86.h"
#include "VECT_rule_structs.h"
#include "VectLoopSelect.h"
#include "capstone/capstone.h"
#include "janus_arch.h"

static bool needBroadcast(Loop &loop, Instruction &instr);

static void prepareLoopHeader(JanusContext *gc, Loop &loop)
{
    RSLoopHeader &header = loop.header;
    header.staticID = loop.id;
    header.loopStartAddr = loop.start->instrs->pc;
    header.registerToCopy = loop.registerToCopy.bits;
    header.registerToMerge = loop.registerToMerge.bits;
    /* Scratch register (4 provided) */
    header.scratchReg0 = loop.scratchRegs.regs.reg0;
    header.scratchReg1 = loop.scratchRegs.regs.reg1;
    header.scratchReg2 = loop.scratchRegs.regs.reg2;
    header.scratchReg3 = loop.scratchRegs.regs.reg3;

    // DOALL Block based parallelisation
    // TODO: intelligent determine the policy
    header.schedule = PARA_DOALL_BLOCK;
}

void generateVectorRulesForLoop(JanusContext *gc, Loop *loop)
{
    BasicBlock *entry = loop->parent->getCFG().entry;
    /* Get the array of instructions */
    Instruction *instrs = loop->parent->instrs.data();

    RewriteRule rule;
    VECT_RULE *vrule;

    int id = loop->id;

    // step 1: retrieve the alignment information
    bool aligned = alignmentAnalysis(*loop);

    if (loop->peelDistance)
        return;

    /* Add debug rules at the start end finish of the loop */
    if (loop->ancestors.size() == 0) {
        /* Loop start and finish */
        for (auto bid : loop->init) {
            rule = RewriteRule(PARA_LOOP_INIT, entry + bid, POST_INSERT);
            insertRule(id, rule, entry + bid);
        }

        for (auto bid : loop->exit) {
            rule = RewriteRule(PARA_LOOP_FINISH, entry + bid, PRE_INSERT);
            insertRule(id, rule, entry + bid);
        }
    }

    /* Insert annotation in the outer loop for debugging purposes */
    for (auto outer : loop->ancestors) {
        if (outer->level == 0) {
            // outermost loop init
            for (auto bid : outer->init) {
                BasicBlock *bb = entry + bid;
                rule = RewriteRule(PARA_OUTER_LOOP_INIT, bb, POST_INSERT);
                insertRule(id, rule, bb);
            }

            // outermost loop finish
            for (auto bid : outer->exit) {
                BasicBlock *bb = entry + bid;
                rule = RewriteRule(PARA_OUTER_LOOP_END, bb, POST_INSERT);
                insertRule(id, rule, bb);
            }
        }
    }

#ifdef JANUS_X86
    // step 1: insert broadcast rules
    set<uint32_t> copySet;
    loop->registerToCopy.toSTLSet(copySet);
    for (auto reg : copySet) {
        if (jreg_is_simd(reg)) {
            // get the definition
            Variable v((uint32_t)reg);
            VarState *vs = loop->start->alive(v);
            if (vs != NULL && vs->lastModified) {
                Instruction *instr = vs->lastModified;
                VECT_RULE *vr = new VECT_RULE(instr->block, instr->pc,
                                              instr->id, VECT_BROADCAST);
                rule = *vr->encode();
                rule.ureg0.up = 0;
                insertRule(id, rule, instr->block);
            } else {
                // if variable state not found, it means it is from function
                // argument then add to the init block of the loop
                for (auto bid : loop->init) {
                    rule =
                        RewriteRule(VECT_BROADCAST, entry + bid, POST_INSERT);
                    rule.ureg0.up = 1;
                    rule.ureg0.down = v.size;
                    rule.reg1 = reg;
                    insertRule(id, rule, entry + bid);
                }
            }
        }
    }

    // insert VECT_LOOP_PEEL if not aligned
    if (loop->peelDistance > 0) {
        // insert loop peeling after the loop
        for (auto init : loop->init) {
            BasicBlock *bb = entry + init;
            Instruction *instr = &(bb->instrs[bb->size - 1]);
            VECT_LOOP_PEEL_rule *rule = new VECT_LOOP_PEEL_rule(
                bb, instr->pc, instr->id,
                (uint32_t)(loop->start->instrs->pc - instr->pc),
                (uint16_t)loop->scratchRegs.regs.reg0,
                (uint16_t)loop->scratchRegs.regs.reg1,
                (uint32_t)loop->staticIterCount);
            rule->vectorWordSize = loop->vectorWordSize;
            insertRule(id, *rule->encode(), bb);
        }
    }

    // step 2: examine each loop instructions
    for (auto bid : loop->body) {
        BasicBlock &bb = entry[bid];
        for (int i = 0; i < bb.size; i++) {
            Instruction &instr = bb.instrs[i];
            if (instr.isVectorInstruction()) {
                // extend the existing vector instruction
                bool postIteratorUpdate = checkPostIteratorUpdate(instr);
                VECT_CONVERT_rule *rule = new VECT_CONVERT_rule(
                    &bb, instr.pc, instr.id, 0, postIteratorUpdate,
                    (uint16_t)loop->mainIterator->stride,
                    (uint16_t)loop->freeSIMDRegs.getNextLowest(JREG_XMM0),
                    ALIGNED, 0, loop->staticIterCount);
                insertRule(id, *rule->encode(), &bb);
            }
        }
    }

    // update all strides
    for (auto iterEntry : loop->iterators) {
        Iterator &iter = iterEntry.second;

        if (iter.strideKind == Iterator::INTEGER) {
            // insert rewrite rule on the update instruction
            Instruction *instr = iter.getUpdateInstr();
            if (!instr) {
                cerr << "Induction update instruction not found" << endl;
            }
            rule = RewriteRule(VECT_INDUCTION_STRIDE_UPDATE,
                               instr->block->instrs->pc, instr->pc, instr->id);
            rule.reg0 = 0;
            rule.reg1 = loop->vectorWordSize;
            insertRule(id, rule, instr->block);
        } else if (iter.strideKind == Iterator::SINGLE_VAR) {
            VarState *vs = iter.strideVar;
            if (vs->type == JVAR_REGISTER) {
                uint64_t reg = vs->value;
                // unroll the loop stride before the loop
                for (auto init : loop->init) {
                    BasicBlock *bb = entry + init;
                    rule = RewriteRule(VECT_INDUCTION_STRIDE_UPDATE, bb,
                                       POST_INSERT);
                    rule.reg0 = reg;
                    rule.reg1 = loop->vectorWordSize;
                    insertRule(id, rule, bb);
                }
                // Revert required for VECT_INDUCTION_STRIDE_UPDATE's
                // multiplication of the reg value.
                for (auto bid : loop->exit) {
                    rule = RewriteRule(VECT_INDUCTION_STRIDE_RECOVER,
                                       entry + bid, PRE_INSERT);
                    rule.reg0 = reg;
                    rule.reg1 = loop->vectorWordSize;
                    insertRule(id, rule, entry + bid);
                }
            }
        } else
            continue;
    }
#endif
}

void generateVectorRules(JanusContext *gc)
{
    set<InstOp> supported_opcodes;
    set<InstOp> singles;
    set<InstOp> doubles;
    set<Loop *> selected_loops;

    /* Step 0: set supported opcodes along with which are single precision and
     * double precision */
    setSupportedVectorOpcodes(supported_opcodes, singles, doubles);

    /* Step 1: select loops for vectorisation based on the selected opcode */
    selectVectorisableLoop(gc, selected_loops, supported_opcodes, singles,
                           doubles);

    /* Step 2: generate rules for each loop */
    for (auto l : selected_loops) {
        cout << l->id << " ";
        generateVectorRulesForLoop(gc, l);
    }
    cout << endl;
}

bool alignmentAnalysis(Loop &loop)
{
    BasicBlock *entry = loop.parent->getCFG().entry;
    bool aligned = true;
    int64_t strideImm = 0;

    LOOPLOGLINE("loop " << dec << loop.id << " alignment analysis:");

    for (auto s : loop.arrayAccesses) {
        auto set = s.second;
        for (auto m : set) {
            // the scalar evolution analysis shows the alignment references
            //{start,+,stride}
            LOOPLOGLINE("\t" << *m->escev);
            Expr start = m->escev->start;

            if (start.kind == Expr::INTEGER) {
                // we have to make sure all the memory accesses are multiple of
                // SIMD lanes
                int64_t base = start.i;
                // for avx it must be divisiable by 16
                int peel = base % 16;
                if (peel != 0) {
                    if (!loop.peelDistance) {
                        loop.peelDistance = peel;
                    } else if (loop.peelDistance != peel) {
                        loop.peelDistance = -1;
                        aligned = false;
                    }
                }
            } else
                return false;

            // analyse the strides
            for (auto strideP : m->escev->strides) {
                if (strideP.first->loop->id != loop.id)
                    continue;
                Expr stride = strideP.second;
                if (stride.kind == Expr::INTEGER) {
                    if (strideImm == 0)
                        strideImm = stride.i;
                    else if (strideImm != stride.i)
                        aligned = false;
                }
            }
        }
    }

    if (aligned && strideImm)
        loop.vectorWordSize = strideImm;
    LOOPLOGLINE("\tfixed stride " << strideImm << " peel distance "
                                  << loop.peelDistance);
}

static bool needBroadcast(Loop &loop, Instruction &instr) { return false; }

bool checkPostIteratorUpdate(Instruction &instr)
{
    Function *parent = instr.block->parentFunction;

    for (auto vi : parent->getCFG().getSSAVarRead(instr)) {
        if (vi->type == JVAR_MEMORY) {
            for (auto pred : vi->pred) {
                // none of them should be PHI nodes
                if (pred->isPHI)
                    return false;
            }
        }
    }
    for (auto vo : parent->getCFG().getSSAVarWrite(instr)) {
        if (vo->type == JVAR_MEMORY) {
            for (auto pred : vo->pred) {
                // none of them should be PHI nodes
                if (pred->isPHI)
                    return false;
            }
        }
    }
    return true;
}
