/* Janus Client for Parallelism Planner
 * The binary emulator for generating plan events */

#include <string.h>
#include <stdlib.h>
#include <assert.h>
/* Planner header */
#include "plan.h"
#define REG_S1 DR_REG_RSI
#define REG_S2 DR_REG_RDI

#define N_THRESHOLD 10
#define I_THRESHOLD 100

plan_t command_buffer[MAX_ALLOWED_COMMAND];
plan_t *commands;
const char *app_name;
int emulate_pc;
int instrumentation_switch;
uint64_t total_program_cycles = 0;
uint64_t total_loop_cycles = 0;
plan_profmode_t profmode;
bool external_call_on = false; /*flag to check if shared lib profiling is enabled*/
int iThreshold;		       /* Cut threshold for number of iterations for DOALL_LIGHT mode*/
int nThreshold;		       /* Cut threshold for number of invocations for LIGHT mode*/

static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating);
static void emulator_init();
static void emulator_exit();

static void generate_planner_events(JANUS_CONTEXT, app_pc addr);
static void generate_events_by_rule(JANUS_CONTEXT, instr_t *instr);
static void generate_memory_events(JANUS_CONTEXT, instr_t *instr);
static void generate_shared_lib_events(JANUS_CONTEXT, app_pc addr);

DR_EXPORT void 
dr_init(client_id_t id)
{
    /* Register event callbacks. */
    dr_register_bb_event(event_basic_block);
    dr_register_thread_exit_event(emulator_exit);

    /*Initialise symbol library*/
    if (drsym_init(0) != DRSYM_SUCCESS) {
            printf("WARNING: unable to initialize symbol translation\n");
    }

    /* Initialise GABP components */
    janus_init(id);
    app_name = dr_get_application_name();

    /*Get the profiling from PROF_MODE environment variable (otherwise set to default mode)*/
    char * pMode = getenv("PROF_MODE");
    if (pMode!=NULL){
	if(strcmp(pMode,"DOALL") ==0)
	    profmode=DOALL;
	else if(strcmp(pMode,"DOALL_LIGHT") == 0)
	    profmode = DOALL_LIGHT;
	else if(strcmp(pMode,"FULL") == 0)
	    profmode = FULL;
	else if(strcmp(pMode,"LIGHT")==0)
	    profmode= LIGHT;
	else{ 
	    printf("Invalid profile mode specified. PROF_MODE can be DOALL,DOALL_LIGHT, LIGHT and FULL\n");
	    exit(0);
	}
        printf ("The profiling  mode is: %s\n",pMode);
    }
    else{
        printf ("No profiling mode specified. Default mode is DOALL_LIGHT\n");
	profmode=DOALL_LIGHT;
    }

    /* Get the values of iThreshold and nThreshold from environment variable (otherwise set to default)*/
    iThreshold = getenv("ITHRESHOLD")!=NULL ? atoi(getenv("ITHRESHOLD")) : I_THRESHOLD;
    nThreshold = getenv("NTHRESHOLD")!=NULL ? atoi(getenv("NTHRESHOLD")) : N_THRESHOLD;

    if(rsched_info.mode != JPROF) {
        dr_fprintf(STDOUT,"Static rules not intended for %s!\n",print_janus_mode(rsched_info.mode));
        return;
    }

    emulator_init();
}

static void
emulator_init()
{
    commands = command_buffer;
    emulate_pc = 0;
    instrumentation_switch = 0;

    /* start plan */
    planner_start(rsched_info.channel ,rsched_info.number_of_threads);
}

static void
emulator_exit()
{
    planner_stop();
    if (drsym_exit() != DRSYM_SUCCESS) {
	printf("WARNING: error cleaning up symbol library\n");
    }
}

static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating)
{
    instr_t *instr;
    /* get current basic block starting address */
    app_pc bbAddr = dr_fragment_app_pc(tag);
    /* get the associative static rules */
    RRule *rule = get_static_rule((PCAddress)bbAddr);

    if (rule)
        generate_planner_events(janus_context, bbAddr);
    else if(external_call_on){
    	generate_shared_lib_events(janus_context, bbAddr);
     }
    return DR_EMIT_DEFAULT;
}

static void
generate_planner_events(JANUS_CONTEXT, app_pc addr)
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
#ifdef PLANNER_VERBOSE
                thread_print_rule(0, rule);
#endif
                generate_events_by_rule(janus_context, instr);
            } else
                break;
            rule = rule->next;
        }
	if(external_call_on){ /* if it is a memory access within shared lib, no rules attached so generate memory events explicitly*/
           int instr_opcode  = instr_get_opcode(instr);
           if(instr_reads_memory(instr) || instr_writes_memory(instr) ){
	      generate_memory_events(janus_context, instr);
	   }
	}
        last = instr;
    }

    /* Generate a clean call at the end of block to execute from the command buffer */
    dr_insert_clean_call(drcontext, bb, last, planner_execute, false, 1, OPND_CREATE_INT32(emulate_pc));
}
static void
generate_shared_lib_events(JANUS_CONTEXT, app_pc addr)
{
    int         offset;
    int         id = 0;
    app_pc      current_pc;
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
        int instr_opcode  = instr_get_opcode(instr);
        if(instr_reads_memory(instr) || instr_writes_memory(instr) ){
	   generate_memory_events(janus_context, instr);
        }
	last =instr;
    }
    /* Generate a clean call at the end of block to execute from the command buffer */
    dr_insert_clean_call(drcontext, bb, last, planner_execute, false, 1, OPND_CREATE_INT32(emulate_pc));
}

