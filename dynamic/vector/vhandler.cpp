#include "vhandler.h"
#include "utilities.h"
#include "extend.h"
#include "reduce.h"
#include "init.h"
#include "broadcast.h"
#include "VECT_rule_structs.h"
#include <stdio.h>

GHardware myCPU;
#define SSE4_1_FLAG     0x80000
#define SSE4_2_FLAG     0x100000

static int get_my_vector_width()
{
    if (myCPU == AVX_SUPPORT || myCPU == AVX2_SUPPORT) return 32;
    else return 16;
}

static inline void _native_cpuid(int* cpuinfo, int info)
{
    __asm__ __volatile__(
        "xchg %%ebx, %%edi;"
        "cpuid;"
        "xchg %%ebx, %%edi;"
        :"=a" (cpuinfo[0]), "=D" (cpuinfo[1]), "=c" (cpuinfo[2]), "=d" (cpuinfo[3])
        :"0" (info)
    );
}

unsigned long long _xgetbv(unsigned int index)
{
    unsigned int eax, edx;
    __asm__ __volatile__(
        "xgetbv;"
        : "=a" (eax), "=d"(edx)
        : "c" (index)
    );
    return ((unsigned long long)edx << 32) | eax;
}

GHardware
janus_detect_hardware()
{
    bool sseSupportted = false;
    bool sse2Supportted = false;
    bool sse3Supportted = false;
    bool ssse3Supportted = false;
    bool sse4_1Supportted = false;
    bool sse4_2Supportted = false;
    bool sse4aSupportted = false;
    bool sse5Supportted = false;
    bool avxSupportted = false;
    bool avx2Supportted = false;

    int cpuinfo[4];
    _native_cpuid(cpuinfo, 1);

    // Check SSE, SSE2, SSE3, SSSE3, SSE4.1, and SSE4.2 support
    sseSupportted       = cpuinfo[3] & (1 << 25) || false;
    sse2Supportted      = cpuinfo[3] & (1 << 26) || false;
    sse3Supportted      = cpuinfo[2] & (1 << 0)  || false;
    ssse3Supportted     = cpuinfo[2] & (1 << 9)  || false;
    sse4_1Supportted    = cpuinfo[2] & (1 << 19) || false;
    sse4_2Supportted    = cpuinfo[2] & (1 << 20) || false;
    avxSupportted       = cpuinfo[2] & (1 << 28) || false;
    bool osxsaveSupported = cpuinfo[2] & (1 << 27) || false;
    if (osxsaveSupported && avxSupportted)
    {
        // _XCR_XFEATURE_ENABLED_MASK = 0
        unsigned long long xcrFeatureMask = _xgetbv(0);
        avxSupportted = (xcrFeatureMask & 0x6) == 0x6;
    }
    _native_cpuid(cpuinfo, 7);
    avx2Supportted      = cpuinfo[1] & (1 << 5)  || false;

    if (avx2Supportted) {
        printf("Current CPU supports AVX2 instruction\n");
        return AVX2_SUPPORT;
    } else if (avxSupportted) {
        printf("Current CPU supports AVX instruction\n");
        return AVX_SUPPORT;
    } else if (sse4_2Supportted) {
        printf("Current CPU supports sse4.2 instruction\n");
        return SSE_SUPPORT;
    } else {
        printf("Not recognised CPU\n");
        return NOT_RECOGNISED_CPU;
    }
}

void
vector_induction_update_handler(JANUS_CONTEXT)
{
    instr_t *trigger = get_trigger_instruction(bb,rule);

    int vectorSize = get_my_vector_width();
    int originalVectorSize = rule->reg1;

    if (rule->reg0) {
        //register operand
        PRE_INSERT(bb,trigger,INSTR_CREATE_int1(drcontext));
    } else {
#ifdef JANUS_VERBOSE
    instr_disassemble(drcontext, trigger,STDOUT);
    dr_printf("\n");
#endif
        //immediate value update
        for (int i = 0; i < instr_num_srcs(trigger); i++) {
            opnd_t opnd = instr_get_src(trigger, i);
            if (opnd_is_immed(opnd)) {
                int opndimm = opnd_get_immed_int(opnd);
                opndimm = opndimm * (vectorSize / originalVectorSize);
                instr_set_src(trigger, i, OPND_CREATE_INT32(opndimm));
            }
        }
#ifdef JANUS_VERBOSE
    instr_disassemble(drcontext, trigger,STDOUT);
    dr_printf("\n");
#endif
    }
}

void
vector_loop_init(JANUS_CONTEXT)
{
    instr_t *trigger = get_trigger_instruction(bb,rule);
    //PRE_INSERT(bb,trigger,INSTR_CREATE_int1(drcontext));
}

void
vector_loop_finish(JANUS_CONTEXT)
{
    instr_t *trigger = get_trigger_instruction(bb,rule);
    //PRE_INSERT(bb,trigger,INSTR_CREATE_int1(drcontext));
}


void
vector_induction_recover_handler(JANUS_CONTEXT)
{
    instr_t *trigger = get_trigger_instruction(bb,rule);
}

void
vector_loop_peel_handler(JANUS_CONTEXT)
{
    int vectorSize = get_my_vector_width();
    VECT_LOOP_PEEL_rule info = VECT_LOOP_PEEL_rule(*rule);

    // How many pre-iterations are required to align the main loop?
    uint64_t iterations = info.iterCount % (vectorSize / info.vectorWordSize);

    if (iterations != 0) {
        instr_t *trigger = get_trigger_instruction(bb,rule);
        //PRE_INSERT(bb,trigger,INSTR_CREATE_int1(drcontext));
        uint64_t triggerPC = (uint64_t) instr_get_app_pc(trigger);
        uint64_t startPC = info.startOffset + triggerPC;
        instrlist_t *body = get_loop_body(janus_context, trigger, startPC);
        // Remove jump and conditional.
        instr_t *instr1 = instrlist_last(body);
        instr_t *instr2 = instr_get_prev(instr1);
        instrlist_remove(body, instr1);
        instrlist_remove(body, instr2);

        if (info.unusedReg == 0)
            // If no reg available, copy body multiple times.
            insert_list_after(janus_context, body, trigger, iterations);
        else
            // Insert loop before main loop if reg available for induction (after trigger).
            insert_loop_after(janus_context, &info, body, trigger, iterations);
    }
}

// Extend vector register instructions to use all lanes.
void vector_extend_handler(JANUS_CONTEXT)
{
    instr_t *trigger = get_trigger_instruction(bb,rule);
#ifdef JANUS_VERBOSE
    instr_disassemble(drcontext, trigger,STDOUT);
    dr_printf("\n");
#endif
    //PRE_INSERT(bb,trigger,INSTR_CREATE_int1(drcontext));
    int opcode = instr_get_opcode(trigger);
    int vectorSize = get_my_vector_width();
    set_all_opnd_size(trigger, opnd_size_from_bytes(vectorSize));
    opnd_t dst = instr_get_dst(trigger, 0);
    opnd_t src1 = instr_get_src(trigger, 0);
    VECT_CONVERT_rule info = VECT_CONVERT_rule(*rule);
    if (opcode == OP_vaddss || opcode == OP_addss || opcode == OP_vsubss || opcode == OP_subss 
     || opcode == OP_vmulss || opcode == OP_mulss || opcode == OP_vdivss || opcode == OP_divss) {
        vector_extend_handler_arith(janus_context, &info, trigger, opcode, &dst, &src1, 4, vectorSize/4);
    }
    else if (opcode == OP_vaddsd || opcode == OP_addsd || opcode == OP_vsubsd || opcode == OP_subsd
          || opcode == OP_vmulsd || opcode == OP_mulsd || opcode == OP_vdivsd || opcode == OP_divsd) {
        vector_extend_handler_arith(janus_context, &info, trigger, opcode, &dst, &src1, 8, vectorSize/8);
    }
    else if (opcode == OP_vmovss || opcode == OP_movss || opcode == OP_movaps) {
        vector_extend_handler_mov(janus_context, &info, trigger, &dst, &src1, 4, vectorSize/4);
    }
    else if (opcode == OP_vmovsd || opcode == OP_movsd || opcode == OP_movapd) {
        vector_extend_handler_mov(janus_context, &info, trigger, &dst, &src1, 8, vectorSize/8);
    }
    else if (opcode == OP_pxor || opcode == OP_vpxor) {
        vector_extend_handler_pxor(janus_context, trigger, opcode, &dst, &src1, vectorSize);
    }
}

// Broadcast loaded constant to all lanes.
void vector_broadcast_handler(JANUS_CONTEXT)
{
    opnd_t dst, src;
    instr_t *trigger = get_trigger_instruction(bb,rule);
    instr_t *instr;

    dst = instr_get_dst(trigger, 0);
    src = instr_get_src(trigger, 0);
    int opcode = instr_get_opcode(trigger);
#ifdef JANUS_VERBOSE
    instr_disassemble(drcontext, trigger,STDOUT);
    dr_printf("\n");
#endif
    if (rule->ureg0.up == 1) {
        if (myCPU == AVX_SUPPORT || myCPU == AVX2_SUPPORT) {
            opnd_t op = opnd_create_reg(rule->reg1);
            instr = INSTR_CREATE_vbroadcastss(drcontext, op, op);
            set_dest_size(instr, opnd_size_from_bytes(32), 0);
            PRE_INSERT(bb,trigger,instr);

        } else {
            opnd_t op = opnd_create_reg(rule->reg1);
            instr = INSTR_CREATE_shufps(drcontext, op, op, OPND_CREATE_INT8(0));
            set_dest_size(instr, opnd_size_from_bytes(16), 0);
            PRE_INSERT(bb,trigger,instr);
        }
    }
    else if (opcode == OP_movaps) {
        if (myCPU == AVX_SUPPORT || myCPU == AVX2_SUPPORT) {
            instr = INSTR_CREATE_vbroadcastss(drcontext, dst, dst);
            set_dest_size(instr, opnd_size_from_bytes(32), 0);
            POST_INSERT(bb,trigger,instr);
        } else {
            instr = INSTR_CREATE_shufps(drcontext, dst, dst, OPND_CREATE_INT8(0));
            set_dest_size(instr, opnd_size_from_bytes(16), 0);
            POST_INSERT(bb,trigger,instr);
        }
    }
    else if (opcode == OP_vmovss || opcode == OP_movss) {
        vector_broadcast_handler_mov_single(janus_context, trigger, &dst, &src, opcode);
    }
    else if ((opcode == OP_vmovsd || opcode == OP_movsd)) {
        vector_broadcast_handler_mov_double(janus_context, trigger, &dst, &src, opcode);
    }
    else if (opcode == OP_cvtsi2sd || opcode == OP_vcvtsi2sd) {
        vector_broadcast_handler_cvtsi2sd(janus_context, trigger, &dst, &src, opcode);
    } else {
        //PRE_INSERT(bb,trigger,INSTR_CREATE_int1(drcontext));
        dr_printf("broadcast handler: instruction not yet implemented: ");
        instr_disassemble(drcontext, trigger,STDOUT);
        dr_printf("\n");
    }
}

// Unroll loop by multiplying stride by number of lanes.
void vector_loop_unroll_handler(JANUS_CONTEXT)
{
    instr_t *trigger = get_trigger_instruction(bb,rule);
    int vectorSize = (myCPU == AVX_SUPPORT || myCPU == AVX2_SUPPORT) ? 32 : 16;
    VECT_INDUCTION_STRIDE_UPDATE_rule info = VECT_INDUCTION_STRIDE_UPDATE_rule(*rule);
    // If register referenced multiply existing value, else multiply immediate operand.
    if (info.reg) {
        opnd_t reg = opnd_create_reg(info.reg);
        instr_t *instr = INSTR_CREATE_imul_imm(drcontext, reg, reg, OPND_CREATE_INT32(vectorSize / info.vectorWordSize));
        POST_INSERT(bb,trigger,instr);
    }
    else
        for (int i = 0; i < instr_num_srcs(trigger); i++) {
            opnd_t opnd = instr_get_src(trigger, i);
            if (opnd_is_immed(opnd)) {
                int opndimm = opnd_get_immed_int(opnd);
                opndimm = opndimm * (vectorSize / info.vectorWordSize);
                instr_set_src(trigger, i, OPND_CREATE_INT32(opndimm));
            }
        }
}

// Copy a few iterations of the loop in front of the loop to 
// align the number of iterations with the number of lanes.
void vector_loop_unaligned_handler(JANUS_CONTEXT) 
{
    VECT_LOOP_PEEL_rule info = VECT_LOOP_PEEL_rule(*rule);
    int vectorSize = (myCPU == AVX_SUPPORT || myCPU == AVX2_SUPPORT) ? 32 : 16;
    // How many pre-iterations are required to align the main loop?
    uint64_t iterations = info.iterCount % (vectorSize / info.vectorWordSize);
    if (iterations != 0) {
        instr_t *trigger = get_trigger_instruction(bb,rule);
        uint64_t triggerPC = (uint64_t) instr_get_app_pc(trigger);
        uint64_t startPC = info.startOffset + triggerPC;
        instrlist_t *body = get_loop_body(janus_context, trigger, startPC);
        // Remove jump and conditional.
        instr_t *instr1 = instrlist_last(body);
        instr_t *instr2 = instr_get_prev(instr1);
        instrlist_remove(body, instr1);
        instrlist_remove(body, instr2);
        if (info.unusedReg == 0)
            // If no reg available, copy body multiple times.
            insert_list_after(janus_context, body, trigger, iterations);
        else
            // Insert loop before main loop if reg available for induction (after trigger).
            insert_loop_after(janus_context, &info, body, trigger, iterations);
    }
}

// Horizontally reduce vector register lanes.
void vector_reduce_handler(JANUS_CONTEXT)
{
    instr_t *trigger = get_trigger_instruction(bb,rule);
    VECT_REDUCE_AFTER_rule info = VECT_REDUCE_AFTER_rule(*rule);
    opnd_t reductReg = opnd_create_reg(info.reg);
    opnd_t freeVectReg = opnd_create_reg(info.freeVectReg);
    if (info.isMultiply) {
        reduce_multiply(janus_context, &info, trigger, &reductReg, &freeVectReg);
    }
    else {
        reduce_add(janus_context, &info, trigger, &reductReg, &freeVectReg);
    }
}

// Initialise a vector register's extra lanes with 0s or 1s for reduction.
void vector_init_handler(JANUS_CONTEXT) 
{
    instr_t *trigger = get_trigger_instruction(bb,rule);
    VECT_REDUCE_INIT_rule info = VECT_REDUCE_INIT_rule(*rule);
    opnd_t reductReg = opnd_create_reg(info.reg);
    opnd_t freeVectReg = opnd_create_reg(info.freeVectReg);
    opnd_t freeDoubleReg = opnd_create_reg(info.freeReg - DR_REG_RAX + DR_REG_EAX);
    // Instructions execute from last inserted to first inserted.
    if (info.vectorWordSize == 4) {
        vector_init_single(janus_context, &info, trigger, &reductReg, &freeVectReg, &freeDoubleReg);
    }
    else {
        vector_init_double(janus_context, &info, trigger, &reductReg, &freeVectReg, &freeDoubleReg);
    }
}

// Check a register's value before the loop to decide whether to vectorise.
void vector_reg_check_handler(JANUS_CONTEXT)
{
    // Post insert executes from last inserted up.
    instr_t *trigger = get_trigger_instruction(bb,rule);
    uint64_t triggerPC = (uint64_t) instr_get_app_pc(trigger);
    VECT_REG_CHECK_rule info = VECT_REG_CHECK_rule(*rule);
    uint64_t startPC = info.startOffset + triggerPC;
    instrlist_t *body = get_loop_body(janus_context, trigger, startPC);
    instr_t *condInstr = instrlist_last(bb);
    condInstr = instr_get_prev(condInstr);
    opnd_t reg = opnd_create_reg(info.reg);
    // Insert label after non-vectorised loop to jump to if vectorising.
    instr_t *label = INSTR_CREATE_label(drcontext);
    POST_INSERT(bb, trigger, label);
    instr_t *instr = INSTR_CREATE_jmp_short(drcontext, opnd_create_instr(condInstr));
    POST_INSERT(bb, trigger, instr);
    insert_list_after(janus_context, body, trigger);
    // If value exists, vectorise if reg == value, else vectorise if reg > 0.
    if (info.value) {
        instr = INSTR_CREATE_jcc_short(drcontext, OP_je_short, opnd_create_instr(label));
        POST_INSERT(bb, trigger, instr);
        instr = INSTR_CREATE_cmp(drcontext, reg, OPND_CREATE_INT8(info.value));
        POST_INSERT(bb, trigger, instr);
        // Push reg value onto stack to retrieve after vectorisation.
        instr = INSTR_CREATE_push(drcontext, reg);
        POST_INSERT(bb, trigger, instr);
    }
    else {
        instr = INSTR_CREATE_jcc_short(drcontext, OP_jg_short, opnd_create_instr(label));
        POST_INSERT(bb, trigger, instr);
        instr = INSTR_CREATE_cmp(drcontext, reg, OPND_CREATE_INT8(0));
        POST_INSERT(bb, trigger, instr);
    }
}

// Pop value from stack to revert reg to pre-vectorisation value.
void vector_revert_handler(JANUS_CONTEXT)
{
    instr_t *trigger = get_trigger_instruction(bb,rule);
    VECT_REVERT_rule info = VECT_REVERT_rule(*rule);
    opnd_t reg = opnd_create_reg(info.reg);
    instr_t *instr = INSTR_CREATE_pop(drcontext, reg);
    PRE_INSERT(bb,trigger,instr);
}
