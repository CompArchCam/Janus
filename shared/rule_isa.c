#include "janus.h"

#include <stdio.h>

const char *print_janus_mode(JMode mode) {
    switch(mode) {
        case JPARALLEL: return "Automatic Parallelisation";
        case JVECTOR: return "Automatic Vectornisation";
        case JFCOV: return "Function Coverage Profiling";
        case JLCOV: return "Loop Coverage Profiling";
        case JPROF: return "Automatic Loop Profiling";
        case JANALYSIS: return "Static Binary Analysis";
        case JOPT: return "Automatic Binary Optimiser";
        case JGRAPH: return "Control Flow Graph Generator";
        case JFETCH: return "Automatic software prefetch";
        default: return "Free Mode";
    }
}

const char *print_rule_opcode(RuleOp op)
{
    switch (op) {
        case GNORMAL: return "GNORMAL";
        case PROF_START: return "PROF_START";
        case PROF_LOOP_START: return "PROF_LOOP_START";
        case PROF_LOOP_ITER: return "PROF_LOOP_ITER";
        case PROF_MEM_ACCESS: return "PROF_MEM_ACCESS";
        case PROF_CALL_START: return "PROF_CALL_START";
        case PROF_CALL_FINISH: return "PROF_CALL_FINISH";
        case PROF_LOOP_FINISH: return "PROF_LOOP_FINISH";
        case PROF_FINISH: return "PROF_FINISH";

        case PARA_PRODUCE: return "PARA_PRODUCE";
        case PARA_CONSUME: return "PARA_CONSUME";
        case PARA_FUNC_HEAD: return "PARA_FUNC_HEAD";
        case PARA_FUNC_EXIT: return "PARA_FUNC_EXIT";
        case PARA_CALL_START: return "PARA_CALL_START";
        case PARA_CALL_END: return "PARA_CALL_END";
        case PARA_CALL_START_DOALL: return "PARA_CALL_START_DOALL";
        case PARA_CALL_END_DOALL: return "PARA_CALL_END_DOALL";
        case PARA_MEM_SIGNAL: return "PARA_MEM_SIGNAL";
        case PARA_MEM_WAIT: return "PARA_MEM_WAIT";
        case PARA_DIRECT_WAIT: return "PARA_DIRECT_WAIT";
        case PARA_DIRECT_SIGNAL: return "PARA_DIRECT_SIGNAL";
        case PARA_REG_SIGNAL: return "PARA_REG_SIGNAL";
        case PARA_REG_WAIT: return "PARA_REG_WAIT";
        case PARA_STAT_INC: return "PARA_STAT_INC";
        case THREAD_CREATE: return "THREAD_CREATE";
        case STM_GEN_CODE: return "STM_GEN_CODE";
        case SYNC_GEN_CODE: return "SYNC_GEN_CODE";
        case PARA_ITER_START: return "PARA_ITER_START";
        case PARA_LOOP_ITER_DOALL: return "PARA_LOOP_ITER_DOALL";
        case PARA_LOOP_INIT: return "PARA_LOOP_INIT";
        case PARA_LOOP_ITER: return "PARA_LOOP_ITER";        
        case PARA_LOOP_FINISH: return "PARA_LOOP_FINISH";
        case PARA_SUBCALL_START: return "PARA_SUBCALL_START";
        case PARA_SUBCALL_FINISH: return "PARA_SUBCALL_FINISH";
        case PARA_ALLOCA_STACK: return "PARA_ALLOCA_STACK";
        case PARA_DALLOCA_STACK: return "PARA_DALLOCA_STACK";
        case PARA_FUNC_START: return "PARA_FUNC_START";
        case PARA_TIMER_START: return "PARA_TIMER_START";
        case PARA_TIMER_END: return "PARA_TIMER_END";
        case PARA_UPDATE_MEM_DISP: return "PARA_UPDATE_MEM_DISP";
        case PARA_USER_DEF_CODE: return "PARA_USER_DEF_CODE";
        case MEM_BOUNDS_CHECK: return "MEM_BOUNDS_CHECK";
        case MEM_RECORD_BOUNDS: return "MEM_RECORD_BOUNDS";
        case THREAD_EXIT: return "THREAD_EXIT";
        case PARA_UPDATE_LOOP_BOUND: return "PARA_UPDATE_LOOP_BOUND";
        case STATS_INSERT_COUNTER: return "STATS_INSERT_COUNTER";
        case APP_SPLIT_BLOCK: return "APP_SPLIT_BLOCK";
        case TX_START: return "TX_START";
        case TX_FINISH: return "TX_FINISH";
        case THREAD_SCHEDULE: return "THREAD_SCHEDULE";
        case THREAD_YIELD: return "THREAD_YIELD";
        case TRANS_START: return "TRANS_START";
        case TRANS_COMMIT: return "TRANS_COMMIT";
        case TRANS_COMPARE: return "TRANS_COMPARE";
        case TRANS_PROCEED: return "TRANS_PROCEED";
        case TRANS_WAIT: return "TRANS_WAIT";
        case TRANS_LOAD: return "STM_LOAD";
        case TRANS_STORE: return "STM_STORE";
        case TRANS_PRIVATISE: return "TRANS_PRIVATISE";
        case TRANS_MEM: return "TRANS_MEM";
        case TRANS_MEM_FAST: return "TRANS_MEM_FAST";
        case TRANS_STACK: return "TRANS_STACK";
        case TRANS_STACK_LEA: return "TRANS_STACK_LEA";
        case TRANS_STACK_IND: return "TRANS_STACK_INDIRECT";
        case PARA_UPDATE_ITER: return "PARA_UPDATE_ITER";
        case PARA_UPDATE_CHECK: return "PARA_UPDATE_CHECK";
        case MEM_MAIN_STACK: return "MEM_MAIN_STACK";
        case MEM_SPILL_REG: return "MEM_SPILL_REG";
        case MEM_RECOVER_REG: return "MEM_RECOVER_REG";
        case MEM_SCRATCH_REG: return "MEM_SCRATCH_REG";
        case PARA_RECOVER_FLAG: return "PARA_RECOVER_FLAG";
        case PARA_SPILL_FLAG: return "PARA_SPILL_FLAG";
        case PARA_DELETE_INSTR: return "PARA_DELETE_INSTR";
        case PARA_PRIVATISE_ADDR: return "PARA_PRIVATISE_ADDR";
        case PARA_LOCK_ADDR: return "PARA_LOCK_ADDR";
        case PARA_UNLOCK_ADDR: return "PARA_UNLOCK_ADDR";
        case PARA_OUTER_LOOP_INIT: return "PARA_OUTER_LOOP_INIT";
        case PARA_OUTER_LOOP_END: return "PARA_OUTER_LOOP_END";
        case MEM_PREFETCH: return "MEM_PREFETCH";
        case INSTR_CLONE: return "INSTR_CLONE";
        case INSTR_UPDATE: return "INSTR_UPDATE";
        case INSTR_NOP: return "INSTR_NOP";
        case EM_START: return "EM_START";
        case EM_END: return "EM_END";
        case EM_LOOP_START: return "EM_LOOP_START";
        case EM_LOOP_END: return "EM_LOOP_END";
        case EM_LOOP_ITER: return "EM_LOOP_ITER";
        //case EM_START_TX: return "EM_START_TX";
        //case EM_END_TX: return "EM_END_TX";
        case EM_SYNC_START: return "EM_SYNC_START";
        case EM_SYNC_END: return "EM_SYNC_END";
        case EM_CALL_START: return "EM_CALL_START";
        case EM_CALL_END: return "EM_CALL_END";
        case EM_MEM: return "EM_MEM";
        case EM_REG: return "EM_REG";
        case EM_PRED_CHECK: return "EM_PRED_CHECK";
        case EM_REG_INDUCTION: return "EM_REG_INDUCTION";
        case EM_STACK: return "EM_STACK";
        case EM_SEEN_BB: return "EM_SEEN_BB";
        case EM_STATUS: return "EM_STATUS";
        case JANUS_DEBUG: return "JANUS_DEBUG";
        case REG_RECOVER: return "REG_RECOVER";
        case CTIMER_LOOP_START: return "CTIMER_LOOP_START";
        case CTIMER_LOOP_END: return "CTIMER_LOOP_END";
        case CTIMER_FUNC_START: return "CTIMER_FUNC_START";
        case CTIMER_FUNC_END: return "CTIMER_FUNC_END";
        case CTRACE_CALL_START: return "CTRACE_CALL_START";
        case CTRACE_CALL_END: return "CTRACE_CALL_END";
        case CTRACE_CALL: return "CTRACE_CALL";
        case CTRACE_CALL_RET: return "CTRACE_CALL_RET";
        case PLAN_LOOP_START: return "PLAN_LOOP_START";
        case PLAN_LOOP_ITER: return "PLAN_LOOP_ITER";
        case PLAN_LOOP_END: return "PLAN_LOOP_END";
        case BEEP_START: return "BEEP_START";
        case BEEP_LOOP_START: return "BEEP_LOOP_START";
        case BEEP_LOOP_ITER: return "BEEP_LOOP_ITER";
        case BEEP_SYNC_START: return "BEEP_SYNC_START";
        case BEEP_SYNC_END: return "BEEP_SYNC_END";
        case BEEP_FUNC_START: return "BEEP_FUNC_START";
        case BEEP_FUNC_END: return "BEEP_FUNC_END";
        case BEEP_CALL_START: return "BEEP_CALL_START";
        case BEEP_CALL_END: return "BEEP_CALL_END";
        case BEEP_MEM: return "BEEP_MEM";
        case BEEP_REG: return "BEEP_REG";
        case BEEP_LOOP_FINISH: return "BEEP_LOOP_FINISH";
        case BEEP_STOP: return "BEEP_STOP";
        case BEEP_BLOCK: return "BEEP_BLOCK";
        case OPT_PREFETCH: return "OPT_PREFETCH";
        case OPT_INSERT_MOV: return "OPT_INSERT_MOV";
        case OPT_INSERT_MEM_MOV: return "OPT_INSERT_MEM_MOV";
        case OPT_INSERT_ADD: return "OPT_INSERT_ADD";
        case OPT_INSERT_SUB: return "OPT_INSERT_SUB";
        case OPT_INSERT_IMUL: return "OPT_INSERT_IMUL";
        case OPT_CLONE_INSTR: return "OPT_CLONE_INSTR";
        case OPT_RESTORE_REG: return "OPT_RESTORE_REG";
        case OPT_RESTORE_FLAGS: return "OPT_RESTORE_FLAGS";
        case OPT_RUNCODE: return "OPT_RUNCODE";
        case VECT_FORCE_SSE: return "VECT_FORCE_SSE";
        case VECT_REDUCE_INIT: return "VECT_REDUCE_INIT";
        case VECT_LOOP_PEEL: return "VECT_LOOP_PEEL";
        case VECT_REG_CHECK: return "VECT_REG_CHECK";
        case VECT_CONVERT: return "VECT_CONVERT";
        case VECT_INDUCTION_STRIDE_UPDATE: return "VECT_INDUCTION_STRIDE_UPDATE";
        case VECT_INDUCTION_STRIDE_RECOVER: return "VECT_INDUCTION_STRIDE_RECOVER";
        case VECT_BROADCAST: return "VECT_BROADCAST";
        case VECT_REDUCE_AFTER: return "VECT_REDUCE_AFTER";
        case VECT_REVERT: return "VECT_REVERT";
        default: return "Null";
    }
}

void print_rule(RRule *rule)
{
    printf("%s 0x%lx:(%d)0x%lx reg0:0x%lx, reg1:0x%lx\n",print_rule_opcode(rule->opcode),rule->block_address,IF_VERBOSE_ELSE(rule->id,0),rule->pc,(uint64_t)rule->reg0,(uint64_t)rule->reg1);
}

void thread_print_rule(int tid, RRule *rule)
{
    printf("Thread %d %s 0x%lx:(%d)0x%lx reg0:0x%lx, reg1:0x%lx\n",tid,print_rule_opcode(rule->opcode),rule->block_address,IF_VERBOSE_ELSE(rule->id,0),rule->pc,(uint64_t)rule->reg0,(uint64_t)rule->reg1);
}

