/* JANUS Client for automatic vectorisor */

/* Header file to implement a JANUS client */
#include "janus_api.h"
/* Vector handler */
#include "vhandler.h"

static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating);

DR_EXPORT void 
dr_init(client_id_t id)
{
#ifdef JANUS_VERBOSE
    dr_fprintf(STDOUT,"\n---------------------------------------------------------------\n");
    dr_fprintf(STDOUT,"               Guided Automatic Binary Vectorisation\n");
    dr_fprintf(STDOUT,"---------------------------------------------------------------\n\n");
#endif
    /* Register event callbacks. */
    dr_register_bb_event(event_basic_block);

    /* Initialise JANUS components */
    janus_init(id);

    /* Detect the current hardware */
    myCPU = janus_detect_hardware();

    if(rsched_info.mode != JVECTOR) {
        dr_fprintf(STDOUT,"Static rules not intended for %s!\n",print_janus_mode(rsched_info.mode));
        return;
    }

    IF_VERBOSE(dr_fprintf(STDOUT,"Dynamorio client initialised\n"));
}

static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
	RuleOp rule_opcode;
    //get current basic block starting address
    PCAddress bbAddr = (PCAddress)dr_fragment_app_pc(tag);
    //lookup in the hashtable to check if there is any rule attached to the block
    RRule *rule = get_static_rule(bbAddr);
    //if it is a normal basic block, then omit it.
    if(rule == NULL || myCPU == NOT_RECOGNISED_CPU) return DR_EMIT_DEFAULT;

#ifdef NOT_YET_WORKING_FOR_ALL
    RRule *firstRule = rule;

    // Check whether VECT_FORCE_SSE exists, will only exist before loop body.
    do {
        rule_opcode = rule->opcode;
        switch (rule_opcode) {
            case VECT_FORCE_SSE:
                myCPU = SSE_SUPPORT;
                rule = NULL;
                break;
            // If not loop->init basic block force sse does not exist here.
            case VECT_CONVERT:
            case VECT_REDUCE_AFTER:
            case VECT_REVERT:
                rule = NULL;
                break;
        }
        if (rule) rule = rule->next;
    } while (rule);
    rule = firstRule; 
    // Use reg_check to ensure that the handler is called after unaligned and init but before the others.
    RRule *reg_check = NULL;
#endif
    do {
#ifdef JANUS_VERBOSE
        thread_print_rule(0, rule);
#endif
        rule_opcode = rule->opcode;
        switch (rule_opcode) {
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
            case PARA_LOOP_INIT:
                vector_loop_init(janus_context);
                break;
            case PARA_LOOP_FINISH:
                vector_loop_finish(janus_context);
                break;
            case PARA_OUTER_LOOP_INIT:
                vector_loop_init(janus_context);
                break;
            case PARA_OUTER_LOOP_END:
                vector_loop_finish(janus_context);
                break;
            #ifdef FIX
            case VECT_FORCE_SSE:
                break;
            case VECT_REDUCE_INIT:
                vector_init_handler(janus_context);
                break;
            case VECT_REG_CHECK:
                reg_check = rule;
                break;

            case VECT_INDUCTION_STRIDE_UPDATE:
                if (!rule->reg1 && reg_check) {
                    vector_reg_check_handler(drcontext,bb,reg_check,tag);
                    reg_check = NULL;
                }
                vector_loop_unroll_handler(janus_context);
                break;

            case VECT_REDUCE_AFTER:
                if (reg_check) {
                    vector_reg_check_handler(drcontext,bb,reg_check,tag);
                    reg_check = NULL;
                }
                vector_reduce_handler(janus_context);
                break;
            case VECT_REVERT:
                if (reg_check) {
                    vector_reg_check_handler(drcontext,bb,reg_check,tag);
                    reg_check = NULL;
                }
                vector_revert_handler(janus_context);
                break;
            #endif
            default:
                fprintf(stderr,"In basic block 0x%lx static rule not recognised %d\n",bbAddr,rule_opcode);
                break;
        }
        //This basic block may be annotated with more rules
        rule = rule->next;
    }while(rule);
//    if (reg_check) 
//        vector_reg_check_handler(drcontext,bb,reg_check,tag);
    // instrlist_disassemble(drcontext, (app_pc) tag, bb, STDOUT);
    // dr_printf("\n");
    return DR_EMIT_DEFAULT;
}
