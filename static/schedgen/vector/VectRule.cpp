#include "VectRule.h"
#include "JanusContext.h"
#include "Iterator.h"
//#include "janus_x86.h"
#include "capstone/capstone.h"
#include "VectLoopSelect.h"
#include "VECT_rule_structs.h"
#include "janus_arch.h"

static bool needBroadcast(Loop &loop, Instruction &instr);

static void
prepareLoopHeader(JanusContext *gc, Loop &loop)
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

    //DOALL Block based parallelisation
    //TODO: intelligent determine the policy
    header.schedule = PARA_DOALL_BLOCK;


}

void
generateVectorRulesForLoop(JanusContext *gc, Loop *loop)
{
    cout<<"loop id "<<dec<<loop->id<<endl;

    BasicBlock *entry = loop->parent->entry;
    /* Get the array of instructions */
    Instruction *instrs = loop->parent->instrs.data();

    RewriteRule rule;
    VECT_RULE *vrule;
    uint32_t id = loop->id;
#ifdef JANUS_X86
    //step 1: insert broadcast rules
    set<uint32_t> copySet;
    loop->registerToCopy.toSTLSet(copySet);
    for (auto reg: copySet) {
        if (jreg_is_simd(reg)) {
            //get the definition
            Variable v((uint32_t)reg);
            VarState *vs = loop->start->alive(v);
            if (vs != NULL && vs->lastModified) {
                Instruction *instr = vs->lastModified;
                VECT_RULE *rule = new VECT_RULE(instr->block, instr->pc, instr->id, VECT_BROADCAST);
                insertRule(0, *rule->encode(), instr->block);
            }
        }
    }

    //step 1: retrieve the alignment information
    bool aligned = alignmentAnalysis(*loop);
    //insert VECT_LOOP_PEEL if not aligned
    if (loop->peelDistance > 0) {
        //insert loop peeling after the loop
        for (auto init: loop->init) {
            BasicBlock *bb = entry + init;
            Instruction *instr = &(bb->instrs[bb->size-1]);
            VECT_LOOP_PEEL_rule *rule = new VECT_LOOP_PEEL_rule(bb, instr->pc, instr->id, (uint32_t)(loop->start->instrs->pc-instr->pc),
                (uint16_t)loop->scratchRegs.regs.reg0, (uint16_t)loop->scratchRegs.regs.reg1, (uint32_t)loop->staticIterCount);
            rule->vectorWordSize = loop->vectorWordSize;
            insertRule(0, *rule->encode(), bb);
        }
    }

    //step 2: examine each loop instructions
    for (auto bid: loop->body) {
        BasicBlock &bb = entry[bid];
        for (int i=0; i<bb.size; i++) {
            Instruction &instr = bb.instrs[i];
            if (instr.isVectorInstruction()) {
                //extend the existing vector instruction
                bool postIteratorUpdate = checkPostIteratorUpdate(instr);
                VECT_CONVERT_rule *rule = new VECT_CONVERT_rule(&bb, instr.pc, instr.id, 0, postIteratorUpdate, (uint16_t)loop->mainIterator->stride, 
                (uint16_t)loop->freeSIMDRegs.getNextLowest(JREG_XMM0), ALIGNED, 0, loop->staticIterCount);
                insertRule(0, *rule->encode(), &bb);
            }
        }
    }

    //update all strides
    for (auto iterEntry: loop->iterators) {
        Iterator &iter = iterEntry.second;

        if (iter.strideKind == Iterator::INTEGER) {
            //insert rewrite rule on the update instruction
            Instruction *instr = iter.getUpdateInstr();
            if (!instr) {
                cerr<<"Induction update instruction not found"<<endl;
            }
            rule = RewriteRule(VECT_INDUCTION_STRIDE_UPDATE, instr->block->instrs->pc, instr->pc, instr->id);
            rule.reg0 = 0;
            rule.reg1 = loop->vectorWordSize;
            insertRule(0,rule,instr->block);
        }
        else if (iter.strideKind == Iterator::SINGLE_VAR) {
            VarState *vs = iter.strideVar;
            if (vs->type == JVAR_REGISTER) {
                uint64_t reg = vs->value;
                //unroll the loop stride before the loop
                for (auto init: loop->init) {
                    BasicBlock *bb = entry + init;
                    rule = RewriteRule(VECT_INDUCTION_STRIDE_UPDATE, bb, POST_INSERT);
                    rule.reg0 = reg;
                    rule.reg1 = loop->vectorWordSize;
                    insertRule(0, rule, bb);
                }
                // Revert required for VECT_INDUCTION_STRIDE_UPDATE's multiplication of the reg value.
                for (auto bid: loop->exit) {
                    rule = RewriteRule(VECT_INDUCTION_STRIDE_RECOVER, entry + bid, PRE_INSERT);
                    rule.reg0 = reg;
                    rule.reg1 = loop->vectorWordSize;
                    insertRule(0, rule, entry + bid);
                }
            }
        } else continue;
    }
#endif
}

