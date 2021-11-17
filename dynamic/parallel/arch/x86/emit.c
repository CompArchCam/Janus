#include "emit.h"
#include "jthread.h"
#include "control.h"

/** \brief Generate a code snippet that switches the current stack to a specified stack */
void emit_switch_stack_ptr_aligned(EMIT_CONTEXT)
{
    /* Spill the current stack pointer */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_rel_addr(&(shared->stack_ptr), OPSZ_8),
                            opnd_create_reg(DR_REG_RSP)));

    /* Get the initial alignment */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(s2),
                            opnd_create_reg(DR_REG_RSP)));

    INSERT(bb, trigger,
        INSTR_CREATE_and(drcontext,
                         opnd_create_reg(s2),
                         OPND_CREATE_INT32(0xF)));
    /* Load the local stack pointer */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(DR_REG_RSP),
                            OPND_CREATE_MEM64(TLS, LOCAL_STACK_OFFSET)));

    /* Add alignment */
    INSERT(bb, trigger,
        INSTR_CREATE_add(drcontext,
                         opnd_create_reg(DR_REG_RSP),
                         opnd_create_reg(s2)));

    /* Allocate stack frame */
    INSERT(bb, trigger,
        INSTR_CREATE_sub(drcontext,
                         opnd_create_reg(DR_REG_RSP),
                         OPND_CREATE_INT32(loop->header->stackFrameSize + 0x10)));
}

void emit_restore_stack_ptr_aligned(EMIT_CONTEXT)
{
    /* Spill the current stack pointer */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(DR_REG_RSP),
                            opnd_create_rel_addr(&(shared->stack_ptr), OPSZ_8)));
}


/** \brief Selectively save registers to shared register bank based on the register mask */
void
emit_spill_to_shared_register_bank(EMIT_CONTEXT, uint64_t regMask)
{
    reg_id_t reg;
    int i, cacheline;

    PCAddress shared_addr = (PCAddress)(&(shared->registers));

    //retrieve register mask for GPR and SIMD
    uint32_t gpr_mask = regMask;
    uint32_t simd_mask = gpr_mask >> 16;

    /* load the shared register bank to s0 */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_imm(drcontext,
                             opnd_create_reg(s0),
                             OPND_CREATE_INTPTR(shared_addr)));

    if (gpr_mask) {
        for (reg=DR_REG_RAX; reg<=DR_REG_R15; reg++) {
            i = reg - DR_REG_RAX;

            //only save registers based on the mask
            if (gpr_mask & (1<<i)) {
                if (reg == s0) {
                    //load content of s0 into s2
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_ld(drcontext,
                                            opnd_create_reg(s2),
                                            OPND_CREATE_MEM64(TLS, LOCAL_S0_OFFSET)));
                    //store content of s0 to shared space
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_st(drcontext,
                                            OPND_CREATE_MEM64(s0, i * CACHE_LINE_WIDTH),
                                            opnd_create_reg(s2)));
                }
                else if (reg == s1) {
                    //load content of s1 into s2
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_ld(drcontext,
                                            opnd_create_reg(s2),
                                            OPND_CREATE_MEM64(TLS, LOCAL_S1_OFFSET)));
                    //store content of s1 to shared space
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_st(drcontext,
                                            OPND_CREATE_MEM64(s0, i * CACHE_LINE_WIDTH),
                                            opnd_create_reg(s2)));
                }
                else if (reg == s2) {
                    //load content of s2 into s2
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_ld(drcontext,
                                            opnd_create_reg(s2),
                                            OPND_CREATE_MEM64(TLS, LOCAL_S2_OFFSET)));
                    //store content of s3 to shared space
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_st(drcontext,
                                            OPND_CREATE_MEM64(s0, i * CACHE_LINE_WIDTH),
                                            opnd_create_reg(s2)));
                }
                else if (reg == s3) {
                    //load content of s3 into s2
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_ld(drcontext,
                                            opnd_create_reg(s2),
                                            OPND_CREATE_MEM64(TLS, LOCAL_S3_OFFSET)));
                    //store content of s3 to shared space
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_st(drcontext,
                                            OPND_CREATE_MEM64(s0, i * CACHE_LINE_WIDTH),
                                            opnd_create_reg(s2)));
                }
                else {
                    //for non-scratch (s0, s1) registers
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_st(drcontext,
                                            OPND_CREATE_MEM64(s0, i * CACHE_LINE_WIDTH),
                                            opnd_create_reg(reg)));
                }
            }
        }
    }

    //save simd registers
    if (simd_mask) {
        for (reg=DR_REG_XMM0; reg<=DR_REG_XMM15; reg++) {
            i = reg - DR_REG_XMM0;
            cacheline = i + 16;
            if (simd_mask & (1<<i)) {
                INSERT(bb, trigger,
                    INSTR_CREATE_movdqu(drcontext,
                                        opnd_create_base_disp(s0, DR_REG_NULL, 0,
                                                              cacheline*CACHE_LINE_WIDTH, OPSZ_16),
                                        opnd_create_reg(reg)));
            }
        }
    }

}

