#include "emit.h"
#include "jthread.h"
#include "control.h"
#include "janus_arch.h"


/** \brief Generate a code snippet that switches the current stack to a specified stack */
void emit_switch_stack_ptr_aligned(EMIT_CONTEXT)
{
    /* Spill SP to shared stack ptr */
    INSERT(bb, trigger,
        XINST_CREATE_move(drcontext,
                        opnd_create_reg(s0),
                        opnd_create_reg(DR_REG_SP)));

    insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s2), (ptr_int_t)&(shared->stack_ptr));
    INSERT(bb, trigger,
        instr_create_1dst_1src(drcontext,
                            OP_str,
                            OPND_CREATE_MEM64(s2, 0),
                            opnd_create_reg(s0)));

    /* Spill X29 to shared base ptr, leaving its value in s0 (shared base pointer) */
    INSERT(bb, trigger,
        XINST_CREATE_move(drcontext,
                        opnd_create_reg(s0),
                        opnd_create_reg(DR_REG_X29)));

    insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s2), (ptr_int_t)&(shared->stack_base));
    INSERT(bb, trigger,
        instr_create_1dst_1src(drcontext,
                            OP_str,
                            OPND_CREATE_MEM64(s2, 0),
                            opnd_create_reg(s0)));

    /* Load X29 and SP from local stack ptr */
    INSERT(bb, trigger,
        instr_create_1dst_1src(drcontext,
                            OP_ldr,
                            opnd_create_reg(DR_REG_X29),
                            OPND_CREATE_MEM64(TLS, LOCAL_STACK_OFFSET)))
;
    /* Allocate stack frame (by sub-ing from X29 since it will be the same as SP) */
    INSERT(bb, trigger,
        INSTR_CREATE_sub(drcontext,
                        opnd_create_reg(DR_REG_X29),
                        opnd_create_reg(DR_REG_X29),
                        OPND_CREATE_INT(loop->header->stackFrameSize)));
    /* Copy X29 value to SP */
    INSERT(bb, trigger,
        XINST_CREATE_move(drcontext,
                        opnd_create_reg(DR_REG_SP),
                        opnd_create_reg(DR_REG_X29)));

    /* Save the new stack pointer value to the local stack pointer (required by emit_restore_stack) */
    INSERT(bb, trigger,
        instr_create_1dst_1src(drcontext,
                            OP_str,
                            OPND_CREATE_MEM64(TLS, LOCAL_STACK_OFFSET),
                            opnd_create_reg(DR_REG_X29)));

}

void emit_restore_stack_ptr_aligned(EMIT_CONTEXT)
{
    /* Load X29 from shared stack base */ 
    insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s2), (ptr_int_t)&(shared->stack_base));

    INSERT(bb, trigger,
        instr_create_1dst_1src(drcontext,
                            OP_ldr,
                            opnd_create_reg(DR_REG_X29),
                            OPND_CREATE_MEM64(s2, 0)));

    /* Load SP from shared stack ptr */
    insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s2), (ptr_int_t)&(shared->stack_ptr));
    INSERT(bb, trigger,
        instr_create_1dst_1src(drcontext,
                            OP_ldr,
                            opnd_create_reg(s2),
                            OPND_CREATE_MEM64(s2, 0)));
    INSERT(bb, trigger,
        XINST_CREATE_move(drcontext,
                        opnd_create_reg(DR_REG_SP),
                        opnd_create_reg(s2)));
}

