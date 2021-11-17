#include "Extend.h"
#include "FindInduction.h"
#include "VectUtils.h"
#include "capstone/capstone.h"

static int
getVectorWordSize(MachineInstruction *instr, set<InstOp> &singles, set<InstOp> &doubles) {
    if (singles.find(instr->opcode) != singles.end())
        return 4;
    if (doubles.find(instr->opcode) != doubles.end())
        return 8;
    return -1;
}

static void
updateInputs(MachineInstruction *instr, set<MachineInstruction *> &inputs) {
}

static bool
isInductionMemoryAccess(Operand &op, map<uint32_t, inductRegInfo *> &inductRegs) {
    return 
        (op.structure == OPND_DISPMEM && inductRegs.find(op.dispMem.base) != inductRegs.end())
        || (op.structure == OPND_FULLMEM
            &&  (inductRegs.find(op.fullMem.base) != inductRegs.end()
                || inductRegs.find(op.fullMem.index) != inductRegs.end()));
}

static bool
getMemoryInfo(MachineInstruction *instr, uint8_t &reg, uint64_t &stride,
        map<uint32_t, inductRegInfo *> &inductRegs, Operand op, int32_t &disp) {
    // If base register is induction register.
    if (inductRegs.find(op.fullMem.base) != inductRegs.end()) {
        // Are both registers inductions registers.
        if (inductRegs.find(op.fullMem.index) != inductRegs.end()) {
            LOOPLOGLINE("loop "<<dec<<currentLoopId<<" has non-incremental induction memory access.");
            IF_LOOPLOG(instr->print(&fselectLog));
            return false;
        }
        reg = op.fullMem.index;
        disp = inductRegs[op.fullMem.base]->init;
        // If after induction update.
        if (instr->pc > inductRegs[op.fullMem.base]->modifyingPC) {
            disp += stride;
        }
    }
    else {
        if (op.fullMem.scale != 1) {
            LOOPLOGLINE("loop "<<dec<<currentLoopId<<" has scaled induction memory access.");
            IF_LOOPLOG(instr->print(&fselectLog));
            return false;
        }
        reg = op.fullMem.base;
        disp = inductRegs[op.fullMem.index]->init;
        // If after induction update.
        if (instr->pc > inductRegs[op.fullMem.index]->modifyingPC) {
            disp += stride * op.fullMem.scale;
        }
    }
    return true;
}

static bool
findFullMemoryRegInit(Loop *loop, MachineInstruction *instr, uint8_t &reg, uint64_t &stride,
        map<uint32_t, inductRegInfo *> &inductRegs, Operand op, int32_t &disp, 
        set<VECT_RULE *> &rules) {
    for (auto initVar: loop->initVars) {
        if (initVar->getVar().getRegister() == reg) {
            cout << "found input" << endl;
            if (initVar->lastModified) {
                int64_t initOther;
                bool found = findInit(initVar->lastModified, initOther);
                if (found) {
                    if (reg == op.fullMem.index) {
                        disp += (initOther * op.fullMem.scale) + op.fullMem.disp;
                        // If after induction update.
                        if (instr->pc > inductRegs[reg]->modifyingPC) {
                            disp += stride;
                        }
                    }
                    else {
                        disp = initOther + (disp * op.fullMem.scale) + op.fullMem.disp;
                        // If after induction update.
                        if (instr->pc > inductRegs[reg]->modifyingPC) {
                            disp += stride * op.fullMem.scale;
                        }
                    }
                }
                return found;
            }
            else {
                cout << "phi" << endl;
                return false;
            }
        }
    }
    return false;
}

static bool
getFullMemoryAccessInfo(Loop *loop, MachineInstruction *instr, uint64_t &stride,
        map<uint32_t, inductRegInfo *> &inductRegs, Operand op, int32_t &disp, bool &found, 
        set<VECT_RULE *> &rules) {
    uint8_t reg;
    if (!getMemoryInfo(instr, reg, stride, inductRegs, op, disp)) return false;
    cout << "Reg: " << getRegNameX86(reg) << endl;
    found = findFullMemoryRegInit(loop, instr, reg, stride, inductRegs, op, disp, rules);
    if (!found) {
        if (loop->regWrites.contains(reg)) {
            LOOPLOGLINE("loop "<<dec<<currentLoopId<<" has memory access with unknown register value.");
            IF_LOOPLOG(instr->print(&fselectLog));
            return false;
        }
        else {
            // Insert rule to check that register value is positive to ensure no memory dependence.
            insertRegCheckRule(loop, reg, 0, rules);
        }
    }
    return true;
}

