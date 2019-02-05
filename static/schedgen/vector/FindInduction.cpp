#include "FindInduction.h"
#include <set>

// Return false if cannot vectorise.
static bool
findStride(Loop *loop, MachineInstruction *instr, uint64_t inductReg, uint64_t &stride, uint64_t &strideReg,
    bool &strideFound) {
    for(int i = 0; i < instr->opndCount; i++) {
        if (instr->operands[i].type == OPND_IMM) {
            stride = instr->operands[i].intImm;
            strideFound = true;
        }
        if (instr->operands[i].type == OPND_REG && instr->operands[i].reg != inductReg) {
            if (loop->regWrites.contains(instr->operands[i].reg)) {
                LOOPLOGLINE("loop "<<dec<<currentLoopId<<" induction variable is incremented by a non-constant register.");
                IF_LOOPLOG(instr->print(&fselectLog);)
                return false;
            }
            strideReg = instr->operands[i].reg;
            strideFound = true;
        }
    }
    return true;
}

// Return true if init found.
bool 
findInit(MachineInstruction *instr, int64_t &init) {
    if (instr->isMOV()) {
        for(int i = 0; i < instr->opndCount; i++) {
            if (instr->operands[i].type == OPND_IMM) {
                init = instr->operands[i].intImm;
                return true;
            }
        }
    }
    else if (instr->isXORself()) {
        init = 0;
        return true;
    }
    return false;
}

static bool
checkInductionInstruction(Loop *loop, MachineInstruction *instr, uint64_t inductReg, uint64_t &modifyingPC, int64_t &init,
        uint64_t &stride, uint64_t &strideReg, bool &initFound, bool &strideFound, VECT_RULE *&rule)
{
    if (instr->isADD()) {
        if (rule) {
            LOOPLOGLINE("loop "<<dec<<currentLoopId<<" induction variable has multiple modifying instructions.");
            IF_LOOPLOG(instr->print(&fselectLog);)
            return false;
        }
        for(int i = 0; i < instr->opndCount; i++) {
            if (instr->operands[i].type == OPND_IMM) {
                rule = new VECT_INDUCTION_STRIDE_UPDATE_rule(instr->block, instr->pc, instr->id, 0);
            }
        }
        modifyingPC = instr->pc;
        if (!findStride(loop, instr, inductReg, stride, strideReg, strideFound)) return false;
    }
    else if (!initFound) {
        initFound = findInit(instr, init);
    }
    return true;
}

static void
logFindInductionRejectionVar(bool canHandle, bool initFound, bool strideFound) {

    if (!canHandle)
        return;
    if (!initFound)
        LOOPLOGLINE("loop "<<dec<<currentLoopId<<" couldn't find init value.");
    else if (!strideFound)
        LOOPLOGLINE("loop "<<dec<<currentLoopId<<" couldn't find stride value.");
    else 
        LOOPLOGLINE("loop "<<dec<<currentLoopId<<" stride is an immediate value.");
}

static bool
findInductionVariableVar(Loop *loop, VariableState *induct, bool inductFound, set<uint64_t> &varStrideRegs,
        map<uint32_t, inductRegInfo *> &inductRegs, set<VECT_RULE *> &rules) {

    bool reject = true;
    int64_t init;
    bool canHandle = true, initFound = false, strideFound = false;
    uint64_t modifyingPC;
    uint64_t stride = 0;
    uint64_t strideReg;
    VECT_RULE *rule = NULL;
    for (auto phiNode: induct->phi) {
        if (!phiNode->lastModified) {
            LOOPLOGLINE("loop "<<dec<<currentLoopId<<" induction variable has multiple phi nodes.");
            return false;
        }
        if (!checkInductionInstruction(loop, phiNode->lastModified, induct->getVar().getRegister(), modifyingPC, init, 
                                    stride, strideReg, initFound, strideFound, rule)) {
            canHandle = false;
        }
    }
    if (!canHandle || !initFound || !strideFound || stride != 0) {
        IF_LOOPLOG(logFindInductionRejectionVar(canHandle, initFound, strideFound);)
        return false;
    }
    if (rule)
        rules.insert(rule);
    inductRegs[induct->getVar().getRegister()] = new inductRegInfo(init, modifyingPC);
    varStrideRegs.insert(strideReg);
    return true;
}

