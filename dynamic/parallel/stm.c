/* JANUS Dynamic Transactional Memory *
 * Dynamically rewrites code for speculation 
 * This TM must be built on DynamoRIO */

#include "stm.h"
#include "stm_internal.h"
#include "thread.h"
#include "util.h"
#include "hashtable.h"
#include "sync.h"
#include "loop.h"
#include <string.h>

/* ----------------------------------------- */
/* ----------- Shared flags --------------
/* -----------------------------------------*/
static volatile uint32_t tickets;

static volatile uintptr_t   global_stamp;

static pthread_mutex_t lock;

void init_stm_client(JANUS_CONTEXT)
{
    shared = &shared_region;
    memset(shared, 0, sizeof(janus_shared_t));
    shared->number_of_functions = rule->reg0;
    shared->loops = (Loop_t *)malloc(sizeof(Loop_t) * ginfo.header->numLoops);
}

void gstm_init(void (*rollback_func_ptr)())
{
    int i;

    pthread_mutex_init(&lock, NULL);
}

void gstm_init_thread(void *tls)
{
    int i;

    gtx_t *tx = &(((janus_thread_t *)tls)->tx);
    //allocate translation table and flush table
    //we also need to initialise it with zero
    tx->trans_table = (trans_t *)calloc(HASH_TABLE_SIZE, sizeof(trans_t));
    tx->flush_table = (trans_t **)malloc(HASH_TABLE_SIZE * sizeof(trans_t *));
    //allocate read and write sets
    tx->read_set = (spec_item_t *)malloc(READ_SET_SIZE * sizeof(spec_item_t));
    tx->write_set = (spec_item_t *)malloc(WRITE_SET_SIZE * sizeof(spec_item_t));
    tx->rsptr = tx->read_set;
    tx->wsptr = tx->write_set;

#ifdef GSTM_BOUND_CHECK
    tx->read_set_end = &(tx->read_set[READ_SET_SIZE]);
    tx->write_set_end = &(tx->write_set[WRITE_SET_SIZE]);;
#endif
}

/* We combine the commit and start of a transaction into one handler */
void transaction_start_handler(JANUS_CONTEXT)
{
    instr_t *trigger;

    trigger = get_trigger_instruction(bb,rule);

    /* call generated tx_start code */
    call_code_cache_handler(drcontext,bb,trigger, shared->current_loop->code.tx_start);
}

#ifdef GSTM_VERBOSE

void printhaha(uint64_t id, uint64_t reg)
{
    printf("id %ld haha %s\n",id, get_register_name(reg));
}

void print_commit(uint64_t addr, uint64_t data)
{
    printf("Commit addr %lx data %lx\n",addr,data);
}

void testflag(uint64_t id, uint64_t flag)
{
    printf("id %ld hit %lx!\n",id, flag);
}

void print_migrate()
{
    gtx_t *tx = &(local->tx);

    printf("might: trans_size %lx write set %lx, wptr %lx\n",tx->trans_size,(uint64_t)tx->write_set,(uint64_t)tx->wsptr);
}

void print_commit_success()
{
    printf("Commit successful\n");
}
#endif

