/* JANUS Transactional Memory
 *
 * It contains a C implementation of Janus STM
 * and also handlers to interact with rewrite rules */

#include "stm.h"

/* JANUS threading library */
#include "jthread.h"

/* JANUS just-in-time hashtable library */
#include "jhash.h"

/* JANUS control library */
#include "control.h"


#define REG_IDX(reg) ((reg)-DR_REG_RAX)

///redirect all memory accesses of the basic block to thread speculative memory set
static void
basic_block_speculative_handler(void *drcontext, janus_thread_t *local, instrlist_t *bb);
///Create a read entry in the transaction buffer
static void
janus_spec_create_read_entry(jtx_t *tx, trans_t *entry, uint64_t original_addr);
///Create a write entry in the transaction buffer
static void
janus_spec_create_write_entry(jtx_t *tx, trans_t *entry, uint64_t original_addr, uint64_t data);
///Create a read&write entry in the transaction buffer
static void *
janus_spec_create_read_write_entry(jtx_t *tx, trans_t *entry, uint64_t original_addr);
///Redirect a read entry to write entry
static void *
janus_spec_redirect_read_write_entry(jtx_t *tx, trans_t *entry, uint64_t original_addr);

void janus_thread_init_jitstm(void *tls)
{
    int i;

    jtx_t *tx = &(((janus_thread_t *)tls)->tx);
    //allocate translation table and flush table
    //we also need to initialise it with zero
    tx->trans_table = (trans_t *)calloc(HASH_TABLE_SIZE, sizeof(trans_t));
    tx->flush_table = (trans_t **)malloc(HASH_TABLE_SIZE * sizeof(trans_t *));
    //allocate read and write sets
    tx->read_set = (spec_item_t *)malloc(READ_SET_SIZE * sizeof(spec_item_t));
    tx->write_set = (spec_item_t *)malloc(WRITE_SET_SIZE * sizeof(spec_item_t));
    tx->rsptr = tx->read_set;
    tx->wsptr = tx->write_set;

#ifdef JITSTM_BOUND_CHECK
    tx->read_set_end = &(tx->read_set[READ_SET_SIZE]);
    tx->write_set_end = &(tx->write_set[WRITE_SET_SIZE]);;
#endif

    tx->tls = tls;
    //set threshold
    ((janus_thread_t *)tls)->spill_space.slot5 = 0x600000;
}

/* \brief Janus speculative read 64 bit - c version */
uint64_t janus_spec_read(janus_thread_t *tls, uint64_t original_addr)
{
    int key = HASH_GET_KEY(original_addr);
#ifdef JANUS_STM_VERBOSE
    printf("read %lx ",original_addr);
    printf("key %d ", key);
#endif
    jtx_t *tx = &(tls->tx);
    trans_t *entry = tx->trans_table + key;
    while (entry->original_addr != original_addr) {
        if (entry->original_addr == 0) {
            janus_spec_create_read_entry(tx, entry, original_addr);
            break;
        } else {
            entry++;
            //wrap around
            if (entry == tx->trans_table+HASH_TABLE_SIZE)
                entry = tx->trans_table;
        }
    }
#ifdef JANUS_STM_VERBOSE
    //now entry points to the indirect table
    printf("data %lx\n", entry->redirect_addr->data);
#endif
    //put the returned data in slot4
    tls->spill_space.slot4 = (uint64_t)entry->redirect_addr->data;
    return entry->redirect_addr->data;
}

/* \brief Janus speculative read 64 bit - c version */
void janus_spec_write(janus_thread_t *tls, uint64_t original_addr, uint64_t data)
{
    int key = HASH_GET_KEY(original_addr);
#ifdef JANUS_STM_VERBOSE
    printf("write %lx ",original_addr);
    printf("key %d\n", key);
#endif

    jtx_t *tx = &(tls->tx);
    trans_t *entry = tx->trans_table + key;
    while (entry->original_addr != original_addr) {
        if (entry->original_addr == 0) {
            janus_spec_create_write_entry(tx, entry, original_addr, data);
            break;
        } else {
            entry++;
            //wrap around
            if (entry == tx->trans_table+HASH_TABLE_SIZE)
                entry = tx->trans_table;
        }
    }
#ifdef JANUS_STM_VERBOSE
    //now entry points to the indirect table
    printf("data %lx\n", entry->redirect_addr->data);
#endif
    return;
}

