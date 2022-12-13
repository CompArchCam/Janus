#ifndef _JANUS_HINT_ISA_
#define _JANUS_HINT_ISA_

#include "janus.h"

#ifdef __cplusplus
extern "C" {
#endif
extern char rs_dir[];

/* Rewrite Rules Specification */
typedef enum _rr_type {
    GNORMAL = 0,
    /* Generic Rewrite Rules */
    APP_SPLIT_BLOCK,
    /* -----------------------------------------------------
     * Binary Profiling Rewrite Rules 
     * -----------------------------------------------------*/
    ///Initialize a binary profiler
    PROF_START,
    ///Start profiling a loop.
    PROF_LOOP_START,
    ///Start another loop iteration.
    PROF_LOOP_ITER,
    ///Count the instructions within a basic block (deprecated).
    PROF_MEM_ACCESS,
    ///Mark the start of a function call in a loop
    PROF_CALL_START,
    ///Mark the finish of a function call in a loop
    PROF_CALL_FINISH,
    ///Finish profiling a loop
    PROF_LOOP_FINISH,
    ///Finish the binary profiler
    PROF_FINISH,

    /* ----------------------------------------------------
     * Automatic Parallelisation Rewrite Rules 
     * ----------------------------------------------------*/
    ///Initialize loop context for each thread. (Implicitly combines thread schedule)
    PARA_LOOP_INIT,
    ///Marks the start of loop iteration. Perform different modification based on the parallel approach.
    PARA_LOOP_ITER,
    ///Combine loop contexts from all threads. (Implicitly combines thread yield)
    PARA_LOOP_FINISH,
    ///Update the exit condition of a loop
    PARA_UPDATE_LOOP_BOUND,
    ///Marks the start of the sub function call in the loop
    PARA_SUBCALL_START,
    ///Marks the end of the sub function call in the loop
    PARA_SUBCALL_FINISH,

    ///Combined handler: recover and spill the scratch registers
    MEM_SCRATCH_REG,
    ///Combined handler: recover and spill the check register for a loop
    MEM_RESTORE_CHECK_REG,
    ///Recover the value of four scratch registers from TLS
    MEM_RECOVER_REG,
    ///Redirect the stack access to shared stack
    MEM_MAIN_STACK,
    ///Spill the value of four scratch registers to TLS
    MEM_SPILL_REG,
    ///Runtime array base bound check
    MEM_BOUNDS_CHECK,
    ///Save runtime bounds in thread local storage
    MEM_RECORD_BOUNDS,
    ///Set a flag that the current thread has written to the register, used in conditional merging
    MEM_RECORD_REG_WRITE,
    ///Start a software transaction
    TX_START,
    ///Validate and commit a software transaction
    TX_FINISH,
    ///Schedule threads to jump to a code address. (Separated rule)
    THREAD_SCHEDULE,
    ///Send threads back to the thread pool
    THREAD_YIELD,

    /* ----------------------------------------------------
     * Automatic Vectorisation Rewrite Rules 
     * ----------------------------------------------------*/
    ///Loop peel handler
    VECT_LOOP_PEEL,
    ///Broadcast a certain variable
    VECT_BROADCAST,
    ///Update the stride of the induction variable
    VECT_INDUCTION_STRIDE_UPDATE,
    ///Recover the stride of the induction variable (variable stride)
    VECT_INDUCTION_STRIDE_RECOVER,
    VECT_FORCE_SSE,
    VECT_REDUCE_INIT,
    VECT_REG_CHECK,
    ///Convert a scalar instruction to SIMD version
    VECT_CONVERT,
    VECT_REDUCE_AFTER,
    VECT_REVERT,
    /* ----------------------------------------------------
     * Automatic Prefetch Rewrite Rules 
     * ----------------------------------------------------*/
    ///Copy and update an instruction with specified operand
    INSTR_UPDATE,
    ///Duplicate a range of instructions for a given address
    INSTR_CLONE,
    ///Insert a prefetch instruction with given memory operand
    MEM_PREFETCH,
    ///Insert n number of NOP operations
    INSTR_NOP,
    /* ----------------------------------------------------
     * Statistics and Debugging Rewrite Rules 
     * ----------------------------------------------------*/
    ///Insert a thread private counter at given address, supply a counter id
    STATS_INSERT_COUNTER,
    /* ----------------------------------------------------
     * Bounds Checking for Security Rewrite Rules 
     * ----------------------------------------------------*/
    /* record the base and bound of pointer at malloc*/
    BND_RECORD_SIZE,
    BND_RECORD_BASE,
    /* check the bounds of memory access at pointer derefence*/
    BND_CHECK,
    BND_LEA_LOAD,
    BND_ARITHMETIC,
    /*table maintenance rules for bound checking and traking*/
    TABLE_REG_REG_COPY,
    TABLE_REG_MEM_STORE,
    TABLE_MEM_REG_LOAD,
    TABLE_VALUE_REG,
    TABLE_VALUE_MEM,
    /* ----------------------------------------------------
     * The following rewrite rules are out-dated
     * ----------------------------------------------------*/
    //Cycle timer instructions
    CTIMER_LOOP_START,
    CTIMER_LOOP_END,
    CTIMER_FUNC_START,
    CTIMER_FUNC_END,
    CTRACE_CALL_START,
    CTRACE_CALL_END,
    CTRACE_CALL,
    CTRACE_CALL_RET,
    //Paralleliser instructions
    THREAD_CREATE,
    STM_GEN_CODE,           //generate code for stm
    SYNC_GEN_CODE,          //generate code for synchronisation
    PARA_FUNC_HEAD,
    REG_RECOVER,
    PARA_ALLOCA_STACK,
    PARA_OUTER_LOOP_INIT,
    PARA_LOOP_ITER_DOALL,
    PARA_ITER_START,
    PARA_PRODUCE,           //produce tasks for parallelsing threads
    PARA_CONSUME,           //consume the task when the task finishes
    PARA_CALL_START,
    PARA_CALL_END,
    PARA_CALL_START_DOALL,
    PARA_CALL_END_DOALL,
    TRANS_START,
    PARA_RECOVER_FLAG,
    PARA_DIRECT_WAIT,        //standalone wait handler
    PARA_DIRECT_SIGNAL,      //standalone signal handler
    TRANS_MEM,
    TRANS_MEM_FAST,
    TRANS_STACK,
    TRANS_STACK_LEA,        //load effective address on shared stack
    TRANS_STACK_IND,

    PARA_SPILL_FLAG,
    PARA_REG_WAIT,
    PARA_REG_SIGNAL,
    PARA_MEM_WAIT,
    PARA_MEM_SIGNAL,
    PARA_LOCK_ADDR,
    PARA_UNLOCK_ADDR,
    PARA_STAT_INC,
    TRANS_COMMIT,
    TRANS_PROCEED,
    TRANS_WAIT,
    TRANS_LOAD,
    TRANS_STORE,
    TRANS_COMPARE,
    TRANS_PRIVATISE,
    PARA_UPDATE_MEM_DISP,
    PARA_FUNC_START,  
    PARA_UPDATE_ITER,
    PARA_UPDATE_CHECK,
    PARA_UPDATE_STACK,
    PARA_DELETE_INSTR,
    PARA_PRIVATISE_ADDR,
    PARA_USER_DEF_CODE,
    PARA_OUTER_LOOP_END,
    PARA_DALLOCA_STACK,
    PARA_FUNC_EXIT,
    PARA_TIMER_START,
    PARA_TIMER_END,
    THREAD_EXIT,
    //BEEP rule instructions
    BEEP_START,
    BEEP_BLOCK,
    BEEP_LOOP_START,
    BEEP_LOOP_ITER,
    BEEP_SYNC_START,
    BEEP_SYNC_END,
    BEEP_FUNC_START,
    BEEP_FUNC_END,
    BEEP_CALL_START,
    BEEP_CALL_END,
    BEEP_MEM,
    BEEP_REG,
    BEEP_LOOP_FINISH,
    BEEP_STOP,
    //Execution model instructions
    EM_START,
    EM_LOOP_START,
    EM_LOOP_ITER,
    EM_CALL_START,
    EM_CALL_END,
    EM_SEEN_BB,
    EM_SYNC_START,
    EM_SYNC_END,
    EM_MEM,
    EM_PRED_CHECK, //check prediction
    EM_REG,    //cross-iteration dependences
    EM_REG_INDUCTION,
    EM_MEM_INDUCTION,
    EM_STACK, //stack access
    EM_LOOP_END,
    EM_END,
    EM_STATUS,
    /* Planner rules */
    PLAN_LOOP_START,
    PLAN_LOOP_ITER,
    PLAN_LOOP_END,
    /* Optimiser rules */
    OPT_PREFETCH,
    OPT_INSERT_MOV,
    OPT_INSERT_MEM_MOV,
    OPT_INSERT_ADD,
    OPT_INSERT_SUB,
    OPT_INSERT_IMUL,
    OPT_CLONE_INSTR,
    OPT_RESTORE_REG,
    OPT_RESTORE_FLAGS,
    OPT_SAVE_REG_TO_STACK,
    OPT_RESTORE_REG_FROM_STACK,
    OPT_RUNCODE,
    //debug instructions
    JANUS_DEBUG,
    /*------ ASAN ------*/
    ENABLE_MONITORING,
    UNPOISON_CANARY_SLOT,
    MEM_R_ACCESS,
    MEM_W_ACCESS,
    MEM_RW_ACCESS,
    STORE_CANARY_SLOT,
    POISON_CANARY_SLOT,
    NO_RULE
} RuleOp;

typedef struct rule_reg_t {
    uint32_t            up;
    uint32_t            down;
} RuleReg;

//Static rule instruction format:
//each is 4+4+8+8+8+8 = 40 bytes
typedef struct rule_instr {
    RuleOp              opcode      :16;    //specifies the operation on the binary
    uint32_t            channel     :16;    //channel

#ifdef JANUS_VERBOSE
    uint32_t            id          :32;    //debug info, pass instruction id to the paralleliser
#endif

    PCAddress           block_address;
    PCAddress           pc;                 //holds trigger address

    union {
        uint64_t        reg0;
        RuleReg         ureg0;
    };

    union {
        uint64_t        reg1;
        RuleReg         ureg1;
    };

    struct              rule_instr *next;   //TODO, remove this in the future
} RRule;

/* Utility functions */
const char *print_janus_mode(JMode mode);
const char *print_rule_opcode(RuleOp op);
void print_rule(RRule *rule);
void thread_print_rule(int tid, RRule *rule);

#ifdef __cplusplus
}
#endif

#endif