/** \brief Selectively save registers to shared register bank based on the register mask */
void
emit_spill_to_shared_register_bank(EMIT_CONTEXT, uint64_t regMask)
{
    instr_t *instr;

    PCAddress shared_addr = (PCAddress)(&(shared->registers));
    /* load the shared register bank to s3 (since s0 may be used for stack) */
    insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s3), shared_addr);

    int cacheline;

    for(int reg = JREG_X0; reg <= JREG_X30; reg++)
    {
        cacheline = reg - JREG_X0 + 1;
        if((regMask & get_reg_bit_array(reg)) != 0)
        {
            if (reg == dr_get_stolen_reg()){ // if this is the stolen register
                // load content of stolen register into s2
                dr_insert_get_stolen_reg_value(drcontext, bb, trigger, s2);

                //store content of stolen register to shared space
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_str,
                                        OPND_CREATE_MEM64(s3, cacheline * CACHE_LINE_WIDTH),
                                        opnd_create_reg(s2)));
            }
            else if (reg == s0) {
                //load content of s0 into s2
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_ldr,
                                        opnd_create_reg(s2),
                                        OPND_CREATE_MEM64(TLS, LOCAL_S0_OFFSET)));
                //store content of s0 to shared space
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_str,
                                        OPND_CREATE_MEM64(s3, cacheline * CACHE_LINE_WIDTH),
                                        opnd_create_reg(s2)));
            }
            else if (reg == s1) {
                //load content of s1 into s2
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_ldr,
                                        opnd_create_reg(s2),
                                        OPND_CREATE_MEM64(TLS, LOCAL_S1_OFFSET)));
                //store content of s1 to shared space
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_str,
                                        OPND_CREATE_MEM64(s3, cacheline * CACHE_LINE_WIDTH),
                                        opnd_create_reg(s2)));
            }
            else if (reg == s2) {
                //load content of s2 into s2
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_ldr,
                                        opnd_create_reg(s2),
                                        OPND_CREATE_MEM64(TLS, LOCAL_S2_OFFSET)));
                //store content of s2 to shared space
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_str,
                                        OPND_CREATE_MEM64(s3, cacheline * CACHE_LINE_WIDTH),
                                        opnd_create_reg(s2)));
            }
            else if (reg == s3) {
                //load content of s3 into s2
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_ldr,
                                        opnd_create_reg(s2),
                                        OPND_CREATE_MEM64(TLS, LOCAL_S3_OFFSET)));
                //store content of s3 to shared space
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_str,
                                        OPND_CREATE_MEM64(s3, cacheline * CACHE_LINE_WIDTH),
                                        opnd_create_reg(s2)));
            }
            else {
                //for non-scratch (s0, s1) registers
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_str,
                                        OPND_CREATE_MEM64(s3, cacheline * CACHE_LINE_WIDTH),
                                        opnd_create_reg(reg)));
            }
        }
    }
    for(int reg = JREG_Q0; reg <= JREG_Q31; reg++) //SIMD registers
    {
        cacheline = reg - JREG_Q0 + 32;
        if((regMask & get_reg_bit_array(reg)) != 0)
        {
            INSERT(bb, trigger,
                instr_create_1dst_1src(drcontext,
                                    OP_str,
                                    opnd_create_base_disp(s3, DR_REG_NULL, 0, cacheline * CACHE_LINE_WIDTH, OPSZ_16),
                                    opnd_create_reg(reg)));
        }
    }
}

/** \brief Selectively restore registers from the shared register bank based on the register mask */
void
emit_restore_from_shared_register_bank(EMIT_CONTEXT, uint64_t regMask)
{
    int cacheline;

    instr_t *instr;

    PCAddress shared_addr = (PCAddress)(&(shared->registers));
    /* load the shared register bank to s3 (since s0 may be used for stack) */
    insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s3), shared_addr);

    for(int reg = JREG_X0; reg <= JREG_X30; reg++)
    {
        cacheline = reg - JREG_X0 + 1;
        if((regMask & get_reg_bit_array(reg)) != 0)
        {
            if (reg == s0) {
                //load content of s0 into s2 from shared space
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_ldr,
                                        opnd_create_reg(s2),
                                        OPND_CREATE_MEM64(s3, cacheline * CACHE_LINE_WIDTH)));
                //store content of s0 to TLS
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_str,
                                        OPND_CREATE_MEM64(TLS, LOCAL_S0_OFFSET),
                                        opnd_create_reg(s2)));
            }
            else if (reg == s1) {
                //load content of s1 into s2 from shared space
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_ldr,
                                        opnd_create_reg(s2),
                                        OPND_CREATE_MEM64(s3, cacheline * CACHE_LINE_WIDTH)));

                //store content of s1 to TLS
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_str,
                                        OPND_CREATE_MEM64(TLS, LOCAL_S1_OFFSET),
                                        opnd_create_reg(s2)));
            }
            else if (reg == s2) {
                //load content of s2 into s2 from shared space
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_ldr,
                                        opnd_create_reg(s2),
                                        OPND_CREATE_MEM64(s3, cacheline * CACHE_LINE_WIDTH)));

                //store content of s2 to TLS
                INSERT(bb, trigger,
                   instr_create_1dst_1src(drcontext,
                                        OP_str,
                                        OPND_CREATE_MEM64(TLS, LOCAL_S2_OFFSET),
                                        opnd_create_reg(s2)));
            }
            else if (reg == s3) {
                //load content of s3 into s2 from shared space
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_ldr,
                                        opnd_create_reg(s2),
                                        OPND_CREATE_MEM64(s3, cacheline * CACHE_LINE_WIDTH)));

                //store content of s3 to TLS
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_str,
                                        OPND_CREATE_MEM64(TLS, LOCAL_S3_OFFSET),
                                        opnd_create_reg(s2)));
            }
            else {
                //for non-scratch (s0, s1) registers
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_ldr,
                                        opnd_create_reg(reg),
                                        OPND_CREATE_MEM64(s3, cacheline * CACHE_LINE_WIDTH)));
            }
        }
    }

    for(int reg = JREG_Q0; reg <= JREG_Q31; reg++) //SIMD registers
    {
        cacheline = reg - JREG_Q0 + 32;
        if((regMask & get_reg_bit_array(reg)) != 0)
        {

	    INSERT(bb, trigger,
		instr_create_1dst_1src(drcontext,
				    OP_ldr,
				    opnd_create_reg(reg),
    				    opnd_create_base_disp(s3, DR_REG_NULL, 0, reg * CACHE_LINE_WIDTH, OPSZ_16)));
        }
    }

}

