/* \brief JANUS Dynamic Transactional Memory API and wrappers
 *
 * It also contains a C implementation of the STM */

#ifndef _JANUS_STM_API_
#define _JANUS_STM_API_

#include "janus_api.h"
#include "jthread.h"

#define JANUS_SLOW_STM
//#define JANUS_STM_VERBOSE

/* C implementation of Janus STM */
/* \brief Janus speculative read 64 bit */
uint64_t janus_spec_read(janus_thread_t *tls, uint64_t original_addr);
/* \brief Janus speculative read 64 bit */
void janus_spec_write(janus_thread_t *tls, uint64_t original_addr, uint64_t data);
/* \brief Janus get speculative pointer
 *
 * It can be used for partial memory writes, or read & write pairs */
void *janus_spec_get_ptr(janus_thread_t *tls, uint64_t original_addr);
/* \brief validate the read set and commit the write set */
void *janus_transaction_commit(janus_thread_t *tls);
/* \brief clear the current transaction */
void *janus_transaction_clear(janus_thread_t *tls);
/* \brief abort the current transaction and perform rollback */
void *janus_transaction_abort(janus_thread_t *tls);
///Initialise the data structure for JITSTM
void janus_thread_init_jitstm(void *tls);

/* \brief Dynamic handlers in speculative mode
 *
 * This is only invoked when Janus is in speculative mode in shared library cals */
void master_dynamic_speculative_handlers(void *drcontext, instrlist_t *bb);

///start a transaction
void transaction_start_handler(JANUS_CONTEXT);
///finish a transaction but no commits
void transaction_finish_handler(JANUS_CONTEXT);
///commit a transaction, separated from transaction_finish
void transaction_commit_handler(JANUS_CONTEXT);
///redirect the memory instruction to speculative read & write buffer, clean call version
void speculative_memory_handler(void *drcontext, janus_thread_t *local, instrlist_t *bb, instr_t *instr, int *reg_index);
///redirect the memory instruction to speculative read & write buffer, inline version
void speculative_inline_memory_handler(void *drcontext, janus_thread_t *local, instrlist_t *bb, instr_t *instr);

///commit a transaction, inlined version
instr_t *transaction_inline_commit_handler(JANUS_CONTEXT);
///direct generic memory to speculative accesses, inlined version
void transaction_inline_memory_handler(JANUS_CONTEXT);
///perform a bound check on the stack accesses, if above stack pointer, change to speculative accesses
void transaction_inline_stack_handler(JANUS_CONTEXT);
///privatise a memory access
void transaction_privatise_handler(JANUS_CONTEXT);
///perform a serialization on transaction commits
void transaction_wait_handler(JANUS_CONTEXT);

void function_call_start_handler(JANUS_CONTEXT);
void function_call_end_handler(JANUS_CONTEXT);
void function_call_header_handler(JANUS_CONTEXT);
void function_call_exit_handler(JANUS_CONTEXT);
void redirect_to_shared_stack(JANUS_CONTEXT);

/* \brief Signal handler for the Janus STM
 *
 * If fault occurs in any of the STM routines, control is directed here.
 * It performs the transaction abort and rolledback */
dr_signal_action_t
stm_signal_handler(void *drcontext, dr_siginfo_t *siginfo);
#endif
