#include "janus_api.h"
#include "VECT_rule_structs.h"
#include "vhandler.h"
#include <stdio.h>

void
adjust_operand_for_extension(VECT_CONVERT_rule *info, opnd_t *opnd, int unrollFactor);

void
vector_extend_transform_2opnds(JANUS_CONTEXT, instr_t *trigger, int opcode, opnd_t *dst, opnd_t *src);

void
vector_extend_transform_3opnds(JANUS_CONTEXT, instr_t *trigger, int opcode, opnd_t *dst, opnd_t *src1, 
        opnd_t *src2, opnd_t *spill, int vectorWordSize);

void
save_clobbered_register(JANUS_CONTEXT, instr_t *trigger, opnd_t *dst, opnd_t *src1, opnd_t *src2, 
        int vectorWordSize, int unrollFactor, opnd_t *spill);

void
vector_extend_handler_arith_unaligned(JANUS_CONTEXT, VECT_CONVERT_rule *info, instr_t *trigger, int opcode, 
        opnd_t *dst, opnd_t *src1, opnd_t *src2, int vectorWordSize, int unrollFactor, opnd_t *spill);

void
vector_extend_handler_arith(JANUS_CONTEXT, VECT_CONVERT_rule *info, instr_t *trigger, int opcode,
        opnd_t *dst, opnd_t *src1, int vectorWordSize, int unrollFactor);

void
vector_extend_handler_mov(JANUS_CONTEXT, VECT_CONVERT_rule *info, instr_t *trigger, opnd_t *dst, 
        opnd_t *src1, int vectorWordSize, int unrollFactor);

void
vector_extend_handler_pxor(JANUS_CONTEXT, instr_t *trigger, int opcode, opnd_t *dst, opnd_t *src1,
        int vectorSize);