//return 1 to skip, return 0 to not skip
static void
generate_events_by_rule(JANUS_CONTEXT, instr_t *instr)
{
    RuleOp rule_opcode = rule->opcode;

    switch (rule_opcode) {
        case APP_SPLIT_BLOCK:
            split_block_handler(janus_context);
            return;
        case PROF_LOOP_START:
            instrumentation_switch = 1;
            /* Emit opcode to the command buffer */
            PRE_INSERT(bb, instr,
                INSTR_CREATE_mov_st(drcontext,
                                    opnd_create_rel_addr(&(commands[emulate_pc].opcode), OPSZ_2),
                                    OPND_CREATE_INT16(CM_LOOP_START)));
            emulate_pc++;
        break;
        case PROF_LOOP_ITER:
            PRE_INSERT(bb, instr,
                INSTR_CREATE_mov_st(drcontext,
                                    opnd_create_rel_addr(&(commands[emulate_pc].opcode), OPSZ_2),
                                    OPND_CREATE_INT16(CM_LOOP_ITER)));
            emulate_pc++;
        break;
        case PROF_LOOP_FINISH:
            instrumentation_switch = 0;
            PRE_INSERT(bb, instr,
                INSTR_CREATE_mov_st(drcontext,
                                    opnd_create_rel_addr(&(commands[emulate_pc].opcode), OPSZ_2),
                                    OPND_CREATE_INT16(CM_LOOP_FINISH)));
            emulate_pc++;
        break;
        case PROF_MEM_ACCESS:
            generate_memory_events(janus_context, instr);
        break;
	case PROF_CALL_START:
	     external_call_on = true;
	     /*PRE_INSERT(bb, instr,
                INSTR_CREATE_mov_st(drcontext,
                                    opnd_create_rel_addr(&(commands[emulate_pc].opcode), OPSZ_2),
                                    OPND_CREATE_INT16(CM_CALL_START)));
            emulate_pc++;*/
	break;
	case PROF_CALL_FINISH:
	    external_call_on = false;
            /* PRE_INSERT(bb, instr,
                INSTR_CREATE_mov_st(drcontext,
                                    opnd_create_rel_addr(&(commands[emulate_pc].opcode), OPSZ_2),
                                    OPND_CREATE_INT16(CM_CALL_FINISH)));
            emulate_pc++;*/
	break;
        default:
        break;
    }
}

static void generate_memory_events(JANUS_CONTEXT, instr_t *instr)
{
    int         opcode, num_dsts, num_srcs, i, j;
    uint64_t    dsti, dst2, srci1, srci2, srci3;
    opnd_t      dst,src,mem;
    int         mode = 0;
    int         size = 0;

    if(instr_reads_memory(instr)) mode += 1;
    if(instr_writes_memory(instr)) mode += 2;
    if (mode == 0) return;
    /* get the number of destinations */
    num_dsts = instr_num_dsts(instr);
    num_srcs = instr_num_srcs(instr);

    dr_save_reg(drcontext, bb, instr, REG_S1, SPILL_SLOT_2);
    dr_save_reg(drcontext, bb, instr, REG_S2, SPILL_SLOT_3);

    /* Instrument memory access */
    for(i=0; i<num_srcs; i++) {
        src = instr_get_src(instr, i);
        if(opnd_is_memory_reference(src)) {
            mem = src;
            size = opnd_get_size(src);
        }
    }

    for(i=0; i<num_dsts; i++) {
        dst = instr_get_dst(instr, i);
        if(opnd_is_memory_reference(dst)) {
            mem = dst;
            size = opnd_get_size(dst);
        }
    }

    /* memory address is loaded to register REG_S1 */
    /* change to load effective address to s1 */
    load_effective_address(drcontext, bb, instr, mem, REG_S1, REG_S2);
    int written_offset = 0;
    /* load the plan address */
    PRE_INSERT(bb, instr,
               INSTR_CREATE_mov_imm(drcontext,
                                    opnd_create_reg(REG_S2),
                                    OPND_CREATE_INTPTR((uint64_t)&commands[emulate_pc])));
    /* Store address */
    PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(REG_S2, written_offset),
                            opnd_create_reg(REG_S1)));
    written_offset+=8;
    /* Mov pc/instr.id */
    if(!external_call_on){
	PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(REG_S2, written_offset),
                            OPND_CREATE_INT32(rule->reg0)));
    }
    else{
	assert(instr_get_app_pc(instr)!=NULL && "PC is NULL for shared library instruction!!");
	PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(REG_S2, written_offset),
                            OPND_CREATE_INT32(0xFFFF & atoi(instr_get_app_pc(instr)))));
    }
    written_offset+=8;
    /* Store modes, opcode */
    PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(REG_S2, written_offset),
                            OPND_CREATE_INT32(mode<<24 | size<<16 | CM_MEM)));
    
    /* Store app_pc for to get debug information later */
    written_offset+=8;
    PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(REG_S2, written_offset),
                            OPND_CREATE_INT32((PCAddress)instr_get_app_pc(instr))));
    emulate_pc++;

    dr_restore_reg(drcontext, bb, instr, REG_S1, SPILL_SLOT_2);
    dr_restore_reg(drcontext, bb, instr, REG_S2, SPILL_SLOT_3);
}

unsigned char *print_reg_name(int reg)
{
    return (unsigned char *)get_register_name(reg);
}