void *janus_spec_get_ptr(janus_thread_t *tls, uint64_t original_addr)
{
    int key = HASH_GET_KEY(original_addr);
#ifdef JANUS_STM_VERBOSE
    printf("read & write %lx ",original_addr);
    printf("key %d\n", key);
#endif
    jtx_t *tx = &(tls->tx);
    trans_t *entry = tx->trans_table + key;
    while (entry->original_addr != original_addr) {
        if (entry->original_addr == 0) {
            janus_spec_create_read_write_entry(tx, entry, original_addr);
            break;
        } else {
            entry++;
            //wrap around
            if (entry == tx->trans_table+HASH_TABLE_SIZE)
                entry = tx->trans_table;
        }
    }
#ifdef JANUS_STM_VERBOSE
    //now entry points to the indirect table
    printf("addr %lx\n", entry->redirect_addr->data);
#endif
    tls->spill_space.slot4 = (uint64_t)entry->redirect_addr;
    return entry->redirect_addr;
}


static void
janus_spec_create_read_entry(jtx_t *tx, trans_t *entry, uint64_t original_addr)
{
    entry->original_addr = original_addr;
    //first copy the read value
    spec_item_t *item = tx->rsptr;
    item->addr = original_addr;
    //fault might occur, jump to janus signal handler if fault occurs
    item->data = *(uint64_t *)original_addr;
    //assign pointer to read set
    entry->redirect_addr = item;
    entry->rw = 0;
    //increment read pointer
    tx->rsptr++;
    //register in flush table
    tx->flush_table[tx->trans_size] = entry;
    //increment transaction size
    tx->trans_size++;
#ifdef JITSTM_BOUND_CHECK
    if (tx->rsptr == tx->read_set_end) {
        printf("Janus STM read set full\n");
        exit(-1);
    }
#endif
}

static void
janus_spec_create_write_entry(jtx_t *tx, trans_t *entry, uint64_t original_addr, uint64_t data)
{
    entry->original_addr = original_addr;
    //first copy the read value
    spec_item_t *item = tx->wsptr;
    item->addr = original_addr;
    //fault might occur, jump to janus signal handler if fault occurs
    item->data = data;
    //assign pointer to read set
    entry->redirect_addr = item;
    entry->rw = 1;
    //increment write pointer
    tx->wsptr++;
    //register in flush table
    tx->flush_table[tx->trans_size] = entry;
    //increment transaction size
    tx->trans_size++;
#ifdef JITSTM_BOUND_CHECK
    if (tx->wsptr == tx->write_set_end) {
        printf("Janus STM write set full\n");
        exit(-1);
    }
#endif
}

///Create a read&write entry in the transaction buffer
static void *
janus_spec_create_read_write_entry(jtx_t *tx, trans_t *entry, uint64_t original_addr)
{
    entry->original_addr = original_addr;
    //copy the read value
    spec_item_t *ritem = tx->rsptr;
    ritem->addr = original_addr;
    ritem->data = *(uint64_t *)original_addr;
    tx->rsptr++;

    //copy the write value
    spec_item_t *witem = tx->wsptr;
    witem->addr = original_addr;
    witem->data = ritem->data;
    tx->wsptr++;
    //assign pointer to write set
    entry->redirect_addr = witem;
    entry->rw = 1;
    //register in flush table
    tx->flush_table[tx->trans_size] = entry;
    //increment transaction size
    tx->trans_size++;
#ifdef JITSTM_BOUND_CHECK
    if (tx->rsptr == tx->read_set_end) {
        printf("Janus STM read set full\n");
        exit(-1);
    }
    if (tx->wsptr == tx->write_set_end) {
        printf("Janus STM write set full\n");
        exit(-1);
    }
#endif
}

///Redirect a read entry to write entry
static void *
janus_spec_redirect_read_write_entry(jtx_t *tx, trans_t *entry, uint64_t original_addr)
{
    spec_item_t *witem = tx->wsptr;
    witem->addr = original_addr;
    witem->data = entry->redirect_addr->data;
    tx->wsptr++;
    entry->redirect_addr = witem;
    entry->rw = 1;
}

