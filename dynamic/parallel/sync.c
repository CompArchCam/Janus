#include "sync.h"
#include "stm_internal.h"
#include "thread.h"
#include "util.h"
#include "hashtable.h"
#include <string.h>

void generate_dynamic_sync_code_handler(JANUS_CONTEXT)
{
    int i;
    int num_channels = rule->ureg0.up;
    dr_printf("num of channels %d\n", rule->ureg0.up);

    for(i=0; i<ginfo.number_of_threads; i++) {
        oracle[i]->sync.entries = (sync_entry_t *)malloc(sizeof(sync_entry_t) * num_channels);
        memset(oracle[i]->sync.entries, 0, sizeof(sync_entry_t) * num_channels);
        oracle[i]->sync.size = num_channels;
    }
}

void wait_register_handler(JANUS_CONTEXT)
{
    SReg sregs = local->current_code_cache->sregs;
    reg_id_t tls_reg = sregs.s3;
    reg_id_t sync_reg = rule->reg1;

    instr_t *trigger = get_trigger_instruction(bb, rule);
    instr_t *skip = INSTR_CREATE_label(drcontext);

    /* Firstly, load the data entry address */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_imm(drcontext,
                             opnd_create_reg(sregs.s0),
                             OPND_CREATE_INTPTR(&(local->sync.entries[rule->ureg0.up]))));

    /* Then check the current stamp */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s1),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_STAMP_OFFSET)));
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_sub(drcontext,
                         opnd_create_reg(sregs.s1),
                         opnd_create_rel_addr(&(shared->global_stamp), OPSZ_8)));
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_test(drcontext,
                          opnd_create_reg(sregs.s1),
                          opnd_create_reg(sregs.s1)));
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_jz, opnd_create_instr(skip)));

    instr_t *wait = INSTR_CREATE_label(drcontext);

    PRE_INSERT(bb, trigger, wait);

    /* We also need to check thread yield flag here */
    INSERT(bb, trigger,
        INSTR_CREATE_cmp(drcontext,
                         opnd_create_rel_addr((void *)(&shared->need_yield), OPSZ_4),
                         OPND_CREATE_INT32(1)));
    /* If need yield is on, jump to the pool */
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_je, opnd_create_pc((app_pc)shared->current_loop->code.jump_to_pool)));

    /* Compare the local stamp with data version */
    INSERT(bb, trigger,
        INSTR_CREATE_cmp(drcontext,
                         opnd_create_reg(sregs.s1),
                         OPND_CREATE_MEM64(sregs.s0, 64)));
    /* If local stamp is greater than the data version, wait */
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_jg, opnd_create_instr(wait)));

    /* Now the data is ready
     * Load the data first */
    if (sync_reg <= DR_REG_R15 && sync_reg >= DR_REG_RAX) {
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(sync_reg),
                                OPND_CREATE_MEM64(sregs.s0, 0)));
        PRE_INSERT(bb, trigger, skip);
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                OPND_CREATE_MEM64(tls_reg, LOCAL_STATE_OFFSET + (sync_reg - DR_REG_RAX) * 8),
                                opnd_create_reg(sync_reg)));
    } else {
        DR_ASSERT_MSG(false,
                      "SIMD sync NYI implemented");
        PRE_INSERT(bb, trigger, skip);
    }
}

void signal_register_handler(JANUS_CONTEXT)
{
    SReg sregs = local->current_code_cache->sregs;
    reg_id_t tls_reg = sregs.s3;
    reg_id_t sync_reg = rule->reg1;
    int channel = rule->reg0;

    instr_t *trigger = get_trigger_instruction(bb, rule);
    instr_t *wait_for_unset = INSTR_CREATE_label(drcontext);
    instr_t *instr_next = instr_get_next(trigger);

    /* write to the signal */
    PRE_INSERT(bb, instr_next,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_rel_addr(&(local->next->sync.entries[channel].data), OPSZ_8),
                            opnd_create_reg(sync_reg)));
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s0),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_STAMP_OFFSET)));
    /* Increment version */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_inc(drcontext,
                         opnd_create_reg(sregs.s0)));
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_rel_addr(&(local->next->sync.entries[channel].version), OPSZ_8),
                            opnd_create_reg(sregs.s0)));
    //PRINTREG(sync_reg);
}