/** \brief Selectively restore registers from the shared register bank based on the register mask */
void
emit_restore_from_shared_register_bank(EMIT_CONTEXT, uint64_t regMask)
{
    reg_id_t reg;
    int i, cacheline;

    PCAddress shared_addr = (PCAddress)(&(shared->registers));

    //retrieve register mask for GPR and SIMD
    uint32_t gpr_mask = regMask;
    uint32_t simd_mask = gpr_mask >> 16;

    /* load the shared register bank to s0 */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_imm(drcontext,
                             opnd_create_reg(s0),
                             OPND_CREATE_INTPTR(shared_addr)));

    if (gpr_mask) {
        for (reg=DR_REG_RAX; reg<=DR_REG_R15; reg++) {
            i = reg - DR_REG_RAX;

            //only save registers based on the mask
            if (gpr_mask & (1<<i)) {
                if (reg == s0) {
                    //load the content of s0 to s2
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_ld(drcontext,
                                            opnd_create_reg(s2),
                                            OPND_CREATE_MEM64(s0, i * CACHE_LINE_WIDTH)));
                    //store to the local slot
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_st(drcontext,
                                            OPND_CREATE_MEM64(TLS, LOCAL_S0_OFFSET),
                                            opnd_create_reg(s2)));

                }
                else if (reg == s1) {
                    //load the content of s1 to s2
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_ld(drcontext,
                                            opnd_create_reg(s2),
                                            OPND_CREATE_MEM64(s0, i * CACHE_LINE_WIDTH)));
                    //store to the local slot
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_st(drcontext,
                                            OPND_CREATE_MEM64(TLS, LOCAL_S1_OFFSET),
                                            opnd_create_reg(s2)));
                }
                else if (reg == s2) {
                    //load the content of s2 to s2
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_ld(drcontext,
                                            opnd_create_reg(s2),
                                            OPND_CREATE_MEM64(s0, i * CACHE_LINE_WIDTH)));
                    //store to the local slot
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_st(drcontext,
                                            OPND_CREATE_MEM64(TLS, LOCAL_S2_OFFSET),
                                            opnd_create_reg(s2)));
                }
                else if (reg == s3) {
                    //load the content of s3 to s2
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_ld(drcontext,
                                            opnd_create_reg(s2),
                                            OPND_CREATE_MEM64(s0, i * CACHE_LINE_WIDTH)));
                    //store to the local slot
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_st(drcontext,
                                            OPND_CREATE_MEM64(TLS, LOCAL_S3_OFFSET),
                                            opnd_create_reg(s2)));
                }
                else {
                    //for non-scratch (s0, s1) registers
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_ld(drcontext,
                                            opnd_create_reg(reg),
                                            OPND_CREATE_MEM64(s0, i * CACHE_LINE_WIDTH)));
                }
            }
        }
    }

    //save simd registers
    if (simd_mask) {
        for (reg=DR_REG_XMM0; reg<=DR_REG_XMM15; reg++) {
            i = reg - DR_REG_XMM0;
            cacheline = i + 16;
            if (simd_mask & (1<<i)) {
                INSERT(bb, trigger,
                    INSTR_CREATE_movdqu(drcontext,
                                        opnd_create_reg(reg),
                                        opnd_create_base_disp(s0, DR_REG_NULL, 0,
                                                              cacheline*CACHE_LINE_WIDTH, OPSZ_16)));
            }
        }
    }
}

/** \brief Insert instructions at the trigger to selectively save registers to private register bank based on the register mask */
void
emit_spill_to_private_register_bank(EMIT_CONTEXT, uint64_t regMask, int tid)
{
    reg_id_t reg;

    //retrieve register mask for GPR and SIMD
    uint32_t gpr_mask = regMask;
    uint32_t simd_mask = gpr_mask >> 16;

    if (gpr_mask) {
        for (reg=DR_REG_RAX; reg<=DR_REG_R15; reg++) {
            int i = reg - DR_REG_RAX;
            //only save registers based on the mask
            if (gpr_mask & (1<<i)) {
                if (reg == s0) {
                    //load the content of s0 from TLS
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_ld(drcontext,
                                            opnd_create_reg(s2),
                                            OPND_CREATE_MEM64(TLS, LOCAL_S0_OFFSET)));
                    //store to the private slot
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_st(drcontext,
                                            OPND_CREATE_MEM64(TLS, LOCAL_PSTATE_OFFSET+8*i),
                                            opnd_create_reg(s2)));

                }
                else if (reg == s1) {
                    //load the content of s1 from TLS
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_ld(drcontext,
                                            opnd_create_reg(s2),
                                            OPND_CREATE_MEM64(TLS, LOCAL_S1_OFFSET)));
                    //store to the private slot
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_st(drcontext,
                                            OPND_CREATE_MEM64(TLS, LOCAL_PSTATE_OFFSET+8*i),
                                            opnd_create_reg(s2)));
                }
                else if (reg == s2) {
                    //load the content of s2 from TLS
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_ld(drcontext,
                                            opnd_create_reg(s2),
                                            OPND_CREATE_MEM64(TLS, LOCAL_S2_OFFSET)));
                    //store to the private slot
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_st(drcontext,
                                            OPND_CREATE_MEM64(TLS, LOCAL_PSTATE_OFFSET+8*i),
                                            opnd_create_reg(s2)));
                }
                else if (reg == s3) {
                    //load the content of s3 from TLS
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_ld(drcontext,
                                            opnd_create_reg(s2),
                                            OPND_CREATE_MEM64(TLS, LOCAL_S3_OFFSET)));
                    //store to the private slot
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_st(drcontext,
                                            OPND_CREATE_MEM64(TLS, LOCAL_PSTATE_OFFSET+8*i),
                                            opnd_create_reg(s2)));
                }
                else {
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_st(drcontext,
                                            OPND_CREATE_MEM64(TLS, LOCAL_PSTATE_OFFSET+8*i),
                                            opnd_create_reg(reg)));
                }
            }
        }
    }

    if (simd_mask) {
        for (reg=DR_REG_XMM0; reg<=DR_REG_XMM15; reg++) {
            int i = reg - DR_REG_XMM0;
            if (simd_mask & (1<<i)) {
                INSERT(bb, trigger,
                    INSTR_CREATE_movdqu(drcontext,
                                        opnd_create_base_disp(TLS, 0, 8, LOCAL_PSTATE_OFFSET + PSTATE_SIMD_OFFSET + 16*i, OPSZ_16),
                                        opnd_create_reg(reg)));
            }
        }
    }
}
void restore_scratch_reg_with_s0_conditional(EMIT_CONTEXT, void* source, int scratchOffset){
    //This function is used only in emit_conditional_merge_loop_variables
    //to make its code shorter
    instr_t *jump_over = INSTR_CREATE_label(drcontext);
    INSERT(bb, trigger, 
        INSTR_CREATE_jcc(drcontext, OP_jz,
                            opnd_create_instr(jump_over)));
    //S0 is restored at the end of the loop finish code, so we can use it here
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(s0),
                            opnd_create_rel_addr(source, OPSZ_8)));
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(TLS, scratchOffset),
                            opnd_create_reg(s0)));
    INSERT(bb, trigger, jump_over);

}
/** \brief Insert instructions to dynamically find for each register in registerToConditionalMerge, 
 * which is the last thread that has written to the register, and merge the register from that thread */