static void
logFindInductionRejectionImm(bool initFound, bool strideFound, uint64_t stride) {
    if (!initFound)
        LOOPLOGLINE("loop "<<dec<<currentLoopId<<" couldn't find init value.");
    else if (!strideFound)
        LOOPLOGLINE("loop "<<dec<<currentLoopId<<" couldn't find stride value.");
    else if (stride == 0)
        LOOPLOGLINE("loop "<<dec<<currentLoopId<<" stride is not an immediate value.");
    else
        LOOPLOGLINE("loop "<<dec<<currentLoopId<<" has multiple induction variables with different strides.");
}

static bool
findInductionVariableImm(Loop *loop, VariableState *induct, uint64_t &stride, bool inductFound,
        map<uint32_t, inductRegInfo *> &inductRegs, set<VECT_RULE *> &rules) {

    // Store previous stride value to check for change.
    uint64_t otherStride;
    if (inductFound)
        otherStride = stride;
    bool initFound = false, strideFound = false;
    uint64_t modifyingPC;
    int64_t init;
    uint64_t strideReg;
    VECT_RULE *rule = NULL;
    for (auto phiNode: induct->phi) {
        if (!phiNode->lastModified) {
            LOOPLOGLINE("loop "<<dec<<currentLoopId<<" induction variable has multiple phi nodes.");
            return false;
        }
        if (!checkInductionInstruction(loop, phiNode->lastModified, induct->getVar().getRegister(), modifyingPC, init, 
                stride, strideReg, initFound, strideFound, rule)) return false;
    }
    // Ensure stride is an immediate value (stride != 0) and 
    // that either this is the first induction variable or the stride is the same as the others.
    if (initFound && strideFound && stride != 0 && (!inductFound || otherStride == stride)) {
        if (rule)
            rules.insert(rule);
        inductRegs[induct->getVar().getRegister()] = new inductRegInfo(init, modifyingPC);
    }
    else {
        IF_LOOPLOG(logFindInductionRejectionImm(initFound, strideFound, stride);)
        return false;
    }
    inductFound = true;

    return true;
}

static void
handleVarInductionStride(Loop *loop, uint64_t reg, uint64_t &stride, set<VECT_RULE *> &rules) {
    // Check induction variable has the same stride as the main induction variable.
    insertRegCheckRule(loop, reg, stride, rules);
    // Unroll register stride value before loop.
    for (auto initBid: loop->init) {
        BasicBlock *bb = &(loop->parent->entry[initBid]);
        MachineInstruction *instr = &(bb->instrs[bb->size-1]);
        rules.insert(new VECT_INDUCTION_STRIDE_UPDATE_rule(bb, instr->pc, instr->id, reg));
    }
    // Revert required for VECT_INDUCTION_STRIDE_UPDATE's multiplication of the reg value.
    for (auto bid: loop->exit) {
        BasicBlock *bb = &(loop->parent->entry[bid]);
        rules.insert(new VECT_REVERT_rule(bb, bb->instrs[0].pc, bb->instrs[0].id, reg));
    }
}

bool 
findInductionVariables(Loop *loop, uint64_t &stride, map<uint32_t, inductRegInfo *> &inductRegs, 
        set<VECT_RULE *> &rules) {
    bool inductFound = false;
    set<uint64_t> varStrideRegs;

    for (auto &iter: loop->iterators) {

    }

    for(auto const& value: loop->iterators) {
        if (value.second.kind == janus::Iterator::IteratorKind::INDUCTION_VAR) {
            if (!findInductionVariableVar(loop, value.first, inductFound, varStrideRegs, inductRegs, rules))
                return false;
        }
        else if (value.second.kind == janus::Iterator::IteratorKind::INDUCTION_IMM) {
            if (!findInductionVariableImm(loop, value.first, stride, inductFound, inductRegs, rules))
                return false;
        }
    }
    for (auto reg : varStrideRegs) {
        handleVarInductionStride(loop, reg, stride, rules);
    }

    return true;
}