/* \brief validate the read set and commit the write set */
void *janus_transaction_commit(janus_thread_t *tls)
{
    /* Step 1: validation on the read set,
     * if failed, abort the transaction */
    spec_item_t *entry = tls->tx.read_set;
    spec_item_t *end = tls->tx.rsptr;

    while (entry != end) {
        //compare the read set against the shared memory
        if (entry->data != *(uint64_t *)entry->addr)
            janus_transaction_abort(tls);
        entry++;
    }

    /* Step 2: after all read items validated,
     * commit all writes to memory */
    entry = tls->tx.write_set;
    end = tls->tx.wsptr;

    while (entry != end) {
        //write the data to shared memory
        *(uint64_t *)entry->addr = entry->data;
        entry++;
    }
}

/* \brief abort the current transaction and perform rollback */
void *janus_transaction_abort(janus_thread_t *tls)
{
    void (*rollback)(void *);
    //flush the transaction based on the translation table
    int i;
    trans_t **flush_table = tls->tx.flush_table;
    for (i=0; i<tls->tx.trans_size; i++) {
        flush_table[i]->original_addr = 0;
    }
    //clear the read/write set
    tls->tx.rsptr = tls->tx.read_set;
    tls->tx.wsptr = tls->tx.write_set;
    tls->tx.trans_size = 0;

    //perform call to JIT code for transaction rollback
    //assign function pointer
    rollback = (void (*)(void *))tls->gen_code[shared->current_loop->dynamic_id].thread_loop_init;
    //perform rollback
    rollback(tls);
}

void master_dynamic_speculative_handlers(void *drcontext, instrlist_t *bb)
{
    janus_thread_t *tls = dr_get_tls_field(drcontext);

    if (tls->flag_space.trans_on) {
        //redirect all memory accesses to speculative memory set
        basic_block_speculative_handler(drcontext, tls, bb);
    }

    //add other pure dynamic handlers here
}

///start a transaction
void transaction_start_handler(JANUS_CONTEXT)
{
    //retrieve thread local storage
    janus_thread_t *local = dr_get_tls_field(drcontext);
    instr_t *trigger = get_trigger_instruction(bb,rule);

    //set local.trans_on
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_rel_addr((void *)(&local->flag_space.trans_on), OPSZ_4),
                            OPND_CREATE_INT32(1)));

    //save stack pointer for the transaction
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_rel_addr((void *)(&local->spill_space.slot4), OPSZ_8),
                            opnd_create_reg(DR_REG_RSP)));
}

///finish a transaction but no commits
void transaction_finish_handler(JANUS_CONTEXT)
{
    janus_thread_t *local = dr_get_tls_field(drcontext);
    instr_t *trigger = get_trigger_instruction(bb,rule);

    //unset local.trans_on
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_rel_addr((void *)(&local->flag_space.trans_on), OPSZ_4),
                            OPND_CREATE_INT32(0)));
}

///validate and commit a transaction
void transaction_commit_handler(JANUS_CONTEXT)
{
    //retrieve thread local storage
    janus_thread_t *local = dr_get_tls_field(drcontext);
    instr_t *trigger = get_trigger_instruction(bb,rule);
#ifdef JANUS_SLOW_STM
    dr_insert_clean_call(drcontext, bb, trigger,
                         janus_transaction_commit, false, 1,
                         OPND_CREATE_INTPTR(local));
#else
#ifdef NOT_YET_WORKING_FOR_ALL
    transaction_inline_commit_handler(janus_context);
#endif
#endif
}

static void
count_opnd_reg_uses(int *reg_uses, opnd_t opnd)
{
    int i;
    for (i = 0; i < opnd_num_regs_used(opnd); i++) {
        reg_id_t reg = opnd_get_reg_used(opnd, i);
        if (reg_is_gpr(reg)) {
            reg = reg_to_pointer_sized(reg);
            reg_uses[REG_IDX(reg)]++;

            if (opnd_is_memory_reference(opnd))
                reg_uses[REG_IDX(reg)]++;
        }
    }
}