void wait_memory_handler(JANUS_CONTEXT)
{
    instr_t             *instr, *skipLabel;
    opnd_t              mem_check;
    int                 dynamic_check_range = 0;
    uint32_t              i;

    //get meta information of the instruciton
    GMemInfo info = *(GMemInfo *)(&(rule->reg0));
    uint32_t sregMask = (uint32_t)(rule->ureg1.down);
    uint32_t sindexMask = (uint32_t)(rule->ureg1.up);

    instr_t *trigger = get_trigger_instruction(bb,rule);

    dynamic_code_t *hashfunc = local->current_code_cache;
    SReg sregs = hashfunc->sregs;

    uint32_t sreg0 = sregs.s0;
    uint32_t sreg1 = sregs.s1;
    uint32_t sreg2 = sregs.s2;
    uint32_t sreg3 = sregs.s3;

    int mode = get_instr_mode(trigger);
    int size = info.size;
    int srci = -1;
    int dsti = -1;
    //obtain the memory operands
    opnd_t mem = get_memory_operand(trigger, mode, &srci, &dsti);

    if (rule->reg1)
        insert_recover_scratch_regs(drcontext, bb,trigger,
                                    local, IF_DEBUG_ELSE(rule->id,0),
                                    sregMask, sindexMask, 0);

    /* Mangle stack access */
    if (opnd_uses_reg(mem, DR_REG_XSP)) {
        /* firstly check if it is indirect access */
#ifdef IND_STK_CHECK
        if (opnd_get_index(mem) != DR_REG_NULL) {
            dynamic_check_range = 1;
            /* create memory operand without base */
            mem_check = opnd_create_base_disp(DR_REG_NULL,
                                              opnd_get_index(mem),
                                              opnd_get_scale(mem),
                                              opnd_get_disp(mem),
                                              size);
            /* load the dynamic range into sreg1 */
            load_effective_address(drcontext, bb, trigger, mem_check, sreg1, sreg2);
            /* save arithmetic flag */

        } else {
            /* The offset must be positive to get mangled */
            int disp = opnd_get_disp(mem);
            DR_ASSERT_MSG(disp >= 0, "Negative offset w.r.t RSP");
        }
#endif
        reg_id_t sptr = (sreg0 == opnd_get_index(mem)) ? sreg1 : sreg0;
        //load the main shared stack into sreg0
        PRE_INSERT(bb, trigger,
                   INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(sptr),
                                       opnd_create_rel_addr(&(shared->stack_ptr), OPSZ_8)));
        //create memory operand by replacing the thread-private to shared stack pointer
        mem = opnd_create_base_disp(sptr, opnd_get_index(mem),
                                    opnd_get_scale(mem), opnd_get_disp(mem), size);
        //Now the memory operand has been replaced
        load_effective_address(drcontext,bb,trigger,mem,sreg0,sreg1);
    } else if (opnd_uses_reg(mem,DR_REG_XBP)) {
#ifdef IND_STK_CHECK
        /* Some apps have RBP as their base pointer */
        /* firstly check if it is indirect access */
        if (opnd_get_index(mem) != DR_REG_NULL) {
            dynamic_check_range = 1;
            //create memory operand without base
            mem_check = opnd_create_base_disp(DR_REG_NULL,
                                              opnd_get_index(mem),
                                              opnd_get_scale(mem),
                                              opnd_get_disp(mem),
                                              size);
        } else {
            /* The offset must be positive to get mangled */
            int disp = opnd_get_disp(mem);
            DR_ASSERT_MSG(disp <= 0, "Positive offset w.r.t RBP");
        }
#endif
        //load the main shared stack base into sreg0
        PRE_INSERT(bb, trigger,
                   INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(sreg0),
                                       opnd_create_rel_addr(&(shared->stack_base), OPSZ_8)));
        //create memory operand by replacing the thread-private to shared stack pointer
        mem = opnd_create_base_disp(sreg0, opnd_get_index(mem),
                                    opnd_get_scale(mem), opnd_get_disp(mem), size);
        //Now the memory operand has been replaced
        load_effective_address(drcontext,bb,trigger,mem,sreg0,sreg1);
    } else {
        DR_ASSERT_MSG(false, "Wrong static rule, current instruction is not stack access");
        return;
    }
