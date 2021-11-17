#include "loop.h"
#include "jthread.h"
#include "emit.h"
#include "iterator.h"
#include "control.h"
#include <stddef.h>

#include "janus_arch.h"

#ifdef JANUS_STATS
  #include "stats.h"
#endif

/* Generate the shared loop code */
static void generate_shared_loop_code(void *drcontext, loop_t *loop);

/* Generate the thread private loop code */
static void generate_thread_private_loop_code(void *drcontext, loop_t *loop, int tid);

/* This function is called by all the threads (except the main thread) */
void
janus_generate_loop_code(void *drcontext, void *tls)
{
    int i;
    janus_thread_t *local = (janus_thread_t *)tls;
    int tid = local->id;
    RSchedHeader *header = rsched_info.header;
    RSLoopHeader *loop_header = rsched_info.loop_header;
    /* Step 1:
     * Wait for the main thread to update the loop structure */
    while (!shared->loops);
    loop_t *loops = shared->loops;

    if (tid == shared->warden_thread) {
        /* Step 2: for warden thread, generate the shared code for each loop */
        for (i=0; i<header->numLoops; i++) {
            /* Generate shared_code for this loop */
            generate_shared_loop_code(drcontext, &loops[i]);

            //mark the shared loop code is ready
            shared->code_ready = 1;
        }
    }

    //all the janus threads simply generate the code for itself for each loop
    for (i=0; i<header->numLoops; i++) {
        generate_thread_private_loop_code(drcontext, loops+i, tid);
    }
}

static void
generate_shared_loop_code(void *drcontext, loop_t *loop)
{
    instrlist_t *bb = NULL;

    /* Loop init procedure executed by the main thread */
    bb = build_loop_init_instrlist(drcontext, loop);
    loop->loop_init = generate_runtime_code(drcontext, PAGE_SIZE, bb);

    /* Loop finish procedure for the main thread */
    bb = build_loop_finish_instrlist(drcontext, loop);
    loop->loop_finish = generate_runtime_code(drcontext, PAGE_SIZE, bb);
}

static void
generate_thread_private_loop_code(void *drcontext, loop_t *loop, int tid)
{
    int lid = loop->dynamic_id;
    instrlist_t *bb = NULL;

    /* Loop init procedure executed by the main thread */
    bb = build_thread_loop_init_instrlist(drcontext, loop, tid);
    oracle[tid]->gen_code[lid].thread_loop_init = generate_runtime_code(drcontext, PAGE_SIZE, bb);

    /* Loop finish procedure for the main thread */
    bb = build_thread_loop_finish_instrlist(drcontext, loop, tid);
    oracle[tid]->gen_code[lid].thread_loop_finish = generate_runtime_code(drcontext, PAGE_SIZE, bb);
}

/* Initialise loop parallelisation
 * This handler is called by the main thread
 * and it is thread private */
void loop_init_handler(JANUS_CONTEXT)
{
    instr_t *trigger = get_trigger_instruction(bb,rule);
    instr_t *returnLabel = INSTR_CREATE_label(drcontext);

    //retrieve the current loop (loop id stored in rule reg0)
    loop_t *loop = &(shared->loops[rule->reg0]);

    /* From the start of the loop until the end of the loop
     * we steal two least used registers to maintain our control
     * register s0: shared stack pointer
     * register s1: thread local storage pointer
     */
    reg_id_t s0 = loop->header->scratchReg0;
    reg_id_t s1 = loop->header->scratchReg1;
    reg_id_t tls = s1;

#ifdef SAFE_RUNTIME_CHECK
    /* Switch to single-threaded mode if runtime check fails */
    if (shared->runtime_check_fail)
        ginfo.number_of_threads = 1;
#endif

    /* wait for the code to finish generation */
    while (!shared->code_ready);

#ifdef JANUS_X86
    //retrieve thread local storage
    janus_thread_t *local = dr_get_tls_field(drcontext);

    //If we have a conditional jump that jumps or falls through to the loop,
    //Static Janus will attach the rewrite rule to the jcc instruction
    //We then add some more jumps to ensure loop init code is executed only
    //when the jcc would actually go to the loop
    //Don't do this for regular jumps, since then we want the loop init code before the jmp
    int trig_opcode = instr_get_opcode(trigger);
    bool mustRestoreEflags = false;
    if (instr_is_cbr(trigger))
    {
        //This is the case where we jump into the loop via a conditional jump (or pass through into it)
        //We assemble something like:
        //A: *save EFLAGS*
        //B: jcc C
        //C: *loop init code*
        //D: *restore EFLAGS*
        //E: jcc loop (original trigger)
        //The opcode of B will be inverted from that of E if jumping goes to loop (reg1 == 1)
        DR_ASSERT_MSG(rule->reg1 == 1 || rule->reg1 == 2, 
            "PARA_LOOP_INIT is attached to conditional jump, but no information on if jumping or not jumping goes to loop!\n");
        bool jumpGoesToLoop = (rule->reg1 == 1);

        //We first remove and replace the original trigger jcc instruction
        //Because then it's located in the DynamoRIO instruction locations with a high address
        //And is accessible by a 32-bit jump distance from our new jcc instruction
        instr_t *triggerClone = INSTR_CREATE_jcc(drcontext, instr_get_opcode(trigger), instr_get_target(trigger));
        instr_set_translation(triggerClone, instr_get_app_pc(trigger));
        instrlist_replace(bb, trigger, triggerClone);
        trigger = triggerClone;

        //Now we insert the B jcc
        instr_t *jumpOver = instr_clone(drcontext, trigger);
        instr_set_target(jumpOver, opnd_create_instr(trigger));
        instr_set_translation(jumpOver, 0x0);
        if (jumpGoesToLoop){
            instr_invert_cbr(jumpOver);
        }
        instrlist_meta_preinsert(bb, trigger, jumpOver);
        if (instr_is_cti_short(jumpOver))  //We convert any 8-bit jumps to 32-bit jumps
            instr_convert_short_meta_jmp_to_long(drcontext, bb, jumpOver);


        //Now we need to save EFLAGS, which might get modified during the loop init code
        //So that when we return, the original conditional jump (jcc) swings the same original way
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                opnd_create_rel_addr(&(local->spill_space.slot6), OPSZ_8),
                                opnd_create_reg(DR_REG_RAX)));
        dr_save_arith_flags_to_xax(drcontext, bb, trigger);
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                opnd_create_rel_addr(&(local->spill_space.slot7), OPSZ_8),
                                opnd_create_reg(DR_REG_RAX)));
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(DR_REG_RAX),
                                opnd_create_rel_addr(&(local->spill_space.slot6), OPSZ_8)));

        mustRestoreEflags = true;
    }
#ifdef JANUS_LOOP_TIMER
    /* Start measuring the loop including loop init */
    dr_save_reg(drcontext, bb, trigger, DR_REG_RAX, SPILL_SLOT_1);
    dr_save_reg(drcontext, bb, trigger, DR_REG_RDX, SPILL_SLOT_2);
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_rdtsc(drcontext));
    dr_insert_clean_call(drcontext, bb, trigger,
                         loop_outer_timer_resume, false, 2,
                         opnd_create_reg(DR_REG_RAX),
                         opnd_create_reg(DR_REG_RDX));
    dr_restore_reg(drcontext, bb, trigger, DR_REG_RAX, SPILL_SLOT_1);
    dr_restore_reg(drcontext, bb, trigger, DR_REG_RDX, SPILL_SLOT_2);
#endif

#ifdef JANUS_DEBUG_LOOP_START_END
    PRE_INSERT(bb,trigger,INSTR_CREATE_int1(drcontext));
#endif

    /* spill s1 */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_rel_addr(&(local->spill_space.s1), OPSZ_8),
                            opnd_create_reg(s1)));

    /* load current loop pointer into s1 */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_imm(drcontext,
                             opnd_create_reg(s1),
                             OPND_CREATE_INTPTR(loop)));

    /* Set current loop pointer */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_rel_addr(&(shared->current_loop), OPSZ_8),
                            opnd_create_reg(s1)));

    /* Load TLS into s1 (the value stays alive throughout the entire loop) */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_imm(drcontext,
                             opnd_create_reg(tls),
                             OPND_CREATE_INTPTR(local)));
    /* Spill s0 */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(tls, LOCAL_S0_OFFSET),
                            opnd_create_reg(s0)));
    /* Load the return address pc */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_imm(drcontext,
                             opnd_create_reg(s0),
                             opnd_create_instr(returnLabel)));
    /* Store the return address into [tls, #return_addr] */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(tls, LOCAL_RETURN_OFFSET),
                            opnd_create_reg(s0)));
    /* Jump to loop init dynamic code*/
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_jmp(drcontext,
                         opnd_create_pc(loop->loop_init)));

    /* Execution will be resumed here after loop execution */
    PRE_INSERT(bb, trigger, returnLabel);

    if (mustRestoreEflags){
        //Now we restore EFLAGS so that the trigger jcc swings the same way
        //This is for the case where we jump into the loop via a JCC
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                opnd_create_rel_addr(&(local->spill_space.slot6), OPSZ_8),
                                opnd_create_reg(DR_REG_RAX)));
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(DR_REG_RAX),
                                opnd_create_rel_addr(&(local->spill_space.slot7), OPSZ_8)));
        dr_restore_arith_flags_from_xax(drcontext, bb, trigger);
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(DR_REG_RAX),
                                opnd_create_rel_addr(&(local->spill_space.slot6), OPSZ_8)));
    }
#ifdef JANUS_LOOP_TIMER
    /* Start measuring the loop body without loop init */
    dr_save_reg(drcontext, bb, trigger, DR_REG_RAX, SPILL_SLOT_1);
    dr_save_reg(drcontext, bb, trigger, DR_REG_RDX, SPILL_SLOT_2);
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_rdtsc(drcontext));
    dr_insert_clean_call(drcontext, bb, trigger,
                         loop_inner_timer_resume, false, 2,
                         opnd_create_reg(DR_REG_RAX),
                         opnd_create_reg(DR_REG_RDX));
    dr_restore_reg(drcontext, bb, trigger, DR_REG_RAX, SPILL_SLOT_1);
    dr_restore_reg(drcontext, bb, trigger, DR_REG_RDX, SPILL_SLOT_2);
#endif

#elif JANUS_AARCH64
    instr_t *instr;

#ifdef JANUS_DEBUG_LOOP_START_END
    PRE_INSERT(bb,trigger,instr_create_0dst_1src(drcontext, OP_brk, OPND_CREATE_INT16(0)));
#endif

    //save s1 to use it to store tls pointer
    dr_save_reg(drcontext, bb, trigger, s1, SPILL_SLOT_3);

    /* Load TLS into s1 (the value stays alive throughout the entire loop) */
    insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s1), (ptr_int_t)oracle[0]);

    //spill s0
    instr = instr_create_1dst_1src(drcontext, OP_str, OPND_CREATE_MEM64(tls, LOCAL_S0_OFFSET), opnd_create_reg(s0));
    PRE_INSERT(bb, trigger, instr);

    //spill s1 properly into tls structure
    dr_restore_reg(drcontext, bb, trigger, s0, SPILL_SLOT_3);
    instr = instr_create_1dst_1src(drcontext, OP_str, OPND_CREATE_MEM64(tls, LOCAL_S1_OFFSET), opnd_create_reg(s0));
    PRE_INSERT(bb, trigger, instr);

    /* spill x30 to TLS spill slot 1, since it will be overwritten by bl */ 
    instr = instr_create_1dst_1src(drcontext, OP_str, 
                OPND_CREATE_MEM64(tls, LOCAL_SLOT1_OFFSET), opnd_create_reg(DR_REG_X30));
    PRE_INSERT(bb, trigger, instr);

    /* branch to loop_init */
    instr = instr_create_0dst_1src(drcontext, OP_bl, opnd_create_pc(loop->loop_init));
    PRE_INSERT(bb, trigger, instr);

    /* Restore x30 from TLS spill slot 1 (since it was used to store the return address) */
    instr = instr_create_1dst_1src(drcontext, OP_ldr,
                opnd_create_reg(DR_REG_X30), OPND_CREATE_MEM64(TLS, LOCAL_SLOT1_OFFSET));
    PRE_INSERT(bb, trigger, instr);
#endif
}

/* Finish loop parallelisation
 * This handler is called by all the thread */
void loop_finish_handler(JANUS_CONTEXT)
{
    instr_t *trigger = get_trigger_instruction(bb,rule);
    janus_thread_t *local = dr_get_tls_field(drcontext);

    //retrieve the current loop (loop id stored in rule reg0)
    loop_t *loop = &(shared->loops[rule->reg0]);

    reg_id_t s0 = loop->header->scratchReg0;
    reg_id_t s1 = loop->header->scratchReg1;
    reg_id_t s2 = loop->header->scratchReg2;
    reg_id_t s3 = loop->header->scratchReg3;
    reg_id_t tls = s1;

#ifdef JANUS_X86
    //for the main thread
    if (local->id == 0) {
# ifdef JANUS_LOOP_TIMER
        dr_save_reg(drcontext, bb, trigger, DR_REG_RAX, SPILL_SLOT_1);
        dr_save_reg(drcontext, bb, trigger, DR_REG_RDX, SPILL_SLOT_2);
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_rdtsc(drcontext));
        dr_insert_clean_call(drcontext, bb, trigger,
                             loop_inner_timer_pause, false, 2,
                             opnd_create_reg(DR_REG_RAX),
                             opnd_create_reg(DR_REG_RDX));
        dr_restore_reg(drcontext, bb, trigger, DR_REG_RAX, SPILL_SLOT_1);
        dr_restore_reg(drcontext, bb, trigger, DR_REG_RDX, SPILL_SLOT_2);
# endif /* JANUS_LOOP_TIMER */

        instr_t *returnLabel = INSTR_CREATE_label(drcontext);
        instr_t *skipLabel = INSTR_CREATE_label(drcontext);
        /* We should check the loop_on flag for safety execution
         * If the loop exit block is coming from the non-loop path, the loop_finish should not be executed */
        PRE_INSERT(bb, trigger,
           INSTR_CREATE_cmp(drcontext,
                            opnd_create_rel_addr((void *)&(local->flag_space.loop_on), OPSZ_4),
                            OPND_CREATE_INT32(0)));
        PRE_INSERT(bb, trigger,
           INSTR_CREATE_jcc(drcontext, OP_jz, opnd_create_instr(skipLabel)));

        /* Now loop_on is true, then all four scratch registers are true now
         * Load the return address pc */
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_mov_imm(drcontext,
                                 opnd_create_reg(s0),
                                 opnd_create_instr(returnLabel)));
        /* Store the return address into [tls, #return_addr] */
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                OPND_CREATE_MEM64(tls, LOCAL_RETURN_OFFSET),
                                opnd_create_reg(s0)));
        /* Jump to loop finish dynamic code*/
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_jmp(drcontext,
                             opnd_create_pc(loop->loop_finish)));
        /* Return here after loop finish */
        PRE_INSERT(bb, trigger, returnLabel);

        /* Restore the final TLS register */
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(TLS),
                                OPND_CREATE_MEM64(TLS, LOCAL_TLS_OFFSET)));