/* DynamoRIO handler for modifying memory instruction into speculative instructions */
void transaction_inline_memory_handler(JANUS_CONTEXT)
{
    instr_t             *instr, *skipLabel;
    opnd_t              mem;
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

    //obtain the memory operands
    int is_read = instr_reads_memory(trigger);
    int is_write = instr_writes_memory(trigger);
    int mode;
    int srci = 0;
    int dsti = 0;
    int size = info.size;

    if(is_read && is_write) mode = MEM_READ_AND_WRITE;
    else if(is_read) mode = MEM_READ;
    else if(is_write) mode = MEM_WRITE;

    if(mode == MEM_READ) {
        for (i = 0; i < instr_num_srcs(trigger); i++) {
            if (opnd_is_memory_reference(instr_get_src(trigger, i))) {
                break;
            }
        }
        mem = instr_get_src(trigger,i);
        srci = i;
    }
    else if(mode == MEM_WRITE){
        for (i = 0; i < instr_num_dsts(trigger); i++) {
            if (opnd_is_memory_reference(instr_get_dst(trigger, i))) {
                break;
            }
        }
        mem = instr_get_dst(trigger,i);
        dsti =i;
    }
    else {
        for (i = 0; i < instr_num_srcs(trigger); i++) {
            if (opnd_is_memory_reference(instr_get_src(trigger, i))) {
                break;
            }
        }
        mem = instr_get_src(trigger,i);
        srci = i;

        for (i = 0; i < instr_num_dsts(trigger); i++) {
            if (opnd_is_memory_reference(instr_get_dst(trigger, i))) {
                break;
            }
        }
        dsti = i;
    }
    /*
    if (rule->reg1)
        insert_recover_scratch_regs(drcontext, bb, trigger,
                                    local, rule->id,
                                    sregMask, sindexMask, 0);
    */
    load_effective_address(drcontext,bb,trigger,mem,sreg0,sreg1);

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

    //IF_DEBUG(dr_insert_clean_call(drcontext, bb, trigger, printAddress, false, 2, OPND_CREATE_INT32(rule->id), opnd_create_reg(sreg0)));

    //s1 = local->loop_on
    PRE_INSERT(bb, trigger,
               INSTR_CREATE_mov_ld(drcontext,
                                   opnd_create_reg(reg_64_to_32(sreg1)),
                                   opnd_create_rel_addr((void *)&(local->flag_space.loop_on), OPSZ_4)));
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

    //IF_DEBUG(dr_insert_clean_call(drcontext, bb, trigger, printAddressData, false, 2, OPND_CREATE_INT32(rule->id), opnd_create_reg(sreg0)));

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

    /*
    if (rule->reg1) {
        instr_t *instr_next = instr_get_next(trigger);
        insert_spill_scratch_regs(drcontext,bb,trigger,local,rule->id,sregMask,sindexMask, 0);
    }
    */
    //replace the original memory instruction
    //instrlist_replace(drcontext,trigger,instr);
}

/* DynamoRIO handler for mangling direct stack instructions into speculative instructions */
void transaction_inline_stack_handler(JANUS_CONTEXT)
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
    //#if defined(GSTM_VERBOSE) && defined(GSTM_MEM_TRACE)
    //dr_insert_clean_call(drcontext, bb, trigger, printAddress, false, 2, OPND_CREATE_INT32(rule->id), opnd_create_reg(sreg0));
    //#endif
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

    //#if defined(GSTM_VERBOSE) && defined(GSTM_MEM_TRACE)
    //dr_insert_clean_call(drcontext, bb, trigger, printAddressData, false, 2, OPND_CREATE_INT32(rule->id), opnd_create_reg(sreg0));
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

void printArguments(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t rbp, uint64_t rsp)
{
    printf("arg0 0x%lx arg1 0x%lx arg2 0x%lx rbp 0x%lx rsp 0x%lx\n",arg0,arg1,arg2,rbp,rsp);
}

//Whenever a function call is encountered, generate code for this function call
void function_call_start_handler(JANUS_CONTEXT)
{
    instr_t *trigger = get_trigger_instruction(bb, rule);

    /* Free the old slot, since the spill slot is used for other registers */
    SReg sregs = local->current_code_cache->sregs;
    reg_id_t tls_reg = sregs.s3;

    /* For shared library call, call transaction wait */
    if (rule->reg0) {
        call_code_cache_handler(drcontext, bb, trigger, local->current_code_cache->tx_wait);
    }

    /* Set call_on = true */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM32(tls_reg, CALL_ON_FLAG_OFFSET),
                            OPND_CREATE_INT32(1)));
    /* Load spilled registers */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s0),
                            OPND_CREATE_MEM64(tls_reg, 0)));
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s1),
                            OPND_CREATE_MEM64(tls_reg, 8)));
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s2),
                            OPND_CREATE_MEM64(tls_reg, 16)));
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s3),
                            OPND_CREATE_MEM64(tls_reg, 24)));

    PRINTSTAGE(CALL_START_STAGE);
    #ifdef GSTM_VERBOSE
    dr_insert_clean_call(drcontext, bb, trigger,
                         print_recover_id, false, 5, OPND_CREATE_INT32(rule->reg0),
                         OPND_CREATE_INT32(sregs.s0),
                         OPND_CREATE_INT32(sregs.s1),
                         OPND_CREATE_INT32(sregs.s2),
                         OPND_CREATE_INT32(sregs.s3)
                         );
    #endif
}


