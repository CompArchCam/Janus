#include "stm_internal.h"
#include "stm.h"
#include "thread.h"
#include "loop.h"
#include "printer.h"
#include "hashtable.h"
#include "emit.h"

static void write_out_of_bound_error();
static void read_out_of_bound_error();
static void translate_out_of_bound_error();

void infloop()
{
    while(1);
}

//clean call
void jump_to_loop_end(uint64_t loop_end_addr)
{
    dr_mcontext_t mc;
    mc.flags = DR_MC_INTEGER | DR_MC_CONTROL;
    dr_get_mcontext(local->drcontext, &mc);

    //set address
    mc.xip = (void *)loop_end_addr;
    //Redirect to loop return
    dr_redirect_execution(&mc);
}

gtx_t *
get_transaction_header()
{
    return &(local->tx);
}

/* Main code cache of the STM
 * This code cache probes the translation table
 * If not found, iterate through the table until it either hit or an empty space
 * If it is an empty entry, then create an entry in it
 * Scratch 0: original address
 * Scratch 1: entry start address
 * Scratch 2: An empty scratch register
 * Scratch 3: An empty scratch register

 * Return: Scratch 0: redirected address
 */

instrlist_t *build_transaction_start_instrlist(void *drcontext, SReg sregs, Loop_t *loop)
{
    instrlist_t *bb;
    instr_t *trigger, *instr;
    instr_t *start_tx, *respawn_point;
    reg_id_t reg;
    reg_id_t tls_reg = sregs.s3;
    bb = instrlist_create(drcontext);

    /* Jump back to DR's code cache. */
    trigger = INSTR_CREATE_ret(drcontext);
    APPEND(bb, trigger);

    start_tx = INSTR_CREATE_label(drcontext);
    respawn_point = INSTR_CREATE_label(drcontext);

    INSERT(bb,trigger,INSTR_CREATE_int1(drcontext));

    /* Transaction start:
     * Commit previous transaction and start a new one 
     * All four scratch registers are properly spilled
     * scratch s3 contains TLS when loop_on = 1
     */
    //bb = emit_save_eflags(drcontext, bb, trigger, sregs);
    /* get return address, this is to let the rollback remember the return address */
    INSERT(bb, trigger,
        INSTR_CREATE_pop(drcontext, opnd_create_reg(sregs.s0)));
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(tls_reg, LOCAL_RETURN_OFFSET),
                            opnd_create_reg(sregs.s0)));

    reg_id_t sreg32 = reg_64_to_32((reg_id_t)sregs.s0);

    /* Load transaction_on flag and check whether there is un-commited changes */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg32),
                            OPND_CREATE_MEM32(tls_reg, TRANS_ON_FLAG_OFFSET)));
    /* test rax, rax */
    INSERT(bb, trigger,
        INSTR_CREATE_test(drcontext,
                          opnd_create_reg(sreg32),
                          opnd_create_reg(sreg32)));
    /* jz trans_start */
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_jz, opnd_create_instr(start_tx)));

    /* If not zero, jump to commit */
    INSERT(bb, trigger,
        INSTR_CREATE_call_ind(drcontext,
                              opnd_create_rel_addr(&(loop->code.tx_commit), OPSZ_8)));
    /* Now start the transaction */
    INSERT(bb, trigger, start_tx);

    /* Record the current check point */
    emit_save_checkpoint_context_instrlist(emit_context);

    /* save pc to be the re-spawn point */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(tls_reg, LOCAL_CHECK_POINT_OFFSET + PSTATE_PC_OFFSET),
                            opnd_create_instr(respawn_point)));

    /* -------> Respawn <-----------*/
    /* This is where rollback thread respawns with the saved context */
    INSERT(bb, trigger, respawn_point);

    /* Set local->trans_on = 1 */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM32(tls_reg, TRANS_ON_FLAG_OFFSET),
                            OPND_CREATE_INT32(1)));

    /* predict the register value from the current run */
    //bb = emit_predict_value(drcontext, bb, trigger, sregs, loop);

    /* Load return address once the return address once it is from rollback */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s0),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_RETURN_OFFSET)));
    INSERT(bb, trigger,
        INSTR_CREATE_push(drcontext, opnd_create_reg(sregs.s0)));
    PRINTSTAGE(TRANS_START_STAGE);
    return bb;
}

instrlist_t *build_transaction_commit_instrlist(void *drcontext, SReg sregs, Loop_t *loop)
{
    instrlist_t *bb;
    instr_t *trigger, *instr;
    void *rollback = shared->current_loop->code.tx_rollback;

    bb = instrlist_create(drcontext);

    trigger = INSTR_CREATE_ret(drcontext);
    APPEND(bb, trigger);

    /* Step 1: wait until it is the oldest */
    emit_serial_commit_lock(emit_context);

    /* ----------> Critical Region Start <---------- */
    /* Step 2: validate registers */
    bb = build_stm_validate_instrlist(drcontext, bb, trigger, sregs, rollback);

    /* Step 3: commit changes */
    bb = build_stm_commit_instrlist(drcontext, bb, trigger, sregs);

    /* ----------> Critical Region End   <---------- */
    /* Step 4: unlock */
    emit_serial_commit_unlock(emit_context);

    /* Step 5: clear the transaction */
    INSERT(bb, trigger,
        INSTR_CREATE_call_ind(drcontext,
                              opnd_create_rel_addr(&(loop->code.tx_clear), OPSZ_8)));

    /* Step 6: unset the flag */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM32(sregs.s3, TRANS_ON_FLAG_OFFSET),
                            OPND_CREATE_INT32(0)));

    PRINTSTAGE(TRANS_COMMIT_STAGE);
    return bb;
}

instrlist_t *build_transaction_clear_instrlist(void *drcontext, SReg sregs, Loop_t *loop)
{
    instrlist_t *bb;
    instr_t *trigger, *instr;
    reg_id_t tls_reg = sregs.s3;

    bb = instrlist_create(drcontext);

    instr_t *flushLabel = INSTR_CREATE_label(drcontext);
    instr_t *finishLabel = INSTR_CREATE_label(drcontext);

    trigger = INSTR_CREATE_ret(drcontext);
    APPEND(bb, trigger);

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
    //set transaction size to zero
    INSERT(bb, trigger,
        INSTR_CREATE_xor(drcontext,
                         opnd_create_reg(sregs.s1),
                         opnd_create_reg(sregs.s1)));
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(tls_reg, LOCAL_TRANS_SIZE_OFFSET),
                            opnd_create_reg(sregs.s1)));
    return bb;
}

instrlist_t *
build_transaction_wait_instrlist(void *drcontext, SReg sregs, void *rollback, Loop_t *loop)
{
    instrlist_t *bb;
    instr_t *trigger, *instr;
    reg_id_t tls_reg = sregs.s3;

    bb = instrlist_create(drcontext);

    trigger = INSTR_CREATE_ret(drcontext);
    APPEND(bb, trigger);

    /* Step 1: wait until it is the oldest */
    emit_serial_commit_lock(emit_context);

    /* ----------> Critical Region Start <---------- */
    /* Step 2: validate registers */
    bb = build_stm_validate_instrlist(drcontext, bb, trigger, sregs, rollback);

    PRINTSTAGE(TRANS_WAIT_STAGE);
    return bb;
}

static void temp_rollback()
{
    printf("thread %ld rollback\n", local->id);
    exit(-1);
}

instrlist_t *
build_transaction_rollback_instrlist(void *drcontext, SReg sregs, Loop_t *loop)
{
    instrlist_t *bb;
    instr_t *trigger;
    reg_id_t reg;
    reg_id_t tls_reg = sregs.s3;
    bb = instrlist_create(drcontext);
    /* Jump back to DR's code cache. */
    trigger = INSTR_CREATE_ret(drcontext);
    APPEND(bb, trigger);

    if (loop->type & LOOP_SPEC_INLINE) {
        //clear write set
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(sregs.s0),
                                OPND_CREATE_MEM64(tls_reg, LOCAL_WRITE_SET_OFFSET)));
        INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                OPND_CREATE_MEM64(tls_reg, LOCAL_WRITE_PTR_OFFSET),
                                opnd_create_reg(sregs.s0)));

        // load from the check point
        emit_restore_checkpoint_context_instrlist(emit_context);

        //PRINTISTAGE(TRANS_ROLLBACK_STAGE);
        INSERT(bb, trigger,
            INSTR_CREATE_jmp_ind(drcontext,
                                 OPND_CREATE_MEM64(tls_reg,
                                                   LOCAL_CHECK_POINT_OFFSET + PSTATE_PC_OFFSET)));
        return bb;
    }
//INSERT(bb,trigger,INSTR_CREATE_int1(drcontext));
#ifdef JANUS_PRINT_ROLLBACK
    //s2 from shared, s0 from predict
    PRINTABORT(opnd_create_reg(sregs.s1), opnd_create_reg(sregs.s2), opnd_create_reg(sregs.s0));
#endif
    //INSERT(bb,trigger,INSTR_CREATE_int1(drcontext));
    /* Step 1: clear the current transaction */
    INSERT(bb, trigger,
        INSTR_CREATE_call_ind(drcontext,
                              opnd_create_rel_addr(&(loop->code.tx_clear), OPSZ_8)));

    /* Step 2: Restore from the check point machine states */
    for (reg = DR_REG_RAX; reg<= DR_REG_R15; reg++) {
        /* We don't need to save scratch registers */
        if (reg != sregs.s0 &&
            reg != sregs.s1 &&
            reg != sregs.s2 &&
            reg != sregs.s3) {
            INSERT(bb, trigger,
                INSTR_CREATE_mov_ld(drcontext,
                                    opnd_create_reg(reg),
                                    OPND_CREATE_MEM64(tls_reg,
                                        LOCAL_CHECK_POINT_OFFSET + 8*(reg-DR_REG_RAX))));
        }
    }
    /* Step 3: Restore SIMD from the check point */
    uint32_t simd_mask = shared->current_loop->header->dependingSIMDMask;
    if (simd_mask) {
        for (reg = DR_REG_XMM0; reg<= DR_REG_XMM15; reg++) {
            int i = reg - DR_REG_XMM0;
            if ((1<<i) & simd_mask) {
                INSERT(bb, trigger,
                    INSTR_CREATE_movdqu(drcontext,
                                        opnd_create_reg(reg),
                                        opnd_create_base_disp(tls_reg, DR_REG_NULL, 0,
                                            LOCAL_CHECK_POINT_OFFSET + PSTATE_SIMD_OFFSET + i * 16,
                                            OPSZ_16)));
            }
        }
    }