/** \brief Insert instructions at the trigger to selectively save registers to private register bank based on the register mask */
void
emit_spill_to_private_register_bank(EMIT_CONTEXT, uint64_t regMask, int tid)
{

    instr_t *instr;

    for(int reg = JREG_X0; reg <= JREG_X30; reg++)
    {
        if((regMask & get_reg_bit_array(reg)) != 0)
        {
            if (reg == s0) {
                //load content of s0 into s2
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_ldr,
                                        opnd_create_reg(s2),
                                        OPND_CREATE_MEM64(TLS, LOCAL_S0_OFFSET)));
                //store content of s0 to shared space
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_str,
                                        OPND_CREATE_MEM64(TLS, LOCAL_PSTATE_OFFSET + 8*(reg-JREG_X0)),
                                        opnd_create_reg(s2)));
            }
            else if (reg == s1) {
                //load content of s1 into s2
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_ldr,
                                        opnd_create_reg(s2),
                                        OPND_CREATE_MEM64(TLS, LOCAL_S1_OFFSET)));
                //store content of s1 to shared space
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_str,
                                        OPND_CREATE_MEM64(TLS, LOCAL_PSTATE_OFFSET + 8*(reg-JREG_X0)),
                                        opnd_create_reg(s2)));
            }
            else if (reg == s2) {
                //load content of s2 into s2
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_ldr,
                                        opnd_create_reg(s2),
                                        OPND_CREATE_MEM64(TLS, LOCAL_S2_OFFSET)));
                //store content of s2 to shared space
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_str,
                                        OPND_CREATE_MEM64(TLS, LOCAL_PSTATE_OFFSET + 8*(reg-JREG_X0)),
                                        opnd_create_reg(s2)));
            }
            else if (reg == s3) {
                //load content of s3 into s2
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_ldr,
                                        opnd_create_reg(s2),
                                        OPND_CREATE_MEM64(TLS, LOCAL_S3_OFFSET)));
                //store content of s3 to shared space
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_str,
                                        OPND_CREATE_MEM64(TLS, LOCAL_PSTATE_OFFSET + 8*(reg-JREG_X0)),
                                        opnd_create_reg(s2)));
            }
            else {
                //for non-scratch (s0, s1) registers
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_str,
                                        OPND_CREATE_MEM64(TLS, LOCAL_PSTATE_OFFSET + 8*(reg-JREG_X0)),
                                        opnd_create_reg(reg)));
            }
        }
    }
    for(int reg = JREG_Q0; reg <= JREG_Q31; reg++) //SIMD registers
    {
        if((regMask & get_reg_bit_array(reg)) != 0)
        {
            INSERT(bb, trigger,
                instr_create_1dst_1src(drcontext,
                                    OP_str,
                                    opnd_create_base_disp(TLS, DR_REG_NULL, 0, LOCAL_PSTATE_OFFSET + PSTATE_SIMD_OFFSET + 16*(reg - JREG_Q0), OPSZ_16),
                                    opnd_create_reg(reg)));
        }
    }
}


/** \brief Insert instructions at the trigger to selectively save registers to private register bank based on the register mask
 * it can load from different thread's bank */
