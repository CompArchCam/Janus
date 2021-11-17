/* JANUS Client for function cycle timer
 * Function coverage profiler */

/* Header file to implement a JANUS client */
#include "janus_api.h"
#include "dr_tools.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
using namespace std;

static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating);

static int numFunc;
static uint64_t global_counter;
static uint64_t *function_counter;
static uint64_t *call_counter;
static set<int> liveFunc;
static set<int> liveCall;
static void
timer_thread_init(void *drcontext) {
    function_counter = (uint64_t *)malloc(sizeof(uint64_t) * numFunc);
    memset(function_counter, 0, sizeof(uint64_t)*numFunc);
    call_counter = (uint64_t *)malloc(sizeof(uint64_t) * numFunc);
    memset(call_counter, 0, sizeof(uint64_t)*numFunc);
    global_counter = 0;
}

static void
timer_thread_exit(void *drcontext) {
    FILE *fp = fopen("func_cov.txt","w");
    for (int i=0; i<numFunc; i++) {
        //dr_printf("%d %f\n",i,function_counter[i]/(double)global_counter);
        fprintf(fp,"%d %f %f\n",i,function_counter[i]/(double)global_counter,call_counter[i]/(double)global_counter);
    }
    fclose(fp);
}

static void count(uint32_t cycle)
{
    for (auto funcID: liveFunc) {
        function_counter[funcID] += cycle;
    }
    for (auto funcID: liveCall) {
        call_counter[funcID] += cycle;
    }
    global_counter += cycle;
}

static void function_start(int func_id)
{
    liveFunc.insert(func_id);
}

static void function_end(int func_id)
{
    liveFunc.erase(func_id);
}

static void call_start(int func_id)
{
    liveCall.insert(func_id);
}

static void call_end(int func_id)
{
    liveCall.erase(func_id);
}


DR_EXPORT void 
dr_init(client_id_t id)
{
#ifdef janus_VERBOSE
    dr_fprintf(STDOUT,"\n---------------------------------------------------------------\n");
    dr_fprintf(STDOUT,"               JANUS Function Coverage Profiler\n");
    dr_fprintf(STDOUT,"---------------------------------------------------------------\n\n");
#endif
    /* Register event callbacks. */
    dr_register_bb_event(event_basic_block);    
    dr_register_thread_init_event(timer_thread_init);
    dr_register_thread_exit_event(timer_thread_exit);

    /* Initialise janus components */
    janus_init(id);
    numFunc = rsched_info.number_of_functions;
    dr_printf("Number of functions to be profiled %d\n",numFunc);

    if(rsched_info.mode != JFCOV) {
        dr_fprintf(STDOUT,"Static rules not intended for %s!\n",print_janus_mode(rsched_info.mode));
        return;
    }

#ifdef janus_VERBOSE
    dr_fprintf(STDOUT,"Dynamorio client initialised\n");
#endif
}

static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating)
{
    RuleOp rule_opcode;
    uint32_t bb_size = 0;
    instr_t *instr, *first = instrlist_first_app(bb);
    //get current basic block starting address
    PCAddress bbAddr = (PCAddress)dr_fragment_app_pc(tag);
    //lookup in the hashtable to check if there is any rule attached to the block
    RRule *rule = get_static_rule(bbAddr);

    for (instr = first; instr != NULL; instr = instr_get_next_app(instr)) {
        bb_size++;
    }

    dr_insert_clean_call(drcontext, bb, first,
                         (void *)count, false, 1, OPND_CREATE_INT32(bb_size));

    if(rule == NULL) return DR_EMIT_DEFAULT;

    do {

        #ifdef janus_VERBOSE
        thread_print_rule(0, rule);
        #endif

        rule_opcode = rule->opcode;

        switch (rule_opcode) {
            case CTIMER_FUNC_START:
                insert_call(function_start, 1, OPND_CREATE_INT32(rule->reg0));
                break;
            case CTIMER_FUNC_END:
                insert_call(function_end, 1, OPND_CREATE_INT32(rule->reg0));
                break;
            case CTRACE_CALL_START:
                insert_call(call_start, 1, OPND_CREATE_INT32(rule->reg0));
                break;
            case CTRACE_CALL_END:
                insert_call(call_end, 1, OPND_CREATE_INT32(rule->reg0));
                break;
            default:
                fprintf(stderr,"In basic block 0x%lx static rule not recognised %d\n",bbAddr,rule_opcode);
                break;
        }
        rule = rule->next;
    }while(rule);

    return DR_EMIT_DEFAULT;
}