void 
emit_conditional_merge_loop_variables(EMIT_CONTEXT, int mytid){

    reg_id_t reg;
    uint32_t reg_mask = loop->header->registerToConditionalMerge;
    instr_t *exit = INSTR_CREATE_label(drcontext);
    //Now s3 contains the register mask that still needs to be merged
    INSERT(bb, trigger,
        INSTR_CREATE_mov_imm(drcontext,
                            opnd_create_reg(s3),
                            opnd_create_immed_int(reg_mask, OPSZ_8)));
    for (int readtid = rsched_info.number_of_threads-1; readtid > 0; readtid--){
        INSERT(bb, trigger, 
            INSTR_CREATE_mov_ld(drcontext, 
                                opnd_create_reg(s2), 
                                opnd_create_rel_addr(&(oracle[readtid]->written_regs_mask), OPSZ_8)));
        //Now we check the intersection of the registers that this thread has written to, and the registers that we still need to merge
        //And store that it s2
        //Then s2 contains the registers that we will merge from this thread
        INSERT(bb, trigger,
            INSTR_CREATE_and(drcontext, opnd_create_reg(s2), opnd_create_reg(s3)));
        
        for (reg=DR_REG_RAX; reg<=DR_REG_R15; reg++) {
            int i = reg - DR_REG_RAX;
            if (reg_mask & (1<<i)) {
                //This means register reg is one of the ones to merge
                //Insert code to check if this thread has written to that register
                //And if so, merge from that thread

                //This instruction should be 64 bit if more registers are merged
                INSERT(bb, trigger,
                    INSTR_CREATE_test(drcontext, opnd_create_reg(reg_resize_to_opsz(s2, OPSZ_4)), opnd_create_immed_int(1<<i, OPSZ_4)));
                if (reg == s0){
                    //S0 is restored at the end of the loop finish code, so we can use it here
                    restore_scratch_reg_with_s0_conditional(emit_context, 
                        (void*)oracle[readtid]+LOCAL_PSTATE_OFFSET+8*i, LOCAL_S0_OFFSET);
                }
                else if (reg == s1){
                    restore_scratch_reg_with_s0_conditional(emit_context, 
                        (void*)oracle[readtid]+LOCAL_PSTATE_OFFSET+8*i, LOCAL_S1_OFFSET);
                }
                else if (reg == s2){
                    restore_scratch_reg_with_s0_conditional(emit_context, 
                        (void*)oracle[readtid]+LOCAL_PSTATE_OFFSET+8*i, LOCAL_S2_OFFSET);
                }
                else if (reg == s3){
                    restore_scratch_reg_with_s0_conditional(emit_context, 
                        (void*)oracle[readtid]+LOCAL_PSTATE_OFFSET+8*i, LOCAL_S3_OFFSET);
                }
                else {
                    INSERT(bb, trigger,
                        INSTR_CREATE_cmovcc(drcontext, OP_cmovnz,
                                            opnd_create_reg(reg),
                                            opnd_create_rel_addr((void*)oracle[readtid]+LOCAL_PSTATE_OFFSET+8*i, OPSZ_8)));
                }
            }
        }

        for (reg=DR_REG_XMM0; reg<=DR_REG_XMM15; reg++) {
            int i = reg - DR_REG_XMM0;
            //The simd_mask is the upper 16 bits of the 32 bit reg mask
            if (reg_mask & (1<<(i+16))) {
                INSERT(bb, trigger,
                    INSTR_CREATE_test(drcontext, opnd_create_reg(reg_resize_to_opsz(s2, OPSZ_4)), opnd_create_immed_int(1<<(i+16), OPSZ_4)));
                instr_t *jump_over = INSTR_CREATE_label(drcontext);
                INSERT(bb, trigger, 
                    INSTR_CREATE_jcc(drcontext, OP_jz,
                                        opnd_create_instr(jump_over)));
                INSERT(bb, trigger,
                    INSTR_CREATE_movdqu(drcontext,
                                        opnd_create_reg(reg),
                                        opnd_create_base_disp(TLS, 0, 8, LOCAL_PSTATE_OFFSET + PSTATE_SIMD_OFFSET + 16*i, OPSZ_16)));
                INSERT(bb, trigger, jump_over);
            }
        }
        //Now since we've merged the registers in s2, we subtract that from s3 (the regs we still need to merge)
        INSERT(bb, trigger, 
            INSTR_CREATE_sub(drcontext, 
                            opnd_create_reg(s3), 
                            opnd_create_reg(s2)));
        //If s3 is zero, that means we have no regs left to merge, so don't need to look at the other threads
        INSERT(bb, trigger, 
            INSTR_CREATE_test(drcontext, opnd_create_reg(s3), opnd_create_reg(s3)));
        INSERT(bb, trigger,
            INSTR_CREATE_jcc(drcontext, OP_jz, opnd_create_instr(exit)));

    }
    INSERT(bb, trigger, exit);

}
/** \brief Insert instructions at the trigger to selectively save registers to private register bank based on the register mask
 * it can load from different thread's bank */