#ifdef JANUS_STATS
    INSERT(bb, trigger,
        INSTR_CREATE_add(drcontext,
                         OPND_CREATE_MEM64(tls_reg, LOCAL_NUM_ROLLBACK_OFFSET),
                         OPND_CREATE_INT32(1)));
#endif

    PRINTSTAGE(TRANS_ROLLBACK_STAGE);
    INSERT(bb, trigger,
        INSTR_CREATE_jmp_ind(drcontext,
                             OPND_CREATE_MEM64(tls_reg,
                                               LOCAL_CHECK_POINT_OFFSET + PSTATE_PC_OFFSET)));
    return bb;
}

instrlist_t *create_hashtable_probe_more_read_instrlist(void *drcontext, SReg sregs)
{
    instrlist_t *bb;
    instr_t *trigger, *instr;
    instr_t *loopLabel, *createEntry, *finishLabel, *noOverflow;
    
    uint32_t sreg0 = sregs.s0;
    uint32_t sreg1 = sregs.s1;
    uint32_t sreg2 = sregs.s2;
    uint32_t sreg3 = sregs.s3;
    reg_id_t tls_reg = sreg3;

    bb = instrlist_create(drcontext);
    
    /* Jump back to DR's code cache. */
    trigger = INSTR_CREATE_ret(drcontext);
    APPEND(bb, trigger);

    loopLabel = INSTR_CREATE_label(drcontext);
    createEntry = INSTR_CREATE_label(drcontext);
    finishLabel = INSTR_CREATE_label(drcontext);
    noOverflow = INSTR_CREATE_label(drcontext);

    //s0 original address -> finally redirect address
    //s1 redirected entry
    //s2 empty -> redirected address
    //s3 tls pointer
    //load data into s1
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg2),
                            OPND_CREATE_MEM64(sreg1,0)));

    //iterate the translate table entry unitil it finds a blank or hit entry 
    INSERT(bb, trigger, loopLabel);
    //test s2 s2
    INSERT(bb, trigger,
        INSTR_CREATE_test(drcontext,
                          opnd_create_reg(sreg2),
                          opnd_create_reg(sreg2)));
    //if zero, jump to entry the entry
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_je, opnd_create_instr(createEntry)));

    //if not zero, increment the translation table
    INSERT(bb, trigger,
        INSTR_CREATE_lea(drcontext,
                         opnd_create_reg(sreg1),
                         opnd_create_base_disp(sreg1, 0, 8, sizeof(trans_t), OPSZ_lea)));
    //check the address of the translation table if it overflows
    //load the upperbound
    INSERT(bb, trigger,
        INSTR_CREATE_mov_imm(drcontext,
                             opnd_create_reg(sreg2),
                             OPND_CREATE_INT64(HASH_TABLE_SIZE * sizeof(trans_t))));
    INSERT(bb, trigger,
        INSTR_CREATE_add(drcontext,
                         opnd_create_reg(sreg2),
                         OPND_CREATE_MEM64(sreg3, LOCAL_TRANS_TABLE_OFFSET)));
    //compare with the upper bound
    INSERT(bb, trigger,
        INSTR_CREATE_cmp(drcontext,
                         opnd_create_reg(sreg1),
                         opnd_create_reg(sreg2)));

    //if it is not the same, skip
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_jnz, opnd_create_instr(noOverflow)));
    //if it is the same, change it to the start of the translation table
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg1),
                            OPND_CREATE_MEM64(sreg3, LOCAL_TRANS_TABLE_OFFSET)));
    //no overflows
    INSERT(bb, trigger, noOverflow);
    //examine this entry s1 <- [s2]
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg2),
                            OPND_CREATE_MEM64(sreg1, 0)));
    //cmp s0, s2
    INSERT(bb, trigger,
        INSTR_CREATE_cmp(drcontext,
                         opnd_create_reg(sreg0),
                         opnd_create_reg(sreg2)));
    //if not the same, jump to the start of the loop
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_jnz, opnd_create_instr(loopLabel)));
    //if it is the same, then s0 <- s1 and return
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg0),
                            OPND_CREATE_MEM64(sreg1,8)));
    //jump to finish
    INSERT(bb, trigger,
        INSTR_CREATE_jmp(drcontext, opnd_create_instr(finishLabel)));

    //create entry label
    INSERT(bb, trigger, createEntry);
    /* Create an entry for the read translation
     * Scratch 0: original address
     * Scratch 1: the address of the empty entry 
     */
    //step 1: fill in the translation entry
    //[s1] = s0
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(sreg1, 0),
                            opnd_create_reg(sreg0)));
    //[s1+16] = 0, trans.info = 0;
    INSERT(bb, trigger,
        INSTR_CREATE_xor(drcontext,
                         opnd_create_reg(sreg2),
                         opnd_create_reg(sreg2)));
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(sreg1, 16),
                            opnd_create_reg(sreg2)));
    //s2 = tx->rsptr;
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg2),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_READ_PTR_OFFSET)));
    //[s1+8] = s2, trans.redirect = tx->rsptr;
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(sreg1, 8),
                            opnd_create_reg(sreg2)));

    //step 2: fill in the read set
    //read_set->addr = addr
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(sreg2, 8),
                            opnd_create_reg(sreg0)));

    //s0 = [s0], load from original memory
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg0),
                            OPND_CREATE_MEM64(sreg0, 0)));
    //read_set->data = s0
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(sreg2, 0),
                            opnd_create_reg(sreg0)));

#ifdef GSTM_BOUND_CHECK
    //load read bound
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg0),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_READ_SET_END_OFFSET)));
    //compare with bound
    INSERT(bb, trigger,
        INSTR_CREATE_cmp(drcontext,
                         OPND_CREATE_MEM64(tls_reg, LOCAL_READ_PTR_OFFSET),
                         opnd_create_reg(sreg0)));
    //skip if less
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_je,
                         opnd_create_pc((app_pc)read_out_of_bound_error)));
#endif
    //step 3: fill in the flush table
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg0),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_FLUSH_TABLE_OFFSET)));
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg2),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_TRANS_SIZE_OFFSET)));
    //flush_table[trans_size] = trans_entry
    //[s0+s2*8] = s1
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_base_disp(sreg0,sreg2, 8, 0, OPSZ_8),
                            opnd_create_reg(sreg1)));
    //trans_size++
    INSERT(bb, trigger,
        INSTR_CREATE_add(drcontext,
            OPND_CREATE_MEM64(tls_reg, LOCAL_TRANS_SIZE_OFFSET),
            OPND_CREATE_INT32(1)));
#ifdef GSTM_BOUND_CHECK
    INSERT(bb, trigger,
        INSTR_CREATE_cmp(drcontext,
            OPND_CREATE_MEM64(tls_reg, LOCAL_TRANS_SIZE_OFFSET),
            OPND_CREATE_INT32(HASH_TABLE_SIZE)));
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_je,
                         opnd_create_pc((app_pc)translate_out_of_bound_error)));
#endif
    //return the actual address
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg0),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_READ_PTR_OFFSET)));

    //increment write pointer
    INSERT(bb, trigger,
        INSTR_CREATE_add(drcontext,
                         OPND_CREATE_MEM64(tls_reg, LOCAL_READ_PTR_OFFSET),
                         OPND_CREATE_INT32(sizeof(spec_item_t))));
    //finish label
    INSERT(bb,trigger,finishLabel);
    //Upon returning, sreg1 must have the newly created entry
    return bb;
}

instrlist_t *create_hashtable_probe_more_write_instrlist(void *drcontext, SReg sregs)
{
    instrlist_t *bb;
    instr_t *trigger, *loopLabel, *createEntry, *finishLabel, *noOverflow;
    instr_t *instr;

    uint32_t sreg0 = sregs.s0;
    uint32_t sreg1 = sregs.s1;
    uint32_t sreg2 = sregs.s2;
    uint32_t sreg3 = sregs.s3;
    reg_id_t tls_reg = sreg3;
    bb = instrlist_create(drcontext);

    /* Jump back to DR's code cache. */
    trigger = INSTR_CREATE_ret(drcontext);
    APPEND(bb, trigger);

    loopLabel = INSTR_CREATE_label(drcontext);
    createEntry = INSTR_CREATE_label(drcontext);
    finishLabel = INSTR_CREATE_label(drcontext);
    noOverflow = INSTR_CREATE_label(drcontext);

    //load data into s1
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg2),
                            OPND_CREATE_MEM64(sreg1,0)));

    //iterate the translate table entry unitil it finds a blank or hit entry 
    INSERT(bb, trigger, loopLabel);
    //test s2 s2
    INSERT(bb, trigger,
        INSTR_CREATE_test(drcontext,
                          opnd_create_reg(sreg2),
                          opnd_create_reg(sreg2)));
    //if zero, jump to entry the entry
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext,OP_je,opnd_create_instr(createEntry)));

    //if not zero, increment the translation table
    INSERT(bb, trigger,
        INSTR_CREATE_lea(drcontext,
                         opnd_create_reg(sreg1),
                         opnd_create_base_disp(sreg1, 0, 8, sizeof(trans_t), OPSZ_lea)));
    //check the address of the translation table if it overflows
    //load the upperbound
    INSERT(bb, trigger,
        INSTR_CREATE_mov_imm(drcontext,
                             opnd_create_reg(sreg2),
                             OPND_CREATE_INT64(HASH_TABLE_SIZE * sizeof(trans_t))));
    INSERT(bb, trigger,
        INSTR_CREATE_add(drcontext,
                         opnd_create_reg(sreg2),
                         OPND_CREATE_MEM64(sreg3, LOCAL_TRANS_TABLE_OFFSET)));
    //compare with the upper bound
    INSERT(bb, trigger,
        INSTR_CREATE_cmp(drcontext,
                         opnd_create_reg(sreg1),
                         opnd_create_reg(sreg2)));

    //if it is not the same, skip
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_jnz, opnd_create_instr(noOverflow)));
    //if it is the same, change it to the start of the translation table
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg1),
                            OPND_CREATE_MEM64(sreg3, LOCAL_TRANS_TABLE_OFFSET)));
    //no overflows
    INSERT(bb, trigger, noOverflow);

    //examine this entry s1 <- [s2]
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg2),
                            OPND_CREATE_MEM64(sreg1, 0)));
    //cmp s0, s2
    INSERT(bb, trigger,
        INSTR_CREATE_cmp(drcontext,
                         opnd_create_reg(sreg0),
                         opnd_create_reg(sreg2)));
    //if not the same, jump to the start of the loop
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_jnz, opnd_create_instr(loopLabel)));
    //if it is the same, then s0 <- s1 and return
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg0),
                            OPND_CREATE_MEM64(sreg1,8)));
    //jump to finish
    INSERT(bb, trigger,
        INSTR_CREATE_jmp(drcontext, opnd_create_instr(finishLabel)));

    //create entry label
    INSERT(bb, trigger, createEntry);
    /* Create an entry for the write translation
     * Scratch 0: original address
     * Scratch 1: the address of the empty entry 
     */
    //step 1: fill in the translation entry
    //[s1] = s0
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(sreg1, 0),
                            opnd_create_reg(sreg0)));
    //[s1+16] = 0, trans.info = 0;
    INSERT(bb, trigger,
        INSTR_CREATE_xor(drcontext,
                         opnd_create_reg(sreg2),
                         opnd_create_reg(sreg2)));
    INSERT(bb, trigger,
        INSTR_CREATE_add(drcontext,
                         opnd_create_reg(sreg2),
                         OPND_CREATE_INT32(1)));
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(sreg1, 16),
                            opnd_create_reg(sreg2)));
    //s2 = tx->wsptr;
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg2),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_WRITE_PTR_OFFSET)));
    //[s1+8] = s2, trans.redirect = tx->wsptr;
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(sreg1, 8),
                            opnd_create_reg(sreg2)));
    //write_set->addr = addr
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(sreg2, 8),
                            opnd_create_reg(sreg0)));
    //step 2: fill in the write set
    //read from the real memory
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg0),
                            OPND_CREATE_MEM64(sreg0, 0)));
    //write_set->data = [s0]
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(sreg2, 0),
                            opnd_create_reg(sreg0)));