#ifdef JANUS_STATS
        //increment invocation counter
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_inc(drcontext,
                             opnd_create_rel_addr((void *)&(shared->loop_invocation), OPSZ_8)));
#endif

# ifdef JANUS_LOOP_TIMER
        dr_save_reg(drcontext, bb, trigger, DR_REG_RAX, SPILL_SLOT_1);
        dr_save_reg(drcontext, bb, trigger, DR_REG_RDX, SPILL_SLOT_2);
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_rdtsc(drcontext));
        dr_insert_clean_call(drcontext, bb, trigger,
                             loop_outer_timer_pause, false, 2,
                             opnd_create_reg(DR_REG_RAX),
                             opnd_create_reg(DR_REG_RDX));
        dr_restore_reg(drcontext, bb, trigger, DR_REG_RAX, SPILL_SLOT_1);
        dr_restore_reg(drcontext, bb, trigger, DR_REG_RDX, SPILL_SLOT_2);
# endif /* JANUS_LOOP_TIMER */

#ifdef JANUS_DEBUG_LOOP_START_END
    PRE_INSERT(bb,trigger,INSTR_CREATE_int1(drcontext));
#endif
        /* Skip to here (loop_on flag not true) */
        PRE_INSERT(bb, trigger, skipLabel);
    } else {
        //for the parallelising thread, simply jump to thread private code
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_jmp(drcontext,
                             opnd_create_pc(local->gen_code[loop->dynamic_id].thread_loop_finish)));
    }
#elif JANUS_AARCH64
    int offset;
    //PRE_INSERT(bb,trigger,instr_create_0dst_1src(drcontext, OP_brk, OPND_CREATE_INT16(0)));

    instr_t *skipLabel = INSTR_CREATE_label(drcontext);
    instr_t *mainLabel = INSTR_CREATE_label(drcontext);
    instr_t *endLabel = INSTR_CREATE_label(drcontext);
# ifdef JANUS_SHARED_CC
    /* Check shared->loop_on flag. If true then the scratch regs are available */
    dr_save_reg(drcontext, bb, trigger, s2, SPILL_SLOT_2);

    insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s2), (ptr_int_t)&(shared->loop_on));
    PRE_INSERT(bb, trigger,
        instr_create_1dst_1src(drcontext,
                            OP_ldr,
                            opnd_create_reg(s2),
                            OPND_CREATE_MEM64(s2, 0)));
    PRE_INSERT(bb, trigger,
        instr_create_0dst_2src(drcontext,
                            OP_cbz,
                            opnd_create_instr(skipLabel),
                            opnd_create_reg(s2)));

    /* loop is on so scratch registers are available */
    /* check to see if this is the main thread */
    PRE_INSERT(bb, trigger,
        instr_create_1dst_1src(drcontext,
                            OP_ldr,
                            opnd_create_reg(s2),
                            OPND_CREATE_MEM64(TLS, LOCAL_ID_OFFSET)));
    
    PRE_INSERT(bb, trigger,
        instr_create_0dst_2src(drcontext,
                            OP_cbz,
                            opnd_create_instr(mainLabel),
                            opnd_create_reg(s2)));

    /* for the parallelising thread (tid != 0), 'simply' jump to thread private code */
    // Multi-stage load thread_loop_finish address into s2
    PRE_INSERT(bb, trigger,
        instr_create_1dst_1src(drcontext,
                            OP_ldr,
                            opnd_create_reg(s2),
                            OPND_CREATE_MEM64(TLS, LOCAL_GEN_CODE_OFFSET)));

    offset = sizeof(loop_code_t)*loop->dynamic_id + offsetof(loop_code_t, thread_loop_finish);

    PRE_INSERT(bb, trigger,
        instr_create_1dst_1src(drcontext,
                            OP_ldr,
                            opnd_create_reg(s2),
                            OPND_CREATE_MEM64(s2, offset)));

    PRE_INSERT(bb, trigger,
        instr_create_0dst_1src(drcontext,
                            OP_br,
                            opnd_create_reg(s2)));
                            //opnd_create_pc(local->gen_code[loop->dynamic_id].thread_loop_finish)));

    /* for main thread (tid = 0), jump here */
    PRE_INSERT(bb, trigger, mainLabel);

    /* jump to loop finish dynamic code*/

    //PRE_INSERT(bb, trigger, instr_create_0dst_1src(drcontext, OP_brk, OPND_CREATE_INT16(0)));
    dr_save_reg(drcontext, bb, trigger, DR_REG_X30, SPILL_SLOT_2);
    PRE_INSERT(bb, trigger,
        instr_create_0dst_1src(drcontext,
                            OP_bl,
                            opnd_create_pc(loop->loop_finish)));
    dr_restore_reg(drcontext, bb, trigger, DR_REG_X30, SPILL_SLOT_2);

    /* Restore the final TLS register */
    PRE_INSERT(bb, trigger,
        instr_create_1dst_1src(drcontext,
                            OP_ldr,
                            opnd_create_reg(TLS),
                            OPND_CREATE_MEM64(TLS, LOCAL_TLS_OFFSET)));

#ifdef JANUS_DEBUG_LOOP_START_END
    PRE_INSERT(bb,trigger,instr_create_0dst_1src(drcontext, OP_brk, OPND_CREATE_INT16(0)));
#endif

    /* jump over s2 restore */
    PRE_INSERT(bb, trigger,
        instr_create_0dst_1src(drcontext,
                            OP_b,
                            opnd_create_instr(endLabel)));

    /* if not loop_on */
    PRE_INSERT(bb, trigger, skipLabel);
    dr_restore_reg(drcontext, bb, trigger, s2, SPILL_SLOT_2);

    /* endLabel allows main thread to jump over dr_restore_reg of s2 if loop_on is true */
    PRE_INSERT(bb, trigger, endLabel);
# else
    //for the main thread
    if (local->id == 0) {
        instr_t *returnLabel = INSTR_CREATE_label(drcontext);
        /* We should check the loop_on flag for safety execution
         * If the loop exit block is coming from the non-loop path, the loop_finish should not be executed */

        dr_save_reg(drcontext, bb, trigger, s2, SPILL_SLOT_1);
        insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s2), (ptr_int_t)&(local->flag_space.loop_on));

        PRE_INSERT(bb, trigger,
            instr_create_1dst_1src(drcontext,
                                OP_ldr,
                                opnd_create_reg(s2),
                                OPND_CREATE_MEM64(s2, 0)));
        PRE_INSERT(bb, trigger,
            instr_create_0dst_2src(drcontext,
                                OP_cbz,
                                opnd_create_instr(skipLabel),
                                opnd_create_reg(s2)));


        dr_restore_reg(drcontext, bb, trigger, s2, SPILL_SLOT_1);

        /* Now loop_on is true, then all four scratch registers are true now
         * Load the return address pc */

        PRE_INSERT(bb, trigger,
            instr_create_1dst_1src(drcontext,
                                OP_adr,
                                opnd_create_reg(s0),
                                opnd_create_rel_addr(returnLabel, OPSZ_4)));


        /* Store the return address into [tls, #return_addr] */
        PRE_INSERT(bb, trigger,
            instr_create_1dst_1src(drcontext,
                                OP_str,
                                OPND_CREATE_MEM64(tls, LOCAL_RETURN_OFFSET),
                                opnd_create_reg(s0)));

        /* Jump to loop finish dynamic code*/
        PRE_INSERT(bb, trigger,
            instr_create_0dst_1src(drcontext,
                                OP_bl,
                                opnd_create_pc(loop->loop_finish)));
        /* Return here after loop finish */
        PRE_INSERT(bb, trigger, returnLabel);

        /* Restore the final TLS register */
        PRE_INSERT(bb, trigger,
            instr_create_1dst_1src(drcontext,
                                OP_ldr,
                                opnd_create_reg(TLS),
                                OPND_CREATE_MEM64(TLS, LOCAL_TLS_OFFSET)));

#ifdef JANUS_DEBUG_LOOP_START_END
    PRE_INSERT(bb,trigger,instr_create_0dst_1src(drcontext, OP_brk, OPND_CREATE_INT16(0)));
#endif

        /* Skip to here (loop_on flag not true) */
        PRE_INSERT(bb, trigger, skipLabel);
        dr_restore_reg(drcontext, bb, trigger, s2, SPILL_SLOT_1);
    } else {
        //for the parallelising thread, simply jump to thread private code
        
        PRE_INSERT(bb, trigger,
            instr_create_0dst_1src(drcontext,
                                OP_b,
                                opnd_create_pc(local->gen_code[loop->dynamic_id].thread_loop_finish)));
    }
# endif
#endif
}

/* Iteration handler is only effective on cyclic based parallelsation */
void loop_iteration_handler(JANUS_CONTEXT)
{
    instr_t *trigger = get_trigger_instruction(bb,rule);

    janus_thread_t *local = dr_get_tls_field(drcontext);
    loop_t *loop = &(shared->loops[rule->reg0]);
    reg_id_t s0 = loop->header->scratchReg0;
    reg_id_t s1 = loop->header->scratchReg1;

#ifdef JANUS_X86

  #ifdef JANUS_STATS
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_inc(drcontext,
                         OPND_CREATE_MEM64(TLS, LOCAL_STATS_ITER_OFFSET)));
  #endif

  #ifdef JANUS_DEBUG_RUN_ITERATION_START
    PRE_INSERT(bb,trigger,INSTR_CREATE_int1(drcontext));
  #endif

#elif JANUS_AARCH64
#endif
}

int instr_get_jcc_swapped_opcode(int jccOpcode){
    //If we swap the operands of a cmp instruction
    //This is how we need to alter the jcc opcode
    switch(jccOpcode){
        case OP_jge:
            return OP_jle;
        case OP_jle:
            return OP_jge;
        case OP_jae:
            return OP_jbe;
        case OP_jbe:
            return OP_jae;
        case OP_jl:
            return OP_jg;
        case OP_jg:
            return OP_jl;
        case OP_ja:
            return OP_jb;
        case OP_jb:
            return OP_ja;
        default:
            return jccOpcode; //For jne, je, ...
    }
}

