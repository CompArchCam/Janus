#include "utilities.h"

instr_t *get_movap(void *dc, opnd_t *d, opnd_t *s, int vectorWordSize) {
    if (vectorWordSize == 4) {
        if (myCPU == AVX_SUPPORT || myCPU == AVX2_SUPPORT)
            return INSTR_CREATE_vmovaps(dc, *d, *s);
        else
            return INSTR_CREATE_movaps(dc, *d, *s);
    }
    else {
        if (myCPU == AVX_SUPPORT || myCPU == AVX2_SUPPORT)
            return INSTR_CREATE_vmovapd(dc, *d, *s);
        else
            return INSTR_CREATE_movapd(dc, *d, *s);
    }
}

instr_t *get_movup(void *dc, opnd_t *d, opnd_t *s, int vectorWordSize) {
    if (vectorWordSize == 4) {
        if (myCPU == AVX_SUPPORT || myCPU == AVX2_SUPPORT)
            return INSTR_CREATE_vmovups(dc, *d, *s);
        else
            return INSTR_CREATE_movups(dc, *d, *s);
    }
    else {
        if (myCPU == AVX_SUPPORT || myCPU == AVX2_SUPPORT)
            return INSTR_CREATE_vmovupd(dc, *d, *s);
        else
            return INSTR_CREATE_movupd(dc, *d, *s);
    }
}

void set_source_size(instr_t *instr, opnd_size_t size, int i) {
    opnd_t opnd = instr_get_src(instr, i);
    if (opnd_is_memory_reference(opnd)) {
        opnd_set_size(&opnd, size);
        instr_set_src(instr, i, opnd);
    } else if (opnd_is_reg(opnd)) {
        reg_id_t reg = opnd_get_reg(opnd);
        if (reg_is_ymm(reg)) {
            if (size == OPSZ_16) {
                instr_set_src(instr, i, opnd_create_reg(reg + DR_REG_XMM0 - DR_REG_YMM0));
            }
            else if (size == OPSZ_32) {
                instr_set_src(instr, i, opnd_create_reg(reg));
            }
        }
        else if (reg_is_xmm(reg)) {
            if (size == OPSZ_16) {
                instr_set_src(instr, i, opnd_create_reg(reg));
            }
            else if (size == OPSZ_32) {
                instr_set_src(instr, i, opnd_create_reg(reg - DR_REG_XMM0 + DR_REG_YMM0));
            }
        }
    }
}

void set_dest_size(instr_t *instr, opnd_size_t size, int i) {
    opnd_t opnd = instr_get_dst(instr, i);
    if (opnd_is_memory_reference(opnd)) {
        opnd_set_size(&opnd, size);
        instr_set_dst(instr, i, opnd);
    } else if (opnd_is_reg(opnd)) {
        reg_id_t reg = opnd_get_reg(opnd);
        if (reg_is_ymm(reg)) {
            if (size == OPSZ_16) {
                instr_set_dst(instr, i, opnd_create_reg(reg + DR_REG_XMM0 - DR_REG_YMM0));
            }
            else if (size == OPSZ_32) {
                instr_set_dst(instr, i, opnd_create_reg(reg));
            }
        }
        else if (reg_is_xmm(reg)) {
            if (size == OPSZ_16) {
                instr_set_dst(instr, i, opnd_create_reg(reg));
            }
            else if (size == OPSZ_32) {
                instr_set_dst(instr, i, opnd_create_reg(reg - DR_REG_XMM0 + DR_REG_YMM0));
            }
        }
    }
}

void
set_all_opnd_size(instr_t *instr, opnd_size_t size)
{
    int i;
    for (i = 0; i < instr_num_srcs(instr); i++) {
        set_source_size(instr, size, i);
    }
    for (i = 0; i < instr_num_dsts(instr); i++) {
        set_dest_size(instr, size, i);
    }
}

instrlist_t *get_loop_body(JANUS_CONTEXT, instr_t *trigger, uint64_t startPC) {
    // Assumes body is a single block.
    instrlist_t *body = instrlist_clone(drcontext, bb);
    instr_t *instr2;
    instr_t *instr1 = instrlist_last(body);
    while ((uint64_t) instr_get_app_pc(instr1) != startPC) {
        instr1 = instr_get_prev(instr1);
    }
    instr1 = instr_get_prev(instr1);
    // Remove all instructions above the trigger (start of loop body). 
    while (instr1) {
        instr2 = instr_get_prev(instr1);
        instrlist_remove(body, instr1);
        instr1 = instr2;
    }
    return body;
}