/*
    XXX: Add range checks
    if(dynamic_check_range) {
        DR_ASSERT_MSG(false, "NOT YET IMPLEMENTED");
    }
*/
    /* To prevent TLS being polluted, after loading effective address,
     * load TLS again */
    if (rule->reg1)
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_mov_imm(drcontext,
                                 opnd_create_reg(sreg3),
                                 OPND_CREATE_INT64(local)));

    app_pc spec_procedure;
    /* Now we redirect this address to speculative read/write set */
    /* For memory operand size less than 8 bytes, first align the input address to be the granularity of the STM */
    if(info.size != 8) {
        if (mode == MEM_READ)
            spec_procedure = hashfunc->sync_read_partial;
        else if (mode == MEM_WRITE)
            spec_procedure = hashfunc->sync_write_partial;
        else
            spec_procedure = hashfunc->sync_read_write_partial;
    } else {
        if (mode == MEM_READ)
            spec_procedure = hashfunc->sync_read_full;
        else if (mode == MEM_WRITE)
            spec_procedure = hashfunc->sync_write_full;
        else
            spec_procedure = hashfunc->sync_read_write_full;
    }
    #if defined(GSTM_VERBOSE) && defined(GSTM_MEM_TRACE)
    dr_insert_clean_call(drcontext, bb, trigger, printAddress, false, 2, OPND_CREATE_INT32(IF_DEBUG_ELSE(rule->id,0)), opnd_create_reg(sreg0));
    #endif
    //s1 = local->loop_on
    PRE_INSERT(bb, trigger,
               INSTR_CREATE_mov_ld(drcontext,
                                   opnd_create_reg(reg_64_to_32(sreg1)),
                                   opnd_create_rel_addr((void *)&(local->flag_space.loop_on),OPSZ_4)));
    //test s1, s1
    PRE_INSERT(bb, trigger,
               INSTR_CREATE_test(drcontext,
                                 opnd_create_reg(sreg1),
                                 opnd_create_reg(sreg1)));
    //jz skip
    skipLabel = INSTR_CREATE_label(drcontext);
    PRE_INSERT(bb, trigger,
               INSTR_CREATE_jcc(drcontext, OP_jz,
                                opnd_create_instr(skipLabel)));
    //push the synchronisation entry
    PRE_INSERT(bb,trigger,
        INSTR_CREATE_mov_imm(drcontext,
                             opnd_create_reg(sreg1),
                             OPND_CREATE_INT64(&(local->sync.entries[info.channel].data))));
    PRE_INSERT(bb,trigger,
        INSTR_CREATE_push(drcontext, opnd_create_reg(sreg1)));
    //call spec procedure
    PRE_INSERT(bb,trigger,INSTR_CREATE_call(drcontext,
               opnd_create_pc(spec_procedure)));
    PRE_INSERT(bb,trigger,
        INSTR_CREATE_pop(drcontext, opnd_create_reg(sreg1)));

    PRE_INSERT(bb,trigger,skipLabel);

    //#if defined(GSTM_VERBOSE) && defined(GSTM_MEM_TRACE)
    //dr_insert_clean_call(drcontext, bb, trigger, printAddressData, false, 2, OPND_CREATE_INT32(IF_DEBUG_ELSE(rule->id,0)), opnd_create_reg(sreg0));
    //#endif

    reg_id_t srt_reg = sreg0;

    //recover xreg
    if (info.xreg) {
        if (info.xreg == sreg0) {
            srt_reg = sreg1;
            //move address to s1
            PRE_INSERT(bb, trigger,
                INSTR_CREATE_mov_ld(drcontext,
                                    opnd_create_reg(srt_reg),
                                    opnd_create_reg(sreg0)));
        }
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(info.xreg),
                                opnd_create_rel_addr(&(local->spill_space.s0) + (int)info.opPos, OPSZ_8)));
    }

    //change the original memory operand with indirect memory access [srt_reg]
    instr = reassemble_memory_instructions(drcontext, trigger, mode, info.size, srt_reg, srci, dsti);

    if (rule->reg1) {
        instr_t *instr_next = instr_get_next(trigger);
        insert_spill_scratch_regs(drcontext, bb, trigger, local, IF_DEBUG_ELSE(rule->id,0), sregMask, sindexMask, 0);
    }
    //replace the original memory instruction
    instrlist_replace(drcontext,trigger,instr);

}

