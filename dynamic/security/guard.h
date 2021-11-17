#ifndef _JANUS_GUARD_
#define _JANUS_GUARD_

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

/* Intermediate operation for beep event generation */
typedef enum _opcode {
    CM_UNKNOWN=0,
    CM_RECORD_BASE,
    CM_RECORD_BOUND,
    CM_MEM,
    CM_LOOP_FINISH
} guard_opcode_t;


/* A plan represents an event to be generated at runtime */
typedef struct _access {
    addr_t          addr;
    pc_t            pc;
    addr_t          base;
    addr_t          bound;
    int16_t         opcode;
    int8_t          size;
    int8_t          mode;
    int             id;
    PCAddress       v_pc; /*application PC to get debug information*/
} guard_t;
#ifdef __cplusplus
}
#endif
#endif