void
emit_restore_from_private_register_bank(EMIT_CONTEXT, uint64_t regMask, int mytid, int readtid)
{
    reg_id_t reg;
    reg_id_t read_tls_reg;

    //retrieve register mask for GPR and SIMD
    uint32_t gpr_mask = regMask;
    uint32_t simd_mask = gpr_mask >> 16;


    if (mytid == readtid) {
        read_tls_reg = TLS;
    } else {
        //load input TLS into s3, if tid different
        read_tls_reg = s3;
        INSERT(bb, trigger,
            INSTR_CREATE_mov_imm(drcontext,
                                 opnd_create_reg(read_tls_reg),
                                 OPND_CREATE_INTPTR(oracle[readtid])));
    }

    if (gpr_mask) {
        for (reg=DR_REG_RAX; reg<=DR_REG_R15; reg++) {
            int i = reg - DR_REG_RAX;
            //only save registers based on the mask
            if (gpr_mask & (1<<i)) {
                if (reg == s0) {
                    //load the content of s0 from TLS
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_ld(drcontext,
                                            opnd_create_reg(s2),
                                            OPND_CREATE_MEM64(read_tls_reg, LOCAL_PSTATE_OFFSET+8*i)));
                    //store to the spill slot
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_st(drcontext,
                                            OPND_CREATE_MEM64(TLS, LOCAL_S0_OFFSET),
                                            opnd_create_reg(s2)));

                }
                else if (reg == s1) {
                    //load the content of s1 from TLS
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_ld(drcontext,
                                            opnd_create_reg(s2),
                                            OPND_CREATE_MEM64(read_tls_reg, LOCAL_PSTATE_OFFSET+8*i)));
                    //store to the spill slot
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_st(drcontext,
                                            OPND_CREATE_MEM64(TLS, LOCAL_S1_OFFSET),
                                            opnd_create_reg(s2)));
                }
                else if (reg == s2) {
                    //load the content of s2 from TLS
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_ld(drcontext,
                                            opnd_create_reg(s2),
                                            OPND_CREATE_MEM64(read_tls_reg, LOCAL_PSTATE_OFFSET+8*i)));
                    //store to the spill slot
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_st(drcontext,
                                            OPND_CREATE_MEM64(TLS, LOCAL_S2_OFFSET),
                                            opnd_create_reg(s2)));
                }
                else if (reg == s3) {
                    //load the content of s3 from TLS
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_ld(drcontext,
                                            opnd_create_reg(s2),
                                            OPND_CREATE_MEM64(read_tls_reg, LOCAL_PSTATE_OFFSET+8*i)));
                    //store to the spill slot
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_st(drcontext,
                                            OPND_CREATE_MEM64(TLS, LOCAL_S3_OFFSET),
                                            opnd_create_reg(s2)));
                }
                else {
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_ld(drcontext,
                                            opnd_create_reg(reg),
                                            OPND_CREATE_MEM64(read_tls_reg, LOCAL_PSTATE_OFFSET+8*i)));
                }
            }
        }
    }

    if (simd_mask) {
        for (reg=DR_REG_XMM0; reg<=DR_REG_XMM15; reg++) {
            int i = reg - DR_REG_XMM0;
            if (simd_mask & (1<<i)) {
                INSERT(bb, trigger,
                    INSTR_CREATE_movdqu(drcontext,
                                        opnd_create_reg(reg),
                                        opnd_create_base_disp(TLS, 0, 8, LOCAL_PSTATE_OFFSET + PSTATE_SIMD_OFFSET + 16*i, OPSZ_16)));
            }
        }
    }
}

void
emit_wait_threads_in_pool(EMIT_CONTEXT)
{
    int i;
    for (i=1; i<rsched_info.number_of_threads; i++) {
        //load the threads TLS
        INSERT(bb, trigger,
            INSTR_CREATE_mov_imm(drcontext,
                                opnd_create_reg(s2),
                                OPND_CREATE_INTPTR(oracle[i])));
        instr_t *label = INSTR_CREATE_label(drcontext);
        INSERT(bb, trigger, label);
        //load in_pool flag
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(s0),
                                OPND_CREATE_MEM64(s2, LOCAL_FLAG_IN_POOL)));
        INSERT(bb, trigger,
            INSTR_CREATE_test(drcontext,
                              opnd_create_reg(s0),
                              opnd_create_reg(s0)));
        //proceed if they are one
        INSERT(bb, trigger,
            INSTR_CREATE_jcc(drcontext, OP_jz, opnd_create_instr(label)));
    }
}

/** \brief Insert instructions at the trigger to make sure all janus threads are finished */
void
emit_wait_threads_finish(EMIT_CONTEXT)
{
    int i;
    instr_t *wait;
    for (i=1; i<rsched_info.number_of_threads; i++) {
        wait = INSTR_CREATE_label(drcontext);
        INSERT(bb, trigger, wait);
        INSERT(bb, trigger,
            INSTR_CREATE_cmp(drcontext,
                             opnd_create_rel_addr((void *)(&(oracle[i]->flag_space.finished)), OPSZ_4),
                             OPND_CREATE_INT32(1)));
        INSERT(bb, trigger,
            INSTR_CREATE_jcc(drcontext, OP_jne, opnd_create_instr(wait)));
    }
}

void
emit_schedule_threads(EMIT_CONTEXT)
{
    /* unset shared.need_yield, this must be unset before setting shared.start_run  */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_rel_addr((void *)&(shared->need_yield), OPSZ_4),
                            OPND_CREATE_INT32(0)));
    /* set shared.start_run = 1 */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_rel_addr((void *)&(shared->start_run), OPSZ_4),
                            OPND_CREATE_INT32(1)));
    /* set local.loop_on flag */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM32(TLS, LOOP_ON_FLAG_OFFSET),
                            OPND_CREATE_INT32(1)));
}

/** \brief Generate instructions to move the content of src variable to the dst variable */
void
emit_move_janus_var(EMIT_CONTEXT, JVar dst, JVar src, reg_id_t scratch)
{
    switch (dst.type) {
        case JVAR_REGISTER: {
            opnd_t src_opnd = create_opnd(src);
            if (src.type == JVAR_REGISTER) {
                //dst_reg <- src_reg
                if (dst.value != src.value) {
                    PRE_INSERT(bb, trigger,
                        INSTR_CREATE_mov_ld(drcontext,
                                            opnd_create_reg(reg_with_same_width(dst.value, src_opnd)),
                                            src_opnd));
                }
            } else if (src.type == JVAR_STACK || src.type == JVAR_MEMORY || src.type == JVAR_ABSOLUTE) {
                //dst_reg <- [mem]
                PRE_INSERT(bb, trigger,
                    INSTR_CREATE_mov_ld(drcontext,
                                        opnd_create_reg(reg_with_same_width(dst.value, src_opnd)),
                                        src_opnd));
            } else if (src.type == JVAR_CONSTANT) {
                //dst_reg <- imm
                PRE_INSERT(bb, trigger,
                    INSTR_CREATE_mov_imm(drcontext,
                                         opnd_create_reg(reg_with_same_width(dst.value, src_opnd)),
                                         src_opnd));
            }
            break;
        }
        case JVAR_STACK:
        case JVAR_MEMORY:
        case JVAR_ABSOLUTE: {
            opnd_t dst_opnd = create_opnd(dst);
            opnd_t src_opnd = create_opnd(src);

            if (src.type == JVAR_REGISTER) {
                //dst_mem <- src_reg
                PRE_INSERT(bb, trigger,
                    INSTR_CREATE_mov_st(drcontext,
                                        dst_opnd,
                                        opnd_create_reg(reg_with_same_width(src.value, dst_opnd))));
            } else if (src.type == JVAR_STACK || src.type == JVAR_MEMORY || src.type == JVAR_ABSOLUTE) {
                PRE_INSERT(bb, trigger,
                    INSTR_CREATE_mov_ld(drcontext,
                                        opnd_create_reg(reg_with_same_width(scratch, src_opnd)),
                                        src_opnd));
                PRE_INSERT(bb, trigger,
                    INSTR_CREATE_mov_st(drcontext,
                                        dst_opnd,
                                        opnd_create_reg(reg_with_same_width(scratch, dst_opnd))));
            } else if (src.type == JVAR_CONSTANT) {
                //dst_reg <- imm
                PRE_INSERT(bb, trigger,
                    INSTR_CREATE_mov_imm(drcontext,
                                         dst_opnd,
                                         src_opnd));
            }
            break;
        }
        default: {
            dr_printf("Error: Destination Janus variable not writeable");
            exit(-1);
        }
    }
}

