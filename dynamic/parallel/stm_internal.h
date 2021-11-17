/* JANUS Dynamic Transactional Memory Code Caches*
 * This TM must be built on DynamoRIO */
#ifndef _JANUS_STM_INT_
#define _JANUS_STM_INT_

#include "jitstm.h"

typedef instrlist_t* (*PredictFunction)(void *drcontext, instrlist_t *bb, instr_t *trigger, SReg sregs, VariableProfile *profile, bool full);

/* All code cache constructors */
instrlist_t *
build_transaction_start_instrlist(void *drcontext, SReg sregs, Loop_t *loop);
instrlist_t *
build_transaction_commit_instrlist(void *drcontext, SReg sregs, Loop_t *loop);
instrlist_t *
build_transaction_clear_instrlist(void *drcontext, SReg sregs, Loop_t *loop);
instrlist_t *
build_transaction_rollback_instrlist(void *drcontext, SReg sregs, Loop_t *loop);
instrlist_t *
build_transaction_wait_instrlist(void *drcontext, SReg sregs, void *rollback, Loop_t *loop);

instrlist_t *
build_stm_value_prediction_instrlist(void *drcontext, instrlist_t *bb,
                                     instr_t *trigger, SReg sregs,
                                     int mode, PredictFunction prediction,
                                     VariableProfile *profile, bool full);
/* Perform a partial speculative access on a STM word */
instrlist_t *
build_stm_access_partial_instrlist(void *drcontext, instrlist_t *bb,
                                   instr_t *trigger, SReg sregs,
                                   int mode, PredictFunction prediction,
                                   VariableProfile *profile);
/* Perform a full speculative access on a STM word */
instrlist_t *
build_stm_access_full_instrlist(void *drcontext, instrlist_t *bb,
                                instr_t *trigger, SReg sregs,
                                int mode, PredictFunction prediction,
                                VariableProfile *profile, bool full);

instrlist_t *
build_stm_create_entry_instrlist(void *drcontext, instrlist_t *bb,
                                 instr_t *trigger, SReg sregs,
                                 int mode, PredictFunction prediction,
                                 VariableProfile *profile, bool full);

/* Prediction schemes - call backs */
instrlist_t *
direct_load_prediction(void *drcontext, instrlist_t *bb,
                       instr_t *trigger, SReg sregs, VariableProfile *profile,
                       bool full);
instrlist_t *
induction_prediction(void *drcontext, instrlist_t *bb,
                     instr_t *trigger, SReg sregs, VariableProfile *profile,
                     bool full);

instrlist_t *
build_stm_read_partial_instrlist(void *drcontext, SReg sregs);
instrlist_t *
build_stm_write_partial_instrlist(void *drcontext, SReg sregs);
instrlist_t *
build_stm_read_full_instrlist(void *drcontext, SReg sregs);
instrlist_t *
build_stm_write_full_instrlist(void *drcontext, SReg sregs);

instrlist_t *
build_stm_validate_instrlist(void *drcontext, instrlist_t *bb, instr_t *trigger, SReg sregs, void *rollback);
instrlist_t *
build_stm_commit_instrlist(void *drcontext, instrlist_t *bb, instr_t *trigger, SReg sregs);

instrlist_t *
build_merge_reduction_variable(void *drcontext, instrlist_t *bb, instr_t *trigger, GBitMask reductionMask);
instrlist_t *
create_hashtable_probe_more_read_instrlist(void *drcontext, SReg sregs);
instrlist_t *
create_hashtable_probe_more_write_instrlist(void *drcontext, SReg sregs);

instrlist_t *
create_spec_reg_predict_instrlist(void *drcontext, gtx_t *tx);

#ifdef HOHO
instrlist_t *
emit_save_eflags(void *drcontext, instrlist_t *bb, instr_t *trigger, priv_state_t *mc);

instrlist_t *
emit_restore_eflags(void *drcontext, instrlist_t *bb, instr_t *trigger, priv_state_t *mc);
#endif


