/* \brief JANUS Just-in-time Transactional Memory *
 *
 * All the code is dynamically generated */

#ifndef _JANUS_JITSTM_
#define _JANUS_JITSTM_

#include "emit.h"

/* Enable read/write buffer size check in the JITSTM */
#define JITSTM_BOUND_CHECK

#define STM_READ_ONLY 0
#define STM_WRITE_ONLY 1
#define STM_READ_AND_WRITE 2

/* The maximum size for read set should be less than the size
 * of the translation hash table */
#define READ_SET_SIZE               HASH_TABLE_SIZE
#define WRITE_SET_SIZE              (HASH_TABLE_SIZE / 2)
#define CACHE_LINE_WIDTH            64

/* Basic element for JITSTM storage */
/* If mask is zero, then it is a read, otherwise it is a write */
typedef struct _entry {
    uint64_t                data;           //8 bytes
    uintptr_t               addr;           //8 bytes
} spec_item_t;

/* JITSTM uses a translation table for address redirection
 * The first element is the original address
 * The second element is the redirected address
 * The redirected address always points to a pre-defined thread-private memory region to store data
 * It either points to read or write sets
 *
 * NOTE: IF YOU CHANGE THE SIZE OF THIS STRUCTURE
 * YOU NEED TO CHANGE THE HASHTABLE LOOKUP TOO
 */
typedef struct _trans {
    uintptr_t               original_addr;
    spec_item_t             *redirect_addr;
    uint32_t                rw;
    uint32_t                rollback;
} trans_t;

/* Private transaction structure
 * don't change the order of the structure
 * the offset is being used in the assembly */
typedef struct janus_tx
{
    /* \brief Translation table: must be the first element */
    trans_t             *trans_table;               //+0
    /* \brief Flush table
     *
     * Used for keeping track of already written entries for transaction clear */
    trans_t             **flush_table;              //+8
    /* \brief Speculative read set
     *
     * Keeps all the read entries */
    spec_item_t         *read_set;                  //+16
    /* \brief Speculative read set pointer
     *
     * It always points to a free entry in the read set */
    spec_item_t         *rsptr;                     //+24
    /* \brief Speculative write set
     *
     * Keeps all the write entries */
    spec_item_t         *write_set;                 //+32
    /* \brief Speculative write set pointer
     *
     * It always points to a free entry in the write set */
    spec_item_t         *wsptr;                     //+40
    /* \brief Size of the current transaction */
    uint64_t            trans_size;                 //+48
    /* \brief Register buffers that store the initial value for shared registers */
    priv_state_t        check_point;
    /* \brief Record the initial value for each depending register */
    priv_state_t        reg_read_set;
    /* \brief Scratch private state */
    priv_state_t        private_state;
    /* \brief Call back to TLS */
    void                *tls;
#ifdef JITSTM_BOUND_CHECK
    /* \brief Points to the end of the speculative read set */
    spec_item_t         *read_set_end;
    /* \brief Points to the end of the speculative write set */
    spec_item_t         *write_set_end;
#endif
} jtx_t;

typedef struct _janus_jit_code
{
    void                *clean_call;
} jtx_code_t;

/* List of offsets used in JITSTM */
#define LOCAL_TX_STACK_BASE         (offsetof(spill_state_t, slot4))
#define LOCAL_TX_THRESHOLD_BASE     (offsetof(spill_state_t, slot5))

#ifdef FIX
/* All code cache constructors */
void
emit_tx_start_instrlist(EMIT_CONTEXT);
void
emit_tx_validate_instrlist(EMIT_CONTEXT);
void
emit_tx_commit_instrlist(EMIT_CONTEXT, bool save_all);
void
emit_tx_clear_instrlist(EMIT_CONTEXT);
#endif
#endif