/** \brief Generate instructions to add the content of src variable to the dst variable
 *
 * It uses an additional scratch register if both src and dst are memory operands */
void
emit_add_janus_var(EMIT_CONTEXT, JVar dst, JVar src, reg_id_t scratch, reg_id_t scratch2)
{
    switch (dst.type) {
        case JVAR_REGISTER: {
            opnd_t dst_opnd = create_opnd(dst);
            opnd_t src_opnd = create_opnd(src);
            PRE_INSERT(bb, trigger,
                INSTR_CREATE_add(drcontext,
                                 dst_opnd,
                                 src_opnd));
            break;
        }
        case JVAR_STACK:
        case JVAR_MEMORY:
        case JVAR_ABSOLUTE: {
            opnd_t dst_opnd = create_opnd(dst);
            opnd_t src_opnd = create_opnd(src);

            if (src.type == JVAR_REGISTER) {
                //dst_mem <- src_reg
                PRE_INSERT(bb, trigger,
                    INSTR_CREATE_add(drcontext,
                                     dst_opnd,
                                     opnd_create_reg(reg_with_same_width(src.value, dst_opnd))));
            } else if (src.type == JVAR_STACK || src.type == JVAR_MEMORY || src.type == JVAR_ABSOLUTE) {
                PRE_INSERT(bb, trigger,
                    INSTR_CREATE_mov_ld(drcontext,
                                        opnd_create_reg(reg_with_same_width(scratch, src_opnd)),
                                        src_opnd));
                PRE_INSERT(bb, trigger,
                    INSTR_CREATE_add(drcontext,
                                     dst_opnd,
                                     opnd_create_reg(reg_with_same_width(scratch, dst_opnd))));
            } else if (src.type == JVAR_CONSTANT) {
                //dst_reg <- imm
                PRE_INSERT(bb, trigger,
                    INSTR_CREATE_add(drcontext,
                                     dst_opnd,
                                     src_opnd));
            }
            break;
        }
        default: {
            DR_ASSERT("Destination Janus variable not writeable");
        }
    }
}

#ifdef NOT_YET_WORKING_FOR_ALL
void
emit_save_shared_context_instrlist(EMIT_CONTEXT, bool save_all, bool save_flag)
{
    PCAddress shared_addr = (PCAddress)(&(shared->registers));
    uint32_t gpr_mask = loop->header->registerToCopy;
    uint32_t simd_mask = gpr_mask >> 16;

    int cyclic = loop->type & LOOP_DOCYCLIC;
    reg_id_t tls_reg = sregs.s3;
    reg_id_t thread_reg = sregs.s0;
    reg_id_t reg;

    /* S0 is reserved for the base for saving */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_imm(drcontext,
                             opnd_create_reg(sregs.s0),
                             OPND_CREATE_INTPTR(shared_addr)));

    if (gpr_mask) {
        for (reg=DR_REG_RAX; reg<=DR_REG_R15; reg++) {
            int i = reg - DR_REG_RAX;
            int offset = -1;
            if (reg == sregs.s0) offset = 0;
            else if (reg == sregs.s1) offset = 1;
            else if (reg == sregs.s2) offset = 2;
            else if (reg == sregs.s3) offset = 3;
            if (gpr_mask & (1<<i) && offset == -1) {
                INSERT(bb, trigger,
                    INSTR_CREATE_mov_st(drcontext,
                                        OPND_CREATE_MEM64(sregs.s0, i * CACHE_LINE_WIDTH),
                                        opnd_create_reg(reg)));
            }
        }
    }

    //always saves scratch
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s1),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_STOLEN_OFFSET)));
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(sregs.s0, (tls_reg-DR_REG_RAX) * CACHE_LINE_WIDTH),
                            opnd_create_reg(sregs.s1)));

    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s1),
                            OPND_CREATE_MEM64(tls_reg, 0)));
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(sregs.s0, (sregs.s0-DR_REG_RAX) * CACHE_LINE_WIDTH),
                            opnd_create_reg(sregs.s1)));

    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s1),
                            OPND_CREATE_MEM64(tls_reg, 8)));
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(sregs.s0, (sregs.s1-DR_REG_RAX) * CACHE_LINE_WIDTH),
                            opnd_create_reg(sregs.s1)));

    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s1),
                            OPND_CREATE_MEM64(tls_reg, 16)));
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(sregs.s0, (sregs.s2-DR_REG_RAX) * CACHE_LINE_WIDTH),
                            opnd_create_reg(sregs.s1)));
    if (simd_mask) {
        for (reg=DR_REG_XMM0; reg<=DR_REG_XMM15; reg++) {
            int i = reg - DR_REG_XMM0;
            int cacheline = i + 16;
            if (simd_mask & (1<<i)) {
                INSERT(bb, trigger,
                    INSTR_CREATE_movdqu(drcontext,
                                        opnd_create_base_disp(sregs.s0, DR_REG_NULL, 0,
                                                              cacheline*CACHE_LINE_WIDTH, OPSZ_16),
                                        opnd_create_reg(reg)));
            }
        }
    }
}