void
generateVectorRules(JanusContext *gc)
{
    set<InstOp> supported_opcodes;
    set<InstOp> singles;
    set<InstOp> doubles;
    set<Loop*> selected_loops;

    /* Step 0: set supported opcodes along with which are single precision and double precision */
    setSupportedVectorOpcodes(supported_opcodes, singles, doubles);

    /* Step 1: select loops for vectorisation based on the selected opcode */
    //selectVectorisableLoop(gc, selected_loops, supported_opcodes, singles, doubles);
    selected_loops.insert(&gc->loops[50]);
    selected_loops.insert(&gc->loops[84]);
    //selected_loops.insert(&gc->loops[73]);

    /* Step 2: generate rules for each loop */
    for (auto l: selected_loops) {
        cout <<dec<<l->id<<" ";
        generateVectorRulesForLoop(gc, l);
    }
    cout <<endl;
}

bool alignmentAnalysis(Loop &loop)
{
    BasicBlock *entry = loop.parent->entry;
    bool aligned = true;
    int64_t strideImm = 0;

    for (auto s: loop.memoryAccesses) {
        auto set = s. second;
        for (auto m: set) {
            //the scalar evolution analysis shows the alignment references
            //{start,+,stride}
            cout <<*m->scev<<endl;
            Expr start = m->scev->start;
            Expr stride = m->scev->stride;

            if (stride.kind == Expr::INTEGER) {
                if (strideImm == 0) strideImm = stride.i;
                else if (strideImm != stride.i) aligned = false;
            }

            if (start.kind == Expr::INTEGER) {
                //we have to make sure all the memory accesses are multiple of SIMD lanes
                int64_t base = start.i;
                //for avx it must be divisiable by 16
                int peel = base % 16;
                if (peel != 0) {
                    if (!loop.peelDistance) {
                        loop.peelDistance = peel;
                    }
                    else if (loop.peelDistance != peel) {
                        loop.peelDistance = -1;
                        aligned = false;
                    }
                }
            } else if (start.kind == Expr::EXPANDED) {
                auto result = start.ee->evaluate(&loop);
                if (result.first == Expr::INTEGER) {
                    int64_t base = result.second;
                    //for avx it must be divisiable by 16
                    int peel = base % 16;
                    if (peel != 0) {
                        if (!loop.peelDistance) {
                            loop.peelDistance = peel / strideImm;
                        }
                        else if (loop.peelDistance != (peel / strideImm)) {
                            loop.peelDistance = -1;
                            aligned = false;
                        }
                    }
                } else {
                    cout <<"ex: "<<start<<endl;
                    return false;
                }
            } else {
                cout <<"ex: "<<start<<endl;
                return false;
            }


        }
    }

    if (aligned && strideImm) loop.vectorWordSize = strideImm;
    cout <<"peel distance " << loop.peelDistance <<endl;
}

static bool needBroadcast(Loop &loop, Instruction &instr)
{
    return false;
}

bool checkPostIteratorUpdate(Instruction &instr)
{
    for (auto vi: instr.inputs) {
        if (vi->type == JVAR_MEMORY) {
            for (auto pred: vi->pred) {
                //none of them should be PHI nodes
                if (pred->isPHI) return false;
            }
        }
    }
    return true;
}
