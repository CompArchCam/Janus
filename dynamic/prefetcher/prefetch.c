/* JANUS Client for automatic memory prefetcher */

/* Header file to implement a JANUS client */
#include "janus_api.h"

/* Janus prefetch handlers */
#include "pfhandler.h"

static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating);

DR_EXPORT void 
dr_init(client_id_t id)
{
#ifdef JANUS_VERBOSE
    dr_fprintf(STDOUT,"\n---------------------------------------------------------------\n");
    dr_fprintf(STDOUT,"               Janus Automatic Binary Memory Prefetcher\n");
    dr_fprintf(STDOUT,"---------------------------------------------------------------\n\n");
#endif
    /* Register event callbacks. */
    dr_register_bb_event(event_basic_block);    

    /* Initialise Janus components and file Janus global info */
    janus_init(id);

    if(rsched_info.mode != JFETCH) {
        dr_fprintf(STDOUT,"Rewrite rules not intended for %s!\n",print_janus_mode(rsched_info.mode));
        return;
    }

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

    //if it is a normal basic block, then omit it.
    if(rule == NULL) return DR_EMIT_DEFAULT;

    do {
#ifdef JANUS_VERBOSE
        thread_print_rule(0, rule);
#endif
        rule_opcode = rule->opcode;

        switch (rule_opcode) {
            case APP_SPLIT_BLOCK:
                split_block_handler(janus_context);
                return DR_EMIT_DEFAULT;
            case MEM_PREFETCH:
                memory_prefetch_handler(janus_context);
                break;
            case INSTR_UPDATE:
                instr_update_clone_handler(janus_context);
                break;
            case INSTR_CLONE:
                instr_clone_handler(janus_context);
                break;
            default:
                fprintf(stderr,"In basic block 0x%lx static rule not recognised %d\n",bbAddr,rule_opcode);
                break;
        }
        //This basic block may be annotated with more rules
        rule = rule->next;
    }while(rule);

    return DR_EMIT_DEFAULT;
}