void
loop_update_boundary_handler(JANUS_CONTEXT)
{
    opnd_t op;
    instr_t *trigger = get_trigger_instruction(bb,rule);

#ifdef JANUS_X86
    loop_t *loop = &(shared->loops[rule->reg1]);
    JVarProfile profile = loop->variables[rule->ureg0.down];

    //This signals which operand in the cmp is the induction variable
    //Need to be careful with cases like
    // mov rax, inductionreg
    // cmp rax, checkreg
    // (tests/correctness/v0/cholesky_d)
    // Since now only static Janus knows which operand should we modify and which not
    int inductionVarOpndIndex = rule->ureg0.up; 

    reg_id_t s1 = loop->header->scratchReg1;
    reg_id_t s2 = loop->header->scratchReg2;

    if (instr_get_opcode(trigger) == OP_sub){
        //If we have a sub instruction used for the jcc,
        //we insert a cmp right after, and set that as the trigger
        opnd_t checkOpnd;
        //Here we create the check operand
        if (profile.induction.check.type == JVAR_CONSTANT){
            //JAN-41, currently the static analysis outputs a check value one stride less than what we need
            //TODO: with different jump types, different check values might be needed
            checkOpnd = opnd_create_immed_int(profile.induction.check.value + profile.induction.stride.value, OPSZ_4);
        }
        else{
            checkOpnd = create_opnd(profile.induction.check);
        }
        instr_t *cmp_trigger = INSTR_CREATE_cmp(drcontext, create_opnd(profile.var), checkOpnd);
        POST_INSERT(bb, trigger, cmp_trigger);
        inductionVarOpndIndex = 0;
        trigger = cmp_trigger;
    }
#ifdef JANUS_VERBOSE
    dr_printf("Initial PARA_LOOP_UDPATE_BOUNDS instruction:\n");
    instr_disassemble(drcontext, trigger, STDOUT);
    dr_printf("\n");
#endif

    janus_thread_t *local = dr_get_tls_field(drcontext);
    int thread_id = local->id;
    if (loop->schedule == PARA_DOALL_BLOCK)
    {
        if (thread_id == rsched_info.number_of_threads - 1)
            return; //For the last thread, keep the original loop boundary
        int num_srcs = instr_num_srcs(trigger);
        DR_ASSERT_MSG(num_srcs == 2, "PARA_LOOP_UPDATE_BOUNDS on irregular cmp without exactly 2 operands!\n");
        int checkVarOpndIndex = 1 - inductionVarOpndIndex;
        op = instr_get_src(trigger, checkVarOpndIndex);
        if (profile.induction.check.type == JVAR_CONSTANT) {
            //we need to check the init value
            
            if (profile.induction.init.type != JVAR_CONSTANT) {
                //non-constant induction initiation value means we won't modify the constant in the instruction
                //change to [TLS, local_check]

                //JAN-26
                //To change to the correct memory location operand size, we can't use opnd_get_size(op) if op is constant
                //Compilers might compare 64bit registers against 32bit values (symm, adi in polybench)
                //In which case we need to find the operand size of the register (usually the other operand)
                //Otherwise, DynamoRIO segfaults when encoding the instruction due to different sizes of operands
                opnd_size_t memory_loc_size;
                memory_loc_size = opnd_get_size(instr_get_src(trigger, inductionVarOpndIndex)); 
                instr_set_src(trigger, checkVarOpndIndex, opnd_create_base_disp(loop->header->scratchReg1, 0, 0, LOCAL_CHECK_OFFSET, memory_loc_size));
            }
            else
            {
                //JAN-41, currently the static analysis outputs a check value one stride less than what we need
                int64_t offset = profile.induction.check.value + profile.induction.stride.value;
                //TODO: add cases for variable stride
                if (profile.induction.stride.type = JVAR_CONSTANT)
                {
                    if (opnd_is_reg(op) && checkVarOpndIndex == 0){
                        //This branch (check val constant, but register opnd) can happen as per JAN-59
                        //In this branch we have a cmp like
                        //cmp checkreg, inductionreg
                        //But according to x86 documentation we can't have a cmp like  
                        //cmp immed, inductionreg
                        //So we will need to swap the operands of the cmp
                        //And also invert (jle -> jge, ...) the jcc at the end of the block
                        instr_t *trigger2 = instr_clone(drcontext, trigger);
                        instr_set_src(trigger2, 1, instr_get_src(trigger, 0));
                        instr_set_src(trigger2, 0, instr_get_src(trigger, 1));
                        checkVarOpndIndex = 1; //Now we need to modify the other operand

                        instr_t *jcc = instrlist_last(bb);
                        DR_ASSERT_MSG(instr_is_cbr(jcc), "The last instruction in the BB after PARA_LOOP_UPDATE_BOUNDS should be a conditional jump!\n");
                        instr_set_opcode(jcc, instr_get_jcc_swapped_opcode(instr_get_opcode(jcc)));

                        instrlist_replace(bb, trigger, trigger2);
                        instr_destroy(drcontext, trigger);
                        trigger = trigger2;
                    }

                    //Note: this branch doesn't currently account for different calculations with JL or JG exit instructions
                    offset -= profile.induction.init.value; //Init is constant, so we calculate the new thread iteration limits right here
                    offset = offset / profile.induction.stride.value;
                    offset = offset / rsched_info.number_of_threads;
                    offset = offset * profile.induction.stride.value;
                    offset = (thread_id + 1) * offset;
                    offset += profile.induction.init.value;
                    instr_set_src(trigger, checkVarOpndIndex, OPND_CREATE_INT32(offset));
                }
                else
                {
                    DR_ASSERT_MSG(false, "Variable stride not supported in loop_update_boundary_handler!\n");
                }
            }
        } else if (profile.induction.check.type == JVAR_MEMORY || profile.induction.check.type == JVAR_ABSOLUTE) {
            //For stack check variables, we will modify the value in the thread private stacks, so don't go into this branch
            //For absolute memory addresses, however, change the operand to [TLS, LOCAL_CHECK_OFFSET]
            instr_set_src(trigger, checkVarOpndIndex, opnd_create_base_disp(loop->header->scratchReg1, 0, 0,
                                                            LOCAL_CHECK_OFFSET,
                                                            opnd_get_size(op)));

        } else {
            dr_fprintf(STDERR, "Error: PARA_LOOP_UPDATE_BOUNDS rule attached when loop check variable isn't a constant or memory location! Check var:\n");
            print_var(profile.induction.check);
        }
    } else if (loop->schedule == PARA_DOALL_CYCLIC_CHUNK) {
        if (rule->reg0 == 1)
            instr_set_opcode(trigger, OP_jg);
        else
            instr_set_opcode(trigger, OP_jl);
    }

#ifdef JANUS_LOOP_VERBOSE
    dr_printf("Modified PARA_LOOP_UPDATE_BOUNDS instruction:\n");
    instr_disassemble(drcontext, trigger, STDOUT);
    dr_printf("\n");
#  endif
#elif JANUS_AARCH64
    loop_t *loop = &(shared->loops[rule->reg1]);
    JVarProfile profile = loop->variables[rule->reg0];

    reg_id_t s1 = loop->header->scratchReg1;
    reg_id_t s2 = loop->header->scratchReg2;

    reg_id_t inductionReg; // Register that holds the induction variable loaded from a stack frame (if stack frame)

    PRE_INSERT(bb, trigger,
        instr_create_1dst_1src(drcontext,
                            OP_ldr,
                            opnd_create_reg(s2),
                            OPND_CREATE_MEM64(TLS, LOCAL_CHECK_OFFSET)));

    if(profile.var.type == JVAR_STACKFRAME) {

        // If induction var is in a stack frame, get the register it has been loaded into and cmp with check (loaded into s2)
        inductionReg = (reg_id_t)rule->ureg1.up;

        // cmp inductionReg, s2
        PRE_INSERT(bb, trigger,
            instr_create_1dst_4src(drcontext,
                            OP_subs,
                            opnd_create_reg(DR_REG_XZR),
                            opnd_create_reg(inductionReg),
                            opnd_create_reg(s2),
                            OPND_CREATE_LSL(),
                            OPND_CREATE_INT(0)));
    }
    else{

        // If induction variable is in a register, just cmp with check (loaded into s2)
        // cmp profile.var.value, s2
        PRE_INSERT(bb, trigger,
            instr_create_1dst_4src(drcontext,
                            OP_subs,
                            opnd_create_reg(DR_REG_XZR),
                            opnd_create_reg(profile.var.value),
                            opnd_create_reg(s2),
                            OPND_CREATE_LSL(),
                            OPND_CREATE_INT(0)));
    }

    instrlist_remove(bb, trigger);
#endif
}

/** \brief Constructs a dynamic instruction list for loop initialisation */
//This code is only executed by thread 0
instrlist_t *
build_loop_init_instrlist(void *drcontext, loop_t *loop)
{
    instrlist_t *bb = instrlist_create(drcontext);
    reg_id_t s0 = loop->header->scratchReg0;
    reg_id_t s1 = loop->header->scratchReg1;
    reg_id_t s2 = loop->header->scratchReg2;
    reg_id_t s3 = loop->header->scratchReg3;
    reg_id_t tls = s1;
    instr_t *trigger = NULL;

#ifdef JANUS_X86
    /* Append the jump back instruction */
    trigger = INSTR_CREATE_jmp_ind(drcontext, OPND_CREATE_MEM64(tls, LOCAL_RETURN_OFFSET));
    APPEND(bb, trigger);

    /* step 0: save s2, s3 during the loop init */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(TLS, LOCAL_S2_OFFSET),
                            opnd_create_reg(s2)));
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(TLS, LOCAL_S3_OFFSET),
                            opnd_create_reg(s3)));
#elif JANUS_AARCH64
    instr_t *instr;

    trigger = instr_create_0dst_1src(drcontext, OP_ret, opnd_create_reg(DR_REG_X30));
    APPEND(bb, trigger);

    //spill return address (in link register x30) to TLS
    instr = instr_create_1dst_1src(drcontext, OP_str,
                OPND_CREATE_MEM64(tls, LOCAL_RETURN_OFFSET), opnd_create_reg(DR_REG_X30));
    INSERT(bb, trigger, instr);

    // Restore x30's real value from TLS spill slot 1
    instr = instr_create_1dst_1src(drcontext, OP_ldr,
                opnd_create_reg(DR_REG_X30), OPND_CREATE_MEM64(TLS, LOCAL_SLOT1_OFFSET));
    INSERT(bb, trigger, instr);

    //spill s2
    instr = instr_create_1dst_1src(drcontext, OP_str, OPND_CREATE_MEM64(tls, LOCAL_S2_OFFSET), opnd_create_reg(s2));
    INSERT(bb, trigger, instr);

    //spill s3
    instr = instr_create_1dst_1src(drcontext, OP_str, OPND_CREATE_MEM64(tls, LOCAL_S3_OFFSET), opnd_create_reg(s3));
    INSERT(bb, trigger, instr);

    /* load current loop pointer into s0 */
    insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s0), (ptr_int_t)loop);
    /* load shared current loop address into s2 (s1 is used to store the TLS pointer) */
    insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s2), (ptr_int_t)&(shared->current_loop));
    /* str [s2], s0 */
    instr = instr_create_1dst_1src(drcontext, OP_str, OPND_CREATE_MEM64(s2, 0), opnd_create_reg(s0));
    INSERT(bb, trigger, instr);
#endif
    /* Step 1: switch to private stack and leave the original stack untouched */
    if (loop->header->useStack)
        emit_switch_stack_ptr_aligned(emit_context);

    /* Step 2: selectively save read-only registers and let other threads to copy */
    emit_spill_to_shared_register_bank(emit_context, loop->header->registerToCopy);

    /* Step 3: wait for all other threads to be ready in thread pool */
    emit_wait_threads_in_pool(emit_context);

    /* Step 4: set start_run and schedule threads to execute the loop */
    emit_schedule_threads(emit_context);

    /* Step 5: initialise induction variables (only for the main thread tid=0)
     * specified in induct.c */
    emit_init_loop_variables(emit_context, 0);

    /* Step 6: If we have registers for conditional merging, 
     * clear the written register mask for the thread */
    //This should be a 64-bit move (if we want to use more than 32 registers)
    if (loop->header->registerToConditionalMerge){
        INSERT(bb, trigger, 
            INSTR_CREATE_mov_imm(drcontext, 
                                OPND_CREATE_MEM32(TLS, LOCAL_WRITTEN_REGS_OFFSET),
                                OPND_CREATE_INT32(0)));
    }

    if (loop->schedule == PARA_DOALL_BLOCK) {
#ifdef JANUS_X86
        /* step 6: restore s2, s3. We don't use them for block parallelisation */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(s2),
                                OPND_CREATE_MEM64(TLS, LOCAL_S2_OFFSET)));
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(s3),
                                OPND_CREATE_MEM64(TLS, LOCAL_S3_OFFSET)));
    }
#elif JANUS_AARCH64
        /* step 6: restore s2, s3 */
        INSERT(bb, trigger,
            instr_create_1dst_1src(drcontext,
                                OP_ldr,
                                opnd_create_reg(s2),
                                OPND_CREATE_MEM64(TLS, LOCAL_S2_OFFSET)));
        INSERT(bb, trigger,
            instr_create_1dst_1src(drcontext,
                                OP_ldr,
                                opnd_create_reg(s3),
                                OPND_CREATE_MEM64(TLS, LOCAL_S3_OFFSET)));
    }

    //spill x30's real value to TLS spill slot 1
    instr = instr_create_1dst_1src(drcontext, OP_str,
                OPND_CREATE_MEM64(tls, LOCAL_SLOT1_OFFSET), opnd_create_reg(DR_REG_X30));
    INSERT(bb, trigger, instr);

    // Restore return address in x30 from TLS
    instr = instr_create_1dst_1src(drcontext, OP_ldr,
                opnd_create_reg(DR_REG_X30), OPND_CREATE_MEM64(TLS, LOCAL_RETURN_OFFSET));
    INSERT(bb, trigger, instr);
#endif
    

#ifdef HALT_MAIN_THREAD
    /* Emit an infinite loop here for debugging purpose */
    instr_t *label = INSTR_CREATE_label(drcontext);
    INSERT(bb, trigger, label);
#ifdef JANUS_X86
    INSERT(bb, trigger,
        INSTR_CREATE_jmp(drcontext, opnd_create_instr(label)));
#elif JANUS_AARCH64
#endif

#endif
    return bb;
}

/** \brief Constructs a dynamic instruction list for loop finish */
instrlist_t *
build_loop_finish_instrlist(void *drcontext, loop_t *loop)
{
    /* Generate thread private code for the main thread first */
    instrlist_t *bb = build_thread_loop_finish_instrlist(drcontext, loop, 0);
    return bb;
}

/** \brief Constructs a dynamic instruction list for per-thread loop initialisation
 * The instrlist is called as parallel(void *tls, void *start_addr)
 * in the thread_pool_app.

 * This function is called immediately after the threads leave thread pool
 * After it performs a few maintenance, it then migrates control to the original app */
instrlist_t *
build_thread_loop_init_instrlist(void *drcontext, loop_t *loop, int tid)
{
    instrlist_t *bb = instrlist_create(drcontext);

    reg_id_t s0 = loop->header->scratchReg0;
    reg_id_t s1 = loop->header->scratchReg1;
    reg_id_t s2 = loop->header->scratchReg2;
    reg_id_t s3 = loop->header->scratchReg3;

    instr_t *trigger = NULL;

#ifdef JANUS_X86
    trigger = INSTR_CREATE_jmp_ind(drcontext, OPND_CREATE_MEM64(TLS, LOCAL_RETURN_OFFSET));
    APPEND(bb, trigger);

# ifdef JANUS_DEBUG_RUN_THREADED
    INSERT(bb,trigger,INSTR_CREATE_int1(drcontext));
# endif

    /* We rely on the standard calling convention 
     * RDI: tls 
     * RSI: start_addr */

    /* Load TLS register */
    if (TLS != DR_REG_RDI && TLS != DR_REG_RSI) {
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(TLS),
                                opnd_create_reg(DR_REG_RDI)));
        /* Put the loop start address local return at last */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                OPND_CREATE_MEM64(TLS, LOCAL_RETURN_OFFSET),
                                opnd_create_reg(DR_REG_RSI)));
    } else if (TLS == DR_REG_RSI) {
        /* Put the loop start address local return first */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                OPND_CREATE_MEM64(DR_REG_RDI, LOCAL_RETURN_OFFSET),
                                opnd_create_reg(DR_REG_RSI)));
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(TLS),
                                opnd_create_reg(DR_REG_RDI)));
    } else if (TLS == DR_REG_RDI) {
        INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                OPND_CREATE_MEM64(TLS, LOCAL_RETURN_OFFSET),
                                opnd_create_reg(DR_REG_RSI)));
    }

# elif JANUS_AARCH64
    trigger = INSTR_CREATE_label(drcontext);
    APPEND(bb, trigger);
# ifdef JANUS_DEBUG_RUN_THREADED
    instr_t *debug = instr_create_0dst_1src(drcontext, OP_brk, OPND_CREATE_INT16(0));
    INSERT(bb, trigger, debug);
