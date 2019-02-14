/* JANUS Client for automatic paralleliser */

/* Header file to implement a JANUS client */
#include "janus_api.h"

/* JANUS threading library */
#include "jthread.h"

/* JANUS threads control */
#include "control.h"

#ifdef JANUS_JITSTM
/* JANUS Software Transactional Memory */
#include "stm.h"
#endif

/* JANUS runtime check handlers */
#include "rcheck.h"

/* JANUS Runtime handlers for loops */
#include "loop.h"

/* JANUS Synchronisation Facility */
//#include "sync.h"


#ifdef JANUS_VECT_SUPPORT
/* Janus vector handlers */
#include "vhandler.h"
#endif

static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating);

static void
new_janus_thread(void *drcontext);

static void
exit_janus_thread(void *drcontext);

DR_EXPORT void 
dr_init(client_id_t id)
{
#ifdef JANUS_VERBOSE
    dr_fprintf(STDOUT,"\n---------------------------------------------------------------\n");
    dr_fprintf(STDOUT,"               Janus Automatic Binary Paralleliser\n");
    dr_fprintf(STDOUT,"---------------------------------------------------------------\n\n");
#endif
    /* Register event callbacks. */
    dr_register_bb_event(event_basic_block);    
    dr_register_thread_init_event(janus_thread_spawn_handler);
    dr_register_thread_exit_event(janus_thread_exit_handler);

#ifdef JANUS_JITSTM
    dr_register_signal_event(stm_signal_handler);
#endif

    /* Initialise Janus components and file Janus global info */
    janus_init(id);

    if(rsched_info.mode != JPARALLEL) {
        dr_fprintf(STDOUT,"Rewrite rules not intended for %s!\n",print_janus_mode(rsched_info.mode));
        return;
    }

#ifdef JANUS_VECT_SUPPORT
    /* Detect the current hardware */
    myCPU = janus_detect_hardware();
#endif

    /* Initialise Janus thread system as a dynamorio client */
    if (!janus_thread_system_init()) return;

    /* Generate code cache for creating threads as application */
    create_call_func_code_cache();

    IF_VERBOSE(dr_fprintf(STDOUT,"DynamoRIO client initialised\n"));
}

/* Main execution loop: this will be executed at every initial encounter of new basic block */
static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    RuleOp rule_opcode;
    //get current basic block starting address
    PCAddress bbAddr = (PCAddress)dr_fragment_app_pc(tag);

    //lookup in the hashtable to check if there is any rule attached to the block
    RRule *rule = get_static_rule(bbAddr);

#ifdef JANUS_JITSTM
    //fully dynamic handlers without rewrite rules, only effective in speculative mode
    master_dynamic_speculative_handlers(drcontext, bb);