static bool getMemoryAccessInfo(Loop *loop, MachineInstruction *instr, Operand op, uint64_t &stride, 
        map<uint32_t, inductRegInfo *> &inductRegs, VECT_CONVERT_rule *rule, set<VECT_RULE *> &rules) {
    Aligned aligned = CHECK_REQUIRED;
    int32_t disp = 0;
    if (op.structure == OPND_DISPMEM) {
        disp = inductRegs[op.dispMem.base]->init + op.dispMem.disp;
        // If after induction update.
        if (instr->pc > inductRegs[op.dispMem.base]->modifyingPC) {
            rule->afterModification = true;
            disp += stride;
        }
    }
    else {
        cout << "full memory access" << endl;
        bool foundInit = false;
        if (!getFullMemoryAccessInfo(loop, instr, stride, inductRegs, op, disp, foundInit, rules)) {
            return false;
        }
        // If not found other registers initial value.
        if (!foundInit) {
            cout << "not aligned" << endl;
            aligned = UNALIGNED;
        }
    }
    rule->disp = (uint32_t) disp;
    rule->aligned = aligned;
    return true;
}

static bool
collectExtendRuleData(Loop *loop, MachineInstruction *instr, uint64_t &stride, 
        map<uint32_t, inductRegInfo *> &inductRegs, VECT_CONVERT_rule *rule, set<VECT_RULE *> &rules) 
{
    bool foundMemAccess = false;
    for(int i = 0; i < instr->opndCount; i++) {
        Operand op = instr->operands[i];
        if (op.type == OPND_MEM) {
            foundMemAccess = true;      
            rule->memOpnd = i+1;   
            if (isInductionMemoryAccess(op, inductRegs)) { 
                if (!getMemoryAccessInfo(loop, instr, op, stride, inductRegs, rule, rules)) return false;
            }
            else {
                cout << "not aligned" << endl;
                rule->aligned = UNALIGNED;
            }
        }
    }
    if (!foundMemAccess) {
        rule->aligned = ALIGNED;
    }
    return true;
}

static bool
insertExtendRule(Loop *loop, BasicBlock *bb, MachineInstruction *instr, 
        uint64_t &stride, set<MachineInstruction *> &inputs, map<uint32_t, inductRegInfo *> &inductRegs, 
        RegSet &freeVectRegs, set<VECT_RULE *> &rules) {
    VECT_CONVERT_rule *rule = new VECT_CONVERT_rule(bb, instr->pc, instr->id, 0, false, stride, 
        freeVectRegs.getNextLowest(GREG_XMM0), CHECK_REQUIRED, 0, loop->staticIterCount);
    // Update inputs for broadcast rules.
    updateInputs(instr, inputs);
    if (!collectExtendRuleData(loop, instr, stride, inductRegs, rule, rules))
        return false;
    rules.insert(rule);
    return true;
}

static bool
isReductionInductionRead(MachineInstruction *instr, set<uint32_t> &reductRegs, 
        map<uint32_t, inductRegInfo *> &inductRegs) {
    for (auto input: instr->inputs) {
        Variable var = input->getVar();
        if (var.isRegisterDependent() 
            &&  (reductRegs.find(var.getRegister()) != reductRegs.end()
                || inductRegs.find(var.getRegister()) != inductRegs.end())
            &&  (inductRegs.find(var.getRegister()) == inductRegs.end() 
                || input->type() != VARSTATE_MEMORY_OPND))
            return true;
    }
    return false;
}

static bool
checkReductionInductionRead(MachineInstruction *instr, set<uint32_t> &reductRegs, 
        map<uint32_t, inductRegInfo *> &inductRegs) {
    bool reductRead = false;
    if (isReductionInductionRead(instr, reductRegs, inductRegs)) {
        for (auto output: instr->outputs) {
            Variable var = output->getVar();
            if (var.isEFLAGS()) {
                continue;
            }
            if (var.field.type != VAR_REG) {
                LOOPLOGLINE("loop "<<dec<<currentLoopId<<" writes reduction/induction variable to non-register type.");
                IF_LOOPLOG(instr->print(&fselectLog));
                return false;
            }
            if (reductRegs.find(var.getRegister()) == reductRegs.end() && inductRegs.find(var.getRegister()) == inductRegs.end()) {
                LOOPLOGLINE("loop "<<dec<<currentLoopId<<" write reduction/induction variable to non-reduction/induction register.");
                IF_LOOPLOG(instr->print(&fselectLog));
                return false;
            }
        }
    }
    return true;
}