# endif
    /* We rely on the standard calling convention 
     * X0: tls 
     * X1: start_addr */

    //Store loop start address (in x1) to TLS structure (pointed to by x0)
    //str x1, [x0, #LOCAL_RETURN_OFFSET]
    INSERT(bb, trigger,
        instr_create_1dst_1src(drcontext,
                            OP_str,
                            OPND_CREATE_MEM64(DR_REG_X0, LOCAL_RETURN_OFFSET),
                            opnd_create_reg(DR_REG_X1))); 
    //Copy TLS pointer to s1
    //mov x0 -> s1
    INSERT(bb, trigger,
        XINST_CREATE_move(drcontext, opnd_create_reg(s1), opnd_create_reg(DR_REG_X0)));
#endif

    /* Step 0: align the current stack */
#ifdef JANUS_X86
    if (loop->header->useStack) {
        /* Allocate stack frame */
        INSERT(bb, trigger,
            INSTR_CREATE_sub(drcontext,
                             opnd_create_reg(DR_REG_RSP),
                             OPND_CREATE_INT32(loop->header->stackFrameSize + 0xFF)));
        /* Stack alignment */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(s2),
                                opnd_create_reg(DR_REG_RSP)));
        INSERT(bb, trigger,
            INSTR_CREATE_and(drcontext,
                             opnd_create_reg(s2),
                             OPND_CREATE_INT32(0xFF)));
        INSERT(bb, trigger,
            INSTR_CREATE_sub(drcontext,
                             opnd_create_reg(DR_REG_RSP),
                             opnd_create_reg(s2)));
    }
#elif JANUS_AARCH64

#endif

    /* Step 1: selectively copy all the registers from the shared register bank */
    emit_restore_from_shared_register_bank(emit_context, loop->header->registerToCopy);

    /* Step 2: initialise induction variables
     * specified in iterator.c */
    emit_init_loop_variables(emit_context, tid);

    /* Step 3: If we have registers for conditional merging, 
     * clear the written register mask for the thread */
    //This should be a 64-bit move (if we want to use more than 32 registers)
    if (loop->header->registerToConditionalMerge){
        INSERT(bb, trigger, 
            INSTR_CREATE_mov_imm(drcontext, 
                                OPND_CREATE_MEM32(TLS, LOCAL_WRITTEN_REGS_OFFSET),
                                OPND_CREATE_INT32(0)));
    }
#ifdef JANUS_X86
    if (loop->schedule == PARA_DOALL_BLOCK) {
        /* restore s2, s3. We don't use them for block parallelisation */
        // We need to do this because on MEM_SCRATCH_REG we don't restore s2 or s3!
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(s2),
                                OPND_CREATE_MEM64(TLS, LOCAL_S2_OFFSET)));
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(s3),
                                OPND_CREATE_MEM64(TLS, LOCAL_S3_OFFSET)));
    }
#endif

#ifdef JANUS_AARCH64
    //Load loop start address into s2
    //ldr s2, [TLS, #LOCAL_RETURN_OFFSET]
    INSERT(bb, trigger,
        instr_create_1dst_1src(drcontext,
                            OP_ldr,
                            opnd_create_reg(s2),
                            OPND_CREATE_MEM64(TLS, LOCAL_RETURN_OFFSET)));
    //Branch to loop start
    //br s2
    INSERT(bb, trigger,
        instr_create_0dst_1src(drcontext,
                            OP_br,
                            opnd_create_reg(s2)));
#endif

#ifdef HALT_PARALLEL_THREADS
    instr_t *label = INSTR_CREATE_label(drcontext);
    INSERT(bb, trigger, label);
#ifdef JANUS_X86
    INSERT(bb, trigger,
        INSTR_CREATE_jmp(drcontext, opnd_create_instr(label)));
#elif JANUS_AARCH64
    INSERT(bb, trigger,
        instr_create_0dst_1src(drcontext,
                            OP_b,
                            opnd_create_instr(label)));
#endif
#endif
    return bb;
}

/** \brief Constructs a dynamic instruction list for per-thread loop finish/merge */
instrlist_t *
build_thread_loop_finish_instrlist(void *drcontext, loop_t *loop, int tid)
{
    instrlist_t *bb = instrlist_create(drcontext);

    reg_id_t s0 = loop->header->scratchReg0;
    reg_id_t s1 = loop->header->scratchReg1;
    reg_id_t s2 = loop->header->scratchReg2;
    reg_id_t s3 = loop->header->scratchReg3;
#ifdef JANUS_X86
    /* Append the redzone instruction */
    instr_t *trigger = INSTR_CREATE_int1(drcontext);
    APPEND(bb, trigger);

    /* step 0: save s2, s3 during loop finish code */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(TLS, LOCAL_S2_OFFSET),
                            opnd_create_reg(s2)));
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(TLS, LOCAL_S3_OFFSET),
                            opnd_create_reg(s3)));

    /* Step 1: unset loop_on flag */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM32(TLS, LOOP_ON_FLAG_OFFSET),
                            OPND_CREATE_INT32(0)));

    /* Step 2: save registers to thread private buffer for later merge by the main thread */
    emit_spill_to_private_register_bank(emit_context, loop->header->registerToMerge | loop->header->registerToConditionalMerge, tid);

    /* For Janus parallelising threads */
    if (tid != 0) {
        /* Step 3.1: set finished flag */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                OPND_CREATE_MEM32(TLS, LOCAL_FINISHED_OFFSET),
                                OPND_CREATE_INT32(1)));

        /* Step 3.2: wait here for the main thread to merge its context
           until the main thread sets the thread_yield flag */
        instr_t *wait = INSTR_CREATE_label(drcontext);
        instr_t *jump_to_pool = INSTR_CREATE_label(drcontext);
        INSERT(bb, trigger, wait);
        /* Check need yield flag */
        INSERT(bb, trigger,
            INSTR_CREATE_cmp(drcontext,
                             opnd_create_rel_addr((void *)(&shared->need_yield), OPSZ_4),
                             OPND_CREATE_INT32(1)));
        /* If need yield is on, jump to the pool */
        INSERT(bb, trigger,
            INSTR_CREATE_jcc(drcontext, OP_je, opnd_create_instr(jump_to_pool)));
        INSERT(bb, trigger,
            INSTR_CREATE_jmp(drcontext, opnd_create_instr(wait)));

        INSERT(bb, trigger, jump_to_pool);

        /* Restore stack */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(DR_REG_RSP),
                                OPND_CREATE_MEM64(TLS, LOCAL_STACK_OFFSET)));

        /* Save the thread pool to the redirect slot */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_imm(drcontext,
                                 opnd_create_reg(DR_REG_RDI),
                                 OPND_CREATE_INTPTR(janus_thread_pool_app)));
        /* Put rsi into redirect slot */
        dr_save_reg(drcontext, bb, trigger, DR_REG_RDI, SPILL_SLOT_REDIRECT_NATIVE_TGT);

        /* Assign argument */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_imm(drcontext,
                                 opnd_create_reg(DR_REG_RDI),
                                 OPND_CREATE_INTPTR(oracle[tid])));
        INSERT(bb, trigger,
            INSTR_CREATE_mov_imm(drcontext,
                                 opnd_create_reg(DR_REG_ESI),
                                 OPND_CREATE_INT32(1)));
        /* Load Stack Pointer */
        /* Perform an indirect jump to thread pool */
        INSERT(bb, trigger,
            INSTR_CREATE_jmp(drcontext, opnd_create_pc(dr_redirect_native_target(oracle[tid]->drcontext))));
    }
    else
    {
        /* For the main thread */
        /* Step 3.1: wait for other thread to finish */
        emit_wait_threads_finish(emit_context);

        /* Step 3.2: merge variables */
        emit_merge_loop_variables(emit_context);

        /* Step 3.3 Unset sequential execution flag 
        (necessary if just finished executing a loop with fewer iterations than num. of threads) */
        INSERT(bb, trigger,
               INSTR_CREATE_mov_imm(drcontext,
                                    OPND_CREATE_MEM32(TLS, LOCAL_FLAG_SEQ_OFFSET),
                                    OPND_CREATE_INT32(0)));
        /* Step 3.4 Unset start run flag */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                opnd_create_rel_addr((void *)&(shared->start_run), OPSZ_4),
                                OPND_CREATE_INT32(0)));

        /* Step 3.5 thread_yield flag */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                opnd_create_rel_addr((void *)&(shared->need_yield), OPSZ_4),
                                OPND_CREATE_INT32(1)));

        /* Step 3.6 Restore current stack pointer */
        if (loop->header->useStack)
            emit_restore_stack_ptr_aligned(emit_context);

        /* Step 3.7 Restore scratch registers */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(s0),
                                OPND_CREATE_MEM64(TLS, LOCAL_S0_OFFSET)));
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(s2),
                                OPND_CREATE_MEM64(TLS, LOCAL_S2_OFFSET)));
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(s3),
                                OPND_CREATE_MEM64(TLS, LOCAL_S3_OFFSET)));
        //NB: TLS (s1) register is restored after returning from here!
        /* Resume normal execution */
        INSERT(bb, trigger,
            INSTR_CREATE_jmp_ind(drcontext, OPND_CREATE_MEM64(TLS, LOCAL_RETURN_OFFSET)));
    }

    //insert infinite loop here
    instr_t *label = INSTR_CREATE_label(drcontext);
    INSERT(bb, trigger, label);
    INSERT(bb, trigger,
           INSTR_CREATE_jmp(drcontext, opnd_create_instr(label)));
#elif JANUS_AARCH64
    /* Append the redzone instruction */
    instr_t *trigger = instr_create_0dst_1src(drcontext, OP_brk, OPND_CREATE_INT16(0));
    APPEND(bb, trigger);
    //instr_t *debug = instr_create_0dst_1src(drcontext, OP_brk, OPND_CREATE_INT16(0));
    //INSERT(bb, trigger, debug);
    /* Step 1: unset loop_on flag */
    INSERT(bb, trigger,
        instr_create_1dst_1src(drcontext,
                            OP_str,
                            OPND_CREATE_MEM32(TLS, LOOP_ON_FLAG_OFFSET),
                            opnd_create_reg(DR_REG_WZR)));

    /* Step 2: save registers to thread private buffer for later merge by the main thread */
    emit_spill_to_private_register_bank(emit_context, loop->header->registerToMerge, tid);

    /* For Janus parallelising threads */
    if (tid != 0) {

        /* Step 3.1: set finished flag */

        INSERT(bb, trigger,
            INSTR_CREATE_movz(drcontext, opnd_create_reg(s2),
                            OPND_CREATE_INT16(1),
                            OPND_CREATE_INT8(0)));

        
        INSERT(bb, trigger,
            instr_create_1dst_1src(drcontext,
                                OP_str,
                                OPND_CREATE_MEM32(TLS, LOCAL_FINISHED_OFFSET),
                                opnd_create_reg(reg_64_to_32(s2))));

        /* Step 3.2: wait here for the main thread to merge its context
           until the main thread sets the thread_yield flag */
        instr_t *wait = INSTR_CREATE_label(drcontext);
        instr_t *jump_to_pool = INSTR_CREATE_label(drcontext);
        INSERT(bb, trigger, wait);

        /* Check need yield flag */
        insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s2), (ptr_int_t)&shared->need_yield);

        INSERT(bb, trigger,
            instr_create_1dst_1src(drcontext,
                                OP_ldr,
                                opnd_create_reg(s2),
                                OPND_CREATE_MEM64(s2, 0)));

        /* If need yield is on, jump to the pool */
        /* NOTE: A bit of a cheat - check that it is nonzero not that it is equal to 1 */
        INSERT(bb, trigger,
            instr_create_0dst_2src(drcontext,
                                OP_cbnz,
                                opnd_create_instr(jump_to_pool),
                                opnd_create_reg(s2)));

        INSERT(bb, trigger,
            instr_create_0dst_1src(drcontext, OP_b, opnd_create_instr(wait)));

        INSERT(bb, trigger, jump_to_pool);

        /* Restore stack */
        INSERT(bb, trigger,
            instr_create_1dst_1src(drcontext,
                                OP_ldr,
                                opnd_create_reg(s2),
                                OPND_CREATE_MEM64(TLS, LOCAL_STACK_OFFSET)));
        INSERT(bb, trigger,
            XINST_CREATE_move(drcontext,
                                opnd_create_reg(DR_REG_SP),
                                opnd_create_reg(s2)));


        /* Save the thread pool to the redirect slot */

        insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s2), (ptr_int_t)janus_thread_pool_app);

        dr_save_reg(drcontext, bb, trigger, s2, SPILL_SLOT_REDIRECT_NATIVE_TGT);

        /* Assign argument */
        insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(DR_REG_X0), (ptr_int_t)oracle[tid]);
        insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(DR_REG_X1), (ptr_int_t)1);

        /* Load Stack Pointer */
        /* Perform an indirect jump to thread pool */

        INSERT(bb, trigger,
            instr_create_0dst_1src(drcontext, OP_b, opnd_create_pc(dr_redirect_native_target(oracle[tid]->drcontext))));

    } else {



        /* For the main thread */
        /* Step 3.1: wait for other thread to finish */
        emit_wait_threads_finish(emit_context);

        /* Step 3.2: merge variables */
        emit_merge_loop_variables(emit_context);

        /* Step 3.3 Unset start run flag */
        insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s2), (ptr_int_t)&(shared->start_run));

        INSERT(bb, trigger,
            instr_create_1dst_1src(drcontext,
                                OP_str,
                                OPND_CREATE_MEM64(s2, 0),
                                opnd_create_reg(DR_REG_XZR)));

        /* Step 3.4 thread_yield flag */
        insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s3), (ptr_int_t)1);
        insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s2), (ptr_int_t)&(shared->need_yield));

        INSERT(bb, trigger,
            instr_create_1dst_1src(drcontext,
                                OP_str,
                                OPND_CREATE_MEM64(s2, 0),
                                opnd_create_reg(s3)));

        /* Step 3.5 Restore current stack pointer */
	if (loop->header->useStack)
            emit_restore_stack_ptr_aligned(emit_context);

        /* Step 3.6 Restore scratch registers */
        INSERT(bb, trigger,
            instr_create_1dst_1src(drcontext,
                                OP_ldr,
                                opnd_create_reg(s0),
                                OPND_CREATE_MEM64(TLS, LOCAL_S0_OFFSET)));
        INSERT(bb, trigger,
            instr_create_1dst_1src(drcontext,
                                OP_ldr,
                                opnd_create_reg(s2),
                                OPND_CREATE_MEM64(TLS, LOCAL_S2_OFFSET)));
        INSERT(bb, trigger,
            instr_create_1dst_1src(drcontext,
                                OP_ldr,
                                opnd_create_reg(s3),
                                OPND_CREATE_MEM64(TLS, LOCAL_S3_OFFSET)));

        //INSERT(bb, trigger, instr_create_0dst_1src(drcontext, OP_brk, OPND_CREATE_INT16(0)));
        /* Resume normal execution */
        INSERT(bb, trigger,
            instr_create_0dst_1src(drcontext,
                                OP_ret, opnd_create_reg(DR_REG_X30)));
    }

    //insert infinite loop here
    instr_t *label = INSTR_CREATE_label(drcontext);
    INSERT(bb, trigger, label);
    INSERT(bb, trigger,
           instr_create_0dst_1src(drcontext, OP_b, opnd_create_instr(label)));


