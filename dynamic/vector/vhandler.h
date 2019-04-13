#ifndef __JANUS_VECTOR_HANDLER__
#define __JANUS_VECTOR_HANDLER__

#include "janus_api.h"

typedef enum _hardware
{
	NOT_RECOGNISED_CPU,
	SSE_SUPPORT,
	AVX_SUPPORT,
	AVX2_SUPPORT
} GHardware;

#ifdef __cplusplus
extern "C" {
#endif

GHardware
janus_detect_hardware();

void
vector_extend_handler(JANUS_CONTEXT);

void
vector_broadcast_handler(JANUS_CONTEXT);

void
vector_loop_unroll_handler(JANUS_CONTEXT);

void
vector_loop_unaligned_handler(JANUS_CONTEXT);

void
vector_reduce_handler(JANUS_CONTEXT);

void
vector_init_handler(JANUS_CONTEXT);

void
vector_reg_check_handler(JANUS_CONTEXT);

void
vector_revert_handler(JANUS_CONTEXT);

void
vector_induction_update_handler(JANUS_CONTEXT);

void
vector_induction_recover_handler(JANUS_CONTEXT);

void
vector_loop_peel_handler(JANUS_CONTEXT);

void
vector_loop_init(JANUS_CONTEXT);

void
vector_loop_finish(JANUS_CONTEXT);

extern GHardware myCPU;

#ifdef __cplusplus
}
#endif
#endif