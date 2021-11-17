#include "jhash.h"
#include "thread.h"
#include "stm_internal.h"
#include "stm.h"

/* Emit code to generate function like:
 * trans_t *hash_table_lookup(uintptr_t addr)
 * the original address is in s0
 * return the entry to be in s0
 */
instrlist_t *
build_hashtable_lookup_instrlist(void *drcontext, instrlist_t *bb,
                                 instr_t *trigger, SReg sregs,
                                 instr_t *hitLabel) {

    uint32_t sreg0 = sregs.s0;
    uint32_t sreg1 = sregs.s1;
    uint32_t sreg2 = sregs.s2;
    uint32_t sreg3 = sregs.s3;
    reg_id_t tls_reg = sreg3;

    if (bb == NULL) {
        bb = instrlist_create(drcontext);
        /* Jump back to DR's code cache. */
        trigger = INSTR_CREATE_ret(drcontext);
        APPEND(bb, trigger);
    }

    instr_t *returnLabel = INSTR_CREATE_label(drcontext);
    instr_t *loopLabel = INSTR_CREATE_label(drcontext);

    /* ----------------Get hash key ----------------*/
    /* mov s1 <- s0 */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg1),
                            opnd_create_reg(sreg0)));
    /* and s1 <- s1, #HASH_KEY_MASK */
    INSERT(bb, trigger,
        INSTR_CREATE_and(drcontext,
                         opnd_create_reg(sreg1),
                         OPND_CREATE_INT32(HASH_KEY_MASK)));
    /* load s2 = local->trans_table */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg2),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_TRANS_TABLE_OFFSET)));
    /* entry address = s2 + sizeof(trans_t) * s1 = s2 + 24 * s1
       s1 = s2 + 8 * (2 * s1 + s1) */
    /* lea s1 <- [s1 + s1*2] */
    INSERT(bb, trigger,
        INSTR_CREATE_lea(drcontext,
                         opnd_create_reg(sreg1),
                         opnd_create_base_disp(sreg1, sreg1, 2, 0, OPSZ_lea)));
    /* lea s1 <- [s2 + s1*8] */
    INSERT(bb, trigger,
        INSTR_CREATE_lea(drcontext,
                         opnd_create_reg(sreg1),
                         opnd_create_base_disp(sreg2, sreg1, 8, 0, OPSZ_lea)));
    /* ----------------Find match ----------------*/
    /* Now s1 contains the direct entry, load the original address to s2 */
    INSERT(bb, trigger, loopLabel);
    INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(sreg2),
                                OPND_CREATE_MEM64(sreg1, 0)));
    /* Find hit */
    INSERT(bb, trigger,
        INSTR_CREATE_cmp(drcontext,
                         opnd_create_reg(sreg0),
                         opnd_create_reg(sreg2)));
    /* If equal, then we get a hit */
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_jz ,opnd_create_instr(hitLabel)));
    /* If not equal, then check whether it is empty */
    /* test s2 s2 */
    INSERT(bb, trigger,
        INSTR_CREATE_test(drcontext,
                          opnd_create_reg(sreg2),
                          opnd_create_reg(sreg2)));
    /* If zero, then we get an empty hit */
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_jz ,opnd_create_instr(returnLabel)));
    /* If not emptry, increment the translation table 
     * add s1, sizeof(trans_t) */
    INSERT(bb, trigger,
        INSTR_CREATE_add(drcontext,
                         opnd_create_reg(sreg1),
                         OPND_CREATE_INT32(sizeof(trans_t))));
    /* Load the upper bound */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_imm(drcontext,
                             opnd_create_reg(sreg2),
                             OPND_CREATE_INT64(HASH_TABLE_SIZE * sizeof(trans_t))));
    INSERT(bb, trigger,
        INSTR_CREATE_add(drcontext,
                         opnd_create_reg(sreg2),
                         OPND_CREATE_MEM64(tls_reg, LOCAL_TRANS_TABLE_OFFSET)));
    INSERT(bb, trigger,
        INSTR_CREATE_cmp(drcontext,
                         opnd_create_reg(sreg1),
                         opnd_create_reg(sreg2)));
    /* If it is not the same as upper bound, check this entry */
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_jnz, opnd_create_instr(loopLabel)));

    /* If it is the same as upper bound, reload the start of the table */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg1),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_TRANS_TABLE_OFFSET)));
    INSERT(bb, trigger, INSTR_CREATE_jmp(drcontext, opnd_create_instr(loopLabel)));

    /* Now s0 contains the original address
     * s1 contains the pointer to entry
     * and if s2 is zero, then it is a emtry slot */
    INSERT(bb, trigger, returnLabel);
    return bb;
}


instrlist_t *
create_hashtable_redirect_rw_instrlist(void *drcontext, SReg sregs)
{
    instrlist_t *bb;
    instr_t *trigger;
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

    /* we only require s1 to store the entry address */

    //load original address: s0 = [s1]
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg0),
                            OPND_CREATE_MEM64(sreg1, 0)));
    //load current wsptr value
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(sreg2),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_WRITE_PTR_OFFSET)));
    //update wptr.addr
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(sreg2,8),
                            opnd_create_reg(sreg0)));
    //load read ptr: s0
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg0),
                            OPND_CREATE_MEM64(sreg1, 8)));
    //load read data -> s3 = [s2]
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg0),
                            OPND_CREATE_MEM64(sreg0,0)));
    //update wptr.data
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(sreg2,0),
                            opnd_create_reg(sreg0)));
    //update meta info
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(sreg1,16),
                            OPND_CREATE_INT32(1)));
    //update entry redirection to wsptr
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(sreg1, 8),
                            opnd_create_reg(sreg2)));
    //increment write pointer
    INSERT(bb, trigger,
        INSTR_CREATE_add(drcontext,
                         OPND_CREATE_MEM64(tls_reg, LOCAL_WRITE_PTR_OFFSET),
                         OPND_CREATE_INT32(sizeof(spec_item_t))));
    //move s2 to s0
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_reg(sreg0),
                            opnd_create_reg(sreg2)));
    return bb;
}