void
emit_save_private_context_instrlist(EMIT_CONTEXT)
{
    uint32_t gpr_mask = loop->header->dependingGPRMask;
    uint32_t simd_mask = loop->header->dependingSIMDMask;
    int use_scratch = loop->header->spilledScratch;
    reg_id_t reg;
    reg_id_t tls_reg = sregs.s3;

    if (gpr_mask) {
        for (reg=DR_REG_RAX; reg<=DR_REG_R15; reg++) {
            int i = reg - DR_REG_RAX;
            if (use_scratch) {
                int offset = -1;
                if (reg == sregs.s0) offset = 0;
                else if (reg == sregs.s1) offset = 1;
                else if (reg == sregs.s2) offset = 2;
                else if (reg == sregs.s3) offset = 3;

                /* Scratch register polluted, so load from bufferred space */
                if (offset != -1 && gpr_mask & (1<<i)) {
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_ld(drcontext,
                                            opnd_create_reg(sregs.s0),
                                            OPND_CREATE_MEM64(tls_reg, offset * 8)));
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_st(drcontext,
                                            OPND_CREATE_MEM64(tls_reg, LOCAL_STATE_OFFSET + 8*i),
                                            opnd_create_reg(sregs.s0)));
                    continue;
                }
            }
            if (gpr_mask & (1<<i)) {
                INSERT(bb, trigger,
                    INSTR_CREATE_mov_st(drcontext,
                                        OPND_CREATE_MEM64(tls_reg, LOCAL_STATE_OFFSET + 8*i),
                                        opnd_create_reg(reg)));
            }
        }
    }

    if (simd_mask) {
        for (reg=DR_REG_XMM0; reg<=DR_REG_XMM15; reg++) {
            int i = reg - DR_REG_XMM0;
            if (simd_mask & (1<<i)) {
                INSERT(bb, trigger,
                    INSTR_CREATE_movdqu(drcontext,
                                        opnd_create_base_disp(tls_reg, 0, 8, LOCAL_STATE_OFFSET + PSTATE_SIMD_OFFSET + 16*i, OPSZ_16),
                                        opnd_create_reg(reg)));
            }
        }
    }
}

void
emit_save_checkpoint_context_instrlist(EMIT_CONTEXT)
{
    uint32_t gpr_mask = loop->header->dependingGPRMask;
    uint32_t simd_mask = loop->header->dependingSIMDMask;
    int use_scratch = loop->header->spilledScratch;
    reg_id_t reg;
    reg_id_t tls_reg = sregs.s3;
    if (gpr_mask) {
        for (reg=DR_REG_RAX; reg<=DR_REG_R15; reg++) {
            int i = reg - DR_REG_RAX;
            if (use_scratch) {
                int offset = -1;
                if (reg == sregs.s0) offset = 0;
                else if (reg == sregs.s1) offset = 1;
                else if (reg == sregs.s2) offset = 2;
                else if (reg == sregs.s3) offset = 3;

                /* Scratch register polluted, so load from bufferred space */
                if (offset != -1 && gpr_mask & (1<<i)) {
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_ld(drcontext,
                                            opnd_create_reg(sregs.s0),
                                            OPND_CREATE_MEM64(tls_reg, offset * 8)));
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_st(drcontext,
                                            OPND_CREATE_MEM64(tls_reg, LOCAL_CHECK_POINT_OFFSET + 8*i),
                                            opnd_create_reg(sregs.s0)));
                    continue;
                }
            }
            //save stk pointer as well
            if (gpr_mask & (1<<i) || reg == DR_REG_RSP) {
                INSERT(bb, trigger,
                    INSTR_CREATE_mov_st(drcontext,
                                        OPND_CREATE_MEM64(tls_reg, LOCAL_CHECK_POINT_OFFSET + 8*i),
                                        opnd_create_reg(reg)));
            }
        }
    }

    if (simd_mask) {
        for (reg=DR_REG_XMM0; reg<=DR_REG_XMM15; reg++) {
            int i = reg - DR_REG_XMM0;
            if (simd_mask & (1<<i)) {
                INSERT(bb, trigger,
                    INSTR_CREATE_movdqu(drcontext,
                                        opnd_create_base_disp(tls_reg, 0, 8, LOCAL_CHECK_POINT_OFFSET + PSTATE_SIMD_OFFSET + 16*i, OPSZ_16),
                                        opnd_create_reg(reg)));
            }
        }
    }
}

void
emit_restore_checkpoint_context_instrlist(EMIT_CONTEXT)
{
    uint32_t gpr_mask = loop->header->dependingGPRMask;
    uint32_t simd_mask = loop->header->dependingSIMDMask;
    int use_scratch = loop->header->spilledScratch;
    reg_id_t reg;
    reg_id_t tls_reg = sregs.s3;
    if (gpr_mask) {
        for (reg=DR_REG_RAX; reg<=DR_REG_R15; reg++) {
            int i = reg - DR_REG_RAX;
            if (use_scratch) {
                int offset = -1;
                if (reg == sregs.s0) offset = 0;
                else if (reg == sregs.s1) offset = 1;
                else if (reg == sregs.s2) offset = 2;
                else if (reg == sregs.s3) offset = 3;

                /* Scratch register polluted, so load from bufferred space */
                if (offset != -1 && gpr_mask & (1<<i)) {
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_ld(drcontext,
                                            opnd_create_reg(sregs.s0),
                                            OPND_CREATE_MEM64(tls_reg, LOCAL_CHECK_POINT_OFFSET + 8*i)));
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_st(drcontext,
                                            OPND_CREATE_MEM64(tls_reg, offset * 8),
                                            opnd_create_reg(sregs.s0)));
                    continue;
                }
            }
            //save stk pointer as well
            if (gpr_mask & (1<<i) || reg == DR_REG_RSP) {
                INSERT(bb, trigger,
                    INSTR_CREATE_mov_ld(drcontext,
                                        opnd_create_reg(reg),
                                        OPND_CREATE_MEM64(tls_reg, LOCAL_CHECK_POINT_OFFSET + 8*i)));
            }
        }
    }

    if (simd_mask) {
        for (reg=DR_REG_XMM0; reg<=DR_REG_XMM15; reg++) {
            int i = reg - DR_REG_XMM0;
            if (simd_mask & (1<<i)) {
                INSERT(bb, trigger,
                    INSTR_CREATE_movdqu(drcontext,
                                        opnd_create_reg(reg),
                                        opnd_create_base_disp(tls_reg, 0, 8, LOCAL_CHECK_POINT_OFFSET + PSTATE_SIMD_OFFSET + 16*i, OPSZ_16)));
            }
        }
    }
}

