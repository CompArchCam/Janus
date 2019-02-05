/*! \file loop.h
 *  \brief Runtime handlers and code generators for rewrite rules for loops
 */
#ifndef _JANUS_LOOP_HANDLER_
#define _JANUS_LOOP_HANDLER_

#include "janus_api.h"

/** \brief Information for a dynamic loop */
typedef struct _loop {
    /** \brief static id */
    int                     static_id;
    /** \brief dynamic id */
    int                     dynamic_id;
    /** \brief loop start address */
    uint64_t                start_addr;
    /** \brief generated code for loop init (main thread) */
    void                    *loop_init;
    /** \brief generated code for loop finish (main thread) */
    void                    *loop_finish;
    /** \brief an array of variables for this loop */
    JVarProfile             *variables;
    /** \brief number of loop variables */
    int                     var_count;
    /** \brief policy of how threads are scheduled */
    SchedulePolicy          schedule;
    /** \brief loop header retrieved from the rewrite schedule */
    RSLoopHeader            *header;
} loop_t;

/** \brief JIT compiled routine for loop init/finish
 *
 * This loop code is thread private and different per thread per loop. */
typedef struct _generated_code {
    void *thread_loop_init;
    void *thread_loop_finish;
} loop_code_t;

/** \brief Janus dynamic handler for RRule: PARA_LOOP_INIT */
void
loop_init_handler(JANUS_CONTEXT);

/** \brief Janus dynamic handler for RRule: PARA_LOOP_FINISH */
void
loop_finish_handler(JANUS_CONTEXT);

/** \brief Janus dynamic handler for RRule: PARA_LOOP_ITER */
void
loop_iteration_handler(JANUS_CONTEXT);

/** \brief Janus dynamic handler for RRule: PARA_UPDATE_LOOP_BOUND */
void
loop_update_boundary_handler(JANUS_CONTEXT);

/** \brief Janus dynamic handler for RRule: PARA_SUBCALL_START */
void
loop_subcall_start_handler(JANUS_CONTEXT);

/** \brief Janus dynamic handler for RRule: PARA_SUBCALL_FINISH */
void
loop_subcall_finish_handler(JANUS_CONTEXT);

/** \brief Janus dynamic handler for RRule: MEM_SPILL_REG */
void
scratch_register_spill_handler(JANUS_CONTEXT);

/** \brief Janus dynamic handler for RRule: MEM_RECOVER_REG */
void
scratch_register_restore_handler(JANUS_CONTEXT);

/** \brief Janus dynamic handler for RRule: MEM_MAIN_STACK */
void
loop_switch_to_main_stack_handler(JANUS_CONTEXT);

/** \brief Janus dynamic handler for RRule: MEM_SCRATCH_REG */
void
loop_scratch_register_handler(JANUS_CONTEXT);

/** \brief Janus dynamic handler for RRule: MEM_BOUNDS_CHECK */
void
loop_array_bound_check_handler(JANUS_CONTEXT);

/** \brief Janus dynamic handler for RRule: PARA_OUTER_LOOP_INIT */
void
loop_outer_init_handler(JANUS_CONTEXT);

/** \brief Janus dynamic handler for RRule: PARA_OUTER_LOOP_END */
void
loop_outer_finish_handler(JANUS_CONTEXT);

/** \brief Dynamically generate the init/finish code
 * This function is called by all threads immediately they are created and initialised
 * The loop skeleton is used for quick execution of the loop components */
void
janus_generate_loop_code(void *drcontext, void *tls);

/** \brief Constructs a dynamic instruction list for loop initialisation */
instrlist_t *
build_loop_init_instrlist(void *drcontext, loop_t *loop);

/** \brief Constructs a dynamic instruction list for loop finish/merge */
instrlist_t *
build_loop_finish_instrlist(void *drcontext, loop_t *loop);

/** \brief Constructs a dynamic instruction list for per-thread loop initialisation */
instrlist_t *
build_thread_loop_init_instrlist(void *drcontext, loop_t *loop, int tid);

/** \brief Constructs a dynamic instruction list for per-thread loop finish/merge */
instrlist_t *
build_thread_loop_finish_instrlist(void *drcontext, loop_t *loop, int tid);

#endif