#ifdef GSTM_BOUND_CHECK
    //load read bound
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg0),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_WRITE_SET_END_OFFSET)));
    //compare with bound
    INSERT(bb, trigger,
        INSTR_CREATE_cmp(drcontext,
                         OPND_CREATE_MEM64(tls_reg, LOCAL_WRITE_PTR_OFFSET),
                         opnd_create_reg(sreg0)));
    //skip if less
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_je,
                         opnd_create_pc((app_pc)write_out_of_bound_error)));
#endif
    //step 3: fill in the flush table
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg0),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_FLUSH_TABLE_OFFSET)));
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg2),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_TRANS_SIZE_OFFSET)));
    //flush_table[trans_size] = trans_entry
    //[s0+s2*8] = s1
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_base_disp(sreg0,sreg2, 8, 0, OPSZ_8),
                            opnd_create_reg(sreg1)));
    //trans_size++
    INSERT(bb, trigger,
        INSTR_CREATE_add(drcontext,
            OPND_CREATE_MEM64(tls_reg, LOCAL_TRANS_SIZE_OFFSET),
            OPND_CREATE_INT32(1)));
#ifdef GSTM_BOUND_CHECK
    INSERT(bb, trigger,
        INSTR_CREATE_cmp(drcontext,
            OPND_CREATE_MEM64(tls_reg, LOCAL_TRANS_SIZE_OFFSET),
            OPND_CREATE_INT32(HASH_TABLE_SIZE)));
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_je,
                         opnd_create_pc((app_pc)translate_out_of_bound_error)));
#endif
    //return the actual address
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg0),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_WRITE_PTR_OFFSET)));

    //increment write pointer
    INSERT(bb, trigger,
        INSTR_CREATE_add(drcontext,
                         OPND_CREATE_MEM64(tls_reg, LOCAL_WRITE_PTR_OFFSET),
                         OPND_CREATE_INT32(sizeof(spec_item_t))));
    //dr_insert_clean_call(drcontext, bb, trigger, testhoho, false, 1, opnd_create_reg(sreg1));
    //finish label
    INSERT(bb,trigger,finishLabel);
    return bb;
}

/* Predict the value from the shared data structure */
instrlist_t *
emit_predict_value(void *drcontext, instrlist_t *bb, instr_t *trigger, SReg sregs, Loop_t *loop)
{
    int i;
    reg_id_t tls_reg = sregs.s3;
    uint32_t mask;

    PCAddress shared_addr = (PCAddress)(&(shared->registers.rax));

    instr_t *distanceKnown = INSTR_CREATE_label(drcontext);
    instr_t *distanceOverflow = INSTR_CREATE_label(drcontext);
    instr_t *distanceNotKnown = INSTR_CREATE_label(drcontext);
    instr_t *oldestPredict = INSTR_CREATE_label(drcontext);
    instr_t *finishPredict = INSTR_CREATE_label(drcontext);
    /* The mask should not include the scratch register
     * Assume s3 contains the TLS pointer
     * calculate the distance to oldest thread,
     * store the distance to s2 */
#ifdef CIRCULAR_DISTANCE
    INSERT(bb, trigger,
        INSTR_CREATE_xor(drcontext,
                         opnd_create_reg(sregs.s2),
                         opnd_create_reg(sregs.s2)));
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s1),
                            opnd_create_reg(tls_reg)));

    INSERT(bb, trigger, distanceNotKnown);

    /* Check whether this the the oldest */
    INSERT(bb, trigger,
        INSTR_CREATE_cmp(drcontext,
                         OPND_CREATE_MEM32(sregs.s1, LOCAL_FLAG_CAN_COMMIT),
                         OPND_CREATE_INT32(1)));
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_je, opnd_create_instr(distanceKnown)));

    INSERT(bb, trigger,
        INSTR_CREATE_add(drcontext,
                         opnd_create_reg(sregs.s2),
                         OPND_CREATE_INT32(1)));
    /* Compare with the number of threads */
    INSERT(bb, trigger,
        INSTR_CREATE_cmp(drcontext,
                         opnd_create_reg(sregs.s2),
                         OPND_CREATE_INT32(ginfo.number_of_threads)));
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_je, opnd_create_instr(distanceOverflow)));
    /* Load next TLS */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s1),
                            OPND_CREATE_MEM64(sregs.s1, LOCAL_PREV_OFFSET)));
    INSERT(bb, trigger,
        INSTR_CREATE_jmp(drcontext, opnd_create_instr(distanceNotKnown)));

    INSERT(bb, trigger, distanceOverflow);
    INSERT(bb, trigger,
        INSTR_CREATE_xor(drcontext,
                         opnd_create_reg(sregs.s2),
                         opnd_create_reg(sregs.s2)));
#else
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s2),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_STAMP_OFFSET)));
    INSERT(bb, trigger,
        INSTR_CREATE_sub(drcontext,
                         opnd_create_reg(sregs.s2),
                         opnd_create_rel_addr((void *)&(shared->global_stamp), OPSZ_8)));
#endif
    INSERT(bb, trigger, distanceKnown);

    /* After this point, distance is known 
     * if distance is zero, jump to oldest predict */
    /* Save distance */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(tls_reg, LOCAL_DISTANCE_OFFSET),
                            opnd_create_reg(sregs.s2)));
    INSERT(bb, trigger,
        INSTR_CREATE_test(drcontext,
                          opnd_create_reg(sregs.s2),
                          opnd_create_reg(sregs.s2)));
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_jz, opnd_create_instr(oldestPredict)));

    /* Then we preform prediction for each of the variable */
    for (i=0; i<loop->num_of_variables; i++) {
        JVarProfile profile = loop->variables[i];
        GVar var = profile.var;
#ifdef GSTM_VERBOSE
        print_var_profile(&profile);
#endif
        bb = emit_predict_variable_value(drcontext, bb, trigger, sregs, &profile);
    }
    /* Finish prediction */
    INSERT(bb, trigger,
        INSTR_CREATE_jmp(drcontext, opnd_create_instr(finishPredict)));

    INSERT(bb, trigger, oldestPredict);
    mask = shared->current_loop->header->dependingGPRMask;
    for (i=0; i<16; i++) {
        reg_id_t reg = DR_REG_RAX + i;
        if (reg == DR_REG_RSP) continue;
        if ((1 << i) & mask) {
            int slot = -1;
            if (reg == sregs.s0) slot = 0;
            else if (reg == sregs.s1) slot = 1;
            else if (reg == sregs.s2) slot = 2;
            else if (reg == sregs.s3) {
                slot = 3;
                reg = sregs.s0;
            }
            //load from the shared register
            INSERT(bb, trigger,
                INSTR_CREATE_mov_ld(drcontext,
                                    opnd_create_reg(reg),
                                    opnd_create_rel_addr((void *)(shared_addr + CACHE_LINE_WIDTH * i), OPSZ_8)));
            //store to private buffer
            INSERT(bb, trigger,
                INSTR_CREATE_mov_st(drcontext,
                                    OPND_CREATE_MEM64(sregs.s3, LOCAL_STATE_OFFSET + 8 * i),
                                    opnd_create_reg(reg)));
            //if it is scratch register, spill to slots too
            if (slot != -1)
                INSERT(bb, trigger,
                    INSTR_CREATE_mov_st(drcontext,
                                        OPND_CREATE_MEM64(sregs.s3, slot * 8),
                                        opnd_create_reg(reg)));
        }
    }

    mask = shared->current_loop->header->dependingSIMDMask;
    for (i=0; i<16; i++) {
        reg_id_t reg = DR_REG_XMM0 + i;
        if ((1<<i) & mask) {
            //load from the shared register
            INSERT(bb, trigger,
                INSTR_CREATE_movdqu(drcontext,
                                    opnd_create_reg(reg),
                                    opnd_create_rel_addr((void *)(shared_addr + (i+16)*CACHE_LINE_WIDTH), OPSZ_16)));
            //store to private buffer
            INSERT(bb, trigger,
                INSTR_CREATE_movdqu(drcontext,
                                    opnd_create_base_disp(sregs.s3, DR_REG_NULL, 0, LOCAL_STATE_OFFSET + PSTATE_SIMD_OFFSET + 16 * i, OPSZ_16),
                                    opnd_create_reg(reg)));
        }
    }
    INSERT(bb, trigger, finishPredict);
    PRINTSTAGE(TRANS_PREDICT_STAGE);
    return bb;
}

