/* JANUS Dynamic Transactional Memory *
 * This TM must be built on DynamoRIO */
#ifndef _JANUS_STM_
#define _JANUS_STM_

#include "janus_api.h"

/* Parameters for dynamic STM */
#define HASH_MASK_WIDTH             22
#define HASH_TABLE_SIZE             (1 << HASH_MASK_WIDTH)
#define HASH_KEY_MASK 				(HASH_TABLE_SIZE - 1)
/* The STM only allows entry size of 8 */
#define HASH_KEY_ALIGH_MASK 		7
/* The maximum size for read set should be less than the size
 * of the translation hash table */
#define READ_SET_SIZE               HASH_TABLE_SIZE
#define WRITE_SET_SIZE              (HASH_TABLE_SIZE / 2)
#define CACHE_LINE_WIDTH            64

void init_stm_client(JANUS_CONTEXT);

//Records the rollback functions
void gstm_init(void (*rollback_func_ptr)());

void gstm_init_thread(void *tls);

//start a transaction (includes commit)
void transaction_start_handler(JANUS_CONTEXT);
//commit a transaction, return the trigger instruction
instr_t *transaction_commit_handler(JANUS_CONTEXT);

void transaction_inline_memory_handler(JANUS_CONTEXT);

void transaction_inline_stack_handler(JANUS_CONTEXT);

void transaction_privatise_handler(JANUS_CONTEXT);

void transaction_wait_handler(JANUS_CONTEXT);

void register_spill_handler(JANUS_CONTEXT);
void register_recover_handler(JANUS_CONTEXT);
void flag_spill_handler(JANUS_CONTEXT);
void flag_recover_handler(JANUS_CONTEXT);

void function_call_start_handler(JANUS_CONTEXT);


void function_call_end_handler(JANUS_CONTEXT);

void function_call_header_handler(JANUS_CONTEXT);
void function_call_exit_handler(JANUS_CONTEXT);

void redirect_to_shared_stack(JANUS_CONTEXT);

/* Signal handler for the STM */
dr_signal_action_t
stm_signal_handler(void *drcontext, dr_siginfo_t *siginfo);

#endif
