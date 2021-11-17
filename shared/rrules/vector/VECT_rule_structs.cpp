#include "VECT_rule_structs.h"

#ifdef _STATIC_ANALYSIS_COMPILED
#include "BasicBlock.h"
#include "stdio.h"
#endif

using namespace std;
using namespace janus;

VECT_RULE::VECT_RULE(janus::BasicBlock *bb, PCAddress pc, uint32_t ID, RuleOp opcode): bb(bb), pc(pc), ID(ID), opcode(opcode) {}

VECT_RULE::VECT_RULE() {}

    #ifdef _STATIC_ANALYSIS_COMPILED
RewriteRule *VECT_RULE::encode() {
    RewriteRule *res = new RewriteRule(opcode, bb->instrs[0].pc, pc, ID);
    return res;
}
    #endif

//INIT
VECT_REDUCE_INIT_rule::VECT_REDUCE_INIT_rule(janus::BasicBlock *bb, PCAddress pc, uint32_t ID, uint16_t reg, uint16_t init, 
        uint32_t freeReg, uint32_t freeVectReg): 
            VECT_RULE(bb, pc, ID, VECT_REDUCE_INIT), reg(reg), init(init), freeReg(freeReg), freeVectReg(freeVectReg) 
{
    opcode = VECT_REDUCE_INIT;
}

VECT_REDUCE_INIT_rule::VECT_REDUCE_INIT_rule(RRule &rule) {
    pc = rule.pc;
    freeReg = rule.ureg0.down;
    init = (uint16_t) (rule.ureg0.up >> 16);
    reg = (uint16_t) (rule.ureg0.up & 0xffff);
    vectorWordSize = rule.ureg1.down;
    freeVectReg = rule.ureg1.up;
}

    #ifdef _STATIC_ANALYSIS_COMPILED
RewriteRule *VECT_REDUCE_INIT_rule::encode() {
    RewriteRule *res = VECT_RULE::encode();
    res->ureg0.down = freeReg;
    res->ureg0.up = (init << 16) | (reg & 0xffff);
    res->ureg1.down = vectorWordSize;
    res->ureg1.up = freeVectReg;
    return res;
}

void VECT_REDUCE_INIT_rule::updateVectorWordSize(uint32_t _vectorWordSize) {
    vectorWordSize = _vectorWordSize;
}
    #endif

//UNALIGNED
VECT_LOOP_PEEL_rule::VECT_LOOP_PEEL_rule(janus::BasicBlock *bb, PCAddress pc, uint32_t ID, uint32_t startOffset, 
        uint16_t unusedReg, uint16_t freeReg, uint32_t iterCount):
            VECT_RULE(bb, pc, ID, VECT_LOOP_PEEL), startOffset(startOffset), unusedReg(unusedReg), 
            freeReg(freeReg), iterCount(iterCount) 
{
    opcode = VECT_LOOP_PEEL;
}

VECT_LOOP_PEEL_rule::VECT_LOOP_PEEL_rule(RRule &rule) {
    
    pc = rule.pc;
    startOffset = rule.ureg0.down;
    iterCount = rule.ureg0.up;
    unusedReg = (uint16_t) (rule.ureg1.down >> 16);
    freeReg = (uint16_t) (rule.ureg1.down & 0xffff);
    vectorWordSize = rule.ureg1.up;
}

    #ifdef _STATIC_ANALYSIS_COMPILED
RewriteRule *VECT_LOOP_PEEL_rule::encode() {
    RewriteRule *res = VECT_RULE::encode();
    res->ureg0.down = startOffset;
    res->ureg0.up = iterCount;
    res->ureg1.down = (unusedReg << 16) | (freeReg & 0xffff);
    res->ureg1.up = vectorWordSize;
    return res;
}

void VECT_LOOP_PEEL_rule::updateVectorWordSize(uint32_t _vectorWordSize) {
    vectorWordSize = _vectorWordSize;
}
    #endif

//REG_CHECK
VECT_REG_CHECK_rule::VECT_REG_CHECK_rule(janus::BasicBlock *bb, PCAddress pc, uint32_t ID, uint64_t startOffset, 
        uint32_t reg, uint32_t value):
            VECT_RULE(bb, pc, ID, VECT_REG_CHECK), startOffset(startOffset), reg(reg), value(value) 
{
    opcode = VECT_REG_CHECK;
}

VECT_REG_CHECK_rule::VECT_REG_CHECK_rule(RRule &rule) {
    
    pc = rule.pc;
    startOffset = rule.reg0;
    reg = rule.ureg1.down;
    value = rule.ureg1.up;
}

    #ifdef _STATIC_ANALYSIS_COMPILED
RewriteRule *VECT_REG_CHECK_rule::encode() {
    RewriteRule *res = VECT_RULE::encode();
    res->reg0 = startOffset;
    res->ureg1.down = reg;
    res->ureg1.up = value;
    return res;
}
    #endif
    
