#include "extend.h"
#include "utilities.h"

void vector_broadcast_handler_mov_single(JANUS_CONTEXT, instr_t *trigger, opnd_t *dst, opnd_t *src, int opcode) {
    instr_t *instr;
    if (myCPU == AVX_SUPPORT || myCPU == AVX2_SUPPORT) {
        // Broadcast only works for memory accesses.
        if (opnd_is_memory_reference(*src)) {
            instr = INSTR_CREATE_vbroadcastss(drcontext, *dst, *src);
            set_dest_size(instr, opnd_size_from_bytes(32), 0);
            PRE_INSERT(bb,trigger,instr);
            instrlist_remove(bb, trigger);
        }
        else {
            // Copies xmm to ymm.
            instr = INSTR_CREATE_vperm2f128(drcontext, *dst, *dst, *dst, OPND_CREATE_INT8(0));
            set_dest_size(instr, opnd_size_from_bytes(32), 0);
            POST_INSERT(bb,trigger,instr);
            // Replicates value in first lane across all lanes.
            instr = INSTR_CREATE_shufps(drcontext, *dst, *dst, OPND_CREATE_INT8(0));
            set_dest_size(instr, opnd_size_from_bytes(16), 0);
            POST_INSERT(bb,trigger,instr);
        }
    }
    else {
        // Replicates value in first lane across all lanes.
        instr = INSTR_CREATE_shufps(drcontext, *dst, *dst, OPND_CREATE_INT8(0));
        set_dest_size(instr, opnd_size_from_bytes(16), 0);
        POST_INSERT(bb,trigger,instr);
    }
}

void vector_broadcast_handler_mov_double(JANUS_CONTEXT, instr_t *trigger, opnd_t *dst, opnd_t *src, int opcode) {
    instr_t *instr;
    if (myCPU == AVX_SUPPORT || myCPU == AVX2_SUPPORT) {
        // Broadcast only works for memory accesses.
        if (opnd_is_memory_reference(*src)) {
            instr = INSTR_CREATE_vbroadcastsd(drcontext, *dst, *src);
            set_dest_size(instr, opnd_size_from_bytes(32), 0);
            PRE_INSERT(bb,trigger,instr);
            instrlist_remove(bb, trigger);
        }
        else {
            // Copies xmm to ymm.
            instr = INSTR_CREATE_vperm2f128(drcontext, *dst, *dst, *dst, OPND_CREATE_INT8(0));
            set_dest_size(instr, opnd_size_from_bytes(32), 0);
            POST_INSERT(bb,trigger,instr);
            // Duplicates value in first lane across both lanes.
            instr = INSTR_CREATE_movddup(drcontext, *dst, *dst);
            set_dest_size(instr, opnd_size_from_bytes(16), 0);
            POST_INSERT(bb,trigger,instr);
        }
    }
    else {
        // Duplicates value in first lane across both lanes.
        instr = INSTR_CREATE_movddup(drcontext, *dst, *dst);
        set_dest_size(instr, opnd_size_from_bytes(16), 0);
        POST_INSERT(bb,trigger,instr);
    }
}

void vector_broadcast_handler_cvtsi2sd(JANUS_CONTEXT, instr_t *trigger, opnd_t *dst, opnd_t *src, int opcode) {
    instr_t *instr;
    //Post insert executes from last inserted up.
    if (myCPU == AVX_SUPPORT || myCPU == AVX2_SUPPORT) {
        // Copies xmm to ymm.
        instr = INSTR_CREATE_vperm2f128(drcontext, *dst, *dst, *dst, OPND_CREATE_INT8(0));
        set_dest_size(instr, opnd_size_from_bytes(32), 0);
        POST_INSERT(bb,trigger,instr);
        // Duplicates value in first lane across both lanes.
        instr = INSTR_CREATE_vmovddup(drcontext, *dst, *dst);
        set_dest_size(instr, opnd_size_from_bytes(32), 0);
    }
    else {
        // Duplicates value in first lane across both lanes.
        instr = INSTR_CREATE_movddup(drcontext, *dst, *dst);
        set_dest_size(instr, opnd_size_from_bytes(16), 0);
    }
    POST_INSERT(bb,trigger,instr);
}