#include "janus_api.h"
#include "VECT_rule_structs.h"
#include "vhandler.h"
#include <stdio.h>

void
vector_init_single_avx(JANUS_CONTEXT, VECT_REDUCE_INIT_rule *info, instr_t *trigger, opnd_t *reductReg, 
        opnd_t *spillVector, opnd_t *freeDoubleReg);

void
vector_init_single_sse(JANUS_CONTEXT, VECT_REDUCE_INIT_rule *info, instr_t *trigger, opnd_t *reductReg,
        opnd_t *spillVector, opnd_t *freeDoubleReg);

void
vector_init_single(JANUS_CONTEXT, VECT_REDUCE_INIT_rule *info, instr_t *trigger, opnd_t *reductReg, 
        opnd_t *spillVector, opnd_t *freeDoubleReg);

void
vector_init_double_avx(JANUS_CONTEXT, VECT_REDUCE_INIT_rule *info, instr_t *trigger, opnd_t *reductReg, 
        opnd_t *spillVector, opnd_t *freeDoubleReg);

void
vector_init_double_sse(JANUS_CONTEXT, VECT_REDUCE_INIT_rule *info, instr_t *trigger, opnd_t *reductReg,
        opnd_t *spillVector, opnd_t *freeDoubleReg);

void vector_init_double(JANUS_CONTEXT, VECT_REDUCE_INIT_rule *info, instr_t *trigger, opnd_t *reductReg, 
        opnd_t *spillVector, opnd_t *freeDoubleReg);