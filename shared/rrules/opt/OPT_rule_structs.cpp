#include "OPT_rule_structs.h"
#include "BasicBlock.h"
#include "Variable.h"
#include "stdio.h"

using namespace std;
using namespace janus;

//Base class
OPT_HINT::OPT_HINT(BasicBlock *bb, PCAddress pc, uint16_t ID) : pc(pc), ID(ID) {
    blockAddr = bb->instrs[0].pc;
    next = NULL;
}

//PREFETCH
OPT_PREFETCH_rule::OPT_PREFETCH_rule(BasicBlock *bb, PCAddress pc, uint16_t ID, Variable var):
                                                                            OPT_HINT(bb, pc, ID) {
    blockAddr = bb->instrs[0].pc;
    scale = var.field.scale;
    opnd = var.field;
    opcode = OPT_PREFETCH;
}

OPT_PREFETCH_rule::OPT_PREFETCH_rule(RRule &rule) {
    blockAddr = rule.block_address;
    pc = rule.pc;
    ID = rule.ureg0.down;
    scale = rule.ureg0.up;
    reg1 = rule.reg1;
    opcode = OPT_PREFETCH;
}

    #ifdef _STATIC_ANALYSIS_COMPILED
RewriteRule *OPT_PREFETCH_rule::encode() {
    RewriteRule *res = new RewriteRule(OPT_PREFETCH, blockAddr, pc, ID);
    res->ureg0.down = ID;
    res->ureg0.up = scale;
    res->reg1 = reg1;
    return res;
}
    #endif

//MOV
OPT_INSERT_MOV_rule::OPT_INSERT_MOV_rule(janus::BasicBlock *bb, PCAddress pc, uint16_t ID,
                                                                        int32_t dst, int32_t src):
                                                        OPT_HINT(bb, pc, ID), dst(dst), src(src) {
    opcode = OPT_INSERT_MOV;
}

OPT_INSERT_MOV_rule::OPT_INSERT_MOV_rule(RRule &rule):
                                          OPT_HINT(rule.block_address, rule.pc, rule.ureg0.down) {
    reg1 = rule.reg1;
    opcode = OPT_INSERT_MOV;
}

    #ifdef _STATIC_ANALYSIS_COMPILED
RewriteRule *OPT_INSERT_MOV_rule::encode() {
    RewriteRule *res = new RewriteRule(OPT_INSERT_MOV, blockAddr, pc, ID);
    res->ureg0.down = ID;
    res->reg1 = reg1;

    return res;
}
    #endif

//MEMORY MOV
OPT_INSERT_MEM_MOV_rule::OPT_INSERT_MEM_MOV_rule(janus::BasicBlock *bb, PCAddress pc, uint16_t ID,
                                uint32_t dst, janus::Variable var): OPT_HINT(bb, pc, ID), dst(dst) {
    scale = var.field.scale;
    src = var.field;
    opcode = OPT_INSERT_MEM_MOV;
}

OPT_INSERT_MEM_MOV_rule::OPT_INSERT_MEM_MOV_rule(RRule &rule) {
    blockAddr = rule.block_address;
    pc = rule.pc;
    ID = rule.ureg0.down & ((((uint32_t)1) << 16) - 1);
    scale = rule.ureg0.down << 16;
    dst = rule.ureg0.up;
    reg1 = rule.reg1;
    opcode = OPT_INSERT_MEM_MOV;
}

    #ifdef _STATIC_ANALYSIS_COMPILED
RewriteRule *OPT_INSERT_MEM_MOV_rule::encode() {
    RewriteRule *res = new RewriteRule(OPT_INSERT_MEM_MOV, blockAddr, pc, ID);
    res->ureg0.down = ID | (((uint32_t)scale) >> 16);
    res->ureg0.up = dst;
    res->reg1 = reg1;
    return res;
}
    #endif

//ADD
OPT_INSERT_ADD_rule::OPT_INSERT_ADD_rule(janus::BasicBlock *bb, PCAddress pc, uint16_t ID,
                                               bool source_opnd_type, uint32_t dst, uint64_t src):
                    OPT_HINT(bb, pc, ID), source_opnd_type(source_opnd_type), dst(dst), src(src) {
    opcode = OPT_INSERT_ADD;
}

