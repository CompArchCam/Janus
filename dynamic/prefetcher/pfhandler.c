#include "pfhandler.h"
#include "code_gen.h"

void memory_prefetch_handler(JANUS_CONTEXT) {
    instr_t *trigger = get_trigger_instruction(bb,rule);
    //PRE_INSERT(bb,trigger,INSTR_CREATE_int1(drcontext));

    JVar mem = decode_jvar(rule);
    opnd_t memo = create_opnd(mem);
    opnd_set_size(&memo, OPSZ_prefetch);

#ifdef JANUS_VERBOSE
    instr_disassemble(drcontext, trigger,STDOUT);
    dr_printf("\n");
    opnd_disassemble(drcontext, memo, STDOUT);
    dr_printf("\n");
#endif
    //insert the prefetch instruction
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_prefetcht0(drcontext, memo));
}


void instr_clone_handler(JANUS_CONTEXT) {
    instr_t *trigger = get_trigger_instruction(bb,rule);
    int mode = rule->ureg0.up;
    int step = rule->ureg0.down;
    int length = rule->ureg1.down;
    //PRE_INSERT(bb,trigger,INSTR_CREATE_int1(drcontext));

    if (mode == 1) {
        instr_t *instr = trigger;
        //find the corresponding instruction to be cloned
        while (step) {
            step--;
            instr = instr_get_next_app(instr);
        }
        //perform the clone
        while (length) {
            length--;
            instr_t *cloned = instr_clone(drcontext, instr);
            instr_set_meta_no_translation(cloned);
            PRE_INSERT(bb, trigger, cloned);
            instr = instr_get_next_app(instr);
        }
    } else {
        dr_printf("Instruction clone mode not yet implemented\n");
    }
}

void instr_update_clone_handler(JANUS_CONTEXT) {
    instr_t *trigger = get_trigger_instruction(bb,rule);
    int mode = rule->ureg0.up;
    uint64_t prefetch_distance = rule->reg1;
    opnd_t op;
    int i, srci = -1;
    //PRE_INSERT(bb,trigger,INSTR_CREATE_int1(drcontext));
    if (mode == 1) {
        instr_t *instr = trigger;
        //perform the clone and update
        instr_t *cloned = instr_clone(drcontext, instr);
        instr_set_meta_no_translation(cloned);
        //update the dynamic operand
        if (instr_reads_memory(cloned)) {
            for (i = 0; i < instr_num_srcs(cloned); i++) {
                op = instr_get_src(cloned, i);
                if (opnd_is_memory_reference(op)) {
                    srci = i;
                    break;
                }
            }
        }

        if (srci != -1) {
            int disp = opnd_get_disp(op);
            disp += prefetch_distance;
            opnd_set_disp(&op, disp);
            instr_set_src(cloned, srci, op);

        }
        PRE_INSERT(bb, trigger, cloned);
    } else {
        dr_printf("Instruction clone mode not yet implemented\n");
    }
}