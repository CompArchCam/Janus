#ifndef _JANUS_PLAN_
#define _JANUS_PLAN_

#include "janus_api.h"
/*-----Debug Symbol info----*/
#include "dr_api.h"
# include "drsyms.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
//#define PLANNER_VERBOSE
#define NUM_OF_REGS 16
#define GRANULARITY 4
//#define SINGLE_INVOCATION_MODE
#define MAX_ALLOWED_COMMAND 4096
#define CROSS_DEPD_ONLY
//#define INSTR_COUNTER

typedef uintptr_t   addr_t;
typedef uintptr_t   pc_t;
typedef uintptr_t   iter_t;
typedef uintptr_t   cycle_t;

/* Intermediate operation for beep event generation */
typedef enum _opcode {
    CM_UNKNOWN=0,
    CM_REG_READ,
    CM_REG_WRITE,
    CM_MEM_READ,
    CM_MEM_WRITE,
    CM_MEM,
    CM_LOOP_START,
    CM_LOOP_ITER,
    CM_LOOP_FINISH,
    CM_BLOCK,
    CM_DELAY
    /*CM_CALL_START,
    CM_CALL_FINISH*/
} plan_opcode_t;

/* profiling modes for plan*/
typedef enum _profmode{
    DOALL=1,
    DOALL_LIGHT,
    LIGHT,
    FULL
} plan_profmode_t;

/* A plan represents an event to be generated at runtime */
typedef struct _access {
    addr_t          addr;
    pc_t            pc;
    int16_t         opcode;
    int8_t          size;
    int8_t          mode;
    int             id;
    PCAddress       v_pc; /*application PC to get debug information*/
} plan_t;

/* PLAN events */
void planner_start(int loop_id, int num_threads);
void planner_stop();
void planner_execute(int command_size);

/* Loop events */
void planner_begin_loop();
void planner_new_loop_iteration();
void planner_finish_loop();

unsigned char *print_reg_name(int reg);

/* A buffer to store commands per basic block
 * This is to remove duplicate clean calls and speedup the emulation process */
extern plan_t *commands;
extern int emulate_pc;
extern int instrumentation_switch;
extern uint64_t total_program_cycles;
extern const char *app_name;
extern const char *app_full_name;
extern plan_profmode_t profmode;
extern int iThreshold;
extern int nThreshold;
extern bool stopDependencyChecks;

#ifdef __cplusplus
}
#endif
#endif