static bool
isAcceptedNonVectorInstruction(MachineInstruction *instr, map<uint32_t, inductRegInfo *> &inductRegs,
        set<uint32_t> &reductRegs) {
    if (instr->isCMP() || instr->isType(INSN_CONTROL)) {
        return true;
    }
    for (auto value: inductRegs) {
        auto reg = value.first;
        if (instr->writeReg(reg))
            return true;
    }
    for (auto reg: reductRegs) {
        if (instr->writeReg(reg))
            return true;
    }
    return false;
}

static bool
checkVectorWordSize(MachineInstruction *instr, set<InstOp> &singles, set<InstOp> &doubles, uint32_t &vectorWordSize) {
    int wordSize = getVectorWordSize(instr, singles, doubles);
    if (wordSize > 0) {
        if (!vectorWordSize) {
            vectorWordSize = wordSize;
        }
        else if (vectorWordSize != wordSize) {
            LOOPLOGLINE("loop "<<dec<<currentLoopId<<" contains both single and double precision instructions.");
            return false;
        }
    }
    return true;
}

static bool
extendInstruction(Loop *loop, MachineInstruction *instr, set<InstOp> &singles, set<InstOp> &doubles,
        BasicBlock *bb, set<MachineInstruction *> &inputs, uint64_t &stride, uint32_t &vectorWordSize,
        map<uint32_t, inductRegInfo *> &inductRegs, set<uint32_t> &reductRegs, RegSet &freeVectRegs, 
        set<VECT_RULE *> &rules) {
    if ((instr->opcode == X86_INS_PXOR || instr->opcode == X86_INS_VPXOR)
            && instr->hasMemoryAccess()){
        LOOPLOGLINE("loop "<<dec<<currentLoopId<<" contains pxor instruction with memory access.");
        IF_LOOPLOG(instr->print(&fselectLog));
        return false;
    }
    if (!checkVectorWordSize(instr, singles, doubles, vectorWordSize)) return false;
    if (!checkReductionInductionRead(instr, reductRegs, inductRegs)) return false;
    if (!(instr->opcode == X86_INS_CVTSI2SD || instr->opcode == X86_INS_VCVTSI2SD)) {
        if (!insertExtendRule(loop, bb, instr, stride, inputs, inductRegs, freeVectRegs, rules))
            return false;
    }
    return true;
}

static bool
tryExtendInstruction(Loop *loop, MachineInstruction *instr, set<InstOp> &singles, set<InstOp> &doubles,
        BasicBlock *bb, set<MachineInstruction *> &inputs, uint64_t &stride, uint32_t &vectorWordSize,
        map<uint32_t, inductRegInfo *> &inductRegs, set<uint32_t> &reductRegs, RegSet &freeVectRegs, 
        set<VECT_RULE *> &rules) {
    if (instr->isXMMInstruction()) {
        if (!extendInstruction(loop, instr, singles, doubles, bb, inputs, stride, vectorWordSize, inductRegs, 
            reductRegs, freeVectRegs, rules)) return false;
    }
    else {
        if (!isAcceptedNonVectorInstruction(instr, inductRegs, reductRegs)) {
            LOOPLOGLINE("loop "<<dec<<currentLoopId<<" invalid non-vector instruction found.");
            IF_LOOPLOG(instr->print(&fselectLog));
            return false;
        }
    }
    return true;
}

bool
insertExtendRules(Loop *loop, set<InstOp> &singles, set<InstOp> &doubles, BasicBlock *bb,
    set<MachineInstruction *> &inputs, uint64_t &stride, uint32_t &vectorWordSize,
    map<uint32_t, inductRegInfo *> &inductRegs, set<uint32_t> &reductRegs, RegSet &freeVectRegs, 
    set<VECT_RULE *> &rules)
{
    vectorWordSize = 0;
    for (int i=0; i<bb->size; i++) {
        MachineInstruction *instr = &(bb->instrs[i]);
        if (!tryExtendInstruction(loop, instr, singles, doubles, bb, inputs, stride, vectorWordSize, inductRegs, 
            reductRegs, freeVectRegs, rules)) return false;
    }
    if (!vectorWordSize) {
        LOOPLOGLINE("loop "<<dec<<currentLoopId<<" has no vector instructions.");
        return false;
    }
    if (stride != vectorWordSize) {
        LOOPLOGLINE("loop "<<dec<<currentLoopId<<" has non-unit stride for " << vectorWordSize <<" byte word size.");
        return false;
    }
    cout << "VectorWordSize: " << vectorWordSize << endl;
    return true;
}