static void
get_least_used_registers(void *drcontext, instrlist_t *bb, int *reg_uses, int *reg_index)
{
    int i,j;

    for (i=0; i<NUM_OF_GENERAL_REGS; i++) {
        reg_uses[i]=0;
        reg_index[i]=i+DR_REG_RAX;
    }

    //mark rsp and rbp not usable
    reg_uses[REG_IDX(DR_REG_RSP)] = 65536;
    reg_uses[REG_IDX(DR_REG_RBP)] = 65536;

    instr_t *instr;

    for (instr = instrlist_first_app(bb); instr != NULL; instr = instr_get_next_app(instr)) {
        if (instr_is_app(instr)) {
            for (i = 0; i < instr_num_dsts(instr); i++)
                count_opnd_reg_uses(reg_uses, instr_get_dst(instr, i));
            for (i = 0; i < instr_num_srcs(instr); i++)
                count_opnd_reg_uses(reg_uses, instr_get_src(instr, i));
        }
    }

    //sort the array, here we just use bubble sort for simplicity
    for (i=0; i<NUM_OF_GENERAL_REGS; i++) {
        for (j=i+1; j<NUM_OF_GENERAL_REGS; j++) {
            if (reg_uses[i] > reg_uses[j]) {
                int temp = reg_uses[j];
                reg_uses[j] = reg_uses[i];
                reg_uses[i] = temp;
                temp = reg_index[j];
                reg_index[j] = reg_index[i];
                reg_index[i] = temp;
            }
        }
    }
}

static bool
basic_block_need_speculative_transformation(void *drcontext, instrlist_t *bb)
{
    instr_t *instr;
    int count = 0;
    bool branchHasMem = false;
    for (instr = instrlist_first_app(bb); instr != NULL; instr = instr_get_next_app(instr)) {
        int is_read = instr_reads_memory(instr);
        int is_write = instr_writes_memory(instr);
        int is_local_stack = instr_reads_from_reg(instr, DR_REG_RSP, DR_QUERY_INCLUDE_ALL);
        int opcode = instr_get_opcode(instr);
        //skip compare and bulk loads/stores
        if (opcode == OP_cmp || opcode == OP_test ||
            opcode == OP_rep_stos || opcode == OP_rep_lods) continue;
        if (instr_is_app(instr) && !is_local_stack && (is_read || is_write)) {
            if (instr_is_cti(instr)) branchHasMem = true;
            count++;
        }

    }
    if (branchHasMem && count == 1) return false;
    if (count) return true;
    return false;
}

static bool
instr_need_speculative_transformation(instr_t *instr) {
    int is_read = instr_reads_memory(instr);
    int is_write = instr_writes_memory(instr);
    int is_local_stack = instr_reads_from_reg(instr, DR_REG_RSP, DR_QUERY_INCLUDE_ALL);
    int opcode = instr_get_opcode(instr);
    //currently 16-byte accesses are not handled
    //skip compare and bulk loads/stores
    if (opcode == OP_cmp || opcode == OP_test ||
        opcode == OP_rep_stos || opcode == OP_rep_lods) return false;
    if (instr_is_app(instr) && !is_local_stack && (is_read || is_write)) return true;
    return false;
}