void signal_memory_handler(JANUS_CONTEXT)
{
    instr_t             *instr, *skipLabel, *instr_next;
    opnd_t              mem_check;
    int                 dynamic_check_range = 0;
    uint32_t              i;

    //get meta information of the instruciton
    GMemInfo info = *(GMemInfo *)(&(rule->reg0));
    uint32_t sregMask = (uint32_t)(rule->ureg1.down);
    uint32_t sindexMask = (uint32_t)(rule->ureg1.up);

    instr_t *trigger = get_trigger_instruction(bb,rule);

    dynamic_code_t *hashfunc = local->current_code_cache;
    SReg sregs = hashfunc->sregs;

    uint32_t sreg0 = sregs.s0;
    uint32_t sreg1 = sregs.s1;
    uint32_t sreg2 = sregs.s2;
    uint32_t sreg3 = sregs.s3;

    int mode = get_instr_mode(trigger);
    int size = info.size;
    int srci = -1;
    int dsti = -1;
    //obtain the memory operands
    opnd_t mem = get_memory_operand(trigger, mode, &srci, &dsti);

    if (rule->reg1)
        insert_recover_scratch_regs(drcontext, bb,trigger,
                                    local, IF_DEBUG_ELSE(rule->id,0),
                                    sregMask, sindexMask, 0);

    /* Mangle stack access */
    if (opnd_uses_reg(mem, DR_REG_XSP)) {
        /* firstly check if it is indirect access */
#ifdef IND_STK_CHECK
        if (opnd_get_index(mem) != DR_REG_NULL) {
            dynamic_check_range = 1;
            /* create memory operand without base */
            mem_check = opnd_create_base_disp(DR_REG_NULL,
                                              opnd_get_index(mem),
                                              opnd_get_scale(mem),
                                              opnd_get_disp(mem),
                                              size);
            /* load the dynamic range into sreg1 */
            load_effective_address(drcontext, bb, trigger, mem_check, sreg1, sreg2);
            /* save arithmetic flag */

        } else {
            /* The offset must be positive to get mangled */
            int disp = opnd_get_disp(mem);
            DR_ASSERT_MSG(disp >= 0, "Negative offset w.r.t RSP");
        }
#endif
        reg_id_t sptr = (sreg0 == opnd_get_index(mem)) ? sreg1 : sreg0;
        //load the main shared stack into sreg0
        PRE_INSERT(bb, trigger,
                   INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(sptr),
                                       opnd_create_rel_addr(&(shared->stack_ptr), OPSZ_8)));
        //create memory operand by replacing the thread-private to shared stack pointer
        mem = opnd_create_base_disp(sptr, opnd_get_index(mem),
                                    opnd_get_scale(mem), opnd_get_disp(mem), size);
        //Now the memory operand has been replaced
        load_effective_address(drcontext,bb,trigger,mem,sreg0,sreg1);
    } else if (opnd_uses_reg(mem,DR_REG_XBP)) {
#ifdef IND_STK_CHECK
        /* Some apps have RBP as their base pointer */
        /* firstly check if it is indirect access */
        if (opnd_get_index(mem) != DR_REG_NULL) {
            dynamic_check_range = 1;
            //create memory operand without base
            mem_check = opnd_create_base_disp(DR_REG_NULL,
                                              opnd_get_index(mem),
                                              opnd_get_scale(mem),
                                              opnd_get_disp(mem),
                                              size);
        } else {
            /* The offset must be positive to get mangled */
            int disp = opnd_get_disp(mem);
            DR_ASSERT_MSG(disp <= 0, "Positive offset w.r.t RBP");
        }
#endif
        //load the main shared stack base into sreg0
        PRE_INSERT(bb, trigger,
                   INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(sreg0),
                                       opnd_create_rel_addr(&(shared->stack_base), OPSZ_8)));
        //create memory operand by replacing the thread-private to shared stack pointer
        mem = opnd_create_base_disp(sreg0, opnd_get_index(mem),
                                    opnd_get_scale(mem), opnd_get_disp(mem), size);
        //Now the memory operand has been replaced
        load_effective_address(drcontext,bb,trigger,mem,sreg0,sreg1);
    } else {
        DR_ASSERT_MSG(false, "Wrong static rule, current instruction is not stack access");
        return;
    }
