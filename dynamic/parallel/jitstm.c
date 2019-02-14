#include "jitstm.h"
#include "jthread.h"

#ifdef FIX
void
emit_tx_start_instrlist(EMIT_CONTEXT);

void
emit_tx_validate_instrlist(EMIT_CONTEXT)
{
    void *rollback = loop->code.tx_rollback;
    PCAddress shared_addr = (PCAddress)(&(shared->registers));
    uint32_t gpr_mask = loop->header->validateGPRMask;
    uint32_t simd_mask = loop->header->validateSIMDMask;
    reg_id_t tls_reg = sregs.s3;
    reg_id_t reg;

    /* Step 1: validate GPR registers */
    if (gpr_mask) {
        for (reg=DR_REG_RAX; reg<=DR_REG_R15; reg++) {
            if (reg == DR_REG_RSP) continue;
            int i = reg - DR_REG_RAX;
            if (gpr_mask & (1<<i)) {
                INSERT(bb, trigger,
                    INSTR_CREATE_mov_ld(drcontext,
                                        opnd_create_reg(sregs.s0),
                                        OPND_CREATE_MEM64(tls_reg, LOCAL_REG_READ_SET_OFFSET + 8*i)));
#ifdef JANUS_PRINT_ROLLBACK
                PRINTREG(reg);
                PRINTREG(sregs.s0);
                INSERT(bb, trigger,
                    INSTR_CREATE_mov_ld(drcontext,
                                        opnd_create_reg(sregs.s1),
                                        opnd_create_rel_addr((void *)(shared_addr + i * CACHE_LINE_WIDTH), OPSZ_8)));
                PRINTREG(sregs.s1);
#endif
                INSERT(bb, trigger,
                    INSTR_CREATE_cmp(drcontext,
                                     opnd_create_rel_addr((void *)(shared_addr + i * CACHE_LINE_WIDTH), OPSZ_8),
                                     opnd_create_reg(sregs.s0)));
                //if not equal, jump to failure
                INSERT(bb, trigger,
                    INSTR_CREATE_jcc(drcontext, OP_jnz, opnd_create_pc((app_pc)rollback)));
            }
        }
    }

    /* Step 2: validate SIMD registers */
    if (simd_mask) {
        for (reg=DR_REG_XMM0; reg<=DR_REG_XMM15; reg++) {
            int i = reg - DR_REG_XMM0;
            if (simd_mask & (1<<i)) {
                opnd_t spec_opnd1 = OPND_CREATE_MEM64(tls_reg, LOCAL_STATE_OFFSET + PSTATE_SIMD_OFFSET + i*16);
                opnd_t spec_opnd2 = OPND_CREATE_MEM64(tls_reg, LOCAL_STATE_OFFSET + PSTATE_SIMD_OFFSET + i*16 + 8);
                opnd_t shared_opnd1 = opnd_create_rel_addr((void *)(shared_addr + (i+16) * CACHE_LINE_WIDTH), OPSZ_8);
                opnd_t shared_opnd2 = opnd_create_rel_addr((void *)(shared_addr + (i+16) * CACHE_LINE_WIDTH + 8), OPSZ_8);
                //load from speculative buffer
                INSERT(bb, trigger,
                    INSTR_CREATE_mov_ld(drcontext,
                                        opnd_create_reg(sregs.s0),
                                        spec_opnd1));
                //compare with shared register
                INSERT(bb, trigger,
                    INSTR_CREATE_cmp(drcontext,
                                     opnd_create_reg(sregs.s0),
                                     shared_opnd1));
                //if not equal, jump to failure
                INSERT(bb, trigger,
                    INSTR_CREATE_jcc(drcontext, OP_jnz, opnd_create_pc((app_pc)rollback)));
                //load from speculative buffer
                INSERT(bb, trigger,
                    INSTR_CREATE_mov_ld(drcontext,
                                        opnd_create_reg(sregs.s0),
                                        spec_opnd2));
                //compare with shared register
                INSERT(bb, trigger,
                    INSTR_CREATE_cmp(drcontext,
                                     opnd_create_reg(sregs.s0),
                                     shared_opnd2));
                //if not equal, jump to failure
                INSERT(bb, trigger,
                    INSTR_CREATE_jcc(drcontext, OP_jnz, opnd_create_pc((app_pc)rollback)));            }
        }
    }

    /* Step 3: validate memories */
    instr_t *loopLabel, *finishLabel;
    loopLabel = INSTR_CREATE_label(drcontext);
    finishLabel = INSTR_CREATE_label(drcontext);

    //load read set start address
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s0),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_READ_SET_OFFSET)));

    //start of a loop
    INSERT(bb, trigger, loopLabel);
    //cmp s0, s1
    INSERT(bb, trigger,
        INSTR_CREATE_cmp(drcontext,
                         opnd_create_reg(sregs.s0),
                         OPND_CREATE_MEM64(tls_reg, LOCAL_READ_PTR_OFFSET)));
    //check if the current entry is equal and greater than the rsptr, if equal, jump to finish
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_jz, opnd_create_instr(finishLabel)));
    //load address in the read set
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s2),
                            OPND_CREATE_MEM64(sregs.s0, 8)));
    //load data from the original memory
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s2),
                            OPND_CREATE_MEM64(sregs.s2, 0)));
    INSERT(bb, trigger,
        INSTR_CREATE_cmp(drcontext,
                         OPND_CREATE_MEM64(sregs.s0, 0),
                         opnd_create_reg(sregs.s2)));
    //if different, jump to rollback
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_jnz, opnd_create_pc((app_pc)rollback)));
    //increment the pointer
    INSERT(bb, trigger,
        INSTR_CREATE_add(drcontext,
                         opnd_create_reg(sregs.s0),
                         OPND_CREATE_INT32(sizeof(spec_item_t))));
    //jump to start of the loop
    INSERT(bb, trigger,
        INSTR_CREATE_jmp(drcontext, opnd_create_instr(loopLabel)));

    INSERT(bb, trigger, finishLabel);
}