#endif
    return bb;
}

void
loop_scratch_register_handler(JANUS_CONTEXT)
{
    int loop_id = rule->reg1;
    loop_t *loop = &(shared->loops[loop_id]);

    reg_id_t s0 = loop->header->scratchReg0;
    reg_id_t s1 = loop->header->scratchReg1;
    reg_id_t s2 = loop->header->scratchReg2;
    reg_id_t s3 = loop->header->scratchReg3;

    reg_id_t reg;
    int i;

    instr_t *trigger = get_trigger_instruction(bb, rule);
    instr_t *trigger_next = instr_get_next_app(trigger);

#ifdef JANUS_X86
    //retrieve thread local storage
    janus_thread_t *local = dr_get_tls_field(drcontext);

    int read_tls = instr_reads_from_reg(trigger, TLS, DR_QUERY_INCLUDE_ALL);
    int write_tls = instr_writes_to_reg(trigger, TLS, DR_QUERY_INCLUDE_ALL);
    int read_s0 = instr_reads_from_reg(trigger, s0, DR_QUERY_INCLUDE_ALL);
    int write_s0 = instr_writes_to_reg(trigger, s0, DR_QUERY_INCLUDE_ALL);

    //simple case: no TLS touched
    if (!read_tls && !write_tls) {
        if (read_s0) {
            PRE_INSERT(bb, trigger,
                INSTR_CREATE_mov_ld(drcontext,
                                    opnd_create_reg(s0),
                                    OPND_CREATE_MEM64(TLS, LOCAL_S0_OFFSET)));
        }

        if (write_s0) {
            PRE_INSERT(bb, trigger_next,
                INSTR_CREATE_mov_st(drcontext,
                                    OPND_CREATE_MEM64(TLS, LOCAL_S0_OFFSET),
                                    opnd_create_reg(s0)));
        }
    } else {
        //complex case: TLS touched

        //load TLS from stolen slot
        if (read_tls) {
            PRE_INSERT(bb, trigger,
                INSTR_CREATE_mov_ld(drcontext,
                                    opnd_create_reg(TLS),
                                    OPND_CREATE_MEM64(TLS, LOCAL_TLS_OFFSET)));
        }

        if (read_s0) {
            PRE_INSERT(bb, trigger,
                INSTR_CREATE_mov_ld(drcontext,
                                    opnd_create_reg(s0),
                                    opnd_create_rel_addr(&(local->spill_space.s0), OPSZ_8)));
        }

        if (write_s0) {
            PRE_INSERT(bb, trigger_next,
                INSTR_CREATE_mov_st(drcontext,
                                    opnd_create_rel_addr(&(local->spill_space.s0), OPSZ_8),
                                    opnd_create_reg(s0)));
        }

        if (write_tls) {
            PRE_INSERT(bb, trigger_next,
                INSTR_CREATE_mov_st(drcontext,
                                    opnd_create_rel_addr(&(local->spill_space.s1), OPSZ_8),
                                    opnd_create_reg(TLS)));
        }
    }

    if (read_s0 || write_s0) {
        //restore shared stack pointer
        PRE_INSERT(bb, trigger_next,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(s0),
                                opnd_create_rel_addr(&(shared->stack_ptr), OPSZ_8)));
    }

    if (read_tls || write_tls) {
        //restore TLS
        PRE_INSERT(bb, trigger_next,
            INSTR_CREATE_mov_imm(drcontext,
                                 opnd_create_reg(TLS),
                                 OPND_CREATE_INTPTR(local)));
    }
#elif JANUS_AARCH64

    reg_id_t TLSReg = s1; // Either s1 or a temporary replacement if s1 is restored/spilled

    if (instr_writes_to_reg(trigger, TLS, DR_QUERY_INCLUDE_ALL) || instr_reads_from_reg(trigger, TLS, DR_QUERY_INCLUDE_ALL)) {
        // Choose a temporary replacement for the usual TLS register, s1
        if(!(instr_writes_to_reg(trigger, s2, DR_QUERY_INCLUDE_ALL) || instr_reads_from_reg(trigger, s2, DR_QUERY_INCLUDE_ALL))) {
            TLSReg = s2;
        }
        if(!(instr_writes_to_reg(trigger, s3, DR_QUERY_INCLUDE_ALL) || instr_reads_from_reg(trigger, s3, DR_QUERY_INCLUDE_ALL))) {
            TLSReg = s3;
        }
        else {
            DR_ASSERT("No scratch registers free to hold TLS address while instruction uses s1");
        }

        // Move TLS to temporary reg before instruction
        PRE_INSERT(bb, trigger,
            XINST_CREATE_move(drcontext,
                            opnd_create_reg(TLSReg),
                            opnd_create_reg(TLS)));
        // If necessary, restore the TLS register
        if (instr_reads_from_reg(trigger, TLS, DR_QUERY_INCLUDE_ALL)){
            PRE_INSERT(bb, trigger,
                instr_create_1dst_1src(drcontext,
                                    OP_ldr,
                                    opnd_create_reg(TLS),
                                    OPND_CREATE_MEM64(TLSReg, LOCAL_TLS_OFFSET)));
        }
        // If necessary, spill the TLS register
        if (instr_writes_to_reg(trigger, TLS, DR_QUERY_INCLUDE_ALL)){
            PRE_INSERT(bb, trigger_next,
                instr_create_1dst_1src(drcontext,
                                    OP_str,
                                    OPND_CREATE_MEM64(TLSReg, LOCAL_TLS_OFFSET),
                                    opnd_create_reg(TLS)));
        }

        // Copy the TLS address back from the temporary register to the usual TLS register
        PRE_INSERT(bb, trigger_next,
            XINST_CREATE_move(drcontext,
                            opnd_create_reg(TLS),
                            opnd_create_reg(TLSReg)));
    }
 
    if (instr_reads_from_reg(trigger, s2, DR_QUERY_INCLUDE_ALL)) {
        PRE_INSERT(bb, trigger,
            instr_create_1dst_1src(drcontext,
                                OP_ldr,
                                opnd_create_reg(s2),
                                OPND_CREATE_MEM64(TLSReg, LOCAL_S2_OFFSET)));
    }
    if (instr_reads_from_reg(trigger, s3, DR_QUERY_INCLUDE_ALL)) {
        PRE_INSERT(bb, trigger,
            instr_create_1dst_1src(drcontext,
                                OP_ldr,
                                opnd_create_reg(s3),
                                OPND_CREATE_MEM64(TLSReg, LOCAL_S3_OFFSET)));
    }
    if (instr_reads_from_reg(trigger, s0, DR_QUERY_INCLUDE_ALL)) {
        PRE_INSERT(bb, trigger,
            instr_create_1dst_1src(drcontext,
                                OP_ldr,
                                opnd_create_reg(s0),
                                OPND_CREATE_MEM64(TLSReg, LOCAL_S0_OFFSET)));
    }

    if (instr_writes_to_reg(trigger, s2, DR_QUERY_INCLUDE_ALL)){
        PRE_INSERT(bb, trigger_next,
            instr_create_1dst_1src(drcontext,
                                OP_str,
                                OPND_CREATE_MEM64(TLSReg, LOCAL_S2_OFFSET),
                                opnd_create_reg(TLS)));
    }
    if (instr_writes_to_reg(trigger, s3, DR_QUERY_INCLUDE_ALL)){
        PRE_INSERT(bb, trigger_next,
            instr_create_1dst_1src(drcontext,
                                OP_str,
                                OPND_CREATE_MEM64(TLSReg, LOCAL_S3_OFFSET),
                                opnd_create_reg(TLS)));
    }

    if (instr_writes_to_reg(trigger, s0, DR_QUERY_INCLUDE_ALL)){
        PRE_INSERT(bb, trigger_next,
            instr_create_1dst_1src(drcontext,
                                OP_str,
                                OPND_CREATE_MEM64(TLSReg, LOCAL_S0_OFFSET),
                                opnd_create_reg(TLS)));

    }

    if (instr_reads_from_reg(trigger, s0, DR_QUERY_INCLUDE_ALL) || instr_writes_to_reg(trigger, s0, DR_QUERY_INCLUDE_ALL)){
        // Restore shared stack ptr into s0
        insert_mov_immed_ptrsz(drcontext, bb, trigger_next, opnd_create_reg(s0), (ptr_int_t)&(shared->stack_base));
        PRE_INSERT(bb, trigger_next,
            instr_create_1dst_1src(drcontext,
                                OP_ldr,
                                opnd_create_reg(s0),
                                OPND_CREATE_MEM64(s0, 0)));
    }
#endif
}

void
loop_restore_check_register_handler(JANUS_CONTEXT)
{
    int loop_id = rule->reg1;
    reg_id_t check_reg_id = rule->reg0; //If the RR is used for a check register, this will hold its id
    loop_t *loop = &(shared->loops[loop_id]);

    reg_id_t s0 = loop->header->scratchReg0;
    reg_id_t s1 = loop->header->scratchReg1;
    reg_id_t s2 = loop->header->scratchReg2;
    reg_id_t s3 = loop->header->scratchReg3;

    reg_id_t reg;
    int i;

    instr_t *trigger = get_trigger_instruction(bb, rule);
    instr_t *trigger_next = instr_get_next_app(trigger);

#ifdef JANUS_X86
    //retrieve thread local storage
    janus_thread_t *local = dr_get_tls_field(drcontext);

    int read_check = instr_reads_from_reg(trigger, check_reg_id, DR_QUERY_INCLUDE_ALL);
    //A parallelized loop shouldn't write to its check register

    if (read_check)
    { //If the loop check register is used in an instruction, we need to restore its original value
        //Before the instruction
        //JAN-80, if s1 is check reg, the scratch register handler should do the restoration
        if (check_reg_id != s1){
            PRE_INSERT(bb, trigger,
                    INSTR_CREATE_mov_st(drcontext,
                                        OPND_CREATE_MEM64(s1, LOCAL_SLOT4_OFFSET),
                                        opnd_create_reg(check_reg_id)));
            
            void *regSpill = (void *)&(shared->registers); //The value we need should have been spilled to the shared register bank
            PRE_INSERT(bb, trigger,
                    INSTR_CREATE_mov_ld(drcontext,
                                        opnd_create_reg(check_reg_id),
                                        opnd_create_rel_addr(regSpill + (check_reg_id - JREG_RAX) * CACHE_LINE_WIDTH, OPSZ_8)));
            
            // After the instruction
            PRE_INSERT(bb, trigger_next,
                    INSTR_CREATE_mov_ld(drcontext,
                                        opnd_create_reg(check_reg_id),
                                        OPND_CREATE_MEM64(s1, LOCAL_SLOT4_OFFSET)));
        }

    }
#elif JANUS_AARCH64
    DR_ASSERT_MSG(false, "Restore check reg AARCH64 not implemented!\n");
#endif
}

void 
loop_record_reg_write_handler(JANUS_CONTEXT){
    uint64_t regMask = rule->reg0; //regMask has the registers that will be written to in this BB
    int loop_id = rule->reg1;
    loop_t *loop = &(shared->loops[loop_id]);

    reg_id_t s1 = loop->header->scratchReg1;
    //This should be a 64-bit move (if we want to use more than 32 registers),
    //but I couldn't get Dynamorio to like it
    PRE_INSERT(bb, get_trigger_instruction(bb, rule),
        INSTR_CREATE_or(drcontext, 
                        OPND_CREATE_MEM32(s1, LOCAL_WRITTEN_REGS_OFFSET),
                        opnd_create_immed_int(regMask, OPSZ_4)));
}

//the handler is invoked when an instruction performs a write to scratch registers,
//we need to spill the scratch registers to scratch slot.
void
scratch_register_spill_handler(JANUS_CONTEXT)
{
    uint64_t regMask = rule->reg0;
    int loop_id = rule->reg1;
    loop_t *loop = &(shared->loops[loop_id]);

    reg_id_t s0 = loop->header->scratchReg0;
    reg_id_t s1 = loop->header->scratchReg1;
    reg_id_t s2 = loop->header->scratchReg2;
    reg_id_t s3 = loop->header->scratchReg3;
    reg_id_t reg;
    int i;

    instr_t *trigger = get_trigger_instruction(bb, rule);
    instr_t *trigger_next = instr_get_next_app(trigger);

#ifdef JANUS_X86
    //retrieve thread local storage
    janus_thread_t *local = dr_get_tls_field(drcontext);

    //we assume register s0 always contains the shared stack pointer
    //the s1 always stores the TLS

    //we spill the value after the trigger instruction
    //if the TLS needs to be spilled
    if (regMask & get_reg_bit_array(TLS)) {
        //immediately spill s1 after the trigger instruction
        POST_INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                opnd_create_rel_addr(&(local->spill_space.s1), OPSZ_8),
                                opnd_create_reg(TLS)));
        //recover TLS at last
        PRE_INSERT(bb, trigger_next,
            INSTR_CREATE_mov_imm(drcontext,
                                 opnd_create_reg(TLS),
                                 OPND_CREATE_INTPTR(local)));

    }

    //spill s0
    if ((regMask & get_reg_bit_array(s0))) {
        if (!(regMask & get_reg_bit_array(s1))) {
            POST_INSERT(bb, trigger,
                INSTR_CREATE_mov_st(drcontext,
                                    OPND_CREATE_MEM64(TLS, LOCAL_S0_OFFSET),
                                    opnd_create_reg(s0)));
        } else {
            POST_INSERT(bb, trigger,
                INSTR_CREATE_mov_st(drcontext,
                                    opnd_create_rel_addr(&(local->spill_space.s0), OPSZ_8),
                                    opnd_create_reg(s0)));
        }

        //restore shared stack pointer
        PRE_INSERT(bb, trigger_next,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(s0),
                                opnd_create_rel_addr(&(shared->stack_ptr), OPSZ_8)));
    }

    if (loop->schedule != PARA_DOALL_BLOCK) {
        if (regMask & get_reg_bit_array(s2)) {
            PRE_INSERT(bb, trigger_next,
                INSTR_CREATE_mov_st(drcontext,
                                    OPND_CREATE_MEM64(TLS, LOCAL_S2_OFFSET),
                                    opnd_create_reg(s2)));
        }
        if (regMask & get_reg_bit_array(s3)) {
            PRE_INSERT(bb, trigger_next,
                INSTR_CREATE_mov_st(drcontext,
                                    OPND_CREATE_MEM64(TLS, LOCAL_S3_OFFSET),
                                    opnd_create_reg(s3)));
        }
    }