instr_t *sse_to_avx(void *drcontext, instr_t *instr) {
    opnd_t dst = instr_get_dst(instr, 0);
    opnd_t src1 = instr_get_src(instr, 0);
    switch (instr_get_opcode(instr)) {
        case OP_movss:
            return INSTR_CREATE_vmovss(drcontext, dst, src1);
        case OP_addss:
            return INSTR_CREATE_vaddss(drcontext, dst, dst, src1);
        case OP_subss:
            return INSTR_CREATE_vsubss(drcontext, dst, dst, src1);
        case OP_mulss:
            return INSTR_CREATE_vmulss(drcontext, dst, dst, src1);
        case OP_divss:
            return INSTR_CREATE_vdivss(drcontext, dst, dst, src1);
        case OP_movsd:
            return INSTR_CREATE_vmovsd(drcontext, dst, src1);
        case OP_addsd:
            return INSTR_CREATE_vaddsd(drcontext, dst, dst, src1);
        case OP_subsd:
            return INSTR_CREATE_vsubsd(drcontext, dst, dst, src1);
        case OP_mulsd:
            return INSTR_CREATE_vmulsd(drcontext, dst, dst, src1);
        case OP_divsd:
            return INSTR_CREATE_vdivsd(drcontext, dst, dst, src1);
        default:
            return instr;
    }
}

void insert_list_after(JANUS_CONTEXT, instrlist_t *body, instr_t *trigger) {
    instr_t *instrToInsert = instrlist_last(body);
    while (instrToInsert) {
        instr_t *temp = instr_get_prev(instrToInsert);
        instrlist_remove(body, instrToInsert);
        if ((myCPU == AVX_SUPPORT || myCPU == AVX2_SUPPORT) && instr_is_sse_or_sse2(instrToInsert)) {
            instrToInsert = sse_to_avx(drcontext, instrToInsert);
        }
        instr_set_meta_no_translation(instrToInsert);
        POST_INSERT(bb, trigger, instrToInsert);
        instrToInsert = temp;
    }
}


void insert_list_after(JANUS_CONTEXT, instrlist_t *body, instr_t *trigger, int copies) {
    for (; copies > 0; copies--) {
        instrlist_t *toInsert = instrlist_clone(drcontext, body);
        insert_list_after(janus_context, toInsert, trigger);
    }
}

void insert_loop_after(JANUS_CONTEXT, VECT_LOOP_PEEL_rule *info, instrlist_t *body, instr_t *trigger, uint64_t iterations) {
    // Push and pop used as freeReg is not currently trusted to be correct.
    instr_t *instr, *label;
    opnd_t freeReg;
    if (info->freeReg == 0)
        freeReg = opnd_create_reg(info->unusedReg);
    else
        freeReg = opnd_create_reg(info->freeReg);
    if (iterations > 1) {
        label = INSTR_CREATE_label(drcontext);
        // If no free reg must save and restore register for induction.
        if (!info->freeReg) {
            instr = INSTR_CREATE_pop(drcontext, freeReg);
            POST_INSERT(bb, trigger, instr);
        }
        instr = INSTR_CREATE_jcc_short(drcontext, OP_jnz_short, opnd_create_instr(label));
        POST_INSERT(bb, trigger, instr);
        instr = INSTR_CREATE_dec(drcontext, freeReg);
        POST_INSERT(bb, trigger, instr);
    }
    insert_list_after(janus_context, body, trigger);
    if (iterations > 1) {
        POST_INSERT(bb, trigger, label);
        instr = INSTR_CREATE_mov_imm(drcontext, freeReg, OPND_CREATE_INT32(iterations));
        POST_INSERT(bb, trigger, instr);
        // If no free reg must save and restore register for induction.
        if (!info->freeReg) {
            instr = INSTR_CREATE_push(drcontext, freeReg);
            POST_INSERT(bb, trigger, instr);
        }
    }
}