//EXTEND
VECT_CONVERT_rule::VECT_CONVERT_rule(janus::BasicBlock *bb, PCAddress pc, uint32_t ID, uint32_t disp,
    bool afterModication, uint16_t stride, uint16_t freeVectReg, Aligned aligned, uint16_t memOpnd,
    uint16_t iterCount):
            VECT_RULE(bb, pc, ID, VECT_CONVERT), disp(disp), stride(stride), aligned(aligned), memOpnd(memOpnd),
                iterCount(iterCount), freeVectReg(freeVectReg) , afterModification(afterModication)
{
    opcode = VECT_CONVERT;
}

VECT_CONVERT_rule::VECT_CONVERT_rule(RRule &rule) {
    
    pc = rule.pc;
    disp = rule.ureg0.down;
    afterModification = (bool) (rule.ureg0.up >> 16);
    stride = (uint16_t) (rule.ureg0.up & 0xffff);
    aligned = (Aligned) (rule.ureg1.down >> 16);
    freeVectReg = (uint16_t) (rule.ureg1.down & 0xffff);
    iterCount = (uint16_t) (rule.ureg1.up >> 16);
    memOpnd = (uint16_t) (rule.ureg1.up & 0xffff);
}

    #ifdef _STATIC_ANALYSIS_COMPILED
RewriteRule *VECT_CONVERT_rule::encode() {
    RewriteRule *res = VECT_RULE::encode();
    res->ureg0.down = disp;
    res->ureg0.up = ((afterModification ? 1 : 0) << 16) | (stride & 0xffff);
    res->ureg1.down = (aligned << 16) | (freeVectReg & 0xffff);
    res->ureg1.up = (iterCount << 16) | (memOpnd & 0xffff);
    return res;
}
    #endif

//UNROLL
VECT_INDUCTION_STRIDE_UPDATE_rule::VECT_INDUCTION_STRIDE_UPDATE_rule(janus::BasicBlock *bb, PCAddress pc, uint32_t ID, uint64_t reg):
            VECT_RULE(bb, pc, ID, VECT_INDUCTION_STRIDE_UPDATE), reg(reg) 
{
    opcode = VECT_INDUCTION_STRIDE_UPDATE;
}

VECT_INDUCTION_STRIDE_UPDATE_rule::VECT_INDUCTION_STRIDE_UPDATE_rule(RRule &rule) {
    
    pc = rule.pc;
    vectorWordSize = rule.reg0;
    reg = rule.reg1;
}

    #ifdef _STATIC_ANALYSIS_COMPILED
RewriteRule *VECT_INDUCTION_STRIDE_UPDATE_rule::encode() {
    RewriteRule *res = VECT_RULE::encode();
    res->reg0 = vectorWordSize;
    res->reg1 = reg;
    return res;
}

void VECT_INDUCTION_STRIDE_UPDATE_rule::updateVectorWordSize(uint32_t _vectorWordSize) {
    vectorWordSize = _vectorWordSize;
}
    #endif

//REDUCE
VECT_REDUCE_AFTER_rule::VECT_REDUCE_AFTER_rule(janus::BasicBlock *bb, PCAddress pc, uint32_t ID, uint32_t reg, uint32_t freeVectReg, 
        bool isMultiply):
            VECT_RULE(bb, pc, ID, VECT_REDUCE_AFTER), reg(reg), freeVectReg(freeVectReg), isMultiply(isMultiply) 
{
    opcode = VECT_REDUCE_AFTER;
}

VECT_REDUCE_AFTER_rule::VECT_REDUCE_AFTER_rule(RRule &rule) {
    
    pc = rule.pc;
    freeVectReg = rule.ureg0.down;
    reg = rule.ureg0.up;
    vectorWordSize = rule.ureg1.down;
    isMultiply = (bool) rule.ureg1.up;
}

    #ifdef _STATIC_ANALYSIS_COMPILED
RewriteRule *VECT_REDUCE_AFTER_rule::encode() {
    RewriteRule *res = VECT_RULE::encode();
    res->ureg0.down = freeVectReg;
    res->ureg0.up = reg;
    res->ureg1.down = vectorWordSize;
    res->ureg1.up = isMultiply ? 1 : 0;
    return res;
}

void VECT_REDUCE_AFTER_rule::updateVectorWordSize(uint32_t _vectorWordSize) {
    vectorWordSize = _vectorWordSize;
}
    #endif

//REVERT
VECT_REVERT_rule::VECT_REVERT_rule(janus::BasicBlock *bb, PCAddress pc, uint32_t ID, uint64_t reg):
            VECT_RULE(bb, pc, ID, VECT_REVERT), reg(reg) 
{
    opcode = VECT_REVERT;
}

VECT_REVERT_rule::VECT_REVERT_rule(RRule &rule) {
    
    pc = rule.pc;
    reg = rule.reg0;
}

    #ifdef _STATIC_ANALYSIS_COMPILED
RewriteRule *VECT_REVERT_rule::encode() {
    RewriteRule *res = VECT_RULE::encode();
    res->reg0 = reg;
    return res;
}
    #endif