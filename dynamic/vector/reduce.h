#include "janus_api.h"
#include "VECT_rule_structs.h"
#include "vhandler.h"
#include <stdio.h>

void 
reduce_multiply_single(JANUS_CONTEXT, VECT_REDUCE_AFTER_rule *info, instr_t *trigger, 
        opnd_t *reductReg, opnd_t *freeVectReg);

void
reduce_multiply_double(JANUS_CONTEXT, VECT_REDUCE_AFTER_rule *info, instr_t *trigger,
        opnd_t *reductReg, opnd_t *freeVectReg);

void
reduce_multiply(JANUS_CONTEXT, VECT_REDUCE_AFTER_rule *info, instr_t *trigger, opnd_t *reductReg,
        opnd_t *freeVectReg);

void
reduce_add(JANUS_CONTEXT, VECT_REDUCE_AFTER_rule *info, instr_t *trigger, 
        opnd_t *reductReg, opnd_t *freeVectReg);