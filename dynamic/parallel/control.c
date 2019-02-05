#include "control.h"
#include "jthread.h"
#include "loop.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

///Pointers to shared region
janus_shared_t *shared;
///Static region of shared threads space
janus_shared_t shared_region;

int janus_status;

int janus_thread_system_init()
{
	int i;
	int nthreads = rsched_info.number_of_threads;
    RSchedHeader *header = rsched_info.header;
    RSLoopHeader *loop_header = rsched_info.loop_header;
    loop_t *loops;

	/* Allocate shared region */
	shared = &shared_region;
    memset(shared, 0, sizeof(janus_shared_t));
    //shared->number_of_functions = rule->reg0;
    /* Set warden thread to the next parallelising thread */
    if (nthreads > 1)
        shared->warden_thread = 1;
    else
        shared->warden_thread = 0;

    /* Allocate an array of dynamic loop structure */
    shared->loops = (loop_t *)malloc(sizeof(loop_t) * rsched_info.header->numLoops);

	/* An oracle is a global structure that points to all local thread states */
    oracle = (janus_thread_t **)malloc(nthreads*sizeof(janus_thread_t *));

    /* Allocate thread local states */
    for (i = 0; i<nthreads; i++) {
        oracle[i] = (janus_thread_t *)malloc(sizeof(janus_thread_t));
        memset(oracle[i], 0, sizeof(janus_thread_t));
    }

    /* Construct a ring of cores by linking the next and prev pointers */
    for (i = 0; i<nthreads; i++) {
        if (i<(nthreads-1))
            oracle[i]->next = oracle[i+1];
        if (i>0)
            oracle[i]->prev = oracle[i-1];
    }
    oracle[0]->prev = oracle[nthreads-1];
    oracle[nthreads-1]->next = oracle[0];

    loops = shared->loops;
    for (i=0; i<header->numLoops; i++) {
        loops[i].header = &(loop_header[i]);
        loops[i].static_id = loops[i].header->staticID;
        loops[i].dynamic_id = i;
        loops[i].start_addr = loops[i].header->loopStartAddr;
        loops[i].schedule = loops[i].header->schedule;
        /* assign the point to variable section */
        loops[i].variables = (JVarProfile *)((uint64_t)header + loops[i].header->ruleDataOffset);
        loops[i].var_count = loops[i].header->ruleDataSize;
    }

#ifdef JANUS_VERBOSE
    dr_printf("JANUS threading system initialised\n");
#endif

    janus_status = header->numLoops;
    return header->numLoops;
}