/* Emit the code to predict the value for a register/stack variable */
instrlist_t *
emit_predict_variable_value(void *drcontext, instrlist_t *bb,
                            instr_t *trigger, SReg sregs, JVarProfile *profile)
{
    PCAddress shared_addr = (PCAddress)(&(shared->registers.rax));
    PredictFunction prediction_function;
    instr_t *hitLabel;
    GVar var = profile->var;
    reg_id_t sreg0 = sregs.s0;
    reg_id_t sreg1 = sregs.s1;
    reg_id_t sreg2 = sregs.s2;
    reg_id_t tls_reg = sregs.s3;

    /* Distance d is in register s2 */
    switch (var.type) {
    case VAR_REG:
        if (var.value >= DR_REG_RAX && var.value <= DR_REG_R15) {
            /* Load shared->register.var (init value) */
            INSERT(bb, trigger,
                INSTR_CREATE_mov_ld(drcontext,
                                    opnd_create_reg(var.value),
                                    opnd_create_rel_addr((void *)(shared_addr + CACHE_LINE_WIDTH * (var.value - DR_REG_RAX)), OPSZ_8)));

            /* Re-evaluate distance */
            INSERT(bb, trigger,
                INSTR_CREATE_mov_ld(drcontext,
                                    opnd_create_reg(sregs.s2),
                                    OPND_CREATE_MEM64(tls_reg, LOCAL_STAMP_OFFSET)));
            INSERT(bb, trigger,
                INSTR_CREATE_sub(drcontext,
                                 opnd_create_reg(sregs.s2),
                                 opnd_create_rel_addr((void *)&(shared->global_stamp), OPSZ_8)));

            if (profile->type == INDUCTION_PROFILE) {
                /* my_offset =  d * offset */
                INSERT(bb, trigger,
                    INSTR_CREATE_imul_imm(drcontext,
                                          opnd_create_reg(sreg1),
                                          opnd_create_reg(sreg2),
                                          OPND_CREATE_INT32(profile->induction.stride)));

                /* Based on the operation, we have add and sub for the moment */
                if (profile->induction.op == 1) {
                    INSERT(bb, trigger,
                        INSTR_CREATE_add(drcontext,
                                         opnd_create_reg(var.value),
                                         opnd_create_reg(sreg1)));
                } else if (profile->induction.op == 2) {
                    INSERT(bb, trigger,
                        INSTR_CREATE_sub(drcontext,
                                         opnd_create_reg(var.value),
                                         opnd_create_reg(sreg1)));
                }
            }
            /* store the predicted value to the read buffer */
            INSERT(bb, trigger,
                INSTR_CREATE_mov_st(drcontext,
                                    OPND_CREATE_MEM64(tls_reg, LOCAL_STATE_OFFSET + 8 * (var.value - DR_REG_RAX)),
                                    opnd_create_reg(var.value)));
        } else if (var.value >= DR_REG_XMM0 && var.value <= DR_REG_XMM15) {
            //load from the shared register
            INSERT(bb, trigger,
                INSTR_CREATE_movdqu(drcontext,
                                    opnd_create_reg(var.value),
                                    opnd_create_rel_addr((void *)(shared_addr + (var.value-DR_REG_XMM0)*CACHE_LINE_WIDTH), OPSZ_16)));
            //store to private buffer
            INSERT(bb, trigger,
                INSTR_CREATE_movdqu(drcontext,
                                    opnd_create_base_disp(sregs.s3, DR_REG_NULL, 0,
                                                          LOCAL_STATE_OFFSET + PSTATE_SIMD_OFFSET + 16 * (var.value-DR_REG_XMM0), OPSZ_16),
                                    opnd_create_reg(var.value)));
        }
    break;
    case VAR_STK:
        prediction_function = (profile->type == INDUCTION_PROFILE) ?
                              induction_prediction :
                              direct_load_prediction;
        hitLabel = INSTR_CREATE_label(drcontext);

        /* My stack variable is at shared_stk + offset */
        /* load shared stack pointer to s0 */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(sregs.s0),
                                opnd_create_rel_addr(&(shared->stack_ptr), OPSZ_8)));
        /* lea s0, [s0 + offset] */
        INSERT(bb, trigger,
            INSTR_CREATE_lea(drcontext,
                             opnd_create_reg(sregs.s0),
                             OPND_CREATE_MEM_lea(sregs.s0, DR_REG_NULL, 1, (profile->var.value))));
        /* Performs the spec_write */
        if (profile->var.size != 8)
            bb = build_stm_access_partial_instrlist(drcontext, bb, trigger, sregs,
                                                    STM_READ_AND_WRITE,
                                                    prediction_function,
                                                    profile);
        else
            bb = build_stm_access_full_instrlist(drcontext, bb, trigger, sregs,
                                                 STM_READ_AND_WRITE,
                                                 prediction_function,
                                                 profile, true);
        /* restore the distance */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(sregs.s2),
                                OPND_CREATE_MEM64(tls_reg, LOCAL_DISTANCE_OFFSET)));
    break;
    case VAR_STK_BASE:

        prediction_function = (profile->type == INDUCTION_PROFILE) ?
                                               induction_prediction :
                                               direct_load_prediction;
        hitLabel = INSTR_CREATE_label(drcontext);

        /* My stack variable is at shared_stk + offset */
        /* load shared stack pointer to s0 */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(sregs.s0),
                                opnd_create_rel_addr(&(shared->stack_base), OPSZ_8)));
        /* lea s0, [s0 + offset] */
        INSERT(bb, trigger,
            INSTR_CREATE_lea(drcontext,
                             opnd_create_reg(sregs.s0),
                             OPND_CREATE_MEM_lea(sregs.s0, DR_REG_NULL, 1, -(profile->var.value))));
        /* Performs the spec_write */
        if (profile->var.size != 8)
            bb = build_stm_access_partial_instrlist(drcontext, bb, trigger, sregs,
                                                    STM_READ_AND_WRITE,
                                                    prediction_function,
                                                    profile);
        else
            bb = build_stm_access_full_instrlist(drcontext, bb, trigger, sregs,
                                                 STM_READ_AND_WRITE,
                                                 prediction_function,
                                                 profile, true);
        /* restore the distance */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(sregs.s2),
                                OPND_CREATE_MEM64(tls_reg, LOCAL_DISTANCE_OFFSET)));
    break;
    default:

    break;
    }
    return bb;
}

#ifdef HOHO
instrlist_t *
emit_save_eflags(void *drcontext, instrlist_t *bb, instr_t *trigger, SReg sregs)
{
    /* Move rax to scratch reg 0 */
    if(sregs.s0 != DR_REG_RAX) {
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(sregs.s0),
                                opnd_create_reg(DR_REG_RAX)));
    }

    /* load eflags */
    dr_save_arith_flags_to_xax(drcontext, bb, trigger);

    /* Save flags to private context */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(sregs.s3, LOCAL_CHECK_POINT_OFFSET + offsetof())
                            opnd_create_rel_addr(&(mc->state.rflags),OPSZ_4),
                            opnd_create_reg(DR_REG_RAX)));

    /* recover rax */
    if(scratch_reg0 != DR_REG_RAX) {
        INSERT(bb,trigger,INSTR_CREATE_mov_ld(drcontext,opnd_create_reg(DR_REG_RAX),opnd_create_reg(scratch_reg0)));
    }

    return bb;
}

instrlist_t *
emit_restore_eflags(void *drcontext, instrlist_t *bb, instr_t *trigger, SReg sregs)
{
    /* Move rax to scratch reg 0 */
    if(sregs.s0 != DR_REG_RAX) {
        INSERT(bb,trigger,INSTR_CREATE_mov_ld(drcontext,opnd_create_reg(scratch_reg0),opnd_create_reg(DR_REG_RAX)));
    }

    /* Restore xflag from the local state */
    INSERT(bb,trigger,INSTR_CREATE_mov_ld(drcontext,opnd_create_reg(DR_REG_EAX),opnd_create_rel_addr(&(mc->state.rflags),OPSZ_4)));

    dr_restore_arith_flags_from_xax(drcontext, bb, trigger);

    /* recover rax */
    if(scratch_reg0 != DR_REG_RAX) {
        INSERT(bb,trigger,INSTR_CREATE_mov_ld(drcontext,opnd_create_reg(DR_REG_RAX),opnd_create_reg(scratch_reg0)));
    }

    return bb;
}
#endif /* 0 */
/* Validate the initial read of the registers and memories */
instrlist_t *
build_stm_validate_instrlist(void *drcontext, instrlist_t *bb, instr_t *trigger, SReg sregs, void *rollback)
{
    instr_t *failure;
    int i;
    reg_id_t tls_reg = sregs.s3;
    PCAddress shared_addr = (PCAddress)(&(shared->registers.rax));

    if(bb == NULL) {
        bb = instrlist_create(drcontext);
        if(trigger == NULL) {
            /* Jump back to DR's code cache. */
            trigger = INSTR_CREATE_ret(drcontext);
            APPEND(bb, trigger);
        }
    }
    
    uint32_t mask = shared->current_loop->header->dependingGPRMask;

    /* Validation of registers is based on the register mask */
    /* First validate all the registers */
    for (i=0; i<16; i++) {
        reg_id_t  reg = DR_REG_RAX + i;
        if (reg == DR_REG_RSP) continue;
        if ((1 << i) & mask) {
            opnd_t spec_opnd = OPND_CREATE_MEM64(tls_reg, LOCAL_STATE_OFFSET + 8 * i);
            opnd_t shared_opnd = opnd_create_rel_addr((void *)(shared_addr + i * CACHE_LINE_WIDTH), OPSZ_8);
            //load from speculative buffer
            INSERT(bb, trigger,
                INSTR_CREATE_mov_ld(drcontext,
                                    opnd_create_reg(sregs.s0),
                                    spec_opnd));
            //compare with shared register
            INSERT(bb, trigger,
                INSTR_CREATE_cmp(drcontext,
                                 opnd_create_reg(sregs.s0),
                                 shared_opnd));
#ifdef JANUS_STATS
            INSERT(bb, trigger,
                INSTR_CREATE_mov_imm(drcontext,
                                     opnd_create_reg(sregs.s1),
                                     OPND_CREATE_INT32(reg)));
            INSERT(bb, trigger,
                INSTR_CREATE_mov_ld(drcontext,
                                     opnd_create_reg(sregs.s2),
                                     shared_opnd));
#endif
            //if not equal, jump to failure
            INSERT(bb, trigger,
                INSTR_CREATE_jcc(drcontext, OP_jnz, opnd_create_pc((app_pc)rollback)));
        }
    }

    /* Second validate depending simd register */
    uint32_t simd_mask = shared->current_loop->header->dependingSIMDMask;
    if (simd_mask) {
        for (i=0; i<16; i++) {
            if ((1<<i) & mask) {
                opnd_t spec_opnd1 = OPND_CREATE_MEM64(tls_reg, LOCAL_STATE_OFFSET + PSTATE_SIMD_OFFSET + i*16);
                opnd_t spec_opnd2 = OPND_CREATE_MEM64(tls_reg, LOCAL_STATE_OFFSET + PSTATE_SIMD_OFFSET + i*16 + 8);
                opnd_t shared_opnd1 = opnd_create_rel_addr((void *)(shared_addr + (i+16) * CACHE_LINE_WIDTH), OPSZ_8);
                opnd_t shared_opnd2 = opnd_create_rel_addr((void *)(shared_addr + (i+16) * CACHE_LINE_WIDTH + 8), OPSZ_8);
                //load from speculative buffer
                INSERT(bb, trigger,
                    INSTR_CREATE_mov_ld(drcontext,
                                        opnd_create_reg(sregs.s0),
                                        spec_opnd1));
#ifdef JANUS_STATS
            INSERT(bb, trigger,
                INSTR_CREATE_mov_imm(drcontext,
                                     opnd_create_reg(sregs.s1),
                                     OPND_CREATE_INT32(i+16)));
            INSERT(bb, trigger,
                INSTR_CREATE_mov_ld(drcontext,
                                     opnd_create_reg(sregs.s2),
                                     shared_opnd1));
#endif
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
#ifdef JANUS_STATS
            INSERT(bb, trigger,
                INSTR_CREATE_mov_imm(drcontext,
                                     opnd_create_reg(sregs.s1),
                                     OPND_CREATE_INT32(i+16)));
            INSERT(bb, trigger,
                INSTR_CREATE_mov_ld(drcontext,
                                     opnd_create_reg(sregs.s2),
                                     shared_opnd2));
#endif
                //compare with shared register
                INSERT(bb, trigger,
                    INSTR_CREATE_cmp(drcontext,
                                     opnd_create_reg(sregs.s0),
                                     shared_opnd2));
                //if not equal, jump to failure
                INSERT(bb, trigger,
                    INSTR_CREATE_jcc(drcontext, OP_jnz, opnd_create_pc((app_pc)rollback)));
            }
        }
    }

    /* Next validate all memory reads */
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
#ifdef JANUS_STATS
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s1),
                            OPND_CREATE_MEM64(sregs.s0, 8)));
