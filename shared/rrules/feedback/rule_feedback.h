/*! \file rule_feedback.h
 *
 *  Header file that defines interface that feedback to static tool */

#ifndef _JANUS_FEEDBACK_
#define _JANUS_FEEDBACK_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

///A cluster of information collected from profiling
typedef struct _loop_info {
    ///loop id
    uint32_t            id;
    ///fraction of cycles of the loop over the whole execution
    double              coverage;
    ///total iteration count
    uint64_t            total_iteration;
    ///total invocation count
    uint64_t            total_invocation;
    ///profiled average memory accesses
    uint64_t            total_memory_access;
    ///profiled number of memory dependences
    uint64_t            dependence_count;
    ///profiled number of frequent memory dependences
    uint64_t            freq_dependence_count;
} LoopInfo;

#ifdef __cplusplus
}
#endif

#endif