instrlist_t *
emit_predict_value(void *drcontext, instrlist_t *bb, instr_t *trigger, SReg sregs, Loop_t *loop);
instrlist_t *
emit_predict_variable_value(void *drcontext, instrlist_t *bb, instr_t *trigger, SReg sregs, VariableProfile *profile);

gtx_t *get_transaction_header();

//debug macros
void print_scratch(uint64_t s0, uint64_t s1, uint64_t s2, uint64_t s3);
void print_spill_id(int fid, reg_id_t s0, reg_id_t s1, reg_id_t s2, reg_id_t s3);
void print_recover_id(int fid, reg_id_t s0, reg_id_t s1, reg_id_t s2, reg_id_t s3);
void print_reg(uint64_t reg, uint64_t value);
void print_value(uint64_t ptr);
void print_state();
void infloop();
void print_stage(int stage);
void print_rollback_reg(uint64_t reg, uint64_t value, uint64_t pred);

#ifdef GSTM_VERBOSE
void print_commit_memory(uint64_t addr, uint64_t value);

void print_commit_reg(uint64_t reg, uint64_t value);

void printAddress(uint64_t id,uint64_t bbAddr);

void printAddressData(uint64_t id,uint64_t bbAddr);

void printSpill(uint64_t id, uint64_t reg, uint64_t mode, uint64_t value);

#define PRINTSPILL(id, reg, mode) dr_insert_clean_call(drcontext,bb,trigger,printSpill,false,4,OPND_CREATE_INTPTR(id),OPND_CREATE_INTPTR(reg),OPND_CREATE_INTPTR(mode), opnd_create_reg(reg))
#else
#define PRINTSPILL(id, reg, mode)
#endif

#define PRINTSTAGE(S) \
    IF_VERBOSE(dr_insert_clean_call(drcontext,bb,trigger,print_stage,false,1,OPND_CREATE_INT32(S)))
#define PRINTISTAGE(S) \
    dr_insert_clean_call(drcontext,bb,trigger,print_stage,false,1,OPND_CREATE_INT32(S))
#define PRINTSCRATCH dr_insert_clean_call(drcontext,bb,trigger,print_scratch,false,4,opnd_create_reg(scratch_reg0),opnd_create_reg(scratch_reg1),opnd_create_reg(scratch_reg2),opnd_create_reg(scratch_reg3));
#define PRINTREG(A) dr_insert_clean_call(drcontext,bb,trigger,print_reg,false,2,OPND_CREATE_INTPTR(A),opnd_create_reg(A));
#define PRINTABORT(reg,A,B) dr_insert_clean_call(drcontext,bb,trigger,print_rollback_reg,false,3,reg,A,B);
#define PRINTVAL(A) dr_insert_clean_call(drcontext,bb,trigger,print_value,false,1,OPND_CREATE_INTPTR(&(A)));
#define PRINTSTATE  dr_insert_clean_call(drcontext,bb,trigger,print_state,false,0);
#define INFLOOP INSERT(bb,trigger,INSTR_CREATE_jmp(drcontext,opnd_create_pc((void*)infloop)));
//#define PRINTREG(A) dr_insert_clean_call(drcontext,bb,trigger,print_scratch,false,1,opnd_create_reg(A));

#define TRANS_START_STAGE 0
#define TRANS_RESTART_STAGE 1
#define TRANS_VALIDATE_STAGE 2
#define TRANS_COMMIT_STAGE 3
#define TRANS_ROLLBACK_STAGE 4
#define TRANS_WAIT_STAGE 5
#define LOOP_INIT_STAGE 6
#define LOOP_END_STAGE 7
#define SPEC_READ_PART_STAGE 8
#define SPEC_WRITE_PART_STAGE 9
#define SPEC_READ_FULL_STAGE 10
#define SPEC_WRITE_FULL_STAGE 11
#define CALL_START_STAGE 12
#define CALL_END_STAGE 13
#define CALL_HEAD_STAGE 14
#define CALL_EXIT_STAGE 15
#define TRANS_PREDICT_STAGE 16
#endif