#endif
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

    return bb;
}


instrlist_t *
build_stm_commit_instrlist(void *drcontext, instrlist_t *bb, instr_t *trigger, SReg sregs)
{
    int i;
    /* Step 1: commit register changes to shared register structure */
    uint32_t mask = shared->current_loop->header->dependingGPRMask;
    PCAddress shared_addr = (PCAddress)(&(shared->registers.rax));
    reg_id_t tls_reg = sregs.s3;

    /* First commit all the registers */
    for (i=0; i<16; i++) {
        reg_id_t  reg = DR_REG_RAX + i;
        if (reg == DR_REG_RSP) continue;
        int slot = -1;
        if (reg == sregs.s0) slot = 0;
        else if (reg == sregs.s1) slot = 1;
        else if (reg == sregs.s2) slot = 2;
        else if (reg == sregs.s3) slot = 3;

        if ((1 << i) & mask) {
            opnd_t shared_opnd = opnd_create_rel_addr((void *)(shared_addr + i * CACHE_LINE_WIDTH), OPSZ_8);
            if (slot != -1) {
                opnd_t priv_opnd = OPND_CREATE_MEM64(tls_reg, slot * 8);
                reg = sregs.s0;
                //for scratch register, we need to load from the TLS slot
                INSERT(bb, trigger,
                    INSTR_CREATE_mov_ld(drcontext,
                                        opnd_create_reg(reg),
                                        priv_opnd));
            }

            //store to shared registers
            INSERT(bb, trigger,
                INSTR_CREATE_mov_st(drcontext,
                                    shared_opnd,
                                    opnd_create_reg(reg)));
#ifdef GSTM_MEM_TRACE
            dr_insert_clean_call(drcontext, bb, trigger, (void *)print_commit_reg, false, 2, OPND_CREATE_INT64(reg), opnd_create_reg(reg));
#endif
        }
    }

    /* Second commit simd registers */
    mask = shared->current_loop->header->dependingGPRMask;
    for (i=0; i<16; i++) {
        reg_id_t  reg = DR_REG_XMM0 + i;
        if ((1 << i) & mask) {
            opnd_t shared_opnd = opnd_create_rel_addr((void *)(shared_addr + (i+16) * CACHE_LINE_WIDTH), OPSZ_16);
            INSERT(bb, trigger,
                INSTR_CREATE_movdqu(drcontext,
                                    shared_opnd,
                                    opnd_create_reg(reg)));
        }
    }

    /* Step 2: commit memory changes */
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

    return bb;
}


/* sreg0 : app's memory address */
instrlist_t *
build_stm_read_partial_instrlist(void *drcontext, SReg sregs)
{
    instrlist_t *bb;
    instr_t *trigger, *instr;
    uint32_t sreg0 = sregs.s0;
    uint32_t sreg1 = sregs.s1;
    uint32_t sreg2 = sregs.s2;
    uint32_t sreg3 = sregs.s3;
    reg_id_t tls_reg = sreg3;
    bb = instrlist_create(drcontext);
    instr_t *returnLabel = INSTR_CREATE_label(drcontext);
    instr_t *skipLabel = INSTR_CREATE_label(drcontext);
    /* Jump back to DR's code cache. */
    trigger = INSTR_CREATE_ret(drcontext);
    APPEND(bb, trigger);

    //s3 = original address
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_reg(sreg2),
                            opnd_create_reg(sreg0)));
    //s3 = s3 & 0b111 get the least three significant bits
    INSERT(bb, trigger,
        INSTR_CREATE_and(drcontext,
                         opnd_create_reg(sreg2),
                         OPND_CREATE_INT32(HASH_KEY_ALIGH_MASK)));
    //push the bits onto the stack
    INSERT(bb, trigger,
        INSTR_CREATE_push(drcontext, opnd_create_reg(sreg2)));
    //trim the address
    INSERT(bb, trigger,
        INSTR_CREATE_sub(drcontext,
                         opnd_create_reg(sreg0),
                         opnd_create_reg(sreg2)));

    /* s0: trimmed address
     * s1: empty -> key -> redirected address
     * s2: empty -> table start
     */

    //mov s1 <- s0
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_reg(sreg1),
                            opnd_create_reg(sreg0)));

    //get the key to the translation table (lower 16-bits): sreg1 = key
    //and s1 <- s1, #HASH_KEY_MASK
    INSERT(bb, trigger,
        INSTR_CREATE_and(drcontext,
                         opnd_create_reg(sreg1),
                         OPND_CREATE_INT32(HASH_KEY_MASK)));

    //load the translation table start address
    //s2 = table_addr
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg2),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_TRANS_TABLE_OFFSET)));
    //load the translation entry address
    //entry address = s2 + sizeof(trans_t) * s1 = 24 * s1 + s2
    //lea s1 <- [s1 + s1*2]
    INSERT(bb, trigger,
        INSTR_CREATE_lea(drcontext,
                         opnd_create_reg(sreg1),
                         opnd_create_base_disp(sreg1, sreg1, 2, 0, OPSZ_lea)));
    //lea s2 <- [s2 + s1*8]
    INSERT(bb, trigger,
        INSTR_CREATE_lea(drcontext,
                         opnd_create_reg(sreg1),
                         opnd_create_base_disp(sreg2, sreg1, 8, 0, OPSZ_lea)));
    //compare the hash table, s0 and [s1]
    INSERT(bb, trigger,
        INSTR_CREATE_cmp(drcontext,
                         opnd_create_reg(sreg0),
                         OPND_CREATE_MEM64(sreg1,0)));

    //if equal, skip the probe_more procedure
    //XXX: optimise to jump later
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_jz ,opnd_create_instr(skipLabel)));

    //start calling probe more function
    //jump to the probe more procedure
    INSERT(bb, trigger,
        INSTR_CREATE_call(drcontext,
                          opnd_create_pc(local->current_code_cache->probe_more_read)));

    //the skipped probe mode code comes to here
    //s0: trimmed address
    //s1: redirected entry (hit)
    INSERT(bb, trigger, skipLabel);
    //load from the translation table
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg0),
                            OPND_CREATE_MEM64(sreg1,8)));

    /* Add offsets to the address */
    //restore previously stored least significant 3 bits and add to the redirected address
    INSERT(bb, trigger,
        INSTR_CREATE_pop(drcontext, opnd_create_reg(sreg2)));
    INSERT(bb, trigger,
        INSTR_CREATE_add(drcontext,
                         opnd_create_reg(sreg0),
                         opnd_create_reg(sreg2)));

    #ifdef GSTM_MEM_TRACE
    PRINTSTAGE(SPEC_READ_PART_STAGE);
    #endif
    return bb;
}