void function_call_end_handler(JANUS_CONTEXT)
{
    instr_t *trigger = get_trigger_instruction(bb, rule);
    instr_t *skipLabel;

    uint32_t pid = rule->reg0;
    if(pid >= shared->number_of_functions) return;

    if(pid == 0)
        local->current_code_cache = &(shared->current_loop->func);
    else {
        local->current_code_cache = shared->functions[pid];
    }

    SReg sregs = local->current_code_cache->sregs;
    reg_id_t tls_reg = sregs.s3;

    /* Check local->call_on */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_cmp(drcontext,
                         opnd_create_rel_addr((void *)&(local->flag_space.call_on), OPSZ_4),
                         OPND_CREATE_INT32(0)));

    skipLabel = INSTR_CREATE_label(drcontext);
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_je,
                         opnd_create_instr(skipLabel)));
    /* If local->call_on is true, spill scratch registers */
    /* Spill TLS register */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_rel_addr((void *)&(local->spill_space.stolen), OPSZ_8),
                            opnd_create_reg(tls_reg)));
    /* Load tls */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_imm(drcontext,
                             opnd_create_reg(tls_reg),
                             OPND_CREATE_INT64(local)));

    /* Spill the rest three scratch registers */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(tls_reg, 0),
                            opnd_create_reg(sregs.s0)));
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(tls_reg, 8),
                            opnd_create_reg(sregs.s1)));
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(tls_reg, 16),
                            opnd_create_reg(sregs.s2)));
    /* Unset the local->call_on flag */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM32(tls_reg, CALL_ON_FLAG_OFFSET),
                            OPND_CREATE_INT32(0)));
    PRINTSTAGE(CALL_END_STAGE);
    #ifdef GSTM_VERBOSE
    dr_insert_clean_call(drcontext, bb, trigger,
                         print_spill_id, false, 5, OPND_CREATE_INT32(rule->reg0),
                         OPND_CREATE_INT32(sregs.s0),
                         OPND_CREATE_INT32(sregs.s1),
                         OPND_CREATE_INT32(sregs.s2),
                         OPND_CREATE_INT32(sregs.s3)
                         );
    #endif
    /* Update the current hash-table at runtime */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_imm(drcontext,
                            opnd_create_reg(sregs.s0),
                            OPND_CREATE_INTPTR(local->current_code_cache)));
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(tls_reg, LOCAL_HASH_TABLE_OFFSET),
                            opnd_create_reg(sregs.s0)));
    /* Skip label is inserted here */
    PRE_INSERT(bb, trigger, skipLabel);
}

