#include "Broadcast.h"
#include "capstone/capstone.h"

static bool isInsideLoop(Loop *loop, MachineInstruction *instr)
{
    for (auto bid : loop->body) {
        BasicBlock *bb = &(loop->parent->entry[bid]);
        if (instr->pc >= bb->instrs[bb->size - 1].getTargetAddress() &&
            instr->pc <= bb->instrs[bb->size - 1].pc) {
            return true;
        }
    }
    return false;
}

static bool shouldBeBroadcastRule(Loop *loop, MachineInstruction *instr,
                                  set<uint32_t> &reductRegs)
{
    if (!(instr->isXMMInstruction()))
        return false;
    if (instr->opcode == X86_INS_CVTSI2SD || instr->opcode == X86_INS_VCVTSI2SD)
        return true;
    // Is reduction instruction?
    for (auto reg : reductRegs) {
        if (instr->writeReg(reg))
            return false;
    }
    if (isInsideLoop(loop, instr)) {
        bool constant = true;
        for (auto input : instr->inputs) {
            if (!loop->isConstant(input))
                return false;
        }
    }
    return true;
}

void insertBroadcastRules(Loop *loop, set<MachineInstruction *> &inputs,
                          set<uint32_t> &reductRegs, set<VECT_RULE *> &rules)
{
    for (MachineInstruction *instr : inputs) {
        if (shouldBeBroadcastRule(loop, instr, reductRegs)) {
            rules.insert(new VECT_RULE(instr->block, instr->pc, instr->id,
                                       VECT_BROADCAST));
        }
    }
}