#ifndef _JANUS_HASHTABLE_
#define _JANUS_HASHTABLE_

#include "janus_api.h"

#ifdef NOT_YET_WORKING_FOR_ALL

instrlist_t *
build_hashtable_lookup_instrlist(void *drcontext, instrlist_t *bb,
                                 instr_t *trigger, SReg sregs,
                                 instr_t *hitLabel);

instrlist_t *
create_hashtable_redirect_rw_instrlist(void *drcontext, SReg sregs);
#endif

#endif