/*
    XXX: Add range checks
    if(dynamic_check_range) {
        DR_ASSERT_MSG(false, "NOT YET IMPLEMENTED");
    }
*/
    /* To prevent TLS being polluted, after loading effective address,
     * load TLS again */
    if (rule->reg1)
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_mov_imm(drcontext,
                                 opnd_create_reg(sreg3),
                                 OPND_CREATE_INT64(local)));

    app_pc spec_procedure;
    /* Now we redirect this address to speculative read/write set */
    /* For memory operand size less than 8 bytes, first align the input address to be the granularity of the STM */
    if(info.size != 8) {
        if (mode == MEM_READ)
            spec_procedure = hashfunc->spec_read_partial;
        else if (mode == MEM_WRITE)
            spec_procedure = hashfunc->spec_write_partial;
        else
            spec_procedure = hashfunc->spec_read_write_partial;
    } else {
        if (mode == MEM_READ)
            spec_procedure = hashfunc->spec_read_full;
        else if (mode == MEM_WRITE)
            spec_procedure = hashfunc->spec_write_full;
        else
            spec_procedure = hashfunc->spec_read_write_full;
    }
    #if defined(GSTM_VERBOSE) && defined(GSTM_MEM_TRACE)
    dr_insert_clean_call(drcontext, bb, trigger, printAddress, false, 2, OPND_CREATE_INT32(IF_DEBUG_ELSE(rule->id,0)), opnd_create_reg(sreg0));
    #endif
    //s1 = local->loop_on
    PRE_INSERT(bb, trigger,
               INSTR_CREATE_mov_ld(drcontext,
                                   opnd_create_reg(reg_64_to_32(sreg1)),
                                   opnd_create_rel_addr((void *)&(local->flag_space.loop_on),OPSZ_4)));
    //test s1, s1
    PRE_INSERT(bb, trigger,
               INSTR_CREATE_test(drcontext,
                                 opnd_create_reg(sreg1),
                                 opnd_create_reg(sreg1)));
    //jz skip
    skipLabel = INSTR_CREATE_label(drcontext);
    PRE_INSERT(bb, trigger,
               INSTR_CREATE_jcc(drcontext, OP_jz,
                                opnd_create_instr(skipLabel)));
    //call spec procedure
    PRE_INSERT(bb,trigger,INSTR_CREATE_call(drcontext,
               opnd_create_pc(spec_procedure)));

    PRE_INSERT(bb,trigger,skipLabel);

    #if defined(GSTM_VERBOSE) && defined(GSTM_MEM_TRACE)
    dr_insert_clean_call(drcontext, bb, trigger, printAddressData, false, 2, OPND_CREATE_INT32(IF_DEBUG_ELSE(rule->id,0)), opnd_create_reg(sreg0));
    #endif

    reg_id_t srt_reg = sreg0;

    //recover xreg
    if (info.xreg) {
        if (info.xreg == sreg0) {
            srt_reg = sreg1;
            //move address to s1
            PRE_INSERT(bb, trigger,
                INSTR_CREATE_mov_ld(drcontext,
                                    opnd_create_reg(srt_reg),
                                    opnd_create_reg(sreg0)));
        }
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(info.xreg),
                                opnd_create_rel_addr(&(local->spill_space.s0) + (int)info.opPos, OPSZ_8)));
    }

    //change the original memory operand with indirect memory access [srt_reg]
    instr = reassemble_memory_instructions(drcontext, trigger, mode, info.size, srt_reg, srci, dsti);

    if (rule->reg1) {
        instr_t *instr_next = instr_get_next(trigger);
        insert_spill_scratch_regs(drcontext, bb, trigger, local, IF_DEBUG_ELSE(rule->id,0), sregMask, sindexMask, 0);
    }

    instr_next = instr_get_next(trigger);
    //replace the original memory instruction
    instrlist_replace(drcontext,trigger,instr);
    /* After the instruction, emit the signal
     * Load the write data */
    PRE_INSERT(bb, instr_next,
        INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(sreg0), OPND_CREATE_MEM64(sreg0,0)));
    /* Wait until the entry is invalid */
    instr_t *wait_for_unset = INSTR_CREATE_label(drcontext);

    PRE_INSERT(bb, instr_next, wait_for_unset);
    PRE_INSERT(bb, instr_next,
        INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(sreg1),
                            opnd_create_rel_addr(&(local->next->sync.entries[info.channel].version), OPSZ_8)));
    PRE_INSERT(bb, instr_next,
        INSTR_CREATE_test(drcontext, opnd_create_reg(sreg1), opnd_create_reg(sreg1)));
    PRE_INSERT(bb, instr_next,
        INSTR_CREATE_jcc(drcontext, OP_jnz, opnd_create_instr(wait_for_unset)));
    /* If it is unset, write to the signal */
    PRE_INSERT(bb, instr_next,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_rel_addr(&(local->next->sync.entries[info.channel].data), OPSZ_8),
                            opnd_create_reg(sreg0)));
    PRE_INSERT(bb, instr_next,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_rel_addr(&(local->next->sync.entries[info.channel].version), OPSZ_8),
                            OPND_CREATE_INT32(1)));
	dr_insert_clean_call(drcontext,bb,instr_next,print_signal,false,1,opnd_create_reg(sreg0));
}

/* A direct signal handler sends the data from
 * the corresponding write set to a specified channel */