#elif JANUS_AARCH64 //TODO SRTOUT THIS AND restore_handler

    reg_id_t secondTLSReg;
 
    //we assume register s0 always contains the shared stack pointer
    //the s1 always stores the TLS

    //we spill the value after the trigger instruction


    //if the TLS needs to be spilled
    if (regMask & get_reg_bit_array(TLS) || instr_reads_from_reg(trigger, TLS, DR_QUERY_INCLUDE_ALL)) {
        // Choose a temporary replacement for the usual TLS register, s1
        if(!(regMask & get_reg_bit_array(s2) || instr_reads_from_reg(trigger, s2, DR_QUERY_INCLUDE_ALL))) {
            secondTLSReg = s2;
        }
        if(!(regMask & get_reg_bit_array(s3) || instr_reads_from_reg(trigger, s3, DR_QUERY_INCLUDE_ALL))) {
            secondTLSReg = s3;
        }
        else {
            DR_ASSERT("No scratch registers free to hold TLS address while instruction uses s1");
        }

        // If necessary, spill the TLS register (s1)
        if (regMask & get_reg_bit_array(TLS)){
            PRE_INSERT(bb, trigger_next,
                instr_create_1dst_1src(drcontext,
                                    OP_str,
                                    OPND_CREATE_MEM64(secondTLSReg, LOCAL_TLS_OFFSET),
                                    opnd_create_reg(TLS)));
        }

        // Copy the TLS address back from the temporary register to the usual TLS register
        PRE_INSERT(bb, trigger_next,
            XINST_CREATE_move(drcontext,
                            opnd_create_reg(TLS),
                            opnd_create_reg(secondTLSReg)));

        if( !( instr_reads_from_reg(trigger, s0, DR_QUERY_INCLUDE_ALL)
            || instr_reads_from_reg(trigger, s1, DR_QUERY_INCLUDE_ALL)
            || instr_reads_from_reg(trigger, s2, DR_QUERY_INCLUDE_ALL)
            || instr_reads_from_reg(trigger, s3, DR_QUERY_INCLUDE_ALL))) {

            // If no registers read, no restore handler will happen so must move TLS to temporary reg before instruction
            PRE_INSERT(bb, trigger,
                XINST_CREATE_move(drcontext,
                                opnd_create_reg(secondTLSReg),
                                opnd_create_reg(TLS)));
        }
    }

    //spill s0
    if ((regMask & get_reg_bit_array(s0)) &&
        !instr_reads_from_reg(trigger, s0, DR_QUERY_INCLUDE_ALL)) {
        PRE_INSERT(bb, trigger_next,
            instr_create_1dst_1src(drcontext,
                                OP_str,
                                OPND_CREATE_MEM64(TLS, LOCAL_S0_OFFSET),
                                opnd_create_reg(s0)));
        //restore shared stack pointer
        PRE_INSERT(bb, trigger_next,
            instr_create_1dst_1src(drcontext,
                                OP_ldr,
                                opnd_create_reg(s0),
                                opnd_create_rel_addr(&(shared->stack_ptr), OPSZ_8)));

    }

    if (loop->schedule != PARA_DOALL_BLOCK) {
        if (regMask & get_reg_bit_array(s2)) {
            PRE_INSERT(bb, trigger_next,
                instr_create_1dst_1src(drcontext,
                                    OP_str,
                                    OPND_CREATE_MEM64(TLS, LOCAL_S2_OFFSET),
                                    opnd_create_reg(s2)));
        }
        if (regMask & get_reg_bit_array(s3)) {
            PRE_INSERT(bb, trigger_next,
                instr_create_1dst_1src(drcontext,
                                    OP_str,
                                    OPND_CREATE_MEM64(TLS, LOCAL_S3_OFFSET),
                                    opnd_create_reg(s3)));
        }
    }

#endif
}

//the handler is invoked when an instruction performs a read from scratch registers,
//we need to recover the scratch registers from scratch slots before the read.
void
scratch_register_restore_handler(JANUS_CONTEXT)
{
    uint64_t regMask = rule->reg0;
    int loop_id = rule->reg1;
    loop_t *loop = &(shared->loops[loop_id]);

    reg_id_t s0 = loop->header->scratchReg0;
    reg_id_t s1 = loop->header->scratchReg1;
    reg_id_t s2 = loop->header->scratchReg2;
    reg_id_t s3 = loop->header->scratchReg3;
    reg_id_t reg;
    int i;

    instr_t *trigger = get_trigger_instruction(bb, rule);
    instr_t *trigger_next = instr_get_next(trigger);

#ifdef JANUS_X86
    //retrieve thread local storage
    janus_thread_t *local = dr_get_tls_field(drcontext);

    //if the TLS is read
    if (regMask & get_reg_bit_array(TLS)) {
        //recover TLS values
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(TLS),
                                OPND_CREATE_MEM64(TLS, LOCAL_TLS_OFFSET)));
        //if it is not writing back to TLS
        if (!instr_writes_to_reg(trigger, s0, DR_QUERY_INCLUDE_ALL)) {
            PRE_INSERT(bb, trigger_next,
                INSTR_CREATE_mov_imm(drcontext,
                                     opnd_create_reg(TLS),
                                     OPND_CREATE_INTPTR(local)));
        }

        if (regMask & get_reg_bit_array(s0)) {
            if (instr_reads_from_reg(trigger, s0, DR_QUERY_INCLUDE_ALL)) {
                //if the instruction reads s0, we could not use s0 to buffer shared stack pointer
                DR_ASSERT_MSG(false, "s0 is pre-occupied");
            } else {
                //load s0 using pc-relative load
                PRE_INSERT(bb, trigger,
                    INSTR_CREATE_mov_ld(drcontext,
                                        opnd_create_reg(s0),
                                        opnd_create_rel_addr(&(local->spill_space.s0), OPSZ_8)));
                if (!instr_writes_to_reg(trigger, s0, DR_QUERY_INCLUDE_ALL)) {
                    PRE_INSERT(bb, trigger_next,
                        INSTR_CREATE_mov_ld(drcontext,
                                            opnd_create_reg(s0),
                                            opnd_create_rel_addr(&(shared->stack_ptr), OPSZ_8)));
                }
            }
        }

        if (loop->schedule != PARA_DOALL_BLOCK) {
            if (regMask & get_reg_bit_array(s2)) {
                PRE_INSERT(bb, trigger,
                    INSTR_CREATE_mov_ld(drcontext,
                                        opnd_create_reg(s2),
                                        opnd_create_rel_addr(&(local->spill_space.s2), OPSZ_8)));
            }
            if (regMask & get_reg_bit_array(s3)) {
                PRE_INSERT(bb, trigger,
                    INSTR_CREATE_mov_ld(drcontext,
                                        opnd_create_reg(s3),
                                        opnd_create_rel_addr(&(local->spill_space.s3), OPSZ_8)));
            }
        }
    } else {
        //if TLS is not read, then we can simply use TLS
        if (regMask & get_reg_bit_array(s0)) {
            PRE_INSERT(bb, trigger,
                INSTR_CREATE_mov_ld(drcontext,
                                    opnd_create_reg(s0),
                                    OPND_CREATE_MEM64(TLS, LOCAL_S0_OFFSET)));
            //restore shared stack pointer
            if (!instr_writes_to_reg(trigger, s0, DR_QUERY_INCLUDE_ALL)) {
                PRE_INSERT(bb, trigger_next,
                    INSTR_CREATE_mov_ld(drcontext,
                                        opnd_create_reg(s0),
                                        opnd_create_rel_addr(&(shared->stack_ptr), OPSZ_8)));
            }
        }

        if (loop->schedule != PARA_DOALL_BLOCK) {
            if (regMask & get_reg_bit_array(s2)) {
                PRE_INSERT(bb, trigger,
                    INSTR_CREATE_mov_ld(drcontext,
                                        opnd_create_reg(s2),
                                        OPND_CREATE_MEM64(TLS, LOCAL_S2_OFFSET)));
            }
            if (regMask & get_reg_bit_array(s3)) {
                PRE_INSERT(bb, trigger,
                    INSTR_CREATE_mov_ld(drcontext,
                                        opnd_create_reg(s3),
                                        OPND_CREATE_MEM64(TLS, LOCAL_S3_OFFSET)));
            }
        }
    }
#elif JANUS_AARCH64
    reg_id_t secondTLSReg;
    //if the TLS is read
    if (regMask & get_reg_bit_array(TLS) || instr_writes_to_reg(trigger, TLS, DR_QUERY_INCLUDE_ALL)) {

        if (!(regMask & get_reg_bit_array(s2) || instr_writes_to_reg(trigger, s2, DR_QUERY_INCLUDE_ALL))) {
            secondTLSReg = s2;
        }
        else if (!(regMask & get_reg_bit_array(s3) || instr_writes_to_reg(trigger, s3, DR_QUERY_INCLUDE_ALL))) {
            secondTLSReg = s3;
        }
        else
            DR_ASSERT("No scratch register free to hold the TLS");

        // Before instruction, move s1 to free scratch register
        PRE_INSERT(bb, trigger,
            XINST_CREATE_move(drcontext,
                            opnd_create_reg(secondTLSReg),
                            opnd_create_reg(TLS)));

        if( !( instr_writes_to_reg(trigger, s0, DR_QUERY_INCLUDE_ALL)
            || instr_writes_to_reg(trigger, s1, DR_QUERY_INCLUDE_ALL)
            || instr_writes_to_reg(trigger, s2, DR_QUERY_INCLUDE_ALL)
            || instr_writes_to_reg(trigger, s3, DR_QUERY_INCLUDE_ALL) )){
            //If no writes are performed, there is no spill handler call so insert a { mov s1, secondTLSReg } instr 
            PRE_INSERT(bb, trigger_next,
                XINST_CREATE_move(drcontext,
                                opnd_create_reg(TLS),
                                opnd_create_reg(secondTLSReg)));
        }
    }

    if (regMask & get_reg_bit_array(s0)) {
        if (instr_reads_from_reg(trigger, s0, DR_QUERY_INCLUDE_ALL)) {
            //TODO if s0 is used as an offset reg for a stack access, must use another register as shared stack ptr
            DR_ASSERT("s0 is pre-occupied");
        } else {
            // Before instruction, load s0
            PRE_INSERT(bb, trigger,
                instr_create_1dst_1src(drcontext,
                                    OP_ldr,
                                    opnd_create_reg(s0),
                                    OPND_CREATE_MEM64(TLS, LOCAL_S0_OFFSET)));
            // After instruction, reload shared stack pointer
            PRE_INSERT(bb, trigger_next,
                instr_create_1dst_1src(drcontext,
                                    OP_ldr,
                                    opnd_create_reg(s0),
                                    opnd_create_rel_addr(&(shared->stack_ptr), OPSZ_8)));
        }
    }

    if (loop->schedule != PARA_DOALL_BLOCK) {
        if (regMask & get_reg_bit_array(s2)) {
            PRE_INSERT(bb, trigger,
                instr_create_1dst_1src(drcontext,
                                    OP_ldr,
                                    opnd_create_reg(s2),
                                    OPND_CREATE_MEM64(TLS, LOCAL_S2_OFFSET)));
        }
        if (regMask & get_reg_bit_array(s3)) {
            PRE_INSERT(bb, trigger,
                instr_create_1dst_1src(drcontext,
                                    OP_ldr,
                                    opnd_create_reg(s3),
                                    OPND_CREATE_MEM64(TLS, LOCAL_S3_OFFSET)));
        }
    }

    if(regMask & get_reg_bit_array(TLS)) { // Restore TLS last 
        PRE_INSERT(bb, trigger,
            instr_create_1dst_1src(drcontext,
                                OP_ldr,
                                opnd_create_reg(s3),
                                OPND_CREATE_MEM64(TLS, LOCAL_S3_OFFSET)));
    }

#endif
}