instrlist_t *
build_stm_write_partial_instrlist(void *drcontext, SReg sregs)
{
    instrlist_t *bb;
    instr_t *trigger, *instr;
    uint32_t sreg0 = sregs.s0;
    uint32_t sreg1 = sregs.s1;
    uint32_t sreg2 = sregs.s2;
    uint32_t sreg3 = sregs.s3;
    reg_id_t tls_reg = sreg3;

    bb = instrlist_create(drcontext);
    instr_t *returnLabel = INSTR_CREATE_label(drcontext);
    instr_t *skipLabel = INSTR_CREATE_label(drcontext);
    instr_t *skipLabel2 = INSTR_CREATE_label(drcontext);
    /* Jump back to DR's code cache. */
    trigger = INSTR_CREATE_ret(drcontext);
    APPEND(bb, trigger);

    //s3 = original address
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_reg(sreg2),
                            opnd_create_reg(sreg0)));
    //s3 = s3 & 0b111 get the least three significant bits
    INSERT(bb, trigger,
        INSTR_CREATE_and(drcontext,
                         opnd_create_reg(sreg2),
                         OPND_CREATE_INT32(HASH_KEY_ALIGH_MASK)));
    //push the bits onto the stack
    INSERT(bb, trigger,
        INSTR_CREATE_push(drcontext, opnd_create_reg(sreg2)));
    //trim the address
    INSERT(bb, trigger,
        INSTR_CREATE_sub(drcontext,
                         opnd_create_reg(sreg0),
                         opnd_create_reg(sreg2)));

    /* s0: trimmed address
     * s1: empty -> key -> redirected address
     * s2: empty -> table start
     */

    //mov s1 <- s0
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_reg(sreg1),
                            opnd_create_reg(sreg0)));

    //get the key to the translation table (lower 16-bits): sreg1 = key
    //and s1 <- s1. #0xffff
    INSERT(bb, trigger,
        INSTR_CREATE_and(drcontext,
                         opnd_create_reg(sreg1),
                         OPND_CREATE_INT32(HASH_KEY_MASK)));

    //load the translation table start address
    //s2 = table_addr
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg2),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_TRANS_TABLE_OFFSET)));
    //load the translation entry address
    //entry address = s2 + sizeof(trans_t) * s1 = 24 * s1 + s2
    //lea s1 <- [s1 + s1*2]
    INSERT(bb, trigger,
        INSTR_CREATE_lea(drcontext,
                         opnd_create_reg(sreg1),
                         opnd_create_base_disp(sreg1, sreg1, 2, 0, OPSZ_lea)));
    //lea s2 <- [s2 + s1*8]
    INSERT(bb, trigger,
        INSTR_CREATE_lea(drcontext,
                         opnd_create_reg(sreg1),
                         opnd_create_base_disp(sreg2, sreg1, 8, 0, OPSZ_lea)));

    //compare the hash table, s0 and [s1]
    INSERT(bb, trigger,
        INSTR_CREATE_cmp(drcontext,
                         opnd_create_reg(sreg0),
                         OPND_CREATE_MEM64(sreg1,0)));
    //if equal, skip the probe_more procedure
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_jz, opnd_create_instr(skipLabel)));

    //start calling probe more function
    INSERT(bb, trigger,
        INSTR_CREATE_call(drcontext,
                          opnd_create_pc(local->current_code_cache->probe_more_write)));

    //the skipped probe mode code comes to here
    //s0: trimmed address
    //s1: redirected entry (hit)
    INSERT(bb,trigger,skipLabel);

    //s2 <- [s1+0x10]
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg2),
                            OPND_CREATE_MEM64(sreg1, offsetof(trans_t, rw))));
    //test s0, s0
    INSERT(bb, trigger,
        INSTR_CREATE_test(drcontext,
                          opnd_create_reg(sreg2),
                          opnd_create_reg(sreg2)));
    //if not zero, skip the migrate from read to write procedure
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_jnz ,opnd_create_instr(skipLabel2)));

    //if zero, it means it is never written before. Migrate the data from read set to write set
    //call redirect read to write set procedure
    INSERT(bb, trigger,
        INSTR_CREATE_call(drcontext,
                          opnd_create_pc(local->current_code_cache->move_read_write)));

    INSERT(bb,trigger,skipLabel2);

    //load from the translation table
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,opnd_create_reg(sreg0),OPND_CREATE_MEM64(sreg1,8)));

    /* This is the point where we get the redirected address and all book keeping is finished
     * scratch register 0 stores the redirected address */
    INSERT(bb,trigger,returnLabel);

    /* Add offsets to the address */
    //restore previously stored least significant 3 bits and add to the redirected address
    INSERT(bb, trigger,
        INSTR_CREATE_pop(drcontext,opnd_create_reg(sreg2)));
    INSERT(bb, trigger,
        INSTR_CREATE_add(drcontext,
                         opnd_create_reg(sreg0),
                         opnd_create_reg(sreg2)));
    #ifdef GSTM_MEM_TRACE
    PRINTSTAGE(SPEC_WRITE_PART_STAGE);
    #endif
    return bb;
}

instrlist_t *
build_stm_read_full_instrlist(void *drcontext, SReg sregs)
{
    instrlist_t *bb;
    instr_t *trigger, *instr;
    uint32_t sreg0 = sregs.s0;
    uint32_t sreg1 = sregs.s1;
    uint32_t sreg2 = sregs.s2;
    uint32_t sreg3 = sregs.s3;
    reg_id_t tls_reg = sreg3;

    bb = instrlist_create(drcontext);
    instr_t *returnLabel = INSTR_CREATE_label(drcontext);
    instr_t *skipLabel = INSTR_CREATE_label(drcontext);
    /* Jump back to DR's code cache. */
    trigger = INSTR_CREATE_ret(drcontext);
    APPEND(bb, trigger);
    //mov s1 <- s0
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_reg(sreg1),
                            opnd_create_reg(sreg0)));

    //get the key to the translation table (lower 16-bits): sreg1 = key
    //and s1 <- s1, #HASH_KEY_MASK
    INSERT(bb, trigger,
        INSTR_CREATE_and(drcontext,
                         opnd_create_reg(sreg1),
                         OPND_CREATE_INT32(HASH_KEY_MASK)));

    //load the translation table start address
    //s2 = table_addr
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg2),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_TRANS_TABLE_OFFSET)));
    //load the translation entry address
    //entry address = s2 + sizeof(trans_t) * s1 = 24 * s1 + s2
    //lea s1 <- [s1 + s1*2]
    INSERT(bb, trigger,
        INSTR_CREATE_lea(drcontext,
                         opnd_create_reg(sreg1),
                         opnd_create_base_disp(sreg1, sreg1, 2, 0, OPSZ_lea)));
    //lea s2 <- [s2 + s1*8]
    INSERT(bb, trigger,
        INSTR_CREATE_lea(drcontext,
                         opnd_create_reg(sreg1),
                         opnd_create_base_disp(sreg2, sreg1, 8, 0, OPSZ_lea)));

    //compare the hash table, s0 and [s1]
    INSERT(bb, trigger,
        INSTR_CREATE_cmp(drcontext,
                         opnd_create_reg(sreg0),
                         OPND_CREATE_MEM64(sreg1,0)));

    //if equal, skip the probe_more procedure
    //XXX: optimise to jump later
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_jz ,opnd_create_instr(skipLabel)));

    //start calling probe more function
    //jump to the probe more procedure
    INSERT(bb, trigger,
        INSTR_CREATE_call(drcontext,
                          opnd_create_pc(local->current_code_cache->probe_more_read)));
    INSERT(bb, trigger, INSTR_CREATE_ret(drcontext));
    INSERT(bb, trigger, skipLabel);
    //load from the translation table
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg0),
                            OPND_CREATE_MEM64(sreg1,8)));
    // This is the point where we get the redirected address and all book keeping is finished
    // scratch register 0 stores the redirected address
    INSERT(bb,trigger,returnLabel);

    #ifdef GSTM_MEM_TRACE
    PRINTSTAGE(SPEC_READ_FULL_STAGE);
    #endif
    return bb;
}

instrlist_t *
build_stm_write_full_instrlist(void *drcontext, SReg sregs)
{
    instrlist_t *bb;
    instr_t *trigger, *instr;
    uint32_t sreg0 = sregs.s0;
    uint32_t sreg1 = sregs.s1;
    uint32_t sreg2 = sregs.s2;
    uint32_t sreg3 = sregs.s3;
    reg_id_t tls_reg = sreg3;

    bb = instrlist_create(drcontext);
    instr_t *returnLabel = INSTR_CREATE_label(drcontext);
    instr_t *skipLabel = INSTR_CREATE_label(drcontext);
    instr_t *skipLabel2 = INSTR_CREATE_label(drcontext);

    /* Jump back to DR's code cache. */
    trigger = INSTR_CREATE_ret(drcontext);
    APPEND(bb, trigger);

    //mov s1 <- s0
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_reg(sreg1),
                            opnd_create_reg(sreg0)));

    //get the key to the translation table (lower 16-bits): sreg1 = key
    //and s1 <- s1, #HASH_KEY_MASK
    INSERT(bb, trigger,
        INSTR_CREATE_and(drcontext,
                         opnd_create_reg(sreg1),
                         OPND_CREATE_INT32(HASH_KEY_MASK)));

    //load the translation table start address
    //s2 = table_addr
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg2),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_TRANS_TABLE_OFFSET)));
    //load the translation entry address
    //entry address = s2 + sizeof(trans_t) * s1 = 24 * s1 + s2
    //lea s1 <- [s1 + s1*2]
    INSERT(bb, trigger,
        INSTR_CREATE_lea(drcontext,
                         opnd_create_reg(sreg1),
                         opnd_create_base_disp(sreg1, sreg1, 2, 0, OPSZ_lea)));
    //lea s2 <- [s2 + s1*8]
    INSERT(bb, trigger,
        INSTR_CREATE_lea(drcontext,
                         opnd_create_reg(sreg1),
                         opnd_create_base_disp(sreg2, sreg1, 8, 0, OPSZ_lea)));

    //compare the hash table, s0 and [s1]
    INSERT(bb, trigger,
        INSTR_CREATE_cmp(drcontext,
                         opnd_create_reg(sreg0),
                         OPND_CREATE_MEM64(sreg1,0)));

    //if equal, skip the probe_more procedure
    //XXX: optimise to jump later
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_jz ,opnd_create_instr(skipLabel)));

    //start calling probe more function
    //jump to the probe more procedure
    INSERT(bb, trigger,
        INSTR_CREATE_call(drcontext,
                          opnd_create_pc(local->current_code_cache->probe_more_write)));
    INSERT(bb, trigger, INSTR_CREATE_ret(drcontext));
    INSERT(bb, trigger, skipLabel);
    //s2 <- [s1+0x10]
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg2),
                            OPND_CREATE_MEM64(sreg1, offsetof(trans_t, rw))));
    //test s0, s0
    INSERT(bb, trigger,
        INSTR_CREATE_test(drcontext,
                          opnd_create_reg(sreg2),
                          opnd_create_reg(sreg2)));
    //if not zero, skip the migrate from read to write procedure
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_jnz ,opnd_create_instr(skipLabel2)));

    //if zero, it means it is never written before. Migrate the data from read set to write set
    //call redirect read to write set procedure
    INSERT(bb, trigger,
        INSTR_CREATE_call(drcontext,
                          opnd_create_pc(local->current_code_cache->move_read_write)));

    INSERT(bb,trigger,skipLabel2);

    //load from the translation table
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg0),
                            OPND_CREATE_MEM64(sreg1,8)));

    #ifdef GSTM_MEM_TRACE
    PRINTSTAGE(SPEC_WRITE_FULL_STAGE);
    #endif
    return bb;
}

/* Merge of the reduction variable */
instrlist_t *
build_merge_reduction_variable(void *drcontext, instrlist_t *bb, instr_t *trigger, GBitMask reductionMask)
{
    int i;
    PCAddress shared_addr = (PCAddress)(&(shared->registers.rax));
    for (i=0; i<16; i++) {
        reg_id_t reg = DR_REG_RAX + i;
        if (reg == DR_REG_RSP) continue;
        if(reductionMask & (1<<i)) {
            /* XXX: For the moment, we only have move reduction */
            INSERT(bb,trigger,
                INSTR_CREATE_mov_st(drcontext,
                                    opnd_create_rel_addr((void *)(shared_addr + i*CACHE_LINE_WIDTH), OPSZ_8),
                                    opnd_create_reg(reg)));
        }
    }
    return bb;
}

uint32_t pop_register(uint32_t *regMask)
{
    uint32_t value = (uint8_t)(*regMask);

    *regMask = ((*regMask - value) >> 8);

    return value;
}

