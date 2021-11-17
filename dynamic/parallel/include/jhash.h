#ifndef _JANUS_HASHTABLE_
#define _JANUS_HASHTABLE_

#include "janus_api.h"

/* Parameters for JITSTM */
/* \brief Width of the hashmask for the key */
#define HASH_MASK_WIDTH             16
#define HASH_TABLE_SIZE             (1 << HASH_MASK_WIDTH)
#define HASH_KEY_MASK               (HASH_TABLE_SIZE - 1)
/* The STM only allows entry size of 8 */
#define HASH_KEY_ALIGH_MASK         7

#define HASH_GET_KEY(addr) ((addr>>2) & HASH_KEY_MASK)

#ifdef NOT_YET_WORKING_FOR_ALL

instrlist_t *
build_hashtable_lookup_instrlist(void *drcontext, instrlist_t *bb,
                                 instr_t *trigger, SReg sregs,
                                 instr_t *hitLabel);

instrlist_t *
create_hashtable_redirect_rw_instrlist(void *drcontext, SReg sregs);
#endif

#endif
