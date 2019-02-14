/* Lists of API that can be used to write JANUS Client */

#ifndef JANUS_API
#define JANUS_API

//Data structures
#include "janus.h"
//Frontend: Load static rule utilities
#include "front_end.h"
//Modification: JANUS handlers
#include "handler.h"
//Modification: JANUS code generators
#include "code_gen.h"

///Global meta information of the rewrite schedule
extern RSchedInfo rsched_info;
///Enable Janus just in time transactional memory in x86-64 mode
#ifdef JANUS_X86
#define JANUS_JITSTM
#endif

//#define JANUS_DYNAMIC_CHECK
//#define JANUS_DEBUG_RUN
/* \brief Insert a SIGTRAP before and after the loop by the main thread */
//#define JANUS_DEBUG_LOOP_START_END
/* \brief Insert a SIGTRAP before threads leave thread pool */
//#define JANUS_DEBUG_RUN_THREADED
/* \brief Insert a SIGTRAP before each iteration start by all the threads */
//#define JANUS_DEBUG_RUN_ITERATION_START
//#define JANUS_DEBUG_RUN_ITERATION_END
//#define JANUS_VERBOSE
//#define JANUS_LOOP_VERBOSE
//#define GSYNC_VERBOSE
//#define GSTM_VERBOSE
//#define GSTM_MEM_TRACE
//#define JANUS_STATS
//#define JANUS_LOOP_TIMER
//#define JANUS_PRINT_ROLLBACK
//#define JANUS_ORACLE_ASSIST
//#define HALT_MAIN_THREAD
//#define HALT_PARALLEL_THREADS
/* \brief Use thread shared code cache (AArch64 only) */
#ifdef JANUS_AARCH64
#  define JANUS_SHARED_CC
#  define     BRK(trigger)     INSERT(bb, trigger, instr_create_0dst_1src(drcontext, OP_brk, OPND_CREATE_INT16(0)));
#  define PRE_BRK(trigger) PRE_INSERT(bb, trigger, instr_create_0dst_1src(drcontext, OP_brk, OPND_CREATE_INT16(0)));
#elif JANUS_X86
#  define     INT(trigger)     INSERT(bb,trigger,INSTR_CREATE_int1(drcontext));
#  define PRE_INT(trigger) PRE_INSERT(bb,trigger,INSTR_CREATE_int1(drcontext));
#endif
#endif
//