void print_stage(int stage)
{
    switch(stage) {
    case TRANS_START_STAGE:
        printf("Thread %ld transaction started\n",local->id);
        break;
    case TRANS_RESTART_STAGE:
        printf("Thread %ld transaction restarted\n",local->id);
        break;
    case TRANS_VALIDATE_STAGE:
        printf("Thread %ld transaction validated\n",local->id);
        break;
    case TRANS_COMMIT_STAGE:
        printf("Thread %ld transaction committed\n",local->id);
        break;
    case TRANS_ROLLBACK_STAGE:
        printf("Thread %ld transaction roll-back\n",local->id);
        break;
    case TRANS_WAIT_STAGE:
        printf("Thread %ld transaction wait\n",local->id);
        break;
    case LOOP_INIT_STAGE:
        printf("Thread %ld loop %d initialised\n",local->id, shared->current_loop->header->staticID);
        break;
    case LOOP_END_STAGE:
        printf("Thread %ld loop finished\n",local->id);
        break;
    case SPEC_READ_PART_STAGE:
        printf("Thread %ld speculative partial read\n",local->id);
        break;
    case SPEC_WRITE_PART_STAGE:
        printf("Thread %ld speculative partial write\n",local->id);
        break;
    case SPEC_READ_FULL_STAGE:
        printf("Thread %ld speculative full read\n",local->id);
        break;
    case SPEC_WRITE_FULL_STAGE:
        printf("Thread %ld speculative full write\n",local->id);
        break;
    case CALL_START_STAGE:
        printf("Thread %ld call start\n",local->id);
        break;
    case CALL_END_STAGE:
        printf("Thread %ld call end\n",local->id);
        break;
    case CALL_HEAD_STAGE:
        printf("Thread %ld call header\n",local->id);
        break;
    case CALL_EXIT_STAGE:
        printf("Thread %ld call exit\n",local->id);
        break;
    case TRANS_PREDICT_STAGE:
        printf("Thread %ld transaction predict\n",local->id);
        break;
    default:
        printf("Thread %ld haha\n",local->id);
    }
}

#ifdef GSTM_VERBOSE
void print_commit_memory(uint64_t addr, uint64_t value)
{
    printf("commit [%lx] = %lx\n",addr,value);
}

void print_commit_reg(uint64_t reg, uint64_t value)
{
    printf("commit %s = %lx or %ld\n",get_register_name(reg),value,value);
}

void printAddress(uint64_t id,uint64_t bbAddr)
{
    uint64_t addr = bbAddr - (bbAddr%8);
    //printf("%lx\n",bbAddr);
    printf("Thread %ld id %ld addr %lx data %lx\n",local->id,id ,addr,*(uint64_t *)addr);
}

void printAddressData(uint64_t id, uint64_t raddr)
{
    uint64_t addr = raddr - (raddr%8);
    printf("Thread %ld id %ld redirected_addr %lx data %lx\n\n",local->id,id ,addr,*(uint64_t *)addr);
}

void printSpill(uint64_t id, uint64_t reg, uint64_t mode, uint64_t value)
{
    if (mode) {
        printf("id %ld recover %s : %lx\n", id, get_register_name(reg), value);
    } else {
        printf("id %ld spill %s : %lx\n", id, get_register_name(reg), value);
    }
}
#endif

void print_scratch(uint64_t s0, uint64_t s1, uint64_t s2, uint64_t s3)
{
    printf("s0 %lx s1 %lx s2 %lx s3 %lx\n",s0,s1,s2,s3);
}

void print_spill_id(int fid, reg_id_t s0, reg_id_t s1, reg_id_t s2, reg_id_t s3)
{
    printf("Function %d spill %s %s %s %s\n", fid, get_register_name(s0),
            get_register_name(s1), get_register_name(s2), get_register_name(s3));
}

void print_recover_id(int fid, reg_id_t s0, reg_id_t s1, reg_id_t s2, reg_id_t s3)
{
    printf("Function %d recover %s %s %s %s\n", fid, get_register_name(s0),
            get_register_name(s1), get_register_name(s2), get_register_name(s3));
}

void print_value(uint64_t ptr)
{
    printf("thread %ld value %lx\n",local->id,*(uint64_t *)ptr);
}

void print_reg(uint64_t reg, uint64_t value)
{
    printf("thread %ld %s: %lx\n",local->id,get_register_name(reg),value);
}

void print_rollback_reg(uint64_t reg, uint64_t value, uint64_t pred)
{
    if(reg<DR_REG_INVALID) {
        printf("Thread %ld rollback on %s: found value %lx predict value %lx\n",local->id,get_register_name(reg),value,pred);
    }
    else {
        uint64_t predict_value = *((uint64_t *)(pred));
        printf("Thread %ld rollback on memory 0x%lx: found value %lx predict value %lx\n",local->id,reg,value,predict_value);
    }
}

int timesh = 0;

void print_state()
{
    dr_mcontext_t mc;
    mc.size = sizeof(dr_mcontext_t);
    mc.flags = DR_MC_ALL;
 timesh++;
    //copy the machine state to my private copy
    dr_get_mcontext(dr_get_current_drcontext(),&mc);
 
    printf("\n-----------------------------------------------------\n");
    printf("rax\t\t%lx\nrbx\t\t%lx\nrcx\t\t%lx\nrdx\t\t%lx\n",mc.rax,mc.rbx, mc.rcx, mc.rdx);
    printf("rsi\t\t%lx\nrdi\t\t%lx\nrbp\t\t%lx\nrsp\t\t%lx\n",mc.rsi,mc.rdi, mc.rbp, mc.rsp);
    printf("r8\t\t%lx\nr9\t\t%lx\nr10\t\t%lx\nr11\t\t%lx\n",mc.r8,mc.r9, mc.r10, mc.r11);
    printf("r12\t\t%lx\nr13\t\t%lx\nr14\t\t%lx\nr15\t\t%lx\n",mc.r12,mc.r13, mc.r14, mc.r15);
    printf("-----------------------------------------------------\n");
    
    //if(timesh >2)
    //exit(-1);
}

static void
read_out_of_bound_error()
{
    gtx_t *tx = get_transaction_header();
    int size = ((uint64_t)tx->rsptr - (uint64_t)tx->read_set)/ sizeof(spec_item_t);
    printf("STM read set reaches size limit %d, please increase default read set size\n", size);
    exit(-1);
}

static void
write_out_of_bound_error()
{
    gtx_t *tx = get_transaction_header();
    int size = ((uint64_t)tx->wsptr - (uint64_t)tx->write_set)/ sizeof(spec_item_t);
    printf("STM write set reaches size limit %d, please increase default write set size\n", size);
    exit(-1);
}

static void
translate_out_of_bound_error()
{
    gtx_t *tx = get_transaction_header();
    printf("STM hash table reaches size limit %ld, please increase the width of hash mask\n", tx->trans_size);
    exit(-1);
}

/* Prediction callbacks
 * S0 stores the original address,
 * S1 stores the entry to read set,
 * S2 stores the entry to write set
 * S3 stores the tls
 * Return value:
 * Put the predicted value in S0 */
instrlist_t *
direct_load_prediction(void *drcontext, instrlist_t *bb,
                       instr_t *trigger, SReg sregs, JVarProfile *profile,
                       bool full)
{
    /* Load directly from memory */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s0),
                            OPND_CREATE_MEM64(sregs.s0, 0)));
    return bb;
}

instrlist_t *
induction_prediction(void *drcontext, instrlist_t *bb,
                     instr_t *trigger, SReg sregs, JVarProfile *profile,
                     bool full)
{
    /* Load directly from memory */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s0),
                            OPND_CREATE_MEM64(sregs.s0, 0)));
    /* Spill two registers */
    INSERT(bb, trigger,
        INSTR_CREATE_push(drcontext, opnd_create_reg(sregs.s1)));
    INSERT(bb, trigger,
        INSTR_CREATE_push(drcontext, opnd_create_reg(sregs.s2)));
    /* Load distance */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s2),
                            OPND_CREATE_MEM64(sregs.s3, LOCAL_DISTANCE_OFFSET)));
    /* my_offset =  d * offset */
    INSERT(bb, trigger,
           INSTR_CREATE_imul_imm(drcontext,
                                 opnd_create_reg(sregs.s1),
                                 opnd_create_reg(sregs.s2),
                                 OPND_CREATE_INT32(profile->induction.stride)));
    if (!full) {
        /* shift value based on address offset */
        if (sregs.s2 != DR_REG_RCX) {
            dr_save_reg(drcontext, bb, trigger, DR_REG_RCX, SPILL_SLOT_1);
        }
        /* Load offset to CL register
         * the offset was pushed previously at offset 24 */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(DR_REG_RCX),
                                OPND_CREATE_MEM64(DR_REG_RSP, 24)));
        /* offset should have a scale of 8 */
        INSERT(bb, trigger,
               INSTR_CREATE_shl(drcontext,
                                opnd_create_reg(DR_REG_RCX),
                                OPND_CREATE_INT8(3)));
        /* Shift the value of s1 */
        INSERT(bb, trigger,
               INSTR_CREATE_shl(drcontext,
                                opnd_create_reg(sregs.s1),
                                opnd_create_reg(DR_REG_CL)));
        if (sregs.s2 != DR_REG_RCX) {
            dr_restore_reg(drcontext, bb, trigger, DR_REG_RCX, SPILL_SLOT_1);
        }
    }

    /* Based on the operation, we have add and sub for the moment */
    if (profile->induction.op == 1) {
        INSERT(bb, trigger,
            INSTR_CREATE_add(drcontext,
                             opnd_create_reg(sregs.s0),
                             opnd_create_reg(sregs.s1)));
    } else if (profile->induction.op == 2) {
        INSERT(bb, trigger,
            INSTR_CREATE_sub(drcontext,
                             opnd_create_reg(sregs.s0),
                             opnd_create_reg(sregs.s1)));
    }
    /* Now we finishes prediction, restore the values */
    INSERT(bb, trigger,
        INSTR_CREATE_pop(drcontext, opnd_create_reg(sregs.s2)));
    INSERT(bb, trigger,
        INSTR_CREATE_pop(drcontext, opnd_create_reg(sregs.s1)));
    return bb;
}