void direct_signal_handler(JANUS_CONTEXT)
{
    uint64_t signal_address = rule->reg1;
    uint64_t channel = rule->reg0;

    /* Align the signal address */
    signal_address = signal_address - signal_address%8;

    instr_t *trigger = get_trigger_instruction(bb,rule);
    instr_t *skip = INSTR_CREATE_label(drcontext);

    dynamic_code_t *hashfunc = local->current_code_cache;
    SReg sregs = hashfunc->sregs;

    uint32_t sreg0 = sregs.s0;
    uint32_t sreg1 = sregs.s1;
    uint32_t sreg2 = sregs.s2;
    uint32_t tls_reg = sregs.s3;

    /* Check whether threads has finished */
    INSERT(bb, trigger,
        INSTR_CREATE_cmp(drcontext,
                         opnd_create_rel_addr((void *)(&shared->need_yield), OPSZ_4),
                         OPND_CREATE_INT32(1)));
    /* If need yield is on, skip the signal */
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_je, opnd_create_instr(skip)));

    /* Load signal address to sreg0 */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_imm(drcontext,
                             opnd_create_reg(sreg0),
                             OPND_CREATE_INTPTR(signal_address)));

    /* Call stm full write */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_call(drcontext,
                          opnd_create_pc(hashfunc->spec_write_full)));

    /* Now the redirected address is in sreg0, load the write data */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg1),
                            OPND_CREATE_MEM64(sreg0,0)));

    /* Send the signal */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_rel_addr(&(local->next->sync.entries[channel].data), OPSZ_8),
                            opnd_create_reg(sreg1)));
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sreg0),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_STAMP_OFFSET)));
    /* Increment version */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_inc(drcontext,
                         opnd_create_reg(sreg0)));
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_rel_addr(&(local->next->sync.entries[channel].version), OPSZ_8),
                            opnd_create_reg(sreg0)));
    PRE_INSERT(bb, trigger, skip);
#ifdef GSYNC_VERBOSE
    dr_insert_clean_call(drcontext,bb,trigger,print_signal,false,2,opnd_create_reg(sreg1),opnd_create_reg(sreg0));
#endif
}

/* A direct wait handler wait the data from
 * the specified channel and set to its own read set */
void direct_wait_handler(JANUS_CONTEXT)
{
    uint64_t signal_address = rule->reg1;
    uint64_t channel = rule->reg0;

    /* Align the signal address */
    signal_address = signal_address - signal_address%8;

    instr_t *trigger = get_trigger_instruction(bb,rule);

    dynamic_code_t *hashfunc = local->current_code_cache;
    SReg sregs = hashfunc->sregs;

    uint32_t sreg0 = sregs.s0;
    uint32_t sreg1 = sregs.s1;
    uint32_t sreg2 = sregs.s2;
    uint32_t sreg3 = sregs.s3;
    //PRE_INSERT(bb,trigger,INSTR_CREATE_int1(drcontext));
    /* Load signal address to sreg0 */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_imm(drcontext,
                             opnd_create_reg(sreg0),
                             OPND_CREATE_INTPTR(signal_address)));

    /* Push the synchronisation entry */
    PRE_INSERT(bb,trigger,
        INSTR_CREATE_mov_imm(drcontext,
                             opnd_create_reg(sreg1),
                             OPND_CREATE_INTPTR(&(local->sync.entries[rule->reg0].data))));
    PRE_INSERT(bb,trigger,
        INSTR_CREATE_push(drcontext, opnd_create_reg(sreg1)));
    /* Call Sync full write */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_call(drcontext,
                          opnd_create_pc(hashfunc->sync_read_full)));
    PRE_INSERT(bb,trigger,
        INSTR_CREATE_pop(drcontext, opnd_create_reg(sreg1)));
}

/* Prediction callbacks
 * S0 stores the original address,
 * S1 stores the entry to read set,
 * S2 stores the entry to write set
 * S3 stores the tls
 * Return value:
 * Put the predicted value in S0 */

instrlist_t *
sync_prediction(void *drcontext, instrlist_t *bb,
                instr_t *trigger, SReg sregs, JVarProfile *profile,
                bool full)
{
    reg_id_t tls_reg = sregs.s3;
    instr_t *wait, *skip, *start_wait;
    start_wait = INSTR_CREATE_label(drcontext);
    skip = INSTR_CREATE_label(drcontext);
    //INSERT(bb,trigger,INSTR_CREATE_int1(drcontext));

    INSERT(bb, trigger,
        INSTR_CREATE_push(drcontext,
                          opnd_create_reg(sregs.s1)));

    /* Firstly check the current stamp */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s1),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_STAMP_OFFSET)));

    INSERT(bb, trigger,
        INSTR_CREATE_sub(drcontext,
                         opnd_create_reg(sregs.s1),
                         opnd_create_rel_addr(&(shared->global_stamp), OPSZ_8)));

    INSERT(bb, trigger,
        INSTR_CREATE_test(drcontext,
                          opnd_create_reg(sregs.s1),
                          opnd_create_reg(sregs.s1)));
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_jnz, opnd_create_instr(start_wait)));

    /* Directly load from the memory */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s0),
                            OPND_CREATE_MEM64(sregs.s0, 0)));
    INSERT(bb, trigger,
        INSTR_CREATE_jmp(drcontext, opnd_create_instr(skip)));

    /* Load the sync entry */
    INSERT(bb, trigger, start_wait);
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s0),
                            OPND_CREATE_MEM64(DR_REG_RSP, 16)));

    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s1),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_STAMP_OFFSET)));

    wait = INSTR_CREATE_label(drcontext);

    INSERT(bb, trigger, wait);
    /* We also need to check thread yield flag here */
    INSERT(bb, trigger,
        INSTR_CREATE_cmp(drcontext,
                         opnd_create_rel_addr((void *)(&shared->need_yield), OPSZ_4),
                         OPND_CREATE_INT32(1)));
    /* If need yield is on, jump to the pool */
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_je, opnd_create_pc((app_pc)shared->current_loop->code.jump_to_pool)));

    /* Compare the local stamp with data version */
    INSERT(bb, trigger,
        INSTR_CREATE_cmp(drcontext,
                         opnd_create_reg(sregs.s1),
                         OPND_CREATE_MEM64(sregs.s0, 64)));

    /* If local stamp is greater than the data version, wait */
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_jg, opnd_create_instr(wait)));

    /* Now the data is ready
     * Load the data first */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s0),
                            OPND_CREATE_MEM64(sregs.s0, 0)));
    INSERT(bb, trigger, skip);