OPT_INSERT_ADD_rule::OPT_INSERT_ADD_rule(RRule &rule) {
    blockAddr = rule.block_address;
    pc = rule.pc;
    ID = rule.ureg0.down & ((1<<16) - 1);
    source_opnd_type = rule.ureg0.down & (1<<16);
    dst = rule.ureg0.up;
    src = rule.reg1;
    opcode = OPT_INSERT_ADD;
}

    #ifdef _STATIC_ANALYSIS_COMPILED
RewriteRule *OPT_INSERT_ADD_rule::encode() {
    RewriteRule *res = new RewriteRule(OPT_INSERT_ADD, blockAddr, pc, ID);
    res->ureg0.down = ID | (((uint32_t)source_opnd_type) << 16);
    res->ureg0.up = dst;
    res->reg1 = src;

    return res;
}
    #endif

//SUB
OPT_INSERT_SUB_rule::OPT_INSERT_SUB_rule(janus::BasicBlock *bb, PCAddress pc, uint16_t ID,
                                               bool source_opnd_type, uint32_t dst, uint64_t src):
                    OPT_HINT(bb, pc, ID), source_opnd_type(source_opnd_type), dst(dst), src(src) {
    opcode = OPT_INSERT_SUB;
}

OPT_INSERT_SUB_rule::OPT_INSERT_SUB_rule(RRule &rule) {
    blockAddr = rule.block_address;
    pc = rule.pc;
    ID = rule.ureg0.down & ((1<<16) - 1);
    source_opnd_type = rule.ureg0.down & (1<<16);
    dst = rule.ureg0.up;
    src = rule.reg1;
    opcode = OPT_INSERT_SUB;
}

    #ifdef _STATIC_ANALYSIS_COMPILED
RewriteRule *OPT_INSERT_SUB_rule::encode() {
    RewriteRule *res = new RewriteRule(OPT_INSERT_SUB, blockAddr, pc, ID);
    res->ureg0.down = ID | (((uint32_t)source_opnd_type) << 16);
    res->ureg0.up = dst;
    res->reg1 = src;

    return res;
}
    #endif


//IMUL
OPT_INSERT_IMUL_rule::OPT_INSERT_IMUL_rule(janus::BasicBlock *bb, PCAddress pc, uint16_t ID,
                            uint32_t dst, int64_t src): OPT_HINT(bb, pc, ID), dst(dst), src(src) {
    opcode = OPT_INSERT_IMUL;
}

OPT_INSERT_IMUL_rule::OPT_INSERT_IMUL_rule(RRule &rule):
                 OPT_HINT(rule.block_address, rule.pc, rule.ureg0.down) {
    dst = rule.ureg0.up;
    reg1 = rule.reg1;
    opcode = OPT_INSERT_IMUL;
}

    #ifdef _STATIC_ANALYSIS_COMPILED
RewriteRule *OPT_INSERT_IMUL_rule::encode() {
    RewriteRule *res = new RewriteRule(OPT_INSERT_IMUL, blockAddr, pc, ID);
    res->ureg0.down = ID;
    res->ureg0.up = dst;
    res->reg1 = reg1;

    return res;
}
    #endif

//RESTORE_REG
OPT_RESTORE_REG_rule::OPT_RESTORE_REG_rule(janus::BasicBlock *bb, PCAddress pc, uint16_t ID,
                      uint32_t reg, int64_t store): OPT_HINT(bb, pc, ID), reg(reg), store(store) {
    opcode = OPT_RESTORE_REG;
}

OPT_RESTORE_REG_rule::OPT_RESTORE_REG_rule(RRule &rule):
                                          OPT_HINT(rule.block_address, rule.pc, rule.ureg0.down) {
    reg = rule.ureg0.up;
    reg1 = rule.reg1;
    opcode = OPT_RESTORE_REG;
}

    #ifdef _STATIC_ANALYSIS_COMPILED
RewriteRule *OPT_RESTORE_REG_rule::encode() {
    RewriteRule *res = new RewriteRule(OPT_RESTORE_REG, blockAddr, pc, ID);
    res->ureg0.down = ID;
    res->ureg0.up = reg;
    res->reg1 = reg1;

    return res;
}
    #endif

//RESTORE_FLAGS
OPT_RESTORE_FLAGS_rule::OPT_RESTORE_FLAGS_rule(janus::BasicBlock *bb, PCAddress pc, uint16_t ID):
                                                                            OPT_HINT(bb, pc, ID) {
    opcode = OPT_RESTORE_FLAGS;
}

