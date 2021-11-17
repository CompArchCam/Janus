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

    if(ginfo.mode != JVECTOR) {
        dr_fprintf(STDOUT,"Static rules not intended for %s!\n",print_janus_mode(ginfo.mode));
        return;
    }

    IF_VERBOSE(dr_fprintf(STDOUT,"Dynamorio client initialised\n"));
}

static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
	GHINT_OP rule_opcode;
    //get current basic block starting address
    PCAddress bbAddr = (PCAddress)dr_fragment_app_pc(tag);
    //lookup in the hashtable to check if there is any rule attached to the block
    RRule *rule = get_static_rule(bbAddr);
    //if it is a normal basic block, then omit it.
    if(rule == NULL) return DR_EMIT_DEFAULT;

    do {
        rule_opcode = rule->opcode;

        switch (rule_opcode) {
            case VECT_EXTEND:
                vector_extend_handler(janus_context);
                break;
            case VECT_UNROLL:
                vector_unroll_handler(janus_context);
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
