/* JANUS Client for random address counter */

/* Header file to implement a JANUS client */
#include "dr_api.h"
#include <string.h>
#include <stdint.h>

uint64_t global_counter;
uint64_t address;

static void counter_thread_exit(void *drcontext);

static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating);

DR_EXPORT void 
dr_init(client_id_t id)
{
    /* Register event callbacks. */
    dr_register_bb_event(event_basic_block); 
    dr_register_thread_exit_event(counter_thread_exit);
   
    char *option_string = (char *)malloc(256*sizeof(char));
    option_string = strcpy(option_string,dr_get_options(id));

    address = strtol(strtok(option_string," @"), NULL, 16);
    dr_fprintf(STDOUT,"Listening address 0x%lx\n", address);

    global_counter = 0;
}

static void counter_thread_exit(void *drcontext)
{
    dr_fprintf(STDOUT,"PC address 0x%lx executed %ld times\n", address, global_counter);
}

static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *trigger = instrlist_first(bb);

    while (trigger!=NULL) {
        if((uint64_t)instr_get_app_pc(trigger) == address) {
            //insert counter
#ifdef JANUS_X86
            instrlist_meta_preinsert(bb, trigger,
                INSTR_CREATE_inc(drcontext,
                                 opnd_create_rel_addr((void *)&(global_counter), OPSZ_8)));
#endif
        }
        trigger = instr_get_next_app(trigger);
    }

    return DR_EMIT_DEFAULT;
}
