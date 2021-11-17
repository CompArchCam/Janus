#include "init.h"

// Post insert executes from last inserted up.

void vector_init_single_avx(JANUS_CONTEXT, VECT_REDUCE_INIT_rule *info, instr_t *trigger, opnd_t *reductReg, 
        opnd_t *spillVector, opnd_t *freeDoubleReg) {
    // Restore original initial reduction value from spill into first lane.
    instr_t *instr = INSTR_CREATE_insertps(drcontext, *reductReg, *spillVector, OPND_CREATE_INT8(0));
    POST_INSERT(bb,trigger,instr);
    opnd_t bigReduct = opnd_create_reg(info->reg - DR_REG_XMM0 + DR_REG_YMM0);
    // Copy xmm into ymm.
    instr = INSTR_CREATE_vperm2f128(drcontext, bigReduct, bigReduct, bigReduct, OPND_CREATE_INT8(0));
    POST_INSERT(bb,trigger,instr);
    if (info->init) {
        // Convert currently int values into floats.
        instr = INSTR_CREATE_vcvtdq2ps(drcontext, *reductReg, *reductReg);
        POST_INSERT(bb,trigger,instr);
        // Replicate init value across all lanes in xmm.
        instr = INSTR_CREATE_vshufps(drcontext, *reductReg, *reductReg, *reductReg, OPND_CREATE_INT8(0));
        POST_INSERT(bb,trigger,instr);
        // Insert init value from reg into xmm.
        instr = INSTR_CREATE_vpinsrd(drcontext, *reductReg, *reductReg, *freeDoubleReg, OPND_CREATE_INT8(0));
        POST_INSERT(bb,trigger,instr);
        instr = INSTR_CREATE_mov_imm(drcontext, *freeDoubleReg, OPND_CREATE_INT32(info->init));
        POST_INSERT(bb,trigger,instr);
    }
    else {
        // init == 0 so xoring self works.
        instr = INSTR_CREATE_vpxor(drcontext, *reductReg, *reductReg, *reductReg);
        POST_INSERT(bb,trigger,instr);
    }
    // Store original initial reduction value in spill.
    instr = INSTR_CREATE_vmovss(drcontext, *spillVector, *reductReg);
    POST_INSERT(bb,trigger,instr);
}

void vector_init_single_sse(JANUS_CONTEXT, VECT_REDUCE_INIT_rule *info, instr_t *trigger, opnd_t *reductReg,
        opnd_t *spillVector, opnd_t *freeDoubleReg) {
    // Restore original initial reduction value from spill into first lane.
    instr_t *instr = INSTR_CREATE_insertps(drcontext, *reductReg, *spillVector, OPND_CREATE_INT8(0));
    POST_INSERT(bb,trigger,instr);
    if (info->init) {
        // Convert currently int values into floats.
        instr = INSTR_CREATE_cvtdq2ps(drcontext, *reductReg, *reductReg);
        POST_INSERT(bb,trigger,instr);
        // Replicate init value across all lanes in xmm.
        instr = INSTR_CREATE_shufps(drcontext, *reductReg, *reductReg, OPND_CREATE_INT8(0));
        POST_INSERT(bb,trigger,instr);
        // Insert init value from reg into xmm.
        instr = INSTR_CREATE_pinsrd(drcontext, *reductReg, *freeDoubleReg, OPND_CREATE_INT8(0));
        POST_INSERT(bb,trigger,instr);
        instr = INSTR_CREATE_mov_imm(drcontext, *freeDoubleReg, OPND_CREATE_INT32(info->init));
        POST_INSERT(bb,trigger,instr);
    }
    else {
        // init == 0 so xoring self works.
        instr = INSTR_CREATE_pxor(drcontext, *reductReg, *reductReg);
        POST_INSERT(bb,trigger,instr);
    }
    // Store original initial reduction value in spill.
    instr = INSTR_CREATE_movss(drcontext, *spillVector, *reductReg);
    POST_INSERT(bb,trigger,instr);
}

void vector_init_single(JANUS_CONTEXT, VECT_REDUCE_INIT_rule *info, instr_t *trigger, opnd_t *reductReg, 
        opnd_t *spillVector, opnd_t *freeDoubleReg) {
    if (myCPU == AVX_SUPPORT || myCPU == AVX2_SUPPORT) {
        vector_init_single_avx(janus_context, info, trigger, reductReg, spillVector, freeDoubleReg);
    }
    else {
        vector_init_single_sse(janus_context, info, trigger, reductReg, spillVector, freeDoubleReg);
    }
}

