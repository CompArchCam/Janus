/* JANUS Synchronisation Utility */
#ifndef _JANUS_SYNC_
#define _JANUS_SYNC_

#include "janus.h"
#include "janus_api.h"

typedef struct _gsync_entry {
    uint64_t        data;
    uint64_t        _dummy[7]; /* remove false sharing */
    uint64_t        version;
    uint64_t        _dummy2[7];
} sync_entry_t;

typedef struct _gsync {
    /* Array of sync entries */
    sync_entry_t    *entries;
    uint64_t        size;
} gsync_t;

/* This must be called after threads creation */
void generate_dynamic_sync_code_handler(JANUS_CONTEXT);

void wait_register_handler(JANUS_CONTEXT);

void signal_register_handler(JANUS_CONTEXT);

void wait_memory_handler(JANUS_CONTEXT);

void signal_memory_handler(JANUS_CONTEXT);

void direct_signal_handler(JANUS_CONTEXT);

void direct_wait_handler(JANUS_CONTEXT);

void lock_address_handler(JANUS_CONTEXT);

void unlock_address_handler(JANUS_CONTEXT);

instrlist_t *
sync_prediction(void *drcontext, instrlist_t *bb,
                instr_t *trigger, SReg sregs, VariableProfile *profile, bool full);

void print_signal(uint64_t value, uint64_t stamp);
void print_wait(uint64_t value, uint64_t stamp, uint64_t version);

#define PRINTSIGNAL(A) dr_insert_clean_call(drcontext,bb,trigger,print_signal,false,1,opnd_create_reg(A))
#endif