/* TLS must be in the sreg.s3 register */
void
emit_restore_shared_context_instrlist(EMIT_CONTEXT)
{
    PCAddress shared_addr = (PCAddress)(&(shared->registers));
    uint32_t gpr_mask = loop->header->registerToCopy;
    uint32_t simd_mask = gpr_mask >> 16;
    int use_scratch = loop->header->spilledScratch;
    reg_id_t tls_reg = sregs.s3;
    reg_id_t thread_reg = sregs.s0;
    reg_id_t reg;
    int cyclic = loop->type & LOOP_DOCYCLIC;

    /* Load the address of shared registers */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_imm(drcontext,
                             opnd_create_reg(thread_reg),
                             OPND_CREATE_INTPTR(shared_addr)));
    if (gpr_mask) {
        for (reg=DR_REG_RAX; reg<=DR_REG_R15; reg++) {
            if (reg == DR_REG_RSP) continue;
            int i = reg - DR_REG_RAX;
            int offset = -1;

            if (reg == sregs.s0) offset = 0;
            else if (reg == sregs.s1) offset = 1;
            else if (reg == sregs.s2) offset = 2;
            else if (reg == sregs.s3) offset = 3;

            if (gpr_mask & (1<<i) && offset==-1) {
                INSERT(bb, trigger,
                    INSTR_CREATE_mov_ld(drcontext,
                                        opnd_create_reg(reg),
                                        OPND_CREATE_MEM64(thread_reg, i * CACHE_LINE_WIDTH)));
                continue;
            }
            else if (offset != -1) {
                INSERT(bb, trigger,
                    INSTR_CREATE_mov_ld(drcontext,
                                        opnd_create_reg(sregs.s1),
                                        OPND_CREATE_MEM64(thread_reg, i * CACHE_LINE_WIDTH)));

                INSERT(bb, trigger,
                    INSTR_CREATE_mov_st(drcontext,
                                        OPND_CREATE_MEM64(tls_reg, offset * 8),
                                        opnd_create_reg(sregs.s1)));
            }
        }
    }

    /* Restore the content of SIMD registers */
    if (simd_mask) {
        for (reg=DR_REG_XMM0; reg<=DR_REG_XMM15; reg++) {
            int i = reg - DR_REG_XMM0;
            int cacheline = i + 16;
            if (simd_mask & (1<<i)) {
                INSERT(bb, trigger,
                    INSTR_CREATE_movdqu(drcontext,
                                        opnd_create_reg(reg),
                                        opnd_create_base_disp(thread_reg, DR_REG_NULL, 0,
                                                              cacheline*CACHE_LINE_WIDTH, OPSZ_16)));
            }
        }
    }
}

/* TLS must be in the sreg.s3 register */
void
emit_restore_private_context_instrlist(EMIT_CONTEXT)
{
    PCAddress shared_addr = (PCAddress)(&(shared->registers));
    uint32_t gpr_mask = loop->header->dependingGPRMask;
    uint32_t simd_mask = loop->header->dependingSIMDMask;
    int use_scratch = loop->header->spilledScratch;
    reg_id_t tls_reg = sregs.s3;
    reg_id_t thread_reg = sregs.s0;
    reg_id_t reg;

    if (gpr_mask) {
        for (reg=DR_REG_RAX; reg<=DR_REG_R15; reg++) {
            if (reg == DR_REG_RSP) continue;
            int i = reg - DR_REG_RAX;
            if (use_scratch) {
                int offset = -1;
                if (reg == sregs.s0) offset = 0;
                else if (reg == sregs.s1) offset = 1;
                else if (reg == sregs.s2) offset = 2;
                else if (reg == sregs.s3) offset = 3;

                /* Scratch register polluted, so load from bufferred space */
                if (offset != -1 && gpr_mask & (1<<i)) {
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_ld(drcontext,
                                            opnd_create_reg(sregs.s0),
                                            OPND_CREATE_MEM64(tls_reg, LOCAL_STATE_OFFSET + 8*i)));
                    INSERT(bb, trigger,
                        INSTR_CREATE_mov_st(drcontext,
                                            OPND_CREATE_MEM64(tls_reg, offset * 8),
                                            opnd_create_reg(sregs.s0)));
                    continue;
                }
            }
            if (gpr_mask & (1<<i)) {
                INSERT(bb, trigger,
                    INSTR_CREATE_mov_ld(drcontext,
                                        opnd_create_reg(reg),
                                        OPND_CREATE_MEM64(tls_reg, LOCAL_STATE_OFFSET + 8*i)));
            }
        }
    }

    /* Restore the content of SIMD registers */
    if (simd_mask) {
        for (reg=DR_REG_XMM0; reg<=DR_REG_XMM15; reg++) {
            int i = reg - DR_REG_XMM0;
            if (simd_mask & (1<<i)) {
                INSERT(bb, trigger,
                    INSTR_CREATE_movdqu(drcontext,
                                        opnd_create_reg(reg),
                                        opnd_create_base_disp(tls_reg, 0, 8, LOCAL_STATE_OFFSET + PSTATE_SIMD_OFFSET + 16*i, OPSZ_16)));
            }
        }
    }
}

void
emit_wait_threads_in_pool(EMIT_CONTEXT)
{
    int i;
    for (i=1; i<ginfo.number_of_threads; i++) {
        INSERT(bb, trigger,
            INSTR_CREATE_mov_imm(drcontext,
                                opnd_create_reg(sregs.s1),
                                OPND_CREATE_INTPTR(oracle[i])));
        instr_t *label = INSTR_CREATE_label(drcontext);
        INSERT(bb, trigger, label);
        //load in_pool flag
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(sregs.s0),
                                OPND_CREATE_MEM64(sregs.s1, LOCAL_FLAG_IN_POOL)));
        INSERT(bb, trigger,
            INSTR_CREATE_test(drcontext,
                              opnd_create_reg(sregs.s0),
                              opnd_create_reg(sregs.s0)));
        //proceed if they are one
        INSERT(bb, trigger,
            INSTR_CREATE_jcc(drcontext, OP_jz, opnd_create_instr(label)));
    }
}

void
emit_serial_commit_lock(EMIT_CONTEXT)
{
    instr_t *waitLabel;
    reg_id_t tls_reg = sregs.s3;

    waitLabel = INSTR_CREATE_label(drcontext);
    INSERT(bb, trigger, waitLabel);
    /* Check need yield flag */
    INSERT(bb, trigger,
        INSTR_CREATE_cmp(drcontext,
                         opnd_create_rel_addr((void *)(&shared->need_yield), OPSZ_4),
                         OPND_CREATE_INT32(1)));
    app_pc jump_to_pool = (app_pc)shared->current_loop->code.jump_to_pool;

    DR_ASSERT_MSG(jump_to_pool != NULL, "Jump to pool code must be generated prior to emit_serial_commit_lock");
    /* If need yield is on, jump to the pool */
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_je, opnd_create_pc(jump_to_pool)));

    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(reg_64_to_32(sregs.s0)),
                            OPND_CREATE_MEM32(tls_reg, LOCAL_FLAG_CAN_COMMIT)));
    INSERT(bb, trigger,
        INSTR_CREATE_test(drcontext,
                          opnd_create_reg(sregs.s0),
                          opnd_create_reg(sregs.s0)));
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_jz, opnd_create_instr(waitLabel)));
}

