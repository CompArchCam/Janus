#include "FindReduction.h"
#include "capstone/capstone.h"

static void
insertInitRules(Loop *loop, bool isMultiply, VariableState *phiNode, RegSet &freeRegs, RegSet &freeVectRegs,
        set<VECT_RULE *> &rules) {
    for (auto initBid: loop->init) {
        BasicBlock *bb = &(loop->parent->entry[initBid]);
        MachineInstruction *instr = &(bb->instrs[bb->size-1]);
        rules.insert(new VECT_REDUCE_INIT_rule(bb, instr->pc, instr->id, phiNode->getVar().getRegister(),
            isMultiply ? 1 : 0, freeRegs.getNextLowest(JREG_RAX), freeVectRegs.getNextLowest(JREG_XMM0)));
    }
}

static void
insertReduceRules(Loop *loop, bool isMultiply, VariableState *phiNode, RegSet &freeVectRegs, 
        set<VECT_RULE *> &rules) {
    for (auto bid: loop->exit) {
        BasicBlock *bb = &(loop->parent->entry[bid]);
        rules.insert(new VECT_REDUCE_AFTER_rule(bb, bb->instrs[0].pc, bb->instrs[0].id, 
            phiNode->getVar().getRegister(), freeVectRegs.getNextLowest(JREG_XMM0), isMultiply));
    }
}

static void
foundReductionVar(Loop *loop, VariableState *phiNode, set<uint32_t> &reductRegs,
        RegSet &freeRegs, RegSet &freeVectRegs, set<VECT_RULE *> &rules) {
    bool isMultiply = false;
    for (auto instr: phiNode->dependants) {
        //Assume one dependant.
        if (instr->opcode == X86_INS_MULSS || instr->opcode == X86_INS_MULSD
            || instr->opcode == X86_INS_DIVSS || instr->opcode == X86_INS_DIVSD) {
            isMultiply = 1;
        }
    }
    insertInitRules(loop, isMultiply, phiNode, freeRegs, freeVectRegs, rules);
    insertReduceRules(loop, isMultiply, phiNode, freeVectRegs, rules);
    reductRegs.insert(phiNode->getVar().getRegister());
}

bool
findReduction(Loop *loop, map<uint32_t, inductRegInfo *> &inductRegs, set<uint32_t> &reductRegs,
        RegSet &freeRegs, RegSet &freeVectRegs, set<VECT_RULE *> &rules) {
    for (auto phiNode: loop->start->phiNodes) {
        // Is it a dependency?
        if (!phiNode->notUsed && phiNode->type() == VARSTATE_PHI) {
            // Is it a new reduction variable?
            if (inductRegs.find(phiNode->getVar().getRegister()) == inductRegs.end()
                && reductRegs.find(phiNode->getVar().getRegister()) == reductRegs.end()) {
                 if (freeRegs.size() == 0) {
                    LOOPLOGLINE("loop "<<dec<<currentLoopId<<" has no free registers.");
                    return false;
                }
                foundReductionVar(loop, phiNode, reductRegs, freeRegs, freeVectRegs, rules);
            }
        }
    }
    return true;
}