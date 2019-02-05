#include "janus_api.h"
#include "VECT_rule_structs.h"
#include "vhandler.h"
#include <stdio.h>

instr_t *
get_movap(void *dc, opnd_t *d, opnd_t *s, int vectorWordSize);

instr_t *
get_movup(void *dc, opnd_t *d, opnd_t *s, int vectorWordSize);

void 
set_source_size(instr_t *instr, opnd_size_t size, int i);

void
set_dest_size(instr_t *instr, opnd_size_t size, int i);

void
set_all_opnd_size(instr_t *instr, opnd_size_t size);

instrlist_t *
get_loop_body(JANUS_CONTEXT, instr_t *trigger, uint64_t startPC);

void
insert_list_after(JANUS_CONTEXT, instrlist_t *body, instr_t *trigger);

void
insert_list_after(JANUS_CONTEXT, instrlist_t *body, instr_t *trigger, int copies);

void
insert_loop_after(JANUS_CONTEXT, VECT_LOOP_PEEL_rule *info, instrlist_t *body, 
    instr_t *trigger, uint64_t iterations);
