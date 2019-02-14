#include "rcheck.h"
#include "janus_api.h"
#include "control.h"
#include "emit.h"

static void
dynamic_array_bound_check(uint64_t base1, uint64_t range1, uint64_t base2, uint64_t range2)
{
#ifdef JANUS_VERBOSE
    printf("base1 %lx range1 %lx, base2 %lx range2 %lx\n",base1,range1,base2,range2);
#endif
    if ((base1 < base2 && range1 < base2-base1) ||
        (base1 > base2 && range2 < base1-base2)) {
        shared->runtime_check_fail = 0;
        return;
    }

    printf("runtime check fails!\n");
    shared->runtime_check_fail = 1;
}

void
loop_array_bound_record_handler(JANUS_CONTEXT)
{
    int s0,s1,s2,s3;
    int varIndex = (int)rule->reg0;
    //retrieve the current loop (loop id stored in rule reg0)
    loop_t *loop = &(shared->loops[rule->reg1]);
    JVarProfile profile = loop->variables[varIndex];
    if (profile.type != ARRAY_PROFILE) return;

    instr_t *trigger = get_trigger_instruction(bb,rule);
    //PRE_INSERT(bb,trigger,INSTR_CREATE_int1(drcontext));

    JVar bound = profile.array.base;

    if (bound.type == JVAR_REGISTER) {
        PRE_INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                opnd_create_rel_addr(&(loop->variables[varIndex].array.runtime_base_value), OPSZ_8),
                                opnd_create_reg(bound.value)));
    } else {
        dr_printf("Runtime check case not yet handled\n");
    }
}

void
loop_array_bound_check_handler(JANUS_CONTEXT)
{
    /* The loop array bound check is pairwise
     * We take the input of two array bases and its recurrence form and
     * perform runtime verification whether two ranges overlap
     * If check fails, we flush the code cache and re-run the code sequentially
    */
    int varIndex1 = (int)rule->ureg0.up;
    int varIndex2 = (int)rule->ureg0.down;
    int s0,s1,s2,s3;
    if (varIndex1 == -1 || varIndex2 == -1) return;

    //retrieve loop profile
    loop_t *loop = &(shared->loops[rule->reg1]);

    JVarProfile profile1 = loop->variables[varIndex1];
    JVarProfile profile2 = loop->variables[varIndex2];

    if (profile1.type != ARRAY_PROFILE || profile2.type != ARRAY_PROFILE) return;

    instr_t *trigger = get_trigger_instruction(bb,rule);

#ifdef SAFE_RUNTIME_CHECK
#ifdef SLOW_RUNTIME_CHECK
    /* Insert the runtime check function call */
    dr_insert_clean_call(drcontext, bb, trigger,
                     dynamic_array_bound_check, false, 4,
                     opnd_create_rel_addr(&(loop->variables[varIndex1].array.runtime_base_value), OPSZ_8),
                     create_opnd(profile1.array.max_range),
                     opnd_create_rel_addr(&(loop->variables[varIndex2].array.runtime_base_value), OPSZ_8),
                     create_opnd(profile2.array.max_range));
#else
    dr_save_reg(drcontext, bb, trigger, DR_REG_RAX, SPILL_SLOT_1);
    dr_save_reg(drcontext, bb, trigger, DR_REG_RDX, SPILL_SLOT_2);
    dr_save_reg(drcontext, bb, trigger, DR_REG_RCX, SPILL_SLOT_3);
    //PRE_INSERT(bb,trigger,INSTR_CREATE_int1(drcontext));
    /* add max_range to base */
    emit_move_janus_var(emit_context, reg_to_JVar(DR_REG_RAX), profile1.array.base, DR_REG_RCX);
    emit_move_janus_var(emit_context, reg_to_JVar(DR_REG_RDX), profile2.array.base, DR_REG_RCX);
    emit_add_janus_var(emit_context, reg_to_JVar(DR_REG_RAX), profile1.array.max_range, DR_REG_RCX, DR_REG_RCX);
    emit_add_janus_var(emit_context, reg_to_JVar(DR_REG_RDX), profile2.array.max_range, DR_REG_RCX, DR_REG_RCX);
    dr_restore_reg(drcontext, bb, trigger, DR_REG_RAX, SPILL_SLOT_1);
    dr_restore_reg(drcontext, bb, trigger, DR_REG_RDX, SPILL_SLOT_2);
    dr_restore_reg(drcontext, bb, trigger, DR_REG_RCX, SPILL_SLOT_3);
#endif
#endif
}