void
emit_restore_from_private_register_bank(EMIT_CONTEXT, uint64_t regMask, int mytid, int readtid)
{
    reg_id_t reg;
    reg_id_t read_tls_reg;


    if (mytid == readtid) {
        read_tls_reg = TLS;
    } else {
        //load input TLS into s3, if tid different
        read_tls_reg = s3;
        insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(read_tls_reg), (ptr_int_t)(oracle[readtid]));
    }

    int cacheline;

    instr_t *instr;

    for(int reg = JREG_X0; reg <= JREG_X30; reg++)
    {
        cacheline = reg - JREG_X0 + 1;
        if((regMask & get_reg_bit_array(reg)) != 0)
        {
            if (reg == s0) {
                //load content of s0 into s2 from shared space
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_ldr,
                                        opnd_create_reg(s2),
                                        OPND_CREATE_MEM64(read_tls_reg, LOCAL_PSTATE_OFFSET+8*(reg - JREG_X0))));
                //store content of s0 to TLS
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_str,
                                        OPND_CREATE_MEM64(TLS, LOCAL_S0_OFFSET),
                                        opnd_create_reg(s2)));
            }
            else if (reg == s1) {
                //load content of s1 into s2 from shared space
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_ldr,
                                        opnd_create_reg(s2),
                                        OPND_CREATE_MEM64(read_tls_reg, LOCAL_PSTATE_OFFSET+8*(reg - JREG_X0))));

                //store content of s1 to TLS
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_str,
                                        OPND_CREATE_MEM64(TLS, LOCAL_S1_OFFSET),
                                        opnd_create_reg(s2)));
            }
            else if (reg == s2) {
                //load content of s2 into s2 from shared space
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_ldr,
                                        opnd_create_reg(s2),
                                        OPND_CREATE_MEM64(read_tls_reg, LOCAL_PSTATE_OFFSET+8*(reg - JREG_X0))));

                //store content of s2 to TLS
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_str,
                                        OPND_CREATE_MEM64(TLS, LOCAL_S2_OFFSET),
                                        opnd_create_reg(s2)));
            }
            else if (reg == s3) {
                //load content of s3 into s2 from shared space
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_ldr,
                                        opnd_create_reg(s2),
                                        OPND_CREATE_MEM64(read_tls_reg, LOCAL_PSTATE_OFFSET+8*(reg - JREG_X0))));

                //store content of s3 to TLS
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_str,
                                        OPND_CREATE_MEM64(TLS, LOCAL_S3_OFFSET),
                                        opnd_create_reg(s2)));
            }
            else {
                //for non-scratch (s0, s1) registers
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_ldr,
                                        opnd_create_reg(reg), 
                                        OPND_CREATE_MEM64(read_tls_reg, LOCAL_PSTATE_OFFSET+8*(reg - JREG_X0))));
            }
        }
    }

    for(int reg = JREG_Q0; reg <= JREG_Q31; reg++) //SIMD registers
    {
        cacheline = reg - JREG_Q0 + 32;
        if((regMask & get_reg_bit_array(reg)) != 0)
        {

            INSERT(bb, trigger,
                instr_create_1dst_1src(drcontext,
                                    OP_ldr,
                                    opnd_create_reg(reg),
                                    opnd_create_base_disp(read_tls_reg, 0, 8, LOCAL_PSTATE_OFFSET + PSTATE_SIMD_OFFSET + 16*(reg - JREG_Q0), OPSZ_16)));
        }
    }

}



void
emit_wait_threads_in_pool(EMIT_CONTEXT)
{
    int i;
    for (i=1; i<rsched_info.number_of_threads; i++) {

        //load the thread's TLS        
        insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s2), (ptr_int_t)oracle[i]);

        //create label at start of 'waiting loop'
        instr_t *label = INSTR_CREATE_label(drcontext);
        INSERT(bb, trigger, label);

        //load in_pool flag
        INSERT(bb, trigger,
            instr_create_1dst_1src(drcontext,
                                OP_ldr,
                                opnd_create_reg(s3),
                                OPND_CREATE_MEM64(s2, LOCAL_FLAG_IN_POOL)));
        //compare in_pool flag and branch to label if 0 (i.e. proceed if 1) 
        INSERT(bb, trigger,
            instr_create_0dst_2src(drcontext,
                                OP_cbz,
                                opnd_create_instr(label),
                                opnd_create_reg(s3)));
    }
}

void
emit_wait_threads_finish(EMIT_CONTEXT)
{
    int i;
    instr_t *wait;
    for (i=1; i<rsched_info.number_of_threads; i++) {
        wait = INSTR_CREATE_label(drcontext);
        insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s2), (ptr_int_t)&(oracle[i]->flag_space.finished));

        INSERT(bb, trigger, wait);

        /* load and check flag */
        INSERT(bb, trigger,
            instr_create_1dst_1src(drcontext,
                                OP_ldr,
                                opnd_create_reg(s3),
                                OPND_CREATE_MEM64(s2, 0)));
        /* branch if the flag is zero, because that is easier than checking if it doesn't equal 1 */
        INSERT(bb, trigger,
            instr_create_0dst_2src(drcontext,
                                OP_cbz,
                                opnd_create_instr(wait),
                                opnd_create_reg(s3)));
    }
}

void
emit_schedule_threads(EMIT_CONTEXT)
{

    /* unset shared.need_yield, this must be unset before setting shared.start_run  */
    insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s2), (ptr_int_t)&(shared->need_yield));

    INSERT(bb, trigger,
        instr_create_1dst_1src(drcontext,
                            OP_str,
                            OPND_CREATE_MEM64(s2, 0),
                            opnd_create_reg(DR_REG_XZR)));

    /* set shared.start_run = 1 */
    insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s2), (ptr_int_t)&(shared->start_run));
    INSERT(bb, trigger,
        INSTR_CREATE_movz(drcontext, opnd_create_reg(s3),
                        OPND_CREATE_INT16(1),
                        OPND_CREATE_INT8(0)));

    INSERT(bb, trigger,
        instr_create_1dst_1src(drcontext,
                            OP_str,
                            OPND_CREATE_MEM64(s2, 0),
                            opnd_create_reg(s3)));

    /* set shared->loop_on flag */
    //TODO set shared flag instead??
    insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s2), (ptr_int_t)&(shared->loop_on));
    INSERT(bb, trigger,
        INSTR_CREATE_movz(drcontext, opnd_create_reg(s3),
                        OPND_CREATE_INT16(1),
                        OPND_CREATE_INT8(0)));
    INSERT(bb, trigger,
        instr_create_1dst_1src(drcontext,
                            OP_str,
                            OPND_CREATE_MEM64(s2, 0),
                            opnd_create_reg(s3)));
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
                    INSERT(bb, trigger,
                        XINST_CREATE_move(drcontext,
                                            opnd_create_reg(reg_with_same_width(dst.value, src_opnd)),
                                            src_opnd));

                }
            } else if (src.type == JVAR_STACK || src.type == JVAR_STACKFRAME || src.type == JVAR_MEMORY || src.type == JVAR_ABSOLUTE) {
                //dst_reg <- [mem]
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_ldr,
                                        opnd_create_reg(reg_with_same_width(dst.value, src_opnd)),
                                        src_opnd));
            } else if (src.type == JVAR_CONSTANT) {
                //dst_reg <- imm
                insert_mov_immed_ptrsz(drcontext, bb, trigger,
                                    opnd_create_reg(dst.value),
                                    (ptr_int_t)src.value);
            }
            break;

        }
        case JVAR_STACK:
        case JVAR_STACKFRAME:
        case JVAR_MEMORY:
        case JVAR_ABSOLUTE: {
            opnd_t dst_opnd = create_opnd(dst);
            opnd_t src_opnd = create_opnd(src);
            if (src.type == JVAR_REGISTER) {
                //dst_mem <- src_reg
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_str,
                                        dst_opnd,
                                        opnd_create_reg(reg_with_same_width(src.value, dst_opnd))));
            } else if (src.type == JVAR_STACK || src.type == JVAR_STACKFRAME || src.type == JVAR_MEMORY || src.type == JVAR_ABSOLUTE) {
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_ldr,
                                        opnd_create_reg(reg_with_same_width(scratch, src_opnd)),
                                        src_opnd));
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_str,
                                        dst_opnd,
                                        opnd_create_reg(reg_with_same_width(scratch, dst_opnd))));
            } else if (src.type == JVAR_CONSTANT) {
                //dst_mem <- imm
                insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(scratch), (ptr_int_t)src.value);

                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_str,
                                        dst_opnd,
                                        opnd_create_reg(reg_with_same_width(scratch, dst_opnd))));

            }
            break;
        }
        default: {
            DR_ASSERT("Destination Janus variable not writeable");
        }
    }
}

/** \brief Generate instructions to add the content of src variable to the dst variable
 *
 * It uses an additional scratch register if either src or dst are memory operands,
 * and uses 2 scratch registers if both are memory operands */
