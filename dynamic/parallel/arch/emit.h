/*! \file emit.h
 *  \brief Architecture specific code generation handlers.
 */
#ifndef _JANUS_PARA_EMIT_ROUTINE_
#define _JANUS_PARA_EMIT_ROUTINE_

#include "janus_api.h"
#include "loop.h"
#include "state.h"
#include <stddef.h>

#define EMIT_CONTEXT void *drcontext, instrlist_t *bb, instr_t *trigger, loop_t *loop, reg_id_t s0, reg_id_t s1, reg_id_t s2, reg_id_t s3
#define emit_context drcontext,bb,trigger,loop,s0,s1,s2,s3

/* We use s1 as the TLS register */
#define TLS s1
#define LOCAL_TLS_OFFSET LOCAL_S1_OFFSET

/* For all the following handlers:
 * There are four registers s0, s1, s2, s3 ready to use.
 * one of them holds the content of the thread local storage */

/** \brief Insert instructions at the trigger to switch the current stack to a specified stack */
void
emit_switch_stack_ptr_aligned(EMIT_CONTEXT);
/** \brief Insert instructions at the trigger to restore the current stack to the original stack */
void
emit_restore_stack_ptr_aligned(EMIT_CONTEXT);

/** \brief Insert instructions at the trigger to selectively save registers to shared register bank based on the register mask */
void
emit_spill_to_shared_register_bank(EMIT_CONTEXT, uint64_t regMask);

/** \brief Insert instructions at the trigger to selectively restore registers from the shared register bank based on the register mask */
void
emit_restore_from_shared_register_bank(EMIT_CONTEXT, uint64_t regMask);

/** \brief Insert instructions at the trigger to selectively save registers to private register bank based on the register mask */
void
emit_spill_to_private_register_bank(EMIT_CONTEXT, uint64_t regMask, int tid);

/** \brief Insert instructions at the trigger to selectively save registers to private register bank based on the register mask
 *
 * if mytid and readtid is different, it generates code that loads from the other thread's private register bank */
void
emit_restore_from_private_register_bank(EMIT_CONTEXT, uint64_t regMask, int mytid, int readtid);

/** \brief Insert instructions to dynamically find for each register in registerToConditionalMerge, 
 * which is the last thread that has written to the register, and merge the register from that thread */
void
emit_conditional_merge_loop_variables(EMIT_CONTEXT, int mytid);

/** \brief Insert instructions at the trigger to make sure all janus threads are staying in pool */
void
emit_wait_threads_in_pool(EMIT_CONTEXT);

/** \brief Insert instructions at the trigger to make sure all janus threads are finished */
void
emit_wait_threads_finish(EMIT_CONTEXT);

/** \brief Insert instructions at the trigger to schedule all janus thread leaving the thread pool */
void
emit_schedule_threads(EMIT_CONTEXT);

/** \brief Generate instructions to move the content of src variable to the dst variable
 *
 * It uses an additional scratch register if both src and dst are memory operands */
void
emit_move_janus_var(EMIT_CONTEXT, JVar dst, JVar src, reg_id_t scratch);

/** \brief Generate instructions to add the content of src variable to the dst variable
 *
 * It uses an additional scratch register if both src and dst are memory operands */
void
emit_add_janus_var(EMIT_CONTEXT, JVar dst, JVar src, reg_id_t scratch, reg_id_t scratch2);

#ifdef NOT_YET_WORKING_FOR_ALL
void
emit_save_private_context_instrlist(EMIT_CONTEXT);
void
emit_restore_shared_context_instrlist(EMIT_CONTEXT);
void
emit_restore_private_context_instrlist(EMIT_CONTEXT);
void
emit_save_checkpoint_context_instrlist(EMIT_CONTEXT);
void
emit_restore_checkpoint_context_instrlist(EMIT_CONTEXT);
void
emit_serial_commit_lock(EMIT_CONTEXT);
void
emit_serial_commit_unlock(EMIT_CONTEXT);
void
emit_loop_iteration_commit(EMIT_CONTEXT);
void
emit_value_prediction(EMIT_CONTEXT);
void
emit_spill_scratch_register(EMIT_CONTEXT, reg_id_t reg);
#endif
#endif