void vector_init_double_avx(JANUS_CONTEXT, VECT_REDUCE_INIT_rule *info, instr_t *trigger, opnd_t *reductReg, 
        opnd_t *spillVector, opnd_t *freeDoubleReg) {
    opnd_t bigReduct = opnd_create_reg(info->reg - DR_REG_XMM0 + DR_REG_YMM0);
    opnd_t bigSpill = opnd_create_reg(info->freeVectReg - DR_REG_XMM0 + DR_REG_YMM0);
    // Copy xmm into ymm.
    instr_t *instr = INSTR_CREATE_vperm2f128(drcontext, bigReduct, bigSpill, bigReduct, OPND_CREATE_INT8(2));
    POST_INSERT(bb,trigger,instr);
    // Convert currently int values into doubles.
    instr = INSTR_CREATE_vcvtdq2pd(drcontext, *spillVector, *spillVector);
    POST_INSERT(bb,trigger,instr);
    for (int i = 0; i < 2; i++) {
        // Insert init value from reg into both xmm lanes.
        instr = INSTR_CREATE_vpinsrd(drcontext, *spillVector, *spillVector, *freeDoubleReg, OPND_CREATE_INT8(i));
        POST_INSERT(bb,trigger,instr);
    }
    // Convert currently int values into doubles.
    instr = INSTR_CREATE_vcvtdq2pd(drcontext, *reductReg, *reductReg);
    POST_INSERT(bb,trigger,instr);
    // Insert init value from reg into upper xmm lane.
    instr = INSTR_CREATE_vpinsrd(drcontext, *reductReg, *reductReg, *freeDoubleReg, OPND_CREATE_INT8(1));
    POST_INSERT(bb,trigger,instr);
    instr = INSTR_CREATE_mov_imm(drcontext, *freeDoubleReg, OPND_CREATE_INT32(info->init));
    POST_INSERT(bb,trigger,instr);
    // Convert currently initial reduction value to int.
    instr = INSTR_CREATE_vcvtdq2pd(drcontext, *reductReg, *reductReg);
    POST_INSERT(bb,trigger,instr);
}

void vector_init_double_sse(JANUS_CONTEXT, VECT_REDUCE_INIT_rule *info, instr_t *trigger, opnd_t *reductReg,
        opnd_t *spillVector, opnd_t *freeDoubleReg) {
    // Restore original initial reduction value from spill into first lane.
    instr_t *instr = INSTR_CREATE_movsd(drcontext, *reductReg, *spillVector);
    POST_INSERT(bb,trigger,instr);
    if (info->init) {
        // Convert currently int values into doubles.
        instr = INSTR_CREATE_cvtdq2pd(drcontext, *reductReg, *reductReg);
        POST_INSERT(bb,trigger,instr);
        for (int i = 0; i < 2; i++) {
            // Insert init value from reg into both xmm lanes.
            instr = INSTR_CREATE_pinsrd(drcontext, *reductReg, *freeDoubleReg, OPND_CREATE_INT8(i));
            POST_INSERT(bb,trigger,instr);
        }
        instr = INSTR_CREATE_mov_imm(drcontext, *freeDoubleReg, OPND_CREATE_INT32(info->init));
        POST_INSERT(bb,trigger,instr);
    }
    else {
        // init == 0 so xoring self works.
        instr = INSTR_CREATE_pxor(drcontext, *reductReg, *reductReg);
        POST_INSERT(bb,trigger,instr);
    }
    // Store original initial reduction value in spill.
    instr = INSTR_CREATE_movsd(drcontext, *spillVector, *reductReg);
    POST_INSERT(bb,trigger,instr);
}

void vector_init_double(JANUS_CONTEXT, VECT_REDUCE_INIT_rule *info, instr_t *trigger, opnd_t *reductReg, 
        opnd_t *spillVector, opnd_t *freeDoubleReg) {
    if (myCPU == AVX_SUPPORT || myCPU == AVX2_SUPPORT) {
        vector_init_double_avx(janus_context, info, trigger, reductReg, spillVector, freeDoubleReg);
    }
    else {
        vector_init_double_sse(janus_context, info, trigger, reductReg, spillVector, freeDoubleReg);
    }
}