#ifdef GSYNC_VERBOSE
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s1),
                            OPND_CREATE_MEM64(tls_reg, LOCAL_STAMP_OFFSET)));
    INSERT(bb, trigger,
        INSTR_CREATE_push(drcontext,
                         opnd_create_reg(sregs.s2)));

    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s2),
                            OPND_CREATE_MEM64(sregs.s0, 64)));

    dr_insert_clean_call(drcontext,bb,trigger,print_wait,false,2,opnd_create_reg(sregs.s0),opnd_create_reg(sregs.s1),opnd_create_reg(sregs.s2));

    INSERT(bb, trigger,
        INSTR_CREATE_pop(drcontext,
                         opnd_create_reg(sregs.s2)));
#endif
    INSERT(bb, trigger,
        INSTR_CREATE_pop(drcontext,
                         opnd_create_reg(sregs.s1)));
    //INSERT(bb, trigger, INSTR_CREATE_int1(drcontext));
    return bb;
}

/* Try to acquire the address before read 
 * and lock the address once it is acquired */
void lock_address_handler(JANUS_CONTEXT)
{
    int i;
    instr_t *lock = INSTR_CREATE_label(drcontext);
    opnd_t mem;
    reg_id_t s0 = rule->ureg0.up;
    reg_id_t s1 = rule->ureg0.down;
    reg_id_t s2 = rule->ureg1.up;
    reg_id_t s3 = rule->ureg1.down;
    instr_t *trigger = get_trigger_instruction(bb,rule);

    for (i = 0; i < instr_num_srcs(trigger); i++) {
        if (opnd_is_memory_reference(instr_get_src(trigger, i))) {
            break;
        }
    }
    mem = instr_get_src(trigger,i);

    /* Spill s0, s1, s2 */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_rel_addr(&(local->spill_space.dump1), OPSZ_8),
                            opnd_create_reg(s0)));
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_rel_addr(&(local->spill_space.dump2), OPSZ_8),
                            opnd_create_reg(s1)));
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_rel_addr(&(local->spill_space.dump3), OPSZ_8),
                            opnd_create_reg(s2)));
    //PRE_INSERT(bb, trigger,
    //    INSTR_CREATE_mov_st(drcontext,
    //                        opnd_create_rel_addr(&(local->spill_space.dump4), OPSZ_8),
    //                        opnd_create_reg(DR_REG_RAX)));

    /* Get the key from the address */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_lea(drcontext,
                         opnd_create_reg(s0),
                         opnd_create_base_disp(opnd_get_base(mem),
                                               opnd_get_index(mem),
                                               opnd_get_scale(mem),
                                               opnd_get_disp(mem),
                                               OPSZ_lea)));

    //PRE_INSERT(bb, trigger,
    //    INSTR_CREATE_shr(drcontext, opnd_create_reg(s1), OPND_CREATE_INT32(3)));
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_and(drcontext, opnd_create_reg(s0), OPND_CREATE_INT32(0xffff)));
    //PRE_INSERT(bb, trigger,
    //    INSTR_CREATE_xor(drcontext, opnd_create_reg(DR_REG_RAX), opnd_create_reg(DR_REG_RAX)));
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_imm(drcontext, opnd_create_reg(s1), OPND_CREATE_INT32(1)));
    /* Load table address */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(s2), opnd_create_rel_addr(&(shared->lockArray), OPSZ_8)));
    /* Spin lock */
    PRE_INSERT(bb, trigger, lock);
    /* Compare and swap */
    //PRE_INSERT(bb, trigger,
    //    INSTR_CREATE_cmpxchg_8(drcontext, opnd_create_base_disp(s2, s0, 8, 0, OPSZ_8), opnd_create_reg(s1)));
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_xchg(drcontext, opnd_create_base_disp(s2, s0, 8, 0, OPSZ_8), opnd_create_reg(s1)));
    //PRE_INSERT(bb, trigger,
    //    INSTR_CREATE_cmp(drcontext, opnd_create_base_disp(s2, s0, 8, 0, OPSZ_8), opnd_create_reg(s1)));
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_test(drcontext, opnd_create_reg(s1), opnd_create_reg(s1)));
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_jnz, opnd_create_instr(lock)));
    //PRE_INSERT(bb, trigger,
    //    INSTR_CREATE_jcc(drcontext, OP_je, opnd_create_instr(lock)));
    /* Restore */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(s0),
                            opnd_create_rel_addr(&(local->spill_space.dump1), OPSZ_8)));
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(s1),
                            opnd_create_rel_addr(&(local->spill_space.dump2), OPSZ_8)));
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(s2),
                            opnd_create_rel_addr(&(local->spill_space.dump3), OPSZ_8)));
    //PRE_INSERT(bb, trigger,
    //    INSTR_CREATE_mov_ld(drcontext,
    //                        opnd_create_reg(DR_REG_RAX),
    //                        opnd_create_rel_addr(&(local->spill_space.dump4), OPSZ_8)));

    //instrlist_disassemble(drcontext, tag, bb, STDOUT);
}