static void
basic_block_speculative_handler(void *drcontext, janus_thread_t *local, instrlist_t *bb)
{
    instr_t *instr;
    int reg_uses[NUM_OF_GENERAL_REGS];
    int reg_index[NUM_OF_GENERAL_REGS];
    int s0,s1,s2,s3;
    ptr_uint_t aflags ,aflags_temp;
    bool aflags_on = false; 
    bool mem_after_aflag = false;

    instr_t *first = instrlist_first_app(bb);
    instr_t *last = instrlist_last_app(bb);
    instr_t *temp;
    instr_t *aflags_restore = NULL;

    /* Step 0: Check the basic block whether it requires transformation or not */
    if (!basic_block_need_speculative_transformation(drcontext, bb)) {
        return;
    }

    /* Step 1: Perform dynamic register liveness analysis
     * we can't use drreg because TLS is occupied by the Janus TLS */
    get_least_used_registers(drcontext,bb,reg_uses,reg_index);
    /* get four scratch registers */
    s0 = reg_index[0];
    s1 = reg_index[1];
    s2 = reg_index[2];
    s3 = reg_index[3];

    //dr_printf("%s %s %s %s\n",get_register_name(s0),get_register_name(s1),get_register_name(s2),get_register_name(s3));

    /* Step 2: save scratch registers to TLS */
    PRE_INSERT(bb, first,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_rel_addr((void *)(&local->spill_space.slot0), OPSZ_8),
                            opnd_create_reg(s0)));

    /* move local to s0 */
    PRE_INSERT(bb, first,
        INSTR_CREATE_mov_imm(drcontext,
                             opnd_create_reg(s0),
                             OPND_CREATE_INTPTR(local)));

    PRE_INSERT(bb, first,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(s0, LOCAL_SLOT1_OFFSET),
                            opnd_create_reg(s1)));

    PRE_INSERT(bb, first,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(s0, LOCAL_SLOT2_OFFSET),
                            opnd_create_reg(s2)));

    PRE_INSERT(bb, first,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(s0, LOCAL_SLOT3_OFFSET),
                            opnd_create_reg(s3)));

    /* Step 3: generate speculative calls */
    for (instr = first; instr != NULL; instr = instr_get_next_app(instr)) {
        if (instr_need_speculative_transformation(instr)) {
#ifdef JANUS_SLOW_STM
            speculative_memory_handler(drcontext, local, bb, instr, reg_index);
#else
            speculative_inline_memory_handler(drcontext, local, bb, instr);
#endif
        }

        aflags = instr_get_arith_flags(instr, DR_QUERY_INCLUDE_COND_SRCS);

        if (aflags & EFLAGS_WRITE_ARITH) {
            aflags_on = true;
            mem_after_aflag = false;
            //for this aflag write, check whether there is a speculative memory access before consumption
            temp = instr_get_next_app(instr);
            while (temp) {
                aflags_temp = instr_get_arith_flags(temp, DR_QUERY_INCLUDE_COND_SRCS);

                if (instr_need_speculative_transformation(temp))
                    mem_after_aflag = true;
                if (aflags_temp & EFLAGS_READ_ARITH) {
                    aflags_restore = temp;
                    break;
                }

                if (aflags_temp & EFLAGS_WRITE_ARITH)
                    break;

                temp = instr_get_next_app(temp);
            }

            if (mem_after_aflag && aflags_restore) {
                instr_t *next = instr_get_next_app(instr);
                //PRE_INSERT(bb,instr,INSTR_CREATE_int1(drcontext));
                if (s0 != DR_REG_RAX) {
                    PRE_INSERT(bb, next,
                        INSTR_CREATE_mov_st(drcontext,
                                            OPND_CREATE_MEM64(s0, LOCAL_SLOT6_OFFSET),
                                            opnd_create_reg(DR_REG_RAX)));
                    dr_save_arith_flags_to_xax(drcontext, bb, next);
                    PRE_INSERT(bb, next,
                        INSTR_CREATE_mov_st(drcontext,
                                            OPND_CREATE_MEM64(s0, LOCAL_SLOT7_OFFSET),
                                            opnd_create_reg(DR_REG_RAX)));
                    PRE_INSERT(bb, next,
                        INSTR_CREATE_mov_ld(drcontext,
                                            opnd_create_reg(DR_REG_RAX),
                                            OPND_CREATE_MEM64(s0, LOCAL_SLOT6_OFFSET)));

                    //insert restore
                    PRE_INSERT(bb, aflags_restore,
                        INSTR_CREATE_mov_st(drcontext,
                                            OPND_CREATE_MEM64(s0, LOCAL_SLOT6_OFFSET),
                                            opnd_create_reg(DR_REG_RAX)));
                    PRE_INSERT(bb, aflags_restore,
                        INSTR_CREATE_mov_ld(drcontext,
                                            opnd_create_reg(DR_REG_RAX),
                                            OPND_CREATE_MEM64(s0, LOCAL_SLOT7_OFFSET)));                    
                    dr_restore_arith_flags_from_xax(drcontext, bb, aflags_restore);
                    PRE_INSERT(bb, aflags_restore,
                        INSTR_CREATE_mov_ld(drcontext,
                                            opnd_create_reg(DR_REG_RAX),
                                            OPND_CREATE_MEM64(s0, LOCAL_SLOT6_OFFSET)));
                } else {
                    //s0 is the same as RAX
                    //move s0 to s1
                    PRE_INSERT(bb, next,
                        INSTR_CREATE_mov_st(drcontext,
                                            opnd_create_reg(s1),
                                            opnd_create_reg(s0)));
                    dr_save_arith_flags_to_xax(drcontext, bb, next);
                    PRE_INSERT(bb, next,
                        INSTR_CREATE_mov_st(drcontext,
                                            OPND_CREATE_MEM64(s1, LOCAL_SLOT7_OFFSET),
                                            opnd_create_reg(DR_REG_RAX)));
                    //move back
                    PRE_INSERT(bb, next,
                        INSTR_CREATE_mov_st(drcontext,
                                            opnd_create_reg(s0),
                                            opnd_create_reg(s1)));

                    //insert restore
                    PRE_INSERT(bb, aflags_restore,
                        INSTR_CREATE_mov_st(drcontext,
                                            opnd_create_reg(s1),
                                            opnd_create_reg(s0)));
                    PRE_INSERT(bb, aflags_restore,
                        INSTR_CREATE_mov_ld(drcontext,
                                            opnd_create_reg(DR_REG_RAX),
                                            OPND_CREATE_MEM64(s1, LOCAL_SLOT7_OFFSET)));                    
                    dr_restore_arith_flags_from_xax(drcontext, bb, aflags_restore);
                    //move back
                    PRE_INSERT(bb, next,
                        INSTR_CREATE_mov_st(drcontext,
                                            opnd_create_reg(s0),
                                            opnd_create_reg(s1)));
                }
            }
        }

        //if (reg_uses[0] || reg_uses[1] || reg_uses[2] || reg_uses[3]) PRE_INSERT(bb,instr,INSTR_CREATE_int1(drcontext));
        if (reg_uses[0] && instr_uses_reg(instr, s0)) {
            PRE_INSERT(bb, instr,
                INSTR_CREATE_mov_ld(drcontext,
                                    opnd_create_reg(s0),
                                    opnd_create_rel_addr((void *)(&local->spill_space.slot0), OPSZ_8)));
            POST_INSERT(bb, instr,
                INSTR_CREATE_mov_imm(drcontext,
                                     opnd_create_reg(s0),
                                     OPND_CREATE_INTPTR(local)));
            POST_INSERT(bb, instr,
                INSTR_CREATE_mov_st(drcontext,
                                    opnd_create_rel_addr((void *)(&local->spill_space.slot0), OPSZ_8),
                                    opnd_create_reg(s0)));
        }
        if (reg_uses[1] && instr_uses_reg(instr, s1)) {
            PRE_INSERT(bb, instr,
                INSTR_CREATE_mov_ld(drcontext,
                                    opnd_create_reg(s1),
                                    OPND_CREATE_MEM64(s0, LOCAL_SLOT1_OFFSET)));
            POST_INSERT(bb, instr,
                INSTR_CREATE_mov_st(drcontext,
                                    OPND_CREATE_MEM64(s0, LOCAL_SLOT1_OFFSET),
                                    opnd_create_reg(s1)));
        }
        if (reg_uses[2] && instr_uses_reg(instr, s2)) {
            PRE_INSERT(bb, instr,
                INSTR_CREATE_mov_ld(drcontext,
                                    opnd_create_reg(s2),
                                    OPND_CREATE_MEM64(s0, LOCAL_SLOT2_OFFSET)));
            POST_INSERT(bb, instr,
                INSTR_CREATE_mov_st(drcontext,
                                    OPND_CREATE_MEM64(s0, LOCAL_SLOT2_OFFSET),
                                    opnd_create_reg(s2)));
        }
        if (reg_uses[3] && instr_uses_reg(instr, s3)) {
            PRE_INSERT(bb, instr,
                INSTR_CREATE_mov_ld(drcontext,
                                    opnd_create_reg(s3),
                                    OPND_CREATE_MEM64(s0, LOCAL_SLOT3_OFFSET)));
            POST_INSERT(bb, instr,
                INSTR_CREATE_mov_st(drcontext,
                                    OPND_CREATE_MEM64(s0, LOCAL_SLOT3_OFFSET),
                                    opnd_create_reg(s3)));
        }
    }

    /* Restore */
    PRE_INSERT(bb, last,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(s1),
                            OPND_CREATE_MEM64(s0, LOCAL_SLOT1_OFFSET)));

    PRE_INSERT(bb, last,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(s2),
                            OPND_CREATE_MEM64(s0, LOCAL_SLOT2_OFFSET)));

    PRE_INSERT(bb, last,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(s3),
                            OPND_CREATE_MEM64(s0, LOCAL_SLOT3_OFFSET)));

    PRE_INSERT(bb, last,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(s0),
                            opnd_create_rel_addr((void *)(&local->spill_space.slot0), OPSZ_8)));
}

