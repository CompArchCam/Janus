#include "janus_api.h"
#include "vhandler.h"
#include <stdio.h>

void
vector_broadcast_handler_mov_single(JANUS_CONTEXT, instr_t *trigger, opnd_t *dst, opnd_t *src, int opcode);

void
vector_broadcast_handler_mov_double(JANUS_CONTEXT, instr_t *trigger, opnd_t *dst, opnd_t *src, int opcode);

void
vector_broadcast_handler_cvtsi2sd(JANUS_CONTEXT, instr_t *trigger, opnd_t *dst, opnd_t *src, int opcode);