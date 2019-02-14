#ifndef _JANUS_DYNAMIC_THREAD_
#define _JANUS_DYNAMIC_THREAD_

#include "janus_api.h"
#include "state.h"
#include "loop.h"

#ifdef JANUS_STATS
#include "stats.h"
#endif

#ifdef JANUS_JITSTM
/* JANUS Software Transactional Memory internal API */
#include "jitstm.h"
#endif

#include <stddef.h>

#define JANUS_THREAD_STACK_SIZE   (32*1024)

typedef volatile uint32_t flag_t;

/**
 * Per-thread spill space
 * The offset is being used in the assembly.
 * Do not change the order unless necessary.
 */
typedef struct _private_state {
    uint64_t                s0;         /* +0  */
    uint64_t                s1;         /* +8  */
    uint64_t                s2;         /* +16 */
    uint64_t                s3;         /* +24 */
    uint64_t                ret_addr;   /* +32 */
    uint64_t                control_flag;  /* +40 */
    uint64_t                check;      /* + 48 */
    uint64_t                block;      /* + 56 */

    uint64_t                slot0;      /* + 64 */
    uint64_t                slot1;      /* */
    uint64_t                slot2;      /* */
    uint64_t                slot3;      /* */
    uint64_t                slot4;      /* */
    uint64_t                slot5;      /* */
    uint64_t                slot6;      /* */
    uint64_t                slot7;      /* +120*/
} spill_state_t;

/**
 * Per-thread flag space, for conditionally executing
 * modified runtime code.
 * The offset is being used in the assembly.
 * Do not change the order unless necessary.
 */
typedef struct _flag_state {
    flag_t                  loop_on;            /* +0  */
    flag_t                  trans_on;           /* +4  */
    flag_t                  call_on;            /* +8 */
    flag_t                  can_commit;         /* +12 */
    flag_t                  in_pool;            /* +16 */
    flag_t                  stack_allocated;    /* +20 */
    flag_t                  finished;           /* +24 */
    flag_t                  rolledback;         /* +28 */
    flag_t                  exited;             /* +32 */
} flag_state_t;

/**
 * Per-thread data structure
 * This structure private to parallelising threads
 */
typedef struct _thread_local {
    /** \brief Local spill space, most frequently used */
    spill_state_t           spill_space;
    /** \brief Local identifier of threads */
    uint64_t                id;
    /** \brief Local flag space */
    volatile flag_state_t   flag_space;

    /** \brief JIT loop code*/
    loop_code_t             *gen_code;

    uint64_t                distance;
    volatile uint64_t       dummy;
    /* Local stamp */
    uint64_t                local_stamp;
    /** \brief Private register bank */
    priv_state_t            private_state;

#ifdef JANUS_STATS
    stat_state_t            stats;
#endif

#ifdef NOT_YET_WORKING_FOR_ALL
    /* Private space for absolute addresses */
    uint64_t                buffer[PRIVATE_ABS_ADDR_SPACE];
    uint64_t                rbuffer[PRIVATE_ABS_ADDR_SPACE];
    /* Syncronisation channels */
    //gsync_t                 sync;
    void                    *dr_redirect_target;
    dynamic_code_t          *current_code_cache;
#endif

#ifdef JANUS_JITSTM
    /** \brief Janus just-in-time transactional memory */
    jtx_t                   tx;
    /** \brief JIT thread code*/
    jtx_code_t              *tx_gen_code;
#endif
    /* The current DynamoRIO header */
    void                    *drcontext;
    /* stack frame */
    void                    *stack;
    void                    *stack_ptr;
    /* Flag address for the next thread */
    struct _thread_local    *prev;
    struct _thread_local    *next;
    void                    *exits;
    uint64_t                thread_exit_pc;
} janus_thread_t;

/** \brief Global threads oracle */
extern janus_thread_t **oracle;

/* List of offsets in janus_thread_t structure */
#define LOCAL_SPILL_OFFSET      (offsetof(janus_thread_t, spill_space))
#define LOCAL_FLAG_OFFSET       (offsetof(janus_thread_t, flag_space))
#define LOCAL_STACK_OFFSET      (offsetof(janus_thread_t, stack_ptr))
#define LOCAL_STACKBASE_OFFSET  (offsetof(janus_thread_t, stack))
#define LOCAL_PSTATE_OFFSET     (offsetof(janus_thread_t, private_state))
#define LOCAL_ID_OFFSET         (offsetof(janus_thread_t, id))
#define LOCAL_GEN_CODE_OFFSET   (offsetof(janus_thread_t, gen_code))
#ifdef JANUS_STATS
#define LOCAL_STATS_OFFSET      (offsetof(janus_thread_t, stats))
#endif
#ifdef JANUS_JITSTM
#define LOCAL_TX_OFFSET         (offsetof(janus_thread_t, tx))
#endif
/* List of offsets in spill_space structure */
#define LOCAL_S0_OFFSET         (LOCAL_SPILL_OFFSET + offsetof(spill_state_t, s0))
#define LOCAL_S1_OFFSET         (LOCAL_SPILL_OFFSET + offsetof(spill_state_t, s1))
#define LOCAL_S2_OFFSET         (LOCAL_SPILL_OFFSET + offsetof(spill_state_t, s2))
#define LOCAL_S3_OFFSET         (LOCAL_SPILL_OFFSET + offsetof(spill_state_t, s3))
#define LOCAL_RETURN_OFFSET     (LOCAL_SPILL_OFFSET + offsetof(spill_state_t, ret_addr))
#define LOCAL_CHECK_OFFSET      (LOCAL_SPILL_OFFSET + offsetof(spill_state_t, check))
#define LOCAL_BLOCK_SIZE_OFFSET (LOCAL_SPILL_OFFSET + offsetof(spill_state_t, block))
#define LOCAL_SLOT0_OFFSET      (LOCAL_SPILL_OFFSET + offsetof(spill_state_t, slot0))
#define LOCAL_SLOT1_OFFSET      (LOCAL_SPILL_OFFSET + offsetof(spill_state_t, slot1))
#define LOCAL_SLOT2_OFFSET      (LOCAL_SPILL_OFFSET + offsetof(spill_state_t, slot2))
#define LOCAL_SLOT3_OFFSET      (LOCAL_SPILL_OFFSET + offsetof(spill_state_t, slot3))
#define LOCAL_SLOT6_OFFSET      (LOCAL_SPILL_OFFSET + offsetof(spill_state_t, slot6))
#define LOCAL_SLOT7_OFFSET      (LOCAL_SPILL_OFFSET + offsetof(spill_state_t, slot7))

/* List of offsets in flag_space structure */
#define SPECULATION_ON_FLAG_OFFSET   \
 (LOCAL_FLAG_OFFSET + (offsetof(flag_state_t, trans_on)))
#define LOOP_ON_FLAG_OFFSET   \
 (LOCAL_FLAG_OFFSET + (offsetof(flag_state_t, loop_on)))
#define CALL_ON_FLAG_OFFSET   \
 (LOCAL_FLAG_OFFSET + (offsetof(flag_state_t, call_on)))
#define LOCAL_FLAG_CAN_COMMIT \
  LOCAL_FLAG_OFFSET + (offsetof(flag_state_t, can_commit))
#define LOCAL_FLAG_CAN_RUN \
  LOCAL_FLAG_OFFSET + (offsetof(flag_state_t, can_run))
#define LOCAL_FLAG_IN_POOL \
  LOCAL_FLAG_OFFSET + (offsetof(flag_state_t, in_pool))
#define LOCAL_FLAG_NEED_YIELD \
  LOCAL_FLAG_OFFSET + (offsetof(flag_state_t, need_yield))
#define LOCAL_FINISHED_OFFSET \
  LOCAL_FLAG_OFFSET + (offsetof(flag_state_t, finished))
#define LOCAL_FLAG_STACK_ALLOC_OFFSET \
  LOCAL_FLAG_OFFSET + (offsetof(flag_state_t, stack_allocted))
#define LOCAL_FLAG_ROLLEDBACK_OFFSET \
  LOCAL_FLAG_OFFSET + (offsetof(flag_state_t, rolledback))
#define LOCAL_FLAG_SEQ_OFFSET \
  LOCAL_FLAG_OFFSET + (offsetof(flag_state_t, exited))

/** \brief Create janus threads that have their own code cache and rewrite schedule interpretators
 *
 * The number of threads are specified in the global info
 * Once the threads are created, they are placed in the thread pool */
void janus_create_threads();

/** \brief Delete all janus threads */
void janus_delete_threads(void);

/** \brief Thread creation event
 *
 * It will be called whenever an application thread is created. */
void janus_thread_spawn_handler(void *drcontext);

/** \brief Thread exit event 
 *
 * It will be called whenever an application thread is deleted. */
void janus_thread_exit_handler(void *drcontext);

/** \brief Thread pool in application mode */
int janus_thread_pool_app(janus_thread_t *tls, int status);

/** \brief Re-enter thread pool in application mode */
int janus_reenter_thread_pool_app(janus_thread_t *tls);
#endif