void function_call_header_handler(JANUS_CONTEXT)
{
    instr_t *trigger = get_trigger_instruction(bb, rule);

    uint32_t pid = rule->reg0;
    if(pid >= shared->number_of_functions) return;

    uint32_t regMask = rule->reg1;
    if (!regMask) return;

    SReg sregs;
    sregs.s0 = pop_register(&regMask);
    sregs.s1 = pop_register(&regMask);
    sregs.s2 = pop_register(&regMask);
    sregs.s3 = pop_register(&regMask);
    reg_id_t tls_reg = sregs.s3;

    /* Creating hashtable code should be critical */
    janus_lock();
    if (!(shared->functions[pid])) {
        dynamic_code_t *hash_codes = (dynamic_code_t *)malloc(sizeof(dynamic_code_t));
        hash_codes->sregs = sregs;
        generate_dynamic_function(drcontext, hash_codes, LOOP_ALL_TYPE);

        shared->functions[pid] = hash_codes;
#ifdef GSTM_VERBOSE
        dr_printf("Hashtable code generated for function %d\n",pid);
#endif
    }
    /* Get the current hashtable code */
    local->current_code_cache = shared->functions[pid];
    janus_unlock();

    /* Spill TLS register */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_rel_addr((void *)&(local->spill_space.stolen), OPSZ_8),
                            opnd_create_reg(tls_reg)));
    /* Load tls */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_imm(drcontext,
                             opnd_create_reg(tls_reg),
                             OPND_CREATE_INT64(local)));
    /* Spill the rest three scratch registers */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(tls_reg, 0),
                            opnd_create_reg(sregs.s0)));
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(tls_reg, 8),
                            opnd_create_reg(sregs.s1)));
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(tls_reg, 16),
                            opnd_create_reg(sregs.s2)));

    /* Unset the local->call_on flag */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM32(tls_reg, CALL_ON_FLAG_OFFSET),
                            OPND_CREATE_INT32(0)));

    /* Update the current hash-table at runtime */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_imm(drcontext,
                            opnd_create_reg(sregs.s0),
                            OPND_CREATE_INTPTR(local->current_code_cache)));
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(tls_reg, LOCAL_HASH_TABLE_OFFSET),
                            opnd_create_reg(sregs.s0)));

    PRINTSTAGE(CALL_HEAD_STAGE);

    #ifdef GSTM_VERBOSE
    dr_insert_clean_call(drcontext, bb, trigger,
                         print_spill_id, false, 5, OPND_CREATE_INT32(rule->reg0),
                         OPND_CREATE_INT32(sregs.s0),
                         OPND_CREATE_INT32(sregs.s1),
                         OPND_CREATE_INT32(sregs.s2),
                         OPND_CREATE_INT32(sregs.s3)
                         );
    #endif
}

void function_call_exit_handler(JANUS_CONTEXT)
{
    instr_t *trigger = get_trigger_instruction(bb,rule);

    uint32_t pid = rule->reg0;
    if(pid >= shared->number_of_functions) return;

    uint32_t regMask = rule->reg1;
    if (!regMask) return;

    SReg sregs;
    sregs.s0 = pop_register(&regMask);
    sregs.s1 = pop_register(&regMask);
    sregs.s2 = pop_register(&regMask);
    sregs.s3 = pop_register(&regMask);

    reg_id_t tls_reg = sregs.s3;

    /* Creating hashtable code should be critical */
    janus_lock();
    if (!(shared->functions[pid])) {
        dynamic_code_t *hash_codes = (dynamic_code_t *)malloc(sizeof(dynamic_code_t));
        hash_codes->sregs = sregs;
        generate_dynamic_function(drcontext, hash_codes, LOOP_ALL_TYPE);

        shared->functions[pid] = hash_codes;
    }
    /* Get the current hashtable code */
    local->current_code_cache = shared->functions[pid];
    janus_unlock();

    /* Set call_on = true */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM32(tls_reg, CALL_ON_FLAG_OFFSET),
                            OPND_CREATE_INT32(1)));
    /* Load spilled registers */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s0),
                            OPND_CREATE_MEM64(tls_reg, 0)));
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s1),
                            OPND_CREATE_MEM64(tls_reg, 8)));
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s2),
                            OPND_CREATE_MEM64(tls_reg, 16)));
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(sregs.s3),
                            OPND_CREATE_MEM64(tls_reg, 24)));
    PRINTSTAGE(CALL_EXIT_STAGE);
    #ifdef GSTM_VERBOSE
    dr_insert_clean_call(drcontext, bb, trigger,
                         print_recover_id, false, 5, OPND_CREATE_INT32(rule->reg0),
                         OPND_CREATE_INT32(sregs.s0),
                         OPND_CREATE_INT32(sregs.s1),
                         OPND_CREATE_INT32(sregs.s2),
                         OPND_CREATE_INT32(sregs.s3)
                         );
    #endif
}

/* Change the register operand to memory */
void register_spill_handler(JANUS_CONTEXT)
{
    instr_t *trigger = get_trigger_instruction(bb,rule);

    uint32_t regMask = (uint32_t)(rule->ureg0.down);
    uint32_t indexMask = (uint32_t)(rule->ureg0.up);
    uint32_t reg,index;

    insert_spill_scratch_regs(drcontext, bb, trigger, local, IF_DEBUG_ELSE(rule->id,0), regMask, indexMask, rule->reg1);
}

/* Change the register operand to memory */
void register_recover_handler(JANUS_CONTEXT)
{
    instr_t *trigger = get_trigger_instruction(bb,rule);

    uint32_t regMask = (uint32_t)(rule->ureg0.down);
    uint32_t indexMask = (uint32_t)(rule->ureg0.up);
    uint32_t reg,index;

    insert_recover_scratch_regs(drcontext, bb, trigger, local, IF_DEBUG_ELSE(rule->id,0), regMask, indexMask, rule->reg1);
}

void flag_spill_handler(JANUS_CONTEXT)
{
    instr_t *trigger = instr_get_next(get_trigger_instruction(bb,rule));

    dr_save_reg(drcontext, bb, trigger, DR_REG_RAX, SPILL_SLOT_2);
    /* load eflags */
    dr_save_arith_flags_to_xax(drcontext, bb, trigger);
    /* Save flags to private context */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_rel_addr(&(local->spill_space.control_flag),OPSZ_8),
                            opnd_create_reg(DR_REG_RAX)));
    dr_restore_reg(drcontext, bb, trigger, DR_REG_RAX, SPILL_SLOT_2);
}

void flag_recover_handler(JANUS_CONTEXT)
{
    instr_t *trigger = get_trigger_instruction(bb,rule);

    dr_save_reg(drcontext, bb, trigger, DR_REG_RAX, SPILL_SLOT_2);

    /* Restore flags from private context */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(DR_REG_RAX),
                            opnd_create_rel_addr(&(local->spill_space.control_flag),OPSZ_8)));
    /* write to eflags */
    dr_restore_arith_flags_from_xax(drcontext, bb, trigger);
    dr_restore_reg(drcontext, bb, trigger, DR_REG_RAX, SPILL_SLOT_2);
}

void redirect_to_shared_stack(JANUS_CONTEXT)
{
    //thread local storage pinter
    instr_t             *instr;
    opnd_t              mem_check;
    int                 dynamic_check_range = 0;
    uint32_t              i;

    //get meta information of the instruciton
    GMemInfo info = *(GMemInfo *)(&(rule->reg0));
    instr_t *trigger = get_trigger_instruction(bb,rule);
    SReg sregs = local->current_code_cache->sregs;

    int mode = get_instr_mode(trigger);
    int size = info.size;
    int srci = -1;
    int dsti = -1;
    //obtain the memory operands
    opnd_t mem = get_memory_operand(trigger, mode, &srci, &dsti);

    reg_id_t index = opnd_get_index(mem);
    reg_id_t scratch = sregs.s0;

    if (opnd_uses_reg(mem, DR_REG_XSP)) {
        if (index == sregs.s0)
            scratch = sregs.s1;
        //load the main shared stack into scratch
        PRE_INSERT(bb, trigger,
                   INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(scratch),
                                       opnd_create_rel_addr(&(shared->stack_ptr),OPSZ_8)));
        //create memory operand by replacing the thread-private to shared stack pointer
        mem = opnd_create_base_disp(scratch, opnd_get_index(mem),
                                    opnd_get_scale(mem), opnd_get_disp(mem), OPSZ_lea);

        instr = INSTR_CREATE_lea(drcontext, instr_get_dst(trigger, 0), mem);
        PRE_INSERT(bb, trigger, instr);

        instrlist_remove(drcontext,trigger);
    }
}