///redirect the memory instruction to speculative read & write buffer, clean call version
void speculative_memory_handler(void *drcontext, janus_thread_t *local, instrlist_t *bb, instr_t *instr, int *reg_index)
{
    int i, dsti, srci;
    int s0,s1,s2,s3;
    opnd_size_t size;
    instr_t *skipLabel = INSTR_CREATE_label(drcontext);

    s0 = reg_index[0];
    s1 = reg_index[1];
    s2 = reg_index[2];
    s3 = reg_index[3];

#ifdef JANUS_STM_VERBOSE
    instr_disassemble(drcontext, instr,STDOUT);
    dr_printf(" %d\n",local->id);
#endif
    int is_read = instr_reads_memory(instr);
    int is_write = instr_writes_memory(instr);
    int opcode = instr_get_opcode(instr);
    if (size > OPSZ_8) return;
    opnd_t mem;

    if (is_read) {
        for (i = 0; i < instr_num_srcs(instr); i++) {
            if (opnd_is_memory_reference(instr_get_src(instr, i))) break;
        }
        mem = instr_get_src(instr,i);
        size = opnd_get_size(mem);
        srci = i;
    }
    if (is_write) {
        for (i = 0; i < instr_num_dsts(instr); i++) {
            if (opnd_is_memory_reference(instr_get_dst(instr, i))) break;
        }
        mem = instr_get_dst(instr,i);
        size = opnd_get_size(mem);
        dsti = i;
    }

    load_effective_address(drcontext,bb,instr,mem,s1,s2);

    //skip accesses beyond dynamic threshold
    PRE_INSERT(bb, instr,
       INSTR_CREATE_cmp(drcontext,
                        opnd_create_reg(s1),
                        OPND_CREATE_MEM64(s0, LOCAL_TX_THRESHOLD_BASE)));
    PRE_INSERT(bb, instr,
       INSTR_CREATE_jcc(drcontext, OP_jg, opnd_create_instr(skipLabel)));

    if (is_read && !is_write) {
        //read only
        dr_insert_clean_call(drcontext, bb, instr,
                             janus_spec_read, false, 2,
                             opnd_create_reg(s0),
                             opnd_create_reg(s1));
    } else if (!is_read && is_write) {
        //write only
        dr_insert_clean_call(drcontext, bb, instr,
                             janus_spec_write, false, 3,
                             opnd_create_reg(s0),
                             opnd_create_reg(s1),
                             opnd_create_reg(s2));
    } else {
        //read/write, or partial write
        dr_insert_clean_call(drcontext, bb, instr,
                             janus_spec_get_ptr, false, 3,
                             opnd_create_reg(s0),
                             opnd_create_reg(s1));
    }

    PRE_INSERT(bb, instr, skipLabel);
}

///redirect the memory instruction to speculative read & write buffer, inline version
void speculative_inline_memory_handler(void *drcontext, janus_thread_t *local, instrlist_t *bb, instr_t *instr)
{

}

#ifdef NOT_YET_WORKING_FOR_ALL
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

    //replace the original memory instruction
    instrlist_replace(drcontext,trigger,instr);
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

#endif

dr_signal_action_t
stm_signal_handler(void *drcontext, dr_siginfo_t *siginfo)
{
    //XXX: currently directly deliver signal
    return DR_SIGNAL_DELIVER;
#ifdef JANUS_VERBOSE
    printf("thread %ld segfault\n",local->id);
#endif
#ifdef NOT_YET_WORKING_FOR_ALL
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
#endif
}