void
emit_tx_commit_instrlist(EMIT_CONTEXT, bool save_all)
{
    reg_id_t tls_reg = sregs.s3;

    /* Step 1: commit register changes to shared register bank */
    emit_save_shared_context_instrlist(emit_context, save_all, false);

    /* Step 2: commit memory changes from the transaction */
    instr_t *loopLabel = INSTR_CREATE_label(drcontext);
    instr_t *finishLabel = INSTR_CREATE_label(drcontext);

    //load write set start address
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s0),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_WRITE_SET_OFFSET)));
    //load write set current address
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s1),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_WRITE_PTR_OFFSET)));
    //push tls_reg since we lack of registers now
    INSERT(bb, trigger,
        INSTR_CREATE_push(drcontext, opnd_create_reg(tls_reg)));

    //start of a loop
    INSERT(bb, trigger, loopLabel);
    //cmp s0, s1
    INSERT(bb, trigger,
        INSTR_CREATE_cmp(drcontext,
                         opnd_create_reg(sregs.s0),
                         opnd_create_reg(sregs.s1)));
    //check if it is equal and greater than the wsptr, if equal, jump to finish
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_jz, opnd_create_instr(finishLabel)));
    //load data
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(tls_reg),
                            OPND_CREATE_MEM64(sregs.s0,0)));
    //load address
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s2),
                            OPND_CREATE_MEM64(sregs.s0,8)));
    #if defined(GSTM_VERBOSE) && defined(GSTM_MEM_TRACE)
    dr_insert_clean_call(drcontext, bb, trigger, print_commit_memory, false,2,opnd_create_reg(sregs.s2),opnd_create_reg(sregs.s3));
    #endif
    //commit changes
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(sregs.s2,0),
                            opnd_create_reg(tls_reg)));
    //increment the pointer
    INSERT(bb, trigger,
        INSTR_CREATE_add(drcontext,
                         opnd_create_reg(sregs.s0),
                         OPND_CREATE_INT32(sizeof(spec_item_t))));
    //jump to start of the loop
    INSERT(bb,trigger,INSTR_CREATE_jmp(drcontext,opnd_create_instr(loopLabel)));

    //after this point, the transaction is commited
    INSERT(bb,trigger,finishLabel);
    //recover tls reg
    INSERT(bb, trigger,
        INSTR_CREATE_pop(drcontext, opnd_create_reg(tls_reg)));
}