void
transaction_wait_handler(JANUS_CONTEXT)
{
    instr_t *trigger = get_trigger_instruction(bb,rule);
    instr_t *waitLabel;

    call_code_cache_handler(drcontext, bb, trigger, local->current_code_cache->tx_wait);
}

void
transaction_privatise_handler(JANUS_CONTEXT)
{
    instr_t *trigger = get_trigger_instruction(bb, rule);
    opnd_t op, new_op;
    int i, srci=-1, dsti=-1;
    int index = rule->ureg0.up;
    int mode = rule->ureg0.down;
    int loop_id = rule->reg1;
    Loop_t *loop = &(shared->loops[loop_id]);
    SReg sregs = loop->func.sregs;

#ifdef JANUS_LOOP_VERBOSE
    instr_disassemble(drcontext, trigger,STDOUT);
    dr_printf("\n");
#endif

    if (instr_reads_memory(trigger)) {
        for (i = 0; i < instr_num_srcs(trigger); i++) {
            op = instr_get_src(trigger, i);
            if (opnd_is_memory_reference(op)) {
                srci = i;
                break;
            }
        }
    }

    if (instr_writes_memory(trigger)) {
        for (i = 0; i < instr_num_dsts(trigger); i++) {
            op = instr_get_dst(trigger, i);
            if (opnd_is_memory_reference(op)) {
                dsti = i;
                break;
            }
        }
    }

    /* If mode is 1, record the address */
    if (mode == 1) {
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_lea(drcontext, opnd_create_reg(sregs.s0),
                             opnd_create_base_disp(opnd_get_base(op), opnd_get_index(op),
                                                   opnd_get_scale(op), opnd_get_disp(op),
                                                   OPSZ_lea)));

        PRE_INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                opnd_create_rel_addr(&(local->tx.write_set[index].addr), OPSZ_8),
                                opnd_create_reg(sregs.s0)));
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_add(drcontext,
                             OPND_CREATE_MEM64(sregs.s3, LOCAL_WRITE_PTR_OFFSET),
                             OPND_CREATE_INT32(sizeof(spec_item_t))));
    }

    if (srci != -1 || dsti != -1) {
        /* create a new operand */
        new_op = opnd_create_rel_addr((void *)&(local->tx.write_set[index].data), OPSZ_8);
        if (srci != -1)
            instr_set_src(trigger, srci, new_op);
        if (dsti != -1)
            instr_set_dst(trigger, dsti, new_op);
    } else {
        dr_printf("Absolute address not found %lx\n",rule->reg1);
    }
}