instrlist_t *
build_stm_value_prediction_instrlist(void *drcontext, instrlist_t *bb,
                                     instr_t *trigger, SReg sregs,
                                     int mode, PredictFunction prediction,
                                     JVarProfile *profile,
                                     bool full)
{
    reg_id_t sreg0 = sregs.s0;
    reg_id_t sreg1 = sregs.s1;
    reg_id_t sreg2 = sregs.s2;
    reg_id_t tls_reg = sregs.s3;

    /* s0 - original address, s1 - entry address, s2 - rw/entry */
    if (mode == STM_READ_ONLY) {
        INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                OPND_CREATE_MEM64(sreg2, offsetof(spec_item_t, addr)),
                                opnd_create_reg(sreg0)));

        /* --------- Prediction policy inserted here -----------*/
        bb = prediction(drcontext, bb, trigger, sregs, profile, full);
        /* --------- Prediction policy finishes here -----------*/

        /* Write to read set */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                OPND_CREATE_MEM64(sreg2, 0),
                                opnd_create_reg(sreg0)));
        /* Increment read set */
        INSERT(bb, trigger,
            INSTR_CREATE_add(drcontext,
                             OPND_CREATE_MEM64(tls_reg, LOCAL_READ_PTR_OFFSET),
                             OPND_CREATE_INT32(sizeof(spec_item_t))));
    } else if (mode == STM_WRITE_ONLY) {
        INSERT(bb, trigger,
                INSTR_CREATE_mov_st(drcontext,
                                    OPND_CREATE_MEM64(sreg2, offsetof(spec_item_t, addr)),
                                    opnd_create_reg(sreg0)));
        /* We still need load directly from memory for partial writes */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(sreg0),
                                OPND_CREATE_MEM64(sreg0, 0)));
        /* Write to write set */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                OPND_CREATE_MEM64(sreg2, 0),
                                opnd_create_reg(sreg0)));
        /* Increment write set */
        INSERT(bb, trigger,
            INSTR_CREATE_add(drcontext,
                             OPND_CREATE_MEM64(tls_reg, LOCAL_WRITE_PTR_OFFSET),
                             OPND_CREATE_INT32(sizeof(spec_item_t))));
    } else {
        //update both read and write sets
        INSERT(bb, trigger,
            INSTR_CREATE_push(drcontext, opnd_create_reg(sreg1)));
        /* Load read sets */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(sreg1),
                                OPND_CREATE_MEM64(tls_reg, LOCAL_READ_PTR_OFFSET)));
        /* Write addr to read set */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                OPND_CREATE_MEM64(sreg1, 8),
                                opnd_create_reg(sreg0)));
        /* Write addr to write set */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                OPND_CREATE_MEM64(sreg2, 8),
                                opnd_create_reg(sreg0)));

        /* --------- Prediction policy inserted here -----------*/
        bb = prediction(drcontext, bb, trigger, sregs, profile, full);
        /* --------- Prediction policy finishes here -----------*/

        /* Write to read set */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                OPND_CREATE_MEM64(sreg1, 0),
                                opnd_create_reg(sreg0)));
        /* Write to write set */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                OPND_CREATE_MEM64(sreg2, 0),
                                opnd_create_reg(sreg0)));
        /* Increment read set */
        INSERT(bb, trigger,
            INSTR_CREATE_add(drcontext,
                             OPND_CREATE_MEM64(tls_reg, LOCAL_READ_PTR_OFFSET),
                             OPND_CREATE_INT32(sizeof(spec_item_t))));
        /* Increment write set */
        INSERT(bb, trigger,
            INSTR_CREATE_add(drcontext,
                             OPND_CREATE_MEM64(tls_reg, LOCAL_WRITE_PTR_OFFSET),
                             OPND_CREATE_INT32(sizeof(spec_item_t))));
        /* Restore the entry */
        INSERT(bb, trigger,
            INSTR_CREATE_pop(drcontext, opnd_create_reg(sreg1)));
    }
    return bb;
}

/* Create an entry for the translation while performing value prediction */
instrlist_t *
build_stm_create_entry_instrlist(void *drcontext, instrlist_t *bb,
                                 instr_t *trigger, SReg sregs,
                                 int mode, PredictFunction prediction,
                                 JVarProfile *profile,
                                 bool full)
{
    reg_id_t sreg0 = sregs.s0;
    reg_id_t sreg1 = sregs.s1;
    reg_id_t sreg2 = sregs.s2;
    reg_id_t tls_reg = sregs.s3;

    /* s0 - original address, s1 - entry address */
    /* entry->original_addr = s0 */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(sreg1, offsetof(trans_t, original_addr)),
                            opnd_create_reg(sreg0)));
    if (mode == STM_READ_ONLY) {
        /* entry->rw = 0 */
        INSERT(bb, trigger,
            INSTR_CREATE_xor(drcontext,
                             opnd_create_reg(sreg2),
                             opnd_create_reg(sreg2)));
        INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                OPND_CREATE_MEM64(sreg1, 16),
                                opnd_create_reg(sreg2)));
        /* s2 = local->read_ptr */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(sreg2),
                                OPND_CREATE_MEM64(tls_reg, LOCAL_READ_PTR_OFFSET)));
    } else {
        /* entry->rw = 1 */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                OPND_CREATE_MEM32(sreg1, 16),
                                OPND_CREATE_INT32(1)));
        /* s2 = local->write_ptr */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(sreg2),
                                OPND_CREATE_MEM64(tls_reg, LOCAL_WRITE_PTR_OFFSET)));
    }
    /* entry->redirect_addr = s2 */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(sreg1, offsetof(trans_t, redirect_addr)),
                            opnd_create_reg(sreg2)));

    /* Predict the value which is going to put in the entry */
    bb = build_stm_value_prediction_instrlist(drcontext, bb, trigger,
                                              sregs, mode, prediction, profile, full);

    /* Fill in the flush table */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg0),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_FLUSH_TABLE_OFFSET)));
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg2),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_TRANS_SIZE_OFFSET)));
    /* flush_table[trans_size] = trans_entry */
    //[s0+s2*8] = s1
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_base_disp(sreg0, sreg2, 8, 0, OPSZ_8),
                            opnd_create_reg(sreg1)));
    /* trans_size++ */
    INSERT(bb, trigger,
        INSTR_CREATE_add(drcontext,
            OPND_CREATE_MEM64(tls_reg, LOCAL_TRANS_SIZE_OFFSET),
            OPND_CREATE_INT32(1)));
#ifdef GSTM_BOUND_CHECK
    INSERT(bb, trigger,
        INSTR_CREATE_cmp(drcontext,
            OPND_CREATE_MEM64(tls_reg, LOCAL_TRANS_SIZE_OFFSET),
            OPND_CREATE_INT32(HASH_TABLE_SIZE)));
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_je,
                         opnd_create_pc((app_pc)translate_out_of_bound_error)));
#endif
    return bb;
}

/* Perform speculative access partially on a STM word */
instrlist_t *
build_stm_access_partial_instrlist(void *drcontext, instrlist_t *bb,
                                   instr_t *trigger, SReg sregs,
                                   int mode, PredictFunction prediction,
                                   JVarProfile *profile)
{
    reg_id_t sreg0 = sregs.s0;
    reg_id_t sreg1 = sregs.s1;
    reg_id_t sreg2 = sregs.s2;
    reg_id_t tls_reg = sregs.s3;

    if (bb == NULL) {
        bb = instrlist_create(drcontext);
        /* Jump back to DR's code cache. */
        trigger = INSTR_CREATE_ret(drcontext);
        APPEND(bb, trigger);
    }

    instr_t *skipLabel = INSTR_CREATE_label(drcontext);
    instr_t *hitLabel = INSTR_CREATE_label(drcontext);

    /* Since it is a partial read, get the lowest 3-bits offset */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg2),
                            opnd_create_reg(sreg0)));
    /* and s2, #HASH_KEY_ALIGH_MASK */
    INSERT(bb, trigger,
            INSTR_CREATE_and(drcontext,
                             opnd_create_reg(sreg2),
                             OPND_CREATE_INT32(HASH_KEY_ALIGH_MASK)));
    /* trim the address */
    INSERT(bb, trigger,
            INSTR_CREATE_sub(drcontext,
                             opnd_create_reg(sreg0),
                             opnd_create_reg(sreg2)));
    /* Spill the bits onto the stack
     * since there is not enough register */
    INSERT(bb, trigger,
        INSTR_CREATE_push(drcontext, opnd_create_reg(sreg2)));

    /* Perform full access */
    bb = build_stm_access_full_instrlist(drcontext, bb, trigger,
                                         sregs, mode, prediction, profile,
                                         false);

    /* Add offsets to the address */
    INSERT(bb, trigger,
        INSTR_CREATE_pop(drcontext, opnd_create_reg(sreg2)));
    INSERT(bb, trigger,
        INSTR_CREATE_add(drcontext,
                         opnd_create_reg(sreg0),
                         opnd_create_reg(sreg2)));
    return bb;
}

/* Perform a full speculative access on a STM word */
instrlist_t *
build_stm_access_full_instrlist(void *drcontext, instrlist_t *bb,
                                instr_t *trigger, SReg sregs,
                                int mode, PredictFunction prediction,
                                JVarProfile *profile,
                                bool full)
{
    instr_t *instr;
    reg_id_t sreg0 = sregs.s0;
    reg_id_t sreg1 = sregs.s1;
    reg_id_t sreg2 = sregs.s2;
    reg_id_t tls_reg = sregs.s3;

    if (bb == NULL) {
        bb = instrlist_create(drcontext);
        /* Jump back to DR's code cache. */
        trigger = INSTR_CREATE_ret(drcontext);
        APPEND(bb, trigger);
    }

    instr_t *skipLabel = INSTR_CREATE_label(drcontext);
    instr_t *hitLabel = INSTR_CREATE_label(drcontext);

    bb = build_hashtable_lookup_instrlist(drcontext, bb, trigger, sregs, hitLabel);

    /* After lookup, s0 - original address, s1 - entry, s2 - empty */
    /* To this point, we have an empty entry and therefore create an entry */
    bb = build_stm_create_entry_instrlist(drcontext, bb, trigger,
                                          sregs, mode, prediction, profile,
                                          full);
    /* After this, the prediction value is buffered
     * and s1 contains the entry */

    INSERT(bb, trigger, hitLabel);

    /* Specifically for writes, we need to check the rw flag */
    if (mode != STM_READ_ONLY) {
        /* Load the rw flag */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(reg_64_to_32((reg_id_t)sreg2)),
                                OPND_CREATE_MEM32(sreg1, offsetof(trans_t, rw))));
        /* test s2, s2 */
        INSERT(bb, trigger,
            INSTR_CREATE_test(drcontext,
                              opnd_create_reg(sreg2),
                              opnd_create_reg(sreg2)));
        INSERT(bb, trigger,
            INSTR_CREATE_jcc(drcontext, OP_jnz, opnd_create_instr(skipLabel)));
        /* if zero, it means it is never written before.
         * Migrate the data from read set to write set */
        INSERT(bb, trigger,
            INSTR_CREATE_call(drcontext,
                opnd_create_pc(local->current_code_cache->move_read_write)));
    }

    INSERT(bb, trigger, skipLabel);
    /* load from the translation table */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg0),
                            OPND_CREATE_MEM64(sreg1,8)));
    return bb;
}