OPT_RESTORE_FLAGS_rule::OPT_RESTORE_FLAGS_rule(RRule &rule):
                                          OPT_HINT(rule.block_address, rule.pc, rule.ureg0.down) {
    opcode = OPT_RESTORE_FLAGS;
}

    #ifdef _STATIC_ANALYSIS_COMPILED
RewriteRule *OPT_RESTORE_FLAGS_rule::encode() {
    RewriteRule *res = new RewriteRule(OPT_RESTORE_FLAGS, blockAddr, pc, ID);
    res->ureg0.down = ID;
    return res;
}
    #endif

//CLONE_INSTR
OPT_CLONE_INSTR_rule::OPT_CLONE_INSTR_rule(janus::BasicBlock *bb, PCAddress pc, uint16_t ID,
                                 uint8_t operands[4], bool fixedRegisters, uint64_t cloneAddr):
                      OPT_HINT(bb, pc, ID), fixedRegisters(fixedRegisters), cloneAddr(cloneAddr) {
    for (int i=0; i<4; i++) opnd[i] = operands[i];
    opcode = OPT_CLONE_INSTR;
}

OPT_CLONE_INSTR_rule::OPT_CLONE_INSTR_rule(RRule &rule):
                           OPT_HINT(rule.block_address, rule.pc, rule.ureg0.down & ((1<<16) - 1)),
                                                         reg0_up(rule.ureg0.up), reg1(rule.reg1) {
    opcode = OPT_CLONE_INSTR;
    fixedRegisters = rule.ureg0.down & (1<<16);
}

    #ifdef _STATIC_ANALYSIS_COMPILED
RewriteRule *OPT_CLONE_INSTR_rule::encode() {
    RewriteRule *res = new RewriteRule(OPT_CLONE_INSTR, blockAddr, pc, ID);
    res->ureg0.down = ID | (((int64_t)fixedRegisters) << 16);
    res->ureg0.up = reg0_up;
    res->reg1 = reg1;
    
    return res;
}
    #endif

//SAVE_REG_TO_STACK
OPT_SAVE_REG_TO_STACK_rule::OPT_SAVE_REG_TO_STACK_rule(janus::BasicBlock *bb, PCAddress pc,
    uint16_t ID, uint64_t reg): OPT_HINT(bb, pc, ID), reg(reg) {
    opcode = OPT_SAVE_REG_TO_STACK;
}

OPT_SAVE_REG_TO_STACK_rule::OPT_SAVE_REG_TO_STACK_rule(RRule &rule):
                                    OPT_HINT(rule.block_address, rule.pc, rule.ureg0.down),
                                                                                 reg1(rule.reg1) {
    opcode = OPT_SAVE_REG_TO_STACK;
}

    #ifdef _STATIC_ANALYSIS_COMPILED
RewriteRule *OPT_SAVE_REG_TO_STACK_rule::encode() {
    RewriteRule *res = new RewriteRule(OPT_SAVE_REG_TO_STACK, blockAddr, pc, ID);
    res->ureg0.down = ID;
    res->reg1 = reg1;
    
    return res;
}
    #endif

//RESTORE_REG_FROM_STACK
OPT_RESTORE_REG_FROM_STACK_rule::OPT_RESTORE_REG_FROM_STACK_rule(janus::BasicBlock *bb, PCAddress pc,
    uint16_t ID, uint64_t reg): OPT_HINT(bb, pc, ID), reg(reg) {
    opcode = OPT_RESTORE_REG_FROM_STACK;
}

OPT_RESTORE_REG_FROM_STACK_rule::OPT_RESTORE_REG_FROM_STACK_rule(RRule &rule):
                                    OPT_HINT(rule.block_address, rule.pc, rule.ureg0.down),
                                                                                 reg1(rule.reg1) {
    opcode = OPT_RESTORE_REG_FROM_STACK;
}

    #ifdef _STATIC_ANALYSIS_COMPILED
RewriteRule *OPT_RESTORE_REG_FROM_STACK_rule::encode() {
    RewriteRule *res = new RewriteRule(OPT_RESTORE_REG_FROM_STACK, blockAddr, pc, ID);
    res->ureg0.down = ID;
    res->reg1 = reg1;
    
    return res;
}
    #endif