dr_signal_action_t
stm_signal_handler(void *drcontext, dr_siginfo_t *siginfo)
{
    //XXX: currently directly deliver signal
    return DR_SIGNAL_DELIVER;
#ifdef JANUS_VERBOSE
    printf("thread %ld segfault\n",local->id);
#endif
    int i;

    if (!local->flag_space.loop_on) return DR_SIGNAL_DELIVER;
    /* In the signal handler, wait until it is the oldest */
    while (!(local->flag_space.can_commit)) {
        /* If it finds the need yield flag is on, jump back to the pool */
        if (shared->need_yield) {
YIELD_IN_SIGNAL:
#ifdef JANUS_VERBOSE
            printf("thread %ld yield in the signal handler\n",local->id);
#endif
            local->flag_space.can_commit = 0;
            local->flag_space.trans_on = 0;
            local->flag_space.rolledback = 0;
            //we need to clear the transaction before jump to pool
            tx_clear_c(&(local->tx));
            //clear the sync structure
            sync_clear_c(&(local->sync));
            /* Copy pool state */
            if (local->id) {
#ifdef JANUS_VERBOSE
                dr_printf("Thread %ld jumps back to pool from signal handler\n",local->id);
#endif
                //longjmp(local->pool_state, 1);
                siginfo->mcontext->pc = (void *)shared->current_loop->app_loop_end_addr;
            } else {
                //wait all threads jumps to pool
                for (i = 1; i<ginfo.number_of_threads; i++) {
                    while(!(oracle[i]->flag_space.in_pool));
                }
#ifdef JANUS_VERBOSE
                dr_printf("main thread will resume execution\n");
#endif
                siginfo->mcontext->rdi = shared->registers.rdi;
                siginfo->mcontext->rsi = shared->registers.rsi;
                siginfo->mcontext->rbp = shared->registers.rbp;
                siginfo->mcontext->rsp = shared->stack_ptr;
                siginfo->mcontext->rbx = shared->registers.rbx;
                siginfo->mcontext->rdx = shared->registers.rdx;
                siginfo->mcontext->rcx = shared->registers.rcx;
                siginfo->mcontext->rax = shared->registers.rax;
                siginfo->mcontext->r8 = shared->registers.r8;
                siginfo->mcontext->r9 = shared->registers.r9;
                siginfo->mcontext->r10 = shared->registers.r10;
                siginfo->mcontext->r11 = shared->registers.r11;
                siginfo->mcontext->r12 = shared->registers.r12;
                siginfo->mcontext->r13 = shared->registers.r13;
                siginfo->mcontext->r14 = shared->registers.r14;
                siginfo->mcontext->r15 = shared->registers.r15;
                siginfo->mcontext->pc = (void *)shared->current_loop->app_loop_end_addr;
            }
            return DR_SIGNAL_REDIRECT;
        }
    }

    if (shared->need_yield) goto YIELD_IN_SIGNAL;
    /* If the thread jumps out the loop, then it is the oldest */
    /* For the moment we just rollback, since we assume the application won't
       trigger the seg-fault
       XXX: put validation code and if it is correct, deliver the signal to the app */
    if (local->flag_space.rolledback) {
        local->flag_space.rolledback = 0;
        return DR_SIGNAL_DELIVER;
    }
#ifdef JANUS_VERBOSE
    printf("thread %ld rollback in the signal handler\n",local->id);
#endif
    if (ginfo.number_of_threads == 1)
        return DR_SIGNAL_DELIVER;

    tx_clear_c(&(local->tx));
    local->flag_space.trans_on = 0;
    local->flag_space.rolledback = 1;
    /* Copy all the machine states */
    siginfo->mcontext->rdi = local->tx.check_point.state.rdi;
    siginfo->mcontext->rsi = local->tx.check_point.state.rsi;
    siginfo->mcontext->rbp = local->tx.check_point.state.rbp;
    siginfo->mcontext->rsp = local->tx.check_point.state.rsp;
    siginfo->mcontext->rbx = local->tx.check_point.state.rbx;
    siginfo->mcontext->rdx = local->tx.check_point.state.rdx;
    siginfo->mcontext->rcx = local->tx.check_point.state.rcx;
    siginfo->mcontext->rax = local->tx.check_point.state.rax;
    siginfo->mcontext->r8 = local->tx.check_point.state.r8;
    siginfo->mcontext->r9 = local->tx.check_point.state.r9;
    siginfo->mcontext->r10 = local->tx.check_point.state.r10;
    siginfo->mcontext->r11 = local->tx.check_point.state.r11;
    siginfo->mcontext->r12 = local->tx.check_point.state.r12;
    siginfo->mcontext->r13 = local->tx.check_point.state.r13;
    siginfo->mcontext->r14 = local->tx.check_point.state.r14;
    siginfo->mcontext->r15 = local->tx.check_point.state.r15;
    //siginfo->mcontext->pc = (void *)local->tx.check_point.state.pc;
    siginfo->mcontext->pc = (void *)shared->current_loop->loop_start_addr;
    return DR_SIGNAL_REDIRECT;
}

static void glock()
{
    pthread_mutex_lock(&lock);
}

static void gunlock()
{
    pthread_mutex_unlock(&lock);
}
