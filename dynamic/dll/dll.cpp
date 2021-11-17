/* JANUS Client for secure execution */

/* Header file to implement a JANUS client */
#include "janus_api.h"
#include "dr_api.h"
#include <inttypes.h>
#include <iostream>
#include <cstring> 
#include "dll.h"
using namespace std;

int emulate_pc;
uint64_t global_count=0;
void incount(int count);
#define MAX_STR_LEN 512

static void
exit_summary(void *drcontext) {
   cout<<"Total incount: "<<dec<<global_count<<endl;
}


static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating);

static void generate_dll_events(JANUS_CONTEXT);

static void
module_summary(void *drcontext, const module_data_t *info, bool loaded){
    char  filepath[MAX_STR_LEN];
    bool rules_found = false;
//#ifdef VERBOSE
    if(loaded)  
        cout<<"module loaded"<<endl;
    dr_fprintf(STDOUT, " full_name %s \n", info->full_path);
    dr_fprintf(STDOUT, " module_name %s \n", dr_module_preferred_name(info));
    dr_fprintf(STDOUT, " entry" PFX "\n", info->entry_point);
    dr_fprintf(STDOUT, " start" PFX "\n", info->start);
//#endif    
    strcpy(filepath, info->full_path);
    strcat(filepath, ".jrs");
    FILE *file = fopen(filepath, "r");
    if(file != NULL) {
        rules_found=true;
    }
    //fclose(file);
    if(rules_found){
        load_static_rules(filepath, (PCAddress)info->start);
    }
}
DR_EXPORT void 
dr_init(client_id_t id)
{
#ifdef JANUS_VERBOSE
    dr_fprintf(STDOUT,"\n---------------------------------------------------------------\n");
    dr_fprintf(STDOUT,"               Janus DLL Execution\n");
    dr_fprintf(STDOUT,"---------------------------------------------------------------\n\n");
#endif
    /* Register event callbacks. */
    dr_register_module_load_event(module_summary);
    dr_register_bb_event(event_basic_block); 
    dr_register_thread_exit_event(exit_summary);
    /* Initialise janus components */
    janus_init(id);
    if(rsched_info.mode != JDLL) {
        dr_fprintf(STDOUT,"Static rules not intended for %s!\n",print_janus_mode(rsched_info.mode));
        return;
    }
    
#ifdef JANUS_VERBOSE
    dr_fprintf(STDOUT,"Dynamorio client initialised\n");
#endif
}


/* Main execution loop: this will be executed at every initial encounter of new basic block */
static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating)
{
    //get current basic block starting address
    PCAddress bbAddr = (PCAddress)dr_fragment_app_pc(tag);
    //lookup in the hashtable to check if there is any rule attached to the block
    RRule *rule = get_static_rule(bbAddr);
    if (rule){
        generate_dll_events(janus_context);
    }
    return DR_EMIT_DEFAULT;
}

static void
generate_dll_events(JANUS_CONTEXT)
{
    int         offset;
    int         id = 0;
    app_pc      current_pc;
    int         skip = 0;
    instr_t     *instr, *last = NULL;
    int         mode;

    /* clear emulate pc */
    emulate_pc = 0;
    /* Iterate through each original instruction in the block
     * generate dynamic inline instructions that emit commands in the command buffer */
    for (instr = instrlist_first_app(bb);
         instr != NULL;
         instr = instr_get_next_app(instr))
    {

        current_pc = instr_get_app_pc(instr);
        /* Firstly, check whether this instruction is attached to static rules */
        while (rule) {
            if ((app_pc)rule->pc == current_pc) {
                dr_insert_clean_call(drcontext, bb, instr, (void *)incount, false, 1, OPND_CREATE_INT32(rule->reg0));
            } else
                break;
            rule = rule->next;
        }
    }
}
void incount(int count){
    global_count+=count;
}
