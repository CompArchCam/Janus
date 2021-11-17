#include "reduce.h"

void reduce_multiply_single(JANUS_CONTEXT, VECT_REDUCE_AFTER_rule *info, instr_t *trigger, 
        opnd_t *reductReg, opnd_t *freeVectReg) {
    instr_t *instr;
    if (myCPU == AVX_SUPPORT || myCPU == AVX2_SUPPORT) {
        opnd_t bigReductReg = opnd_create_reg(info->reg - DR_REG_XMM0 + DR_REG_YMM0);
        // Copy upper ymm into free xmm.
        instr = INSTR_CREATE_vextractf128(drcontext, *freeVectReg, bigReductReg, OPND_CREATE_INT8(1));
        PRE_INSERT(bb,trigger,instr);
        // Multiply copied upper ymm with lower ymm.
        instr = INSTR_CREATE_vmulps(drcontext, *reductReg, *freeVectReg, *reductReg);
        PRE_INSERT(bb,trigger,instr);
        // First iteration: Multiply upper xmm two lanes by lower xmm two lanes.
        // Second iteration: Multiply lower xmm two lanes together.
        for (int shift = 8; shift >= 4; shift -= 4) {
            instr = INSTR_CREATE_vmovaps(drcontext, *freeVectReg, *reductReg);
            PRE_INSERT(bb,trigger,instr);
            // Shift lanes right.
            instr = INSTR_CREATE_vpsrldq(drcontext, *freeVectReg, OPND_CREATE_INT8(shift), *freeVectReg);
            PRE_INSERT(bb,trigger,instr);
            instr = INSTR_CREATE_vmulps(drcontext, *reductReg, *freeVectReg, *reductReg);
            PRE_INSERT(bb,trigger,instr);
        }
    }
    else {
        // First iteration: Multiply upper two lanes by lower two lanes.
        // Second iteration: Multiply lower two lanes together.
        for (int shift = 8; shift >= 4; shift -= 4) {
            instr = INSTR_CREATE_movaps(drcontext, *freeVectReg, *reductReg);
            PRE_INSERT(bb,trigger,instr);
            // Shift lanes right.
            instr = INSTR_CREATE_psrldq(drcontext, *freeVectReg, OPND_CREATE_INT8(shift));
            PRE_INSERT(bb,trigger,instr);
            instr = INSTR_CREATE_mulps(drcontext, *reductReg, *freeVectReg);
            PRE_INSERT(bb,trigger,instr);
        }
    }
}

void reduce_multiply_double(JANUS_CONTEXT, VECT_REDUCE_AFTER_rule *info, instr_t *trigger,
        opnd_t *reductReg, opnd_t *freeVectReg) {
    instr_t *instr;
    if (myCPU == AVX_SUPPORT || myCPU == AVX2_SUPPORT) {
        opnd_t bigReductReg = opnd_create_reg(info->reg - DR_REG_XMM0 + DR_REG_YMM0);
        // Copy upper ymm into free xmm.
        instr = INSTR_CREATE_vextractf128(drcontext, *freeVectReg, bigReductReg, OPND_CREATE_INT8(1));
        PRE_INSERT(bb,trigger,instr);
        // Multiply copied upper ymm with lower ymm.
        instr = INSTR_CREATE_vmulpd(drcontext, *reductReg, *freeVectReg, *reductReg);
        PRE_INSERT(bb,trigger,instr);
        // Multiply two xmm lanes together.
        instr = INSTR_CREATE_vmovapd(drcontext, *freeVectReg, *reductReg);
        PRE_INSERT(bb,trigger,instr);
        instr = INSTR_CREATE_vpsrldq(drcontext, *freeVectReg, OPND_CREATE_INT8(8), *freeVectReg);
        PRE_INSERT(bb,trigger,instr);
        instr = INSTR_CREATE_vmulpd(drcontext, *reductReg, *freeVectReg, *reductReg);
        PRE_INSERT(bb,trigger,instr);
    }
    else {
        // Multiply two lanes together.
        instr = INSTR_CREATE_movapd(drcontext, *freeVectReg, *reductReg);
        PRE_INSERT(bb,trigger,instr);
        instr = INSTR_CREATE_psrldq(drcontext, *freeVectReg, OPND_CREATE_INT8(8));
        PRE_INSERT(bb,trigger,instr);
        instr = INSTR_CREATE_mulpd(drcontext, *reductReg, *freeVectReg);
        PRE_INSERT(bb,trigger,instr);
    }
}

void reduce_multiply(JANUS_CONTEXT, VECT_REDUCE_AFTER_rule *info, instr_t *trigger, opnd_t *reductReg,
        opnd_t *freeVectReg) {
    if (info->vectorWordSize == 4) {
        reduce_multiply_single(janus_context, info, trigger, reductReg, freeVectReg);
    }
    else {
        reduce_multiply_double(janus_context, info,trigger, reductReg, freeVectReg);
    }
}

void reduce_add(JANUS_CONTEXT, VECT_REDUCE_AFTER_rule *info, instr_t *trigger, 
        opnd_t *reductReg, opnd_t *freeVectReg) {
    instr_t *instr;
    if (info->vectorWordSize == 4) {
        if (myCPU == AVX_SUPPORT || myCPU == AVX2_SUPPORT) {
            opnd_t bigReductReg = opnd_create_reg(info->reg - DR_REG_XMM0 + DR_REG_YMM0);
            // Copy upper ymm into free xmm.
            instr = INSTR_CREATE_vextractf128(drcontext, *freeVectReg, bigReductReg, OPND_CREATE_INT8(1));
            PRE_INSERT(bb,trigger,instr);
            // Add copied upper ymm onto lower ymm.
            instr = INSTR_CREATE_vaddps(drcontext, *reductReg, *freeVectReg, *reductReg);
            PRE_INSERT(bb,trigger,instr);
            instr = INSTR_CREATE_vhaddps(drcontext, *reductReg, *reductReg, *reductReg);
        }
        else {
            instr = INSTR_CREATE_haddps(drcontext, *reductReg, *reductReg);
        }
        // Two horizontal additions sum up all xmm values into the first lane.
        PRE_INSERT(bb,trigger,instr_clone(drcontext, instr));
        PRE_INSERT(bb,trigger,instr);
    }
    else if (info->vectorWordSize == 8) {
        if (myCPU == AVX_SUPPORT || myCPU == AVX2_SUPPORT) {
            opnd_t bigReductReg = opnd_create_reg(info->reg - DR_REG_XMM0 + DR_REG_YMM0);
            // Copy upper ymm into free xmm.
            instr = INSTR_CREATE_vextractf128(drcontext, *freeVectReg, bigReductReg, OPND_CREATE_INT8(1));
            PRE_INSERT(bb,trigger,instr);
            // Add copied upper ymm onto lower ymm.
            instr = INSTR_CREATE_vaddpd(drcontext, *reductReg, *freeVectReg, *reductReg);
            PRE_INSERT(bb,trigger,instr);
            instr = INSTR_CREATE_vhaddpd(drcontext, *reductReg, *reductReg, *reductReg);
        }
        else {
            instr = INSTR_CREATE_haddpd(drcontext, *reductReg, *reductReg);
        }
        // A horizontal addition sums up the two xmm lanes into the first lane.
        PRE_INSERT(bb,trigger,instr);
    }
}