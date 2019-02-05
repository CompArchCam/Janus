#ifndef _JIT_STM_
#define _JIT_STM_

#include "emit.h"

#define GSTM_BOUND_CHECK

#define STM_READ_ONLY 0
#define STM_WRITE_ONLY 1
#define STM_READ_AND_WRITE 2

/* Basic element for STM storage */
/* If mask is zero, then it is a read, otherwise it is a write */
typedef struct _entry {
    uint64_t                data;           //8 bytes
    uintptr_t               addr;           //8 bytes
} spec_item_t;

/* GSTM uses a translation table for address redirection
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
    /* Translation table: must be the first element */
    trans_t             *trans_table;               //+0
    /* Flush table */
    trans_t             **flush_table;              //+8
    /* Speculative Read Set */
    spec_item_t         *read_set;                  //+16
    spec_item_t         *rsptr;                     //+24
    /* Speculative Write Set */
    spec_item_t         *write_set;                 //+32
    spec_item_t         *wsptr;                     //+40
    /* Size of the current transaction */
    uint64_t            trans_size;                 //+48
    /* Register buffers that store the initial value for shared registers */
    priv_state_t        check_point;
    /* Record the initial value for each depending register */
    priv_state_t        reg_read_set;
    /* Scratch private state */
    priv_state_t        private_state;
#ifdef GSTM_BOUND_CHECK
    spec_item_t         *read_set_end;
    spec_item_t         *write_set_end;
#endif
} gtx_t;

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