#include "extend.h"
#include "utilities.h"

void adjust_displacement_for_extension(VECT_CONVERT_rule *info, opnd_t *opnd, int unrollFactor) {
    if (opnd_is_memory_reference(*opnd)) {
        // If after induction update, displacement that expected original increment must be updated.
        if (info->afterModification) {
            int disp = opnd_get_disp(*opnd);
            disp = disp - ((unrollFactor-1)*(info->stride));
            opnd_set_disp(opnd, disp);
        }
    }
}

void vector_extend_transform_2opnds_avx(JANUS_CONTEXT, instr_t *trigger, int opcode, opnd_t *dst, opnd_t *src) {
    instr_t *instr;
    switch (opcode) {
        case OP_addss:
        case OP_vaddss:
            instr = INSTR_CREATE_vaddps(drcontext, *dst, *dst, *src);
            break;
        case OP_subss:
        case OP_vsubss:
            instr = INSTR_CREATE_vsubps(drcontext, *dst, *dst, *src);
            break;
        case OP_mulss:
        case OP_vmulss:
            instr = INSTR_CREATE_vmulps(drcontext, *dst, *dst, *src);
            break;
        case OP_divss:
        case OP_vdivss:
            instr = INSTR_CREATE_vdivps(drcontext, *dst, *dst, *src);
            break;
        case OP_addsd:
        case OP_vaddsd:
            instr = INSTR_CREATE_vaddpd(drcontext, *dst, *dst, *src);
            break;
        case OP_subsd:
        case OP_vsubsd:
            instr = INSTR_CREATE_vsubpd(drcontext, *dst, *dst, *src);
            break;
        case OP_mulsd:
        case OP_vmulsd:
            instr = INSTR_CREATE_vmulpd(drcontext, *dst, *dst, *src);
            break;
        case OP_divsd:
        case OP_vdivsd:
            instr = INSTR_CREATE_vdivpd(drcontext, *dst, *dst, *src);
            break;
    }
    PRE_INSERT(bb,trigger,instr);
}

void vector_extend_transform_2opnds_sse(JANUS_CONTEXT, instr_t *trigger, int opcode, opnd_t *dst, opnd_t *src) {
    instr_t *instr;
    switch (opcode) {
        case OP_addss:
        case OP_vaddss:
            instr = INSTR_CREATE_addps(drcontext, *dst, *src);
            break;
        case OP_subss:
        case OP_vsubss:
            instr = INSTR_CREATE_subps(drcontext, *dst, *src);
            break;
        case OP_mulss:
        case OP_vmulss:
            instr = INSTR_CREATE_mulps(drcontext, *dst, *src);
            break;
        case OP_divss:
        case OP_vdivss:
            instr = INSTR_CREATE_divps(drcontext, *dst, *src);
            break;
        case OP_addsd:
        case OP_vaddsd:
            instr = INSTR_CREATE_addpd(drcontext, *dst, *src);
            break;
        case OP_subsd:
        case OP_vsubsd:
            instr = INSTR_CREATE_subpd(drcontext, *dst, *src);
            break;
        case OP_mulsd:
        case OP_vmulsd:
            instr = INSTR_CREATE_mulpd(drcontext, *dst, *src);
            break;
        case OP_divsd:
        case OP_vdivsd:
            instr = INSTR_CREATE_divpd(drcontext, *dst, *src);
            break;
    }
    PRE_INSERT(bb,trigger,instr);
}

void vector_extend_transform_2opnds(JANUS_CONTEXT, instr_t *trigger, int opcode, opnd_t *dst, opnd_t *src) {
    if (myCPU == AVX_SUPPORT || myCPU == AVX2_SUPPORT) {
        vector_extend_transform_2opnds_avx(janus_context, trigger, opcode, dst, src);
    }
    else {
        vector_extend_transform_2opnds_sse(janus_context, trigger, opcode, dst, src);
    }
}

void vector_extend_transform_3opnds_avx(JANUS_CONTEXT, instr_t *trigger, int opcode, opnd_t *dst,
        opnd_t *src1, opnd_t *src2) {
    instr_t *instr;
    switch (opcode) {
        // Swap sources to ensure memory operand from non-v instruction is in second position.
        case OP_addss:
            instr = INSTR_CREATE_vaddps(drcontext, *dst, *dst, *src1);
            break;
        case OP_vaddss:
            instr = INSTR_CREATE_vaddps(drcontext, *dst, *src1, *src2);
            break;
        case OP_subss:
            instr = INSTR_CREATE_vsubps(drcontext, *dst, *dst, *src1);
            break;
        case OP_vsubss:
            instr = INSTR_CREATE_vsubps(drcontext, *dst, *src1, *src2);
            break;
        case OP_mulss:
            instr = INSTR_CREATE_vmulps(drcontext, *dst, *dst, *src1);
            break;
        case OP_vmulss:
            instr = INSTR_CREATE_vmulps(drcontext, *dst, *src1, *src2);
            break;
        case OP_divss:
            instr = INSTR_CREATE_vdivps(drcontext, *dst, *dst, *src1);
            break;
        case OP_vdivss:
            instr = INSTR_CREATE_vdivps(drcontext, *dst, *src1, *src2);
            break;
        case OP_addsd:
            instr = INSTR_CREATE_vaddpd(drcontext, *dst, *dst, *src1);
            break;
        case OP_vaddsd:
            instr = INSTR_CREATE_vaddpd(drcontext, *dst, *src1, *src2);
            break;
        case OP_subsd:
            instr = INSTR_CREATE_vsubpd(drcontext, *dst, *dst, *src1);
            break;
        case OP_vsubsd:
            instr = INSTR_CREATE_vsubpd(drcontext, *dst, *src1, *src2);
            break;
        case OP_mulsd:
            instr = INSTR_CREATE_vmulpd(drcontext, *dst, *dst, *src1);
            break;
        case OP_vmulsd:
            instr = INSTR_CREATE_vmulpd(drcontext, *dst, *src1, *src2);
            break;
        case OP_divsd:
            instr = INSTR_CREATE_vdivpd(drcontext, *dst, *dst, *src1);
            break;
        case OP_vdivsd:
            instr = INSTR_CREATE_vdivpd(drcontext, *dst, *src1, *src2);
            break;
    }
    PRE_INSERT(bb,trigger,instr);
}

void vector_extend_transform_3opnds_sse(JANUS_CONTEXT, instr_t *trigger, int opcode, opnd_t *dst,
        opnd_t *src1, opnd_t *src2, opnd_t *spill, int vectorWordSize) {
    instr_t *instr;
    if (opnd_get_reg(*src1) == opnd_get_reg(*dst)) {
        vector_extend_transform_2opnds_sse(janus_context, trigger, opcode, dst, src2);
    }
    else {
        if (opnd_get_reg(*src2) != opnd_get_reg(*dst)) {
            instr = get_movap(drcontext, dst, src1, vectorWordSize);
            PRE_INSERT(bb,trigger,instr);
            vector_extend_transform_2opnds_sse(janus_context, trigger, opcode, dst, src2);
        }
        // else dst == src2.
        else if (opcode == OP_vdivss || opcode == OP_vsubss || opcode == OP_vdivsd || opcode == OP_vsubsd) {
            // Must maintain operand order (assumes spill != src1 otherwise we can't resolve).
            if (opnd_get_reg(*spill) != opnd_get_reg(*src1)) {
                instr = get_movap(drcontext, spill, src1, vectorWordSize);
                PRE_INSERT(bb,trigger,instr);
            }
            vector_extend_transform_2opnds_sse(janus_context, trigger, opcode, spill, src2);
            instr = get_movap(drcontext, dst, spill, vectorWordSize);
            PRE_INSERT(bb,trigger,instr);
        }
        else {
            vector_extend_transform_2opnds_sse(janus_context, trigger, opcode, dst, src1);
        }
    }
}

void vector_extend_transform_3opnds(JANUS_CONTEXT, instr_t *trigger, int opcode, opnd_t *dst, opnd_t *src1, 
        opnd_t *src2, opnd_t *spill, int vectorWordSize) {
    if (myCPU == AVX_SUPPORT || myCPU == AVX2_SUPPORT) {
        vector_extend_transform_3opnds_avx(janus_context, trigger, opcode, dst, src1, src2);
    }
    else {
        vector_extend_transform_3opnds_sse(janus_context, trigger, opcode, dst, src1, src2, spill, vectorWordSize);
    }
}