void
loop_switch_to_main_stack_handler(JANUS_CONTEXT)
{
    int loop_id = rule->reg1;
    loop_t *loop = &(shared->loops[loop_id]);

    reg_id_t s0 = loop->header->scratchReg0;
    reg_id_t s1 = loop->header->scratchReg1;
    reg_id_t s2 = loop->header->scratchReg2;
    reg_id_t s3 = loop->header->scratchReg3;

    instr_t *trigger = get_trigger_instruction(bb,rule);

    opnd_t op, stk, new_stk;

    int i, srci=-1, dsti=-1;
#ifdef JANUS_VERBOSE
    dr_printf("old stack ");
    instr_disassemble(drcontext, trigger,STDOUT);
    dr_printf("\n");
#endif

#ifdef JANUS_X86
    if (instr_reads_memory(trigger)) {
        for (i = 0; i < instr_num_srcs(trigger); i++) {
            op = instr_get_src(trigger, i);
            if (opnd_is_memory_reference(op) &&
                opnd_uses_reg(op, DR_REG_RSP)) {
                stk = op;
                srci = i;
                break;
            }
        }
    }

    if (instr_writes_memory(trigger)) {
        for (i = 0; i < instr_num_dsts(trigger); i++) {
            op = instr_get_dst(trigger, i);
            if (opnd_is_memory_reference(op) &&
                opnd_uses_reg(op, DR_REG_RSP)) {
                stk = op;
                dsti = i;
                break;
            }
        }
    }

    if (instr_get_opcode(trigger) == OP_lea) {
        for (i = 0; i < instr_num_srcs(trigger); i++) {
            op = instr_get_src(trigger, i);
            if (opnd_is_memory_reference(op) &&
                opnd_uses_reg(op, DR_REG_RSP)) {
                stk = op;
                srci = i;
                break;
            }
        }
    }

    if (srci != -1 || dsti != -1) {
        /* create a new operand */
        new_stk = opnd_create_base_disp(s0,
                                        opnd_get_index(stk),
                                        opnd_get_scale(stk),
                                        opnd_get_disp(stk),
                                        opnd_get_size(stk));

        if (srci != -1)
            instr_set_src(trigger, srci, new_stk);
        if (dsti != -1)
            instr_set_dst(trigger, dsti, new_stk);
    }

#elif JANUS_AARCH64

    reg_id_t stackReg = s0; // Default s0, but other if s0 has been spilled

    // Warning: the following code relies on the order in which registers are chosen by loop_scratch_register_handler
    if (instr_writes_to_reg(trigger, s0, DR_QUERY_INCLUDE_ALL) || instr_reads_from_reg(trigger, s0, DR_QUERY_INCLUDE_ALL))   {
        // If TLS is read/written then one scratch reg will be used to store it 
        if (instr_writes_to_reg(trigger, TLS, DR_QUERY_INCLUDE_ALL)
            || instr_reads_from_reg(trigger, TLS, DR_QUERY_INCLUDE_ALL)) {

            // If another scratch reg is used by the instruction, we do not have enough
            if (instr_writes_to_reg(trigger, s2, DR_QUERY_INCLUDE_ALL)
                    || instr_reads_from_reg(trigger, s2, DR_QUERY_INCLUDE_ALL)
                    || instr_writes_to_reg(trigger, s3, DR_QUERY_INCLUDE_ALL)
                    || instr_reads_from_reg(trigger, s3, DR_QUERY_INCLUDE_ALL)) {
                DR_ASSERT("Not enough unused scratch registers to hold TLS (s1) and buffer shared stack (s0)");
            }
            // Otherwise use s3
            else {
                stackReg = s3;
            }
        }
        else {

            if(!(instr_writes_to_reg(trigger, s2, DR_QUERY_INCLUDE_ALL)
                    || instr_reads_from_reg(trigger, s2, DR_QUERY_INCLUDE_ALL))) {
                stackReg = s2;
            }
            if(!(instr_writes_to_reg(trigger, s3, DR_QUERY_INCLUDE_ALL)
                    || instr_reads_from_reg(trigger, s3, DR_QUERY_INCLUDE_ALL))) {
                stackReg = s3;
            }
            else {
                DR_ASSERT("No scratch registers free to buffer shared stack while instruction uses s0");
            }
        }
        // Load shared stack base into replacement shared stack register
        PRE_INSERT(bb, trigger,
            instr_create_1dst_1src(drcontext,
                                OP_ldr,
                                opnd_create_reg(stackReg),
                                opnd_create_rel_addr(&(shared->stack_base), OPSZ_8)));
    }

    if (instr_reads_memory(trigger)) {
        for (i = 0; i < instr_num_srcs(trigger); i++) {
            op = instr_get_src(trigger, i);
            if (opnd_is_memory_reference(op) &&
                opnd_uses_reg(op, DR_REG_X29)) {
                opnd_replace_reg(&op, DR_REG_X29, stackReg);
                instr_set_src(trigger, i, op);
            }
        }
    }

    if (instr_writes_memory(trigger)) {
        for (i = 0; i < instr_num_dsts(trigger); i++) {
            op = instr_get_dst(trigger, i);
            if (opnd_is_memory_reference(op) &&
                opnd_uses_reg(op, DR_REG_X29)) {
                opnd_replace_reg(&op, DR_REG_X29, stackReg);
                instr_set_dst(trigger, i, op);
            }
        }
    }

#endif

#ifdef JANUS_VERBOSE
    dr_printf("new stack ");
    instr_disassemble(drcontext, trigger,STDOUT);
    dr_printf("\n");
#endif
}

void
loop_subcall_start_handler(JANUS_CONTEXT)
{
    int loop_id = rule->reg0;
    loop_t *loop = &(shared->loops[loop_id]);
    reg_id_t s0 = loop->header->scratchReg0;
    reg_id_t s1 = loop->header->scratchReg1;
    reg_id_t s2 = loop->header->scratchReg2;
    reg_id_t s3 = loop->header->scratchReg3;
    instr_t *trigger = get_trigger_instruction(bb, rule);

    /* Recover all scratch registers before performing the function call */
#ifdef JANUS_X86
    if (loop->schedule == PARA_DOALL_BLOCK) {
        //PRE_INSERT(bb,trigger,INSTR_CREATE_int1(drcontext));
        /* For block based parallelisation, only restore s0 and s1 */
        /* Load s0 */
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(s0),
                                OPND_CREATE_MEM64(s1, LOCAL_S0_OFFSET)));
        /* Load s1 (tls) */
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(s1),
                                OPND_CREATE_MEM64(s1, LOCAL_S1_OFFSET)));
    } else {
        //not yet implemented
        DR_ASSERT_MSG(false, "Subcall handler not yet implemented");
    }
#elif JANUS_AARCH64
    if (loop->schedule == PARA_DOALL_BLOCK) {
    /* For block based parallelisation, only restore s0 and s1 */
        if(s0 < DR_REG_X19) { // Since regs x19-x29 are guaranteed to be preserved by the subcall anyway
            /* Load s0 */
            PRE_INSERT(bb, trigger,
                instr_create_1dst_1src(drcontext,
                                    OP_ldr,
                                    opnd_create_reg(s0),
                                    OPND_CREATE_MEM64(TLS, LOCAL_S0_OFFSET)));
        } 
        if(s2 < DR_REG_X19) { // Since regs x19-x29 are guaranteed to be preserved by the subcall anyway
            /* Load s2 */
            PRE_INSERT(bb, trigger,
                instr_create_1dst_1src(drcontext,
                                    OP_ldr,
                                    opnd_create_reg(s2),
                                    OPND_CREATE_MEM64(TLS, LOCAL_S2_OFFSET)));
        }
        if(s3 < DR_REG_X19) { // Since regs x19-x29 are guaranteed to be preserved by the subcall anyway
            /* Load s3 */
            PRE_INSERT(bb, trigger,
                instr_create_1dst_1src(drcontext,
                                    OP_ldr,
                                    opnd_create_reg(s3),
                                    OPND_CREATE_MEM64(TLS, LOCAL_S3_OFFSET)));
        }
        if(s1 < DR_REG_X19) {
            /* Move s1 to x19 since registers x19-x29 must be preserved by the subroutine
                    (according the the procedure calling convention)  but save x19 to spill slot first */
            PRE_INSERT(bb, trigger,
                instr_create_1dst_1src(drcontext,
                                    OP_str,
                                    OPND_CREATE_MEM64(TLS, LOCAL_SLOT1_OFFSET),
                                    opnd_create_reg(DR_REG_X19)));
            PRE_INSERT(bb, trigger,
                XINST_CREATE_move(drcontext,
                                    opnd_create_reg(DR_REG_X19),
                                    opnd_create_reg(s1)));
            /* Load s1 */
            PRE_INSERT(bb, trigger,
                instr_create_1dst_1src(drcontext,
                                    OP_ldr,
                                    opnd_create_reg(s1),
                                    OPND_CREATE_MEM64(TLS, LOCAL_S1_OFFSET)));
        }
    } else {
        //not yet implemented
        DR_ASSERT("Subcall handler not yet implemented");
    }
#endif
}

void
loop_subcall_finish_handler(JANUS_CONTEXT)
{
    int loop_id = rule->reg0;
    loop_t *loop = &(shared->loops[loop_id]);
    reg_id_t s0 = loop->header->scratchReg0;
    reg_id_t s1 = loop->header->scratchReg1;
    reg_id_t s2 = loop->header->scratchReg2;
    reg_id_t s3 = loop->header->scratchReg3;
    instr_t *trigger = get_trigger_instruction(bb, rule);

#ifdef JANUS_X86
    janus_thread_t *local = dr_get_tls_field(drcontext);
    if (loop->schedule == PARA_DOALL_BLOCK) {
        //PRE_INSERT(bb,trigger,INSTR_CREATE_int1(drcontext));
        /* For block based parallelisation, only restore s0 and s1 */
        /* spill s1 (tls) */
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                opnd_create_rel_addr(&(local->spill_space.s1), OPSZ_8),
                                opnd_create_reg(s1)));

        /* Mov tls value */
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_mov_imm(drcontext,
                                 opnd_create_reg(s1),
                                 OPND_CREATE_INTPTR(local)));
        /* spill s0 */
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                OPND_CREATE_MEM64(s1, LOCAL_S0_OFFSET),
                                opnd_create_reg(s0)));
        /* Restore s0 with shared stack */
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(s0),
                                opnd_create_rel_addr(&(shared->stack_ptr), OPSZ_8)));
    } else {
        //not yet implemented
        DR_ASSERT_MSG(false, "Subcall handler not yet implemented");
    }
#elif JANUS_AARCH64
    if (loop->schedule == PARA_DOALL_BLOCK) {
        // Spill registers (that may contain return values) to the TLS
        if (s1 < DR_REG_X19) { // Since regs x19-x29 are guaranteed to be preserved by the subcall anyway
            /* Spill s1 value to TLS (currently in x19) */
            PRE_INSERT(bb, trigger,
                instr_create_1dst_1src(drcontext,
                                    OP_str,
                                    OPND_CREATE_MEM64(DR_REG_X19, LOCAL_S1_OFFSET),
                                    opnd_create_reg(s1)));

            /* s1 value should have been moved into x19 in subcall_start_handler, so move it back */
            PRE_INSERT(bb, trigger,
                XINST_CREATE_move(drcontext,
                                    opnd_create_reg(s1),
                                    opnd_create_reg(DR_REG_X19)));

            /* Restore x19's old value from the TLS */
            PRE_INSERT(bb, trigger,
                instr_create_1dst_1src(drcontext,
                                    OP_ldr,
                                    opnd_create_reg(DR_REG_X19),
                                    OPND_CREATE_MEM64(TLS, LOCAL_SLOT1_OFFSET)));
        }
        if (s0 < DR_REG_X19) {
            /* Spill s0 value to TLS */
            PRE_INSERT(bb, trigger,
                instr_create_1dst_1src(drcontext,
                                    OP_str,
                                    OPND_CREATE_MEM64(TLS, LOCAL_S0_OFFSET),
                                    opnd_create_reg(s0)));

            /* Restore shared stack ptr into s0 */
            insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s0), (ptr_int_t)&(shared->stack_base));
            PRE_INSERT(bb, trigger,
                instr_create_1dst_1src(drcontext,
                                    OP_ldr,
                                    opnd_create_reg(s0),
                                    OPND_CREATE_MEM64(s0, 0)));

        }
        if (s2 < DR_REG_X19) {
            /* Spill s2 value */
            PRE_INSERT(bb, trigger,
                instr_create_1dst_1src(drcontext,
                                    OP_str,
                                    OPND_CREATE_MEM64(TLS, LOCAL_S2_OFFSET),
                                    opnd_create_reg(s2)));
        }
        if (s3 < DR_REG_X19) {
            /* Spill s3 value */
            PRE_INSERT(bb, trigger,
                instr_create_1dst_1src(drcontext,
                                    OP_str,
                                    OPND_CREATE_MEM64(TLS, LOCAL_S3_OFFSET),
                                    opnd_create_reg(s3)));
        }
    } else {
        //not yet implemented
        DR_ASSERT("Subcall handler not yet implemented");
    }
#endif
}

void
loop_outer_init_handler(JANUS_CONTEXT)
{
    instr_t *trigger = get_trigger_instruction(bb,rule);
}

void
loop_outer_finish_handler(JANUS_CONTEXT)
{
    instr_t *trigger = get_trigger_instruction(bb,rule);
}

#ifdef NOT_YET_WORKING_FOR_ALL
void
loop_induction_variable_handler(JANUS_CONTEXT)
{
    int i;
    instr_t *trigger = get_trigger_instruction(bb,rule);
    PCAddress bbAddr = (PCAddress)dr_fragment_app_pc(tag);

    if (ginfo.number_of_threads == 1) return;
    /* Get current loop information */
    Loop_t *loop = &(shared->loops[rule->reg0]);
    /* Generate update function for each induction variable */
    for (i=0; i<loop->num_of_variables; i++) {
        JVarProfile profile = loop->variables[i];
        GVar var = profile.var;
#ifdef GSTM_VERBOSE
        printf("loop %d data size %d ",loop->header->id, loop->num_of_variables);
        print_var_profile(&profile);
#endif
        if (profile.type == INDUCTION_PROFILE) {

            if (profile.induction.op == UPDATE_ADD) {
                //add
                if (var.type == VAR_REG) {
                    PRE_INSERT(bb, trigger,
                        INSTR_CREATE_add(drcontext,
                                         opnd_create_reg(var.value),
                                         OPND_CREATE_INT32((ginfo.number_of_threads-1) * profile.induction.stride)));
                } else {
                    printf("Induction variable not implemented other than registers for DOALL loops\n");
                    exit(-1);
                }
            } else {
                printf("Induction variable operation not implemented for DOALL loops\n");
                exit(-1);
            }
        }
    }

    /* Generate update function for the move reduction variable */
    for (i=0; i<loop->num_of_variables; i++) {
        JVarProfile profile = loop->variables[i];
        GVar var = profile.var;

        if (profile.type == REDUCTION_PROFILE) {
            if (profile.reduction.op == UPDATE_MOV) {
                //add
                if (var.type == VAR_REG) {
                    PRE_INSERT(bb, trigger,
                        INSTR_CREATE_mov_ld(drcontext,
                                            opnd_create_reg(var.value),
                                            opnd_create_reg(profile.reduction.reg)));
                } else {
                    printf("Reduction variable not implemented other than registers for DOALL loops\n");
                    exit(-1);
                }
            }
        }
    }
}