void
emit_serial_commit_unlock(EMIT_CONTEXT)
{
    //The write order is important here
    reg_id_t tls_reg = sregs.s3;
    //set my current canCommit to zero
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM32(tls_reg, LOCAL_FLAG_CAN_COMMIT),
                            OPND_CREATE_INT32(0)));
    //increment my local stamp and global stamp
    INSERT(bb, trigger,
        INSTR_CREATE_add(drcontext,
                         opnd_create_rel_addr((void *)&(shared->global_stamp), OPSZ_8),
                         OPND_CREATE_INT32(1)));
    INSERT(bb, trigger,
        INSTR_CREATE_add(drcontext,
                         OPND_CREATE_MEM32(tls_reg, LOCAL_STAMP_OFFSET),
                         OPND_CREATE_INT32(ginfo.number_of_threads)));
    //load next thread's TLS
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s0),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_NEXT_OFFSET)));
    //set my next thread's canCommit flag to one
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM32(sregs.s0, LOCAL_FLAG_CAN_COMMIT),
                            OPND_CREATE_INT32(1)));
    //set my own rolledback
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM32(tls_reg, LOCAL_FLAG_ROLLEDBACK_OFFSET),
                            OPND_CREATE_INT32(0)));
}

void
emit_loop_iteration_commit(EMIT_CONTEXT)
{
    instr_t *skip = INSTR_CREATE_label(drcontext);

    reg_id_t tls_reg = sregs.s3;

    reg_id_t sreg32 = reg_64_to_32((reg_id_t)sregs.s0);

    /* Load transaction_on flag and check whether there is un-commited changes */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg32),
                            OPND_CREATE_MEM32(tls_reg, TRANS_ON_FLAG_OFFSET)));
    /* test sreg32, sreg32 */
    INSERT(bb, trigger,
        INSTR_CREATE_test(drcontext,
                          opnd_create_reg(sreg32),
                          opnd_create_reg(sreg32)));
    /* jz skip */
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_jz, opnd_create_instr(skip)));

    /* Step 1: wait until it is the oldest */
    emit_serial_commit_lock(emit_context);

    /* ----------> Critical Region Start <---------- */
    /* Step 2: if speculation is enabled */
    if (loop->type & LOOP_DOSPEC || loop->type & LOOP_SPEC_INLINE)
        emit_tx_validate_instrlist(emit_context);

#ifdef JANUS_STM_VERBOSE
    PRINTVAL(shared->global_stamp);
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s0),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_STAMP_OFFSET)));
    PRINTREG(sregs.s0);
#endif
    /* Step 3: commit changes */
    emit_tx_commit_instrlist(emit_context, false);

    /* ----------> Critical Region End   <---------- */
    /* Step 4: unlock */
    emit_serial_commit_unlock(emit_context);

    /* Step 5: clear the transaction */
    emit_tx_clear_instrlist(emit_context);

    INSERT(bb, trigger, skip);
}

/* Predict the value based on the distance to the oldest iteration */
void
emit_value_prediction(EMIT_CONTEXT)
{
    PCAddress shared_addr = (PCAddress)(&(shared->registers));
    uint32_t gpr_mask = loop->header->dependingGPRMask;
    uint32_t simd_mask = loop->header->dependingSIMDMask;
    reg_id_t tls_reg = sregs.s3;
    int i;

    instr_t *oldestPredict = INSTR_CREATE_label(drcontext);
    instr_t *finishPredict = INSTR_CREATE_label(drcontext);

    /* Step 1: calculate the distance from local stamp to global stamp */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s2),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_STAMP_OFFSET)));
    INSERT(bb, trigger,
        INSTR_CREATE_sub(drcontext,
                         opnd_create_reg(sregs.s2),
                         opnd_create_rel_addr((void *)&(shared->global_stamp), OPSZ_8)));
    /* Save distance */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(tls_reg, LOCAL_DISTANCE_OFFSET),
                            opnd_create_reg(sregs.s2)));
    /* check distance */
    INSERT(bb, trigger,
        INSTR_CREATE_test(drcontext,
                          opnd_create_reg(sregs.s2),
                          opnd_create_reg(sregs.s2)));
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_jz, opnd_create_instr(oldestPredict)));

    /* Step 2.1 The following serves the prediction for other threads */
    /* Then we preform prediction for each of the variable */
    for (i=0; i<loop->num_of_variables; i++) {
        emit_depending_variable_value_prediction(drcontext, sregs, loop, bb, trigger, i, false);
    }

    /* Jump to finish */
    INSERT(bb, trigger,
        INSTR_CREATE_jmp(drcontext, opnd_create_instr(finishPredict)));

    /* Step 2.2 The following serves the prediction for the oldest thread */
    INSERT(bb, trigger, oldestPredict);
    for (i=0; i<loop->num_of_variables; i++) {
        emit_depending_variable_value_prediction(drcontext, sregs, loop, bb, trigger, i, true);
    }

    INSERT(bb, trigger, finishPredict);
}

void
emit_spill_scratch_register(EMIT_CONTEXT, reg_id_t reg)
{
    reg_id_t tls_reg = sregs.s3;
    if (reg == sregs.s0) {
        INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                OPND_CREATE_MEM64(tls_reg, 0),
                                opnd_create_reg(reg)));
    }
    if (reg == sregs.s1) {
        INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                OPND_CREATE_MEM64(tls_reg, 8),
                                opnd_create_reg(reg)));
    }
    if (reg == sregs.s2) {
        INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                OPND_CREATE_MEM64(tls_reg, 16),
                                opnd_create_reg(reg)));
    }
    if (reg == sregs.s3) {
        INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                OPND_CREATE_MEM64(tls_reg, 24),
                                opnd_create_reg(sregs.s0)));
    }
}
#endif