#endif

    //if it is a normal basic block, then omit it.
    if(rule == NULL) return DR_EMIT_DEFAULT;

    do {
#ifdef JANUS_VERBOSE
        if (rsched_info.mode == JPARALLEL) {
            janus_thread_t *tls = (janus_thread_t *)dr_get_tls_field(drcontext);
            thread_print_rule(tls->id, rule);
        } else thread_print_rule(0, rule);
#endif

        rule_opcode = rule->opcode;

        switch (rule_opcode) {
            case APP_SPLIT_BLOCK:
                split_block_handler(janus_context);
                return DR_EMIT_DEFAULT;
            case THREAD_CREATE:
                if (!janus_status) break;
                insert_function_call_as_application(janus_context,janus_create_threads);
                /* Special case: only 1 thread */
                if (rsched_info.number_of_threads == 1) {
                    janus_generate_loop_code(drcontext, oracle[0]);
                    shared->code_ready = 1;
                }
                break;
            case THREAD_EXIT:
                #ifdef SLOW_THREAD_EXIT
                    insert_call(janus_delete_threads,0);
                #endif
                break;
            case PARA_LOOP_INIT:
                loop_init_handler(janus_context);
                break;
            case PARA_LOOP_FINISH:
                loop_finish_handler(janus_context);
                break;
            case PARA_LOOP_ITER:
                loop_iteration_handler(janus_context);
                break;
            case PARA_UPDATE_LOOP_BOUND:
                loop_update_boundary_handler(janus_context);
                break;
            case PARA_OUTER_LOOP_INIT:
                loop_outer_init_handler(janus_context);
                break;
            case PARA_OUTER_LOOP_END:
                loop_outer_finish_handler(janus_context);
                break;
            case PARA_SUBCALL_START:
                loop_subcall_start_handler(janus_context);
                break;
            case PARA_SUBCALL_FINISH:
                loop_subcall_finish_handler(janus_context);
                break;
            case MEM_SPILL_REG:
                scratch_register_spill_handler(janus_context);
                break;
            case MEM_RECOVER_REG:
                scratch_register_restore_handler(janus_context);
                break;
            case MEM_MAIN_STACK:
                loop_switch_to_main_stack_handler(janus_context);
                break;
            case MEM_SCRATCH_REG:
                loop_scratch_register_handler(janus_context);
                break;
            case STATS_INSERT_COUNTER:
                #ifdef JANUS_STATS
                stats_insert_counter_handler(janus_context);
                #endif
                break;
            case MEM_BOUNDS_CHECK:
                loop_array_bound_check_handler(janus_context);
                break;
            case MEM_RECORD_BOUNDS:
                loop_array_bound_record_handler(janus_context);
                break;
            case TX_START:
                transaction_start_handler(janus_context);
                break;
            case TX_FINISH:
                transaction_finish_handler(janus_context);
                break;
#ifdef JANUS_VECT_SUPPORT
            case VECT_INDUCTION_STRIDE_UPDATE:
                vector_induction_update_handler(janus_context);
                break;
            case VECT_INDUCTION_STRIDE_RECOVER:
                vector_induction_recover_handler(janus_context);
                break;
            case VECT_LOOP_PEEL:
                vector_loop_peel_handler(janus_context);
                break;
            case VECT_CONVERT:
                vector_extend_handler(janus_context);
                break;
            case VECT_BROADCAST:
                vector_broadcast_handler(janus_context);
                break;
#endif
#ifdef NOT_YET_WORKING_FOR_ALL
            case PARA_LOOP_ITER_DOALL:
                loop_iter_handler_doall(janus_context);
                break;
            case TRANS_START:
                transaction_start_handler(janus_context);
                break;
            case TRANS_MEM:
                transaction_inline_memory_handler(janus_context);
                break;
            case TRANS_STACK:
                transaction_inline_stack_handler(janus_context);
                break;
            case TRANS_PRIVATISE:
                transaction_privatise_handler(janus_context);
                break;
            case TRANS_STACK_LEA:
                redirect_to_shared_stack(janus_context);
                break;
            case PARA_SPILL_FLAG:
                flag_spill_handler(janus_context);
                break;
            case PARA_RECOVER_FLAG:
                flag_recover_handler(janus_context);
                break;
            case PARA_FUNC_HEAD:
                function_call_header_handler(janus_context);
                break;
            case PARA_FUNC_EXIT:
                function_call_exit_handler(janus_context);
                break;
            case PARA_REG_SIGNAL:
                signal_register_handler(janus_context);
                break;
            case PARA_UPDATE_MEM_DISP:
                update_memory_disp_handler(janus_context);
                break;
            case PARA_REG_WAIT:
                wait_register_handler(janus_context);
                break;
            case PARA_MEM_SIGNAL:
                signal_memory_handler(janus_context);
                break;
            case PARA_MEM_WAIT:
                wait_memory_handler(janus_context);
                break;
            case PARA_DIRECT_SIGNAL:
                direct_signal_handler(janus_context);
                break;
            case PARA_DIRECT_WAIT:
                direct_wait_handler(janus_context);
                break;
            case PARA_LOCK_ADDR:
                lock_address_handler(janus_context);
                break;
            case PARA_UNLOCK_ADDR:
                unlock_address_handler(janus_context);
                break;
            case PARA_CALL_START_DOALL:
                function_call_start_doall_handler(janus_context);
                break;
            case PARA_CALL_END_DOALL:
                function_call_end_doall_handler(janus_context);
                break;
            case PARA_UPDATE_STACK:
                loop_induction_stack_handler(janus_context);
                break;
            case TRANS_WAIT:
                transaction_wait_handler(janus_context);
                break;
            case PARA_ALLOCA_STACK:
                allocate_stack_handler(janus_context);
                break;
            case PARA_DALLOCA_STACK:
                dallocate_stack_handler(janus_context);
                break;
            case PARA_DELETE_INSTR:
                delete_instr_handler(janus_context);
                break;
            case PARA_PRIVATISE_ADDR:
                privatise_address_handler(janus_context);
                break;
            case JANUS_DEBUG:
                //insert_call(janus_debug);
                break;
            case PARA_STAT_INC:
                stat_update_handler(janus_context);
                break;
            case PARA_TIMER_START:
                stat_timer_start_handler(janus_context);
                break;
            case PARA_TIMER_END:
                stat_timer_end_handler(janus_context);
                break;
            case PARA_USER_DEF_CODE:
                user_defined_recompilder(janus_context);
                break;
            case PARA_VALIDATE_ALIAS:
                dynamic_alias_check_handler(janus_context);
                break;
#endif
            default:
                fprintf(stderr,"In basic block 0x%lx static rule not recognised %d\n",bbAddr,rule_opcode);
                break;
        }
        //This basic block may be annotated with more rules
        rule = rule->next;
    }while(rule);

    return DR_EMIT_DEFAULT;
}