void
loop_induction_stack_handler(JANUS_CONTEXT)
{
    int loop_id = rule->reg1;
    Loop_t *loop = &(shared->loops[loop_id]);
    SReg sregs = loop->func.sregs;
    reg_id_t tls_reg = sregs.s3;

    instr_t *trigger = get_trigger_instruction(bb,rule);
    opnd_t op, stk, new_stk;
    int i, srci=-1, dsti=-1;
#ifdef JANUS_VERBOSE
    instr_disassemble(drcontext, trigger,STDOUT);
    dr_printf("\n");
#endif

    if (instr_reads_memory(trigger)) {
        for (i = 0; i < instr_num_srcs(trigger); i++) {
            op = instr_get_src(trigger, i);
            if (opnd_is_memory_reference(op) &&
                opnd_uses_reg(op, DR_REG_RSP)) {
                stk = op;
                srci = i;
                break;
            }
        }
    }

    if (instr_writes_memory(trigger)) {
        for (i = 0; i < instr_num_dsts(trigger); i++) {
            op = instr_get_dst(trigger, i);
            if (opnd_is_memory_reference(op) &&
                opnd_uses_reg(op, DR_REG_RSP)) {
                stk = op;
                dsti = i;
                break;
            }
        }
    }

    if (instr_get_opcode(trigger) == OP_lea) {
        for (i = 0; i < instr_num_srcs(trigger); i++) {
            op = instr_get_src(trigger, i);
            if (opnd_is_memory_reference(op) &&
                opnd_uses_reg(op, DR_REG_RSP)) {
                stk = op;
                srci = i;
                break;
            }
        }
    }

    if (srci != -1 || dsti != -1) {
        if (loop->type == LOOP_DOSPEC) {
            PRE_INSERT(bb, trigger,
                INSTR_CREATE_mov_ld(drcontext,
                                    opnd_create_reg(sregs.s0),
                                    opnd_create_rel_addr((void *)&(shared->stack_ptr), OPSZ_8)));
        }
        if (instr_get_opcode(trigger) == OP_cmp &&
            opnd_get_disp(stk) < 0) {
            PRE_INSERT(bb, trigger,
                INSTR_CREATE_mov_imm(drcontext,
                                    opnd_create_reg(sregs.s0),
                                    OPND_CREATE_INT64(local)));
            new_stk = opnd_create_base_disp(sregs.s0,
                                        0,
                                        0,
                                        LOCAL_CFLAG_OFFSET,
                                        opnd_get_size(stk));
        } else if (rule->reg0 != sregs.s0) {
            PRE_INSERT(bb, trigger,
                INSTR_CREATE_mov_ld(drcontext,
                                    opnd_create_reg(sregs.s1),
                                    opnd_create_rel_addr((void *)&(shared->stack_ptr), OPSZ_8)));
            new_stk = opnd_create_base_disp(sregs.s1,
                                            opnd_get_index(stk),
                                            opnd_get_scale(stk),
                                            opnd_get_disp(stk),
                                            opnd_get_size(stk));
        } else {
            /* create a new operand */
            new_stk = opnd_create_base_disp(rule->reg0,
                                            opnd_get_index(stk),
                                            opnd_get_scale(stk),
                                            opnd_get_disp(stk),
                                            opnd_get_size(stk));
        }

        if (srci != -1)
            instr_set_src(trigger, srci, new_stk);
        if (dsti != -1)
            instr_set_dst(trigger, dsti, new_stk);
    }
#ifdef JANUS_VERBOSE
    instr_disassemble(drcontext, trigger,STDOUT);
    dr_printf("\n");
#endif
}

void
stat_update_handler(JANUS_CONTEXT)
{
#ifdef JANUS_STATS
    instr_t *trigger = get_trigger_instruction(bb, rule);
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_add(drcontext,
                         opnd_create_rel_addr(&(local->stats.counter1), OPSZ_8),
                         OPND_CREATE_INT32(1)));
#endif
}

#ifdef JANUS_STATS
static void static_timer_start(long down, long up) {
    local->stats.loop_temp_time = (up<<32) + down;
}

static void static_timer_end(long down, long up) {
    local->stats.loop_time += (up<<32) + down - local->stats.loop_temp_time;
}
#endif

void
stat_timer_start_handler(JANUS_CONTEXT)
{
#ifdef JANUS_STATS
    instr_t *trigger = get_trigger_instruction(bb, rule);
    dr_save_reg(drcontext, bb, trigger, DR_REG_RAX, SPILL_SLOT_1);
    dr_save_reg(drcontext, bb, trigger, DR_REG_RDX, SPILL_SLOT_2);
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_rdtsc(drcontext));
    dr_insert_clean_call(drcontext, bb, trigger,
                         static_timer_start, false, 2,
                         opnd_create_reg(DR_REG_RAX),
                         opnd_create_reg(DR_REG_RDX));
    dr_restore_reg(drcontext, bb, trigger, DR_REG_RAX, SPILL_SLOT_1);
    dr_restore_reg(drcontext, bb, trigger, DR_REG_RDX, SPILL_SLOT_2);
#endif
}

void
stat_timer_end_handler(JANUS_CONTEXT)
{
#ifdef JANUS_STATS
    instr_t *trigger = get_trigger_instruction(bb, rule);
    dr_save_reg(drcontext, bb, trigger, DR_REG_RAX, SPILL_SLOT_1);
    dr_save_reg(drcontext, bb, trigger, DR_REG_RDX, SPILL_SLOT_2);
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_rdtsc(drcontext));
    dr_insert_clean_call(drcontext, bb, trigger,
                         static_timer_end, false, 2,
                         opnd_create_reg(DR_REG_RAX),
                         opnd_create_reg(DR_REG_RDX));
    dr_restore_reg(drcontext, bb, trigger, DR_REG_RAX, SPILL_SLOT_1);
    dr_restore_reg(drcontext, bb, trigger, DR_REG_RDX, SPILL_SLOT_2);
#endif
}

/* Redirect all absolute addresses to local buffer */
void
privatise_address_handler(JANUS_CONTEXT)
{
    instr_t *trigger = get_trigger_instruction(bb, rule);
    opnd_t op, new_op;
    int i, srci=-1, dsti=-1;
#ifdef JANUS_LOOP_VERBOSE
    instr_disassemble(drcontext, trigger,STDOUT);
    dr_printf("\n");
#endif
    if (instr_reads_memory(trigger)) {
        for (i = 0; i < instr_num_srcs(trigger); i++) {
            op = instr_get_src(trigger, i);
            if (opnd_is_rel_addr(op)) {
                srci = i;
                break;
            }
        }
    }

    if (instr_writes_memory(trigger)) {
        for (i = 0; i < instr_num_dsts(trigger); i++) {
            op = instr_get_dst(trigger, i);
            if (opnd_is_rel_addr(op)) {
                dsti = i;
                break;
            }
        }
    }

    if (srci != -1 || dsti != -1) {
        /* Check whether the address matches with static rule */
        if (opnd_get_addr(op) != (app_pc)rule->reg1) {
            dr_printf("Rule for absolute variables not match at runtime! %lx - %lx",opnd_get_addr(op),rule->reg1);
        }
        /* create a new operand */
        new_op = opnd_create_rel_addr((void *)&(local->buffer[rule->ureg0.down]), OPSZ_8);
        if (srci != -1)
            instr_set_src(trigger, srci, new_op);
        if (dsti != -1)
            instr_set_dst(trigger, dsti, new_op);
    } else {
        dr_printf("Absolute address not found %lx\n",rule->reg1);
    }
}

void
dynamic_alias_check_handler(JANUS_CONTEXT)
{
#ifdef JANUS_DYNAMIC_CHECK
    instr_t *trigger = get_trigger_instruction(bb, rule);
    GVar var1 = *(GVar *)&(rule->reg0);
    GVar var2 = *(GVar *)&(rule->reg1);
    var1.size = 8;
    var2.size = 8;
    opnd_t check1, check2;
    check1 = convert_to_dr_opnd(*(GVar *)&(rule->reg0));
    check2 = convert_to_dr_opnd(*(GVar *)&(rule->reg1));

    if (var1.type == VAR_STK && var2.type == VAR_STK) {
        dr_save_reg(drcontext, bb, trigger, DR_REG_R15, SPILL_SLOT_1);
        PRE_INSERT(bb, trigger,
               INSTR_CREATE_mov_ld(drcontext,
                                   opnd_create_reg(DR_REG_R15),
                                   check2));
        PRE_INSERT(bb, trigger,
               INSTR_CREATE_cmp(drcontext,
                                opnd_create_reg(DR_REG_R15),
                                check1));
        dr_restore_reg(drcontext, bb, trigger, DR_REG_R15, SPILL_SLOT_1);
        PRE_INSERT(bb, trigger,
               INSTR_CREATE_jcc(drcontext, OP_je, opnd_create_pc((app_pc)runtime_check_fail)));

    } else {
        PRE_INSERT(bb, trigger,
               INSTR_CREATE_cmp(drcontext,
                                check1,
                                check2));
        PRE_INSERT(bb, trigger,
               INSTR_CREATE_jcc(drcontext, OP_je, opnd_create_pc((app_pc)runtime_check_fail)));
        //instrlist_disassemble(drcontext, 0, bb, STDOUT);
    }
#endif
}

void
user_defined_recompilder(JANUS_CONTEXT)
{
    int scratchSet = rule->ureg0.up;
    int loop_id = rule->ureg0.down;
    uint32_t secondStackReg = rule->reg1;
    Loop_t *loop = &(shared->loops[loop_id]);
    SReg sregs = loop->func.sregs;
    reg_id_t tls_reg = sregs.s3;
    reg_id_t reg;
    int i;
    instr_t *lock = INSTR_CREATE_label(drcontext);
    instr_t *next = INSTR_CREATE_label(drcontext);
    instr_t *trigger = get_trigger_instruction(bb, rule);

    if (rule->ureg0.down == 0) {
        /* Acquire lock to load r11 */
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_mov_imm(drcontext, opnd_create_reg(sregs.s0), OPND_CREATE_INT32(1)));
        /* Spin lock */
        PRE_INSERT(bb, trigger, lock);
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_xchg(drcontext, opnd_create_rel_addr(&(shared->global_lock), OPSZ_8), opnd_create_reg(sregs.s0)));
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_test(drcontext, opnd_create_reg(sregs.s0), opnd_create_reg(sregs.s0)));
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_jcc(drcontext, OP_jnz, opnd_create_instr(lock)));
        /* After the lock is acquired, load the r11 value */
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(DR_REG_R11), opnd_create_rel_addr(&(shared->registers.r11), OPSZ_8)));

        POST_INSERT(bb, trigger, next);
        PRE_INSERT(bb, next,
            INSTR_CREATE_mov_st(drcontext, opnd_create_rel_addr(&(shared->registers.r11), OPSZ_8), opnd_create_reg(DR_REG_R11)));
        PRE_INSERT(bb, next,
            INSTR_CREATE_mov_st(drcontext, opnd_create_rel_addr(&(shared->registers.rcx), OPSZ_8), opnd_create_reg(DR_REG_R11)));
        /* Release the lock */
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_xor(drcontext, opnd_create_reg(sregs.s0), opnd_create_reg(sregs.s0)));
        //PRE_INSERT(bb,next,INSTR_CREATE_int1(drcontext));
        PRE_INSERT(bb, next,
            INSTR_CREATE_mov_st(drcontext, opnd_create_rel_addr(&(shared->global_lock), OPSZ_8), opnd_create_reg(sregs.s0)));
    }   
}

instrlist_t *
build_loop_quick_init_instrlist(void *drcontext, SReg sregs, Loop_t *loop)
{
    int i;
    instrlist_t *bb;
    instr_t *trigger, *instr;
    reg_id_t tls_reg = sregs.s3;
    reg_id_t thread_reg = sregs.s0;
    bool redirect_thread_reg = false;
    reg_id_t reg;

    bb = instrlist_create(drcontext);

    /* Jump back to DR's code cache. */
    trigger = INSTR_CREATE_jmp_ind(drcontext, opnd_create_reg(sregs.s1));
    APPEND(bb, trigger);

}

instrlist_t *
build_loop_quick_finish_instrlist(void *drcontext, SReg sregs, Loop_t *loop)
{
    int i;
    instrlist_t *bb;
    instr_t *trigger, *instr;
    reg_id_t tls_reg = sregs.s3;
    reg_id_t thread_reg = sregs.s0;
    bool redirect_thread_reg = false;
    reg_id_t reg;

    bb = instrlist_create(drcontext);

    /* Jump back to DR's code cache. */
    trigger = INSTR_CREATE_jmp_ind(drcontext, opnd_create_reg(sregs.s1));
    APPEND(bb, trigger);

}

instrlist_t *
build_thread_quick_loop_init_instrlist(void *drcontext, SReg sregs, Loop_t *loop)
{
    int i;
    instrlist_t *bb;
    instr_t *trigger, *instr;
    reg_id_t tls_reg = sregs.s3;
    reg_id_t thread_reg = sregs.s0;
    bool redirect_thread_reg = false;
    reg_id_t reg;

    bb = instrlist_create(drcontext);

    /* Jump back to DR's code cache. */
    trigger = INSTR_CREATE_jmp_ind(drcontext, opnd_create_reg(sregs.s1));
    APPEND(bb, trigger);

}

instrlist_t *
build_thread_quick_loop_finish_instrlist(void *drcontext, SReg sregs, Loop_t *loop)
{
    int i;
    instrlist_t *bb;
    instr_t *trigger, *instr;
    reg_id_t tls_reg = sregs.s3;
    reg_id_t thread_reg = sregs.s0;
    bool redirect_thread_reg = false;
    reg_id_t reg;

    bb = instrlist_create(drcontext);

    /* Jump back to DR's code cache. */
    trigger = INSTR_CREATE_jmp_ind(drcontext, opnd_create_reg(sregs.s1));
    APPEND(bb, trigger);

}
#endif