void
emit_tx_clear_instrlist(EMIT_CONTEXT)
{
    reg_id_t tls_reg = sregs.s3;
    instr_t *flushLabel = INSTR_CREATE_label(drcontext);
    instr_t *finishLabel = INSTR_CREATE_label(drcontext);

    if (!(loop->type & LOOP_SPEC_INLINE)) {
        //load flush table start address
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(sregs.s0),
                                OPND_CREATE_MEM64(tls_reg, LOCAL_FLUSH_TABLE_OFFSET)));
        //load the transaction size
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(sregs.s1),
                                OPND_CREATE_MEM64(tls_reg, LOCAL_TRANS_SIZE_OFFSET)));
        //push tls reg
        INSERT(bb, trigger,
            INSTR_CREATE_push(drcontext, opnd_create_reg(tls_reg)));
        //set tls to zero
        INSERT(bb, trigger,
            INSTR_CREATE_xor(drcontext,
                             opnd_create_reg(tls_reg),
                             opnd_create_reg(tls_reg)));

        INSERT(bb,trigger,flushLabel);

        //check the table size
        INSERT(bb, trigger,
            INSTR_CREATE_test(drcontext,
                              opnd_create_reg(sregs.s1),
                              opnd_create_reg(sregs.s1)));

        //check if transaction size is zero
        INSERT(bb, trigger,
            INSTR_CREATE_jcc(drcontext, OP_jz, opnd_create_instr(finishLabel)));
        //if not zero, clear the current entry
        //decrement size first since the trans_size pointer is post decremented
        INSERT(bb, trigger,
            INSTR_CREATE_sub(drcontext,
                             opnd_create_reg(sregs.s1),
                             OPND_CREATE_INT32(1)));
        //load translation pointer into s2
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(sregs.s2),
                                opnd_create_base_disp(sregs.s0, sregs.s1, 8, 0, OPSZ_8)));
        //set it to zero
        INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                OPND_CREATE_MEM64(sregs.s2,0),
                                opnd_create_reg(sregs.s3)));
        //jmp to top
        INSERT(bb, trigger,
            INSTR_CREATE_jmp(drcontext, opnd_create_instr(flushLabel)));

        //finish commiting
        INSERT(bb,trigger,finishLabel);

        //pop tls reg
        INSERT(bb, trigger,
            INSTR_CREATE_pop(drcontext, opnd_create_reg(tls_reg)));
    }


    //clear both rdptr and wrptr
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s0),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_READ_SET_OFFSET)));
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(tls_reg, LOCAL_READ_PTR_OFFSET),
                            opnd_create_reg(sregs.s0)));
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s0),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_WRITE_SET_OFFSET)));
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(tls_reg, LOCAL_WRITE_PTR_OFFSET),
                            opnd_create_reg(sregs.s0)));

    if (!(loop->type & LOOP_SPEC_INLINE)) {
        //set transaction size to zero
        INSERT(bb, trigger,
            INSTR_CREATE_xor(drcontext,
                             opnd_create_reg(sregs.s1),
                             opnd_create_reg(sregs.s1)));
        INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                OPND_CREATE_MEM64(tls_reg, LOCAL_TRANS_SIZE_OFFSET),
                                opnd_create_reg(sregs.s1)));
    }
}
#endif