/*! \file stats.h
 *  \brief Handlers to collect statistics from the Janus Paralleliser
 */
#ifndef _JANUS_PARALLEL_STATS_
#define _JANUS_PARALLEL_STATS_

#include "janus_api.h"

struct _thread_local;
typedef struct _thread_local janus_thread_t;

/** \brief A collection of runtime characteristics */
typedef struct _stat_state {
    uint64_t    counter0;
    uint64_t    counter1;
    uint64_t    counter2;
    uint64_t    counter3;
    uint64_t    num_of_rollbacks;
    uint64_t    num_of_segfaults;
    uint64_t    time;
    uint64_t    temp_time;
    uint64_t    loop_time;
    uint64_t    loop_temp_time;
    uint64_t    num_of_iterations;
    uint64_t    num_of_invocations;
} stat_state_t;

#ifdef JANUS_STATS
#define LOCAL_STATS_ITER_OFFSET     (LOCAL_STATS_OFFSET + offsetof(stat_state_t, num_of_iterations))
#define LOCAL_COUNTER_OFFSET        (LOCAL_STATS_OFFSET + offsetof(stat_state_t, counter0))
#endif

void stats_insert_counter_handler(JANUS_CONTEXT);

#ifdef JANUS_LOOP_TIMER

void loop_inner_timer_resume(long down, long up);

void loop_inner_timer_pause(long down, long up);

void loop_outer_timer_resume(long down, long up);

void loop_outer_timer_pause(long down, long up);

extern uint64_t loop_inner_counter;
extern uint64_t loop_outer_counter;

#endif //JANUS_LOOP_TIMER

#endif