void unlock_address_handler(JANUS_CONTEXT)
{
    int i;
    opnd_t mem;
    reg_id_t s0 = rule->ureg0.up;
    reg_id_t s1 = rule->ureg0.down;
    reg_id_t s2 = rule->ureg1.up;
    reg_id_t s3 = rule->ureg1.down;

    instr_t *trigger = get_trigger_instruction(bb,rule);
    instr_t *label = INSTR_CREATE_label(drcontext);

    for (i = 0; i < instr_num_dsts(trigger); i++) {
        if (opnd_is_memory_reference(instr_get_dst(trigger, i))) {
            break;
        }
    }
    mem = instr_get_dst(trigger,i);
//PRE_INSERT(bb,trigger,INSTR_CREATE_int1(drcontext));
    POST_INSERT(bb, trigger, label);
    PRE_INSERT(bb, label,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_rel_addr(&(local->spill_space.dump1), OPSZ_8),
                            opnd_create_reg(s0)));
    PRE_INSERT(bb, label,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_rel_addr(&(local->spill_space.dump2), OPSZ_8),
                            opnd_create_reg(s1)));
    PRE_INSERT(bb, label,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_rel_addr(&(local->spill_space.dump3), OPSZ_8),
                            opnd_create_reg(s2)));
    /* Get the key from the address */
    PRE_INSERT(bb, label,
        INSTR_CREATE_lea(drcontext,
                         opnd_create_reg(s0),
                         opnd_create_base_disp(opnd_get_base(mem),
                                               opnd_get_index(mem),
                                               opnd_get_scale(mem),
                                               opnd_get_disp(mem),
                                               OPSZ_lea)));
    //PRE_INSERT(bb, label,
        //INSTR_CREATE_shr(drcontext, opnd_create_reg(s0), OPND_CREATE_INT32(3)));
    PRE_INSERT(bb, label,
        INSTR_CREATE_and(drcontext, opnd_create_reg(s0), OPND_CREATE_INT32(0xffff)));
    PRE_INSERT(bb, label,
        INSTR_CREATE_xor(drcontext, opnd_create_reg(s1), opnd_create_reg(s1)));
    /* Load table address */
    PRE_INSERT(bb, label,
        INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(s2), opnd_create_rel_addr(&(shared->lockArray), OPSZ_8)));

    /* Release the lock */
    PRE_INSERT(bb, label,
        INSTR_CREATE_mov_st(drcontext, opnd_create_base_disp(s2, s0, 8, 0, OPSZ_8), opnd_create_reg(s1)));

    PRE_INSERT(bb, label,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(s0),
                            opnd_create_rel_addr(&(local->spill_space.dump1), OPSZ_8)));
    PRE_INSERT(bb, label,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(s1),
                            opnd_create_rel_addr(&(local->spill_space.dump2), OPSZ_8)));
    PRE_INSERT(bb, label,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(s2),
                            opnd_create_rel_addr(&(local->spill_space.dump3), OPSZ_8)));
}

void print_signal(uint64_t value, uint64_t stamp)
{
	dr_printf("thread %ld sends the signal of data %lx of version %lx\n", local->id, value, stamp);
}

void print_wait(uint64_t value, uint64_t stamp, uint64_t version)
{
    dr_printf("thread %ld waits and reads data %lx of version %lx for iteration %lx\n", local->id, value, version, stamp);
}
