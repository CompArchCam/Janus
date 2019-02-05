#include "vhandler.h"
#include <stdio.h>

GHardware myCPU;
#define SSE4_1_FLAG     0x80000
#define SSE4_2_FLAG     0x100000


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

static void
set_all_opnd_size(instr_t *trigger, opnd_size_t size)
{
    int i;
    for (i = 0; i < instr_num_srcs(trigger); i++) {
        opnd_t opnd = instr_get_src(trigger, i);
        if (opnd_is_memory_reference(opnd)) {
            opnd_set_size(&opnd, size);
            instr_set_src(trigger, i, opnd);
        } else if (opnd_is_reg(opnd)) {
            reg_id_t reg = opnd_get_reg(opnd);
            if (reg_is_xmm(reg) && size == OPSZ_32) {
                instr_set_src(trigger, i, opnd_create_reg(reg-DR_REG_XMM0 + DR_REG_YMM0));
            }
        }
    }
    for (i = 0; i < instr_num_dsts(trigger); i++) {
        opnd_t opnd = instr_get_dst(trigger, i);
        if (opnd_is_memory_reference(opnd)) {
            opnd_set_size(&opnd, size);
            instr_set_dst(trigger, i, opnd);
        } else if (opnd_is_reg(opnd)) {
            reg_id_t reg = opnd_get_reg(opnd);
            if (reg_is_xmm(reg) && size == OPSZ_32) {
                instr_set_dst(trigger, i, opnd_create_reg(reg-DR_REG_XMM0 + DR_REG_YMM0));
            }
        }
    }
}

void vector_extend_handler(JANUS_CONTEXT)
{
    instr_t *trigger = get_trigger_instruction(bb,rule);
    instr_disassemble(drcontext, trigger, STDOUT);
    dr_printf("\n");
    PRE_INSERT(bb,trigger,INSTR_CREATE_int1(drcontext));
    //one to one mapping
    if (instr_get_opcode(trigger) == OP_movaps) {
        if (myCPU == AVX_SUPPORT) {
            instr_set_opcode(trigger, OP_vmovups);
            set_all_opnd_size(trigger, OPSZ_32);
        }
    }
    if (instr_get_opcode(trigger) == OP_addps) {
        if (myCPU == AVX_SUPPORT) {
            instr_set_opcode(trigger, OP_vaddps);
            set_all_opnd_size(trigger, OPSZ_32);
        }
    }
    instr_disassemble(drcontext, trigger, STDOUT);
    dr_printf("\n");
}

void vector_unroll_handler(JANUS_CONTEXT)
{
    instr_t *trigger = get_trigger_instruction(bb,rule);
    instr_disassemble(drcontext, trigger, STDOUT);
    dr_printf("\n");
    int i;
    for (i = 0; i < instr_num_srcs(trigger); i++) {
        opnd_t opnd = instr_get_src(trigger, i);
        if (opnd_is_immed(opnd)) {
            int opndimm = opnd_get_immed_int(opnd);
            if (myCPU == AVX_SUPPORT) {
                opndimm = opndimm * 2;
                instr_set_src(trigger, i, OPND_CREATE_INT32(opndimm));
            }

        }
    }
    instr_disassemble(drcontext, trigger, STDOUT);
    dr_printf("\n");
}