void
emit_add_janus_var(EMIT_CONTEXT, JVar dst, JVar src, reg_id_t scratch, reg_id_t scratch2)
{
    switch (dst.type) {
        case JVAR_REGISTER: {
            opnd_t dst_opnd = create_opnd(dst);
            opnd_t src_opnd = create_opnd(src);
            if(src.type == JVAR_REGISTER) {
                INSERT(bb, trigger,
                    INSTR_CREATE_add(drcontext,
                                     opnd_create_reg(reg_with_same_width(dst.value, src_opnd)),
                                     opnd_create_reg(reg_with_same_width(dst.value, src_opnd)),
                                     src_opnd));
            } else if (src.type == JVAR_STACK || src.type == JVAR_STACKFRAME || src.type == JVAR_MEMORY || src.type == JVAR_ABSOLUTE) { 
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_ldr,
                                        opnd_create_reg(reg_with_same_width(scratch, src_opnd)),
                                        src_opnd));
                INSERT(bb, trigger,
                    INSTR_CREATE_add(drcontext,
                                     opnd_create_reg(reg_with_same_width(dst.value, src_opnd)),
                                     opnd_create_reg(reg_with_same_width(dst.value, src_opnd)),
                                     opnd_create_reg(reg_with_same_width(scratch, src_opnd))));
            } else if (src.type == JVAR_CONSTANT) {
                int i;

                //lazy option
                insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(scratch), src.value);
                INSERT(bb, trigger,
                    INSTR_CREATE_add(drcontext,
                                     opnd_create_reg(dst.value),
                                     opnd_create_reg(dst.value),
                                     opnd_create_reg(reg_with_same_width(scratch, dst_opnd))));
            }
            break;
        }
        case JVAR_STACK:
        case JVAR_STACKFRAME:
        case JVAR_MEMORY:
        case JVAR_ABSOLUTE: {
            opnd_t dst_opnd = create_opnd(dst);
            opnd_t src_opnd = create_opnd(src);
            if (src.type == JVAR_REGISTER) {
                //dr_printf("src: %s, dst %0x\n", get_reg_name(src.value), dst.value);
                //dst_mem <- src_reg
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_ldr,
                                        opnd_create_reg(reg_with_same_width(scratch, dst_opnd)),
                                        dst_opnd));
                INSERT(bb, trigger,
                    INSTR_CREATE_add(drcontext,
                                     opnd_create_reg(reg_with_same_width(scratch, dst_opnd)),
                                     opnd_create_reg(reg_with_same_width(scratch, dst_opnd)),
                                     opnd_create_reg(reg_with_same_width(src.value, dst_opnd))));
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_str,
                                        dst_opnd,
                                        opnd_create_reg(reg_with_same_width(scratch, dst_opnd))));
            } else if (src.type == JVAR_STACK || src.type == JVAR_STACKFRAME || src.type == JVAR_MEMORY || src.type == JVAR_ABSOLUTE) {
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_ldr,
                                        opnd_create_reg(reg_with_same_width(scratch, src_opnd)),
                                        src_opnd));
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_ldr,
                                        opnd_create_reg(reg_with_same_width(scratch2, dst_opnd)),
                                        dst_opnd));
                INSERT(bb, trigger,
                    INSTR_CREATE_add(drcontext,
                                     opnd_create_reg(reg_with_same_width(scratch2, dst_opnd)),
                                     opnd_create_reg(reg_with_same_width(scratch2, dst_opnd)),
                                     opnd_create_reg(reg_with_same_width(scratch, dst_opnd))));
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_str,
                                        dst_opnd,
                                        opnd_create_reg(reg_with_same_width(scratch2, dst_opnd))));

            } else if (src.type == JVAR_CONSTANT) {
                //dst_reg <- imm
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_ldr,
                                        opnd_create_reg(reg_with_same_width(scratch, dst_opnd)),
                                        dst_opnd));
                INSERT(bb, trigger,
                    INSTR_CREATE_add(drcontext,
                                     opnd_create_reg(reg_with_same_width(scratch, dst_opnd)),
                                     opnd_create_reg(reg_with_same_width(scratch, dst_opnd)),
                                     src_opnd));
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_str,
                                        dst_opnd,
                                        opnd_create_reg(reg_with_same_width(scratch, dst_opnd))));
            }
            break;
        }
        default: {
            DR_ASSERT("Destination Janus variable not writeable");
        }
    }
}