void vector_extend_handler_arith_unaligned(JANUS_CONTEXT, VECT_CONVERT_rule *info, instr_t *trigger, int opcode, 
        opnd_t *dst, opnd_t *src1, opnd_t *src2, int vectorWordSize, int unrollFactor, opnd_t *spill) {
    instr_t *instr;
    if (info->memOpnd == 2) {
        instr = get_movup(drcontext, spill, src1, vectorWordSize);
        PRE_INSERT(bb,trigger,instr);
        vector_extend_transform_3opnds(janus_context, trigger, opcode >= OP_vmovss ? opcode : opcode + 342, 
            dst, src2, spill, spill, vectorWordSize);
    }
    else {
        instr = get_movup(drcontext, spill, src2, vectorWordSize);
        PRE_INSERT(bb,trigger,instr);
        vector_extend_transform_3opnds(janus_context, trigger, opcode >= OP_vmovss ? opcode : opcode + 342, 
            dst, spill, src1, spill, vectorWordSize);
    }
}

static bool isAligned(VECT_CONVERT_rule *info, int vectorWordSize, int unrollFactor) {
    bool aligned = info->aligned == ALIGNED;
    if (info->aligned == CHECK_REQUIRED) {
        uint64_t unalignedIterations = info->iterCount % unrollFactor;
        aligned = (info->disp + unalignedIterations * info->stride) % (vectorWordSize*unrollFactor) == 0;
    }
    return aligned;
}

INSTR_INLINE
static opnd_t getFreeVectReg(void *drcontext, VECT_CONVERT_rule *info, int vectorSize) {
    opnd_t spill;
    if (vectorSize == 16) {
        spill = opnd_create_reg(info->freeVectReg);
    }
    else {
        spill = opnd_create_reg(info->freeVectReg - DR_REG_XMM0 + DR_REG_YMM0);
    }
    return spill;
}

void vector_extend_handler_arith(JANUS_CONTEXT, VECT_CONVERT_rule *info, instr_t *trigger, int opcode,
        opnd_t *dst, opnd_t *src1, int vectorWordSize, int unrollFactor) {
        
    instr_t *instr;
    opnd_t spill = getFreeVectReg(drcontext, info, vectorWordSize * unrollFactor);
    opnd_t src2 = instr_get_src(trigger, 1);
    adjust_displacement_for_extension(info, dst, unrollFactor);
    adjust_displacement_for_extension(info, src1, unrollFactor);
    adjust_displacement_for_extension(info, &src2, unrollFactor);
    if (isAligned(info, vectorWordSize, unrollFactor)) {
        vector_extend_transform_3opnds(janus_context, trigger, opcode, dst, src1, &src2, &spill, vectorWordSize);
    }
    else {
        vector_extend_handler_arith_unaligned(janus_context, info, trigger, opcode, dst, src1, &src2, vectorWordSize, unrollFactor, &spill);
    }
    instrlist_remove(bb, trigger);
}

void vector_extend_handler_mov(JANUS_CONTEXT, VECT_CONVERT_rule *info, instr_t *trigger, opnd_t *dst, 
        opnd_t *src1, int vectorWordSize, int unrollFactor) {
    instr_t *instr;
    adjust_displacement_for_extension(info, dst, unrollFactor);
    adjust_displacement_for_extension(info, src1, unrollFactor);
    if (isAligned(info, vectorWordSize, unrollFactor)) {
        instr = get_movap(drcontext, dst, src1, vectorWordSize);
    }
    else {
        instr = get_movup(drcontext, dst, src1, vectorWordSize);
    }
    PRE_INSERT(bb,trigger,instr);
    instrlist_remove(bb, trigger);
}

void vector_extend_handler_pxor(JANUS_CONTEXT, instr_t *trigger, int opcode, opnd_t *dst, opnd_t *src1,
        int vectorSize) {
    if (vectorSize == 32 && opcode == OP_pxor) {
        // Extend pxor to cover ymm.
        instr_t *instr = INSTR_CREATE_vpxor(drcontext, *dst, *dst, *src1);
        PRE_INSERT(bb,trigger,instr);
        instrlist_remove(bb, trigger);
    }
}