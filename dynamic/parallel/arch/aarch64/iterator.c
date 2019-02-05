#include "iterator.h"
#include "jthread.h"
#include "control.h"
#include "code_gen.h"

/** \brief root handlers for initialise a variable */
static void
emit_init_variable(EMIT_CONTEXT, int tid, JVarProfile *profile);

static void inline
emit_init_induction_variable(EMIT_CONTEXT, int tid, JVarProfile *profile);

static void inline
emit_init_induction_variable_block(EMIT_CONTEXT, int tid, JVarProfile *profile);

static void inline
emit_init_induction_variable_cyclic(EMIT_CONTEXT, int tid, JVarProfile *profile);

static void inline
emit_add_offset_to_all_induction(EMIT_CONTEXT, JVar slice, int tid);

/* Move the initial value of induction variable to private copy (STACK only) */
static void inline
emit_privatise_stack_induction_variables(EMIT_CONTEXT);

static void inline        
emit_restore_stack_induction_variables(EMIT_CONTEXT, int read_tid);

/* Calculate the schedule dynamically */
static void inline
emit_divide_block_iteration(EMIT_CONTEXT, JVar lower, JVar upper, JVar stride);

static void inline
create_janus_var_from_reg(JVar *var, reg_id_t reg);

/* Update the loop boundary for each thread */
static void inline
emit_update_thread_loop_boundary(EMIT_CONTEXT, JVar var, JVar stride, JVar check, int tid);

/** \brief Emit induction variable initializations for variables for a given loop */
void emit_init_loop_variables(EMIT_CONTEXT, int tid)
{
    int i;
    bool has_stack_var = false;

    //if there is any stack variables, load the shared stack first
    for (i=0; i<loop->var_count; i++) {
        if (loop->variables[i].var.type == JVAR_STACK || loop->variables[i].var.type == JVAR_STACKFRAME) {
            has_stack_var = true;
            break;
        }
    }

    /* Load shared stack base into s0 for later use */
    if (has_stack_var) {
        insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s2), (ptr_int_t)&(shared->stack_base));
        INSERT(bb, trigger,
            instr_create_1dst_1src(drcontext,
                                OP_ldr,
                                opnd_create_reg(s0),
                                OPND_CREATE_MEM64(s2, 0)));
    }
    /* Now initialise each variable */
    for (i=0; i<loop->var_count; i++) {
        emit_init_variable(emit_context, tid, loop->variables + i);
    }

}

void
emit_merge_loop_variables(EMIT_CONTEXT)
{
    if (loop->schedule == PARA_DOALL_BLOCK) {
        //currently simply restore the value from the last thread to the first thread
        emit_restore_from_private_register_bank(emit_context, loop->header->registerToMerge, 0, rsched_info.number_of_threads-1);
        emit_restore_stack_induction_variables(emit_context, rsched_info.number_of_threads-1);
    }
}

static void
emit_init_variable(EMIT_CONTEXT, int tid, JVarProfile *profile)
{
    /* Start with induction variables with check conditions */
    if (profile->type == INDUCTION_PROFILE &&
        profile->induction.check.type != JVAR_UNKOWN) {
        emit_init_induction_variable(emit_context, tid, profile);
    }
}

static void inline
emit_init_induction_variable(EMIT_CONTEXT, int tid, JVarProfile *profile)
{
    if (loop->schedule == PARA_DOALL_BLOCK){
        emit_init_induction_variable_block(emit_context, tid, profile);
    }
    else if (loop->schedule == PARA_DOALL_CYCLIC_CHUNK)
        emit_init_induction_variable_cyclic(emit_context, tid, profile);
}

static void inline
emit_init_induction_variable_block(EMIT_CONTEXT, int tid, JVarProfile *profile)
{
    JVar var = profile->var;
    JVarUpdate op = profile->induction.op;
    JVar stride = profile->induction.stride;
    JVar check = profile->induction.check;
    JVar init = profile->induction.init;
    JVar slice_var;

    /* privatise stack variables */
    emit_privatise_stack_induction_variables(emit_context);

    /* case 1: stride, init and check are all immediate constants */
    if (init.type == JVAR_CONSTANT &&
        stride.type == JVAR_CONSTANT &&
        check.type == JVAR_CONSTANT) {
        //then we can simply JIT the value
        int slice;
        int loop_bound;



        if (profile->var.size == 4){
            slice = ((int)check.value - (int)init.value) / (int)stride.value;
            /* get slice per thread */
            slice = slice / rsched_info.number_of_threads;
            loop_bound = (int)init.value + (slice * (int)stride.value * (tid + 1));
        }
        else{
            slice = (check.value - init.value) / stride.value;
            /* get slice per thread */
            slice = slice / rsched_info.number_of_threads;
            loop_bound = init.value + (slice * stride.value * (tid + 1));
        }

        slice_var.type = JVAR_CONSTANT;
        slice_var.value = slice;
        slice_var.size = profile->var.size;

        // Do all of the remainder iterations in the last thread
        if(tid == rsched_info.number_of_threads - 1){
            loop_bound = check.value;
        }
        /* add offset to all variables for this loop */
        emit_add_offset_to_all_induction(emit_context, slice_var, tid);

        insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s2), (ptr_int_t)loop_bound);
        INSERT(bb, trigger,
            instr_create_1dst_1src(drcontext,
                                OP_str,
                                OPND_CREATE_MEM64(TLS, LOCAL_CHECK_OFFSET),
                                opnd_create_reg(s2)));
        return;
    }


    emit_divide_block_iteration(emit_context, var, check, stride);

    slice_var.type = JVAR_REGISTER;
    slice_var.value = s2;
    slice_var.size = profile->var.size;

    /* Add to all induction variables based on the slice count */
    if (tid != 0)
        emit_add_offset_to_all_induction(emit_context, slice_var, tid);

    /* Update the boundary of the current thread */
    //for the last thread, keep the original loop boundary
    if (tid != rsched_info.number_of_threads-1)
       emit_update_thread_loop_boundary(emit_context, var, stride, check, tid);

    else if(check.type == JVAR_CONSTANT){//For last thread, move unmodified check to TLS if check is constant
        insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s2), (ptr_int_t)check.value);
        INSERT(bb, trigger,
            instr_create_1dst_1src(drcontext,
                                OP_str,
                                OPND_CREATE_MEM64(TLS, LOCAL_CHECK_OFFSET),
                                opnd_create_reg(s2)));
    }


}

static void inline
emit_init_induction_variable_cyclic(EMIT_CONTEXT, int tid, JVarProfile *profile)
{
    
}

/* Move the initial value of induction variable to private copy (STACK only) */
static void inline
emit_privatise_stack_induction_variables(EMIT_CONTEXT)
{
    int i;
    opnd_t shared_var;
    opnd_t private_copy;
    for (i=0; i<loop->var_count; i++) {
        JVarProfile profile = loop->variables[i];

        if ((profile.type == INDUCTION_PROFILE || profile.type == COPY_PROFILE) && (profile.var.type == JVAR_STACK || profile.var.type == JVAR_STACKFRAME)) {

            shared_var = create_shared_stack_opnd(profile.var, s0);
            private_copy = create_opnd(profile.var);

            /* Load induction initial value from shared stack */
            INSERT(bb, trigger,
                instr_create_1dst_1src(drcontext,
                                    OP_ldr,
                                    opnd_create_reg(reg_with_same_width(s2, shared_var)),
                                    shared_var));
            /* Store to private copy */
            INSERT(bb, trigger,
                instr_create_1dst_1src(drcontext,
                                    OP_str,
                                    private_copy,
                                    opnd_create_reg(reg_with_same_width(s2, private_copy))));
        }
        else if (profile.type == INDUCTION_PROFILE) {
            if (profile.induction.check.type == JVAR_STACKFRAME) {

                shared_var = create_shared_stack_opnd(profile.induction.check, s0);
                private_copy = create_opnd(profile.induction.check);

                /* Load check value from shared stack */
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_ldr,
                                        opnd_create_reg(reg_with_same_width(s2, shared_var)),
                                        shared_var));
                /* Store to private copy */
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_str,
                                        private_copy,
                                        opnd_create_reg(reg_with_same_width(s2, private_copy))));
            }
        }
    }
}

static void inline
emit_restore_stack_induction_variables(EMIT_CONTEXT, int read_tid)
{
    int i;
    opnd_t shared_var;
    opnd_t private_copy;

    // Load the required thread's stack base pointer into s2
    insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s2), (ptr_int_t)&(oracle[read_tid]->stack_ptr));
    INSERT(bb, trigger,
        instr_create_1dst_1src(drcontext,
                            OP_ldr,
                            opnd_create_reg(s2),
                            OPND_CREATE_MEM64(s2, 0)));

    for (i=0; i<loop->var_count; i++) {
        JVarProfile profile = loop->variables[i];

        if (profile.type == INDUCTION_PROFILE) { 
            if (profile.var.type == JVAR_STACK || profile.var.type == JVAR_STACKFRAME) {

                shared_var = create_shared_stack_opnd(profile.var, s0);
                private_copy = create_shared_stack_opnd(profile.var, s2);

                /* Load induction final value from thread's private stack */
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_ldr,
                                        opnd_create_reg(reg_with_same_width(s3, private_copy)),
                                        private_copy));
                /* Store to shared stack */
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_str,
                                        shared_var,
                                        opnd_create_reg(reg_with_same_width(s3, shared_var))));
            }
            if (profile.induction.check.type == JVAR_STACKFRAME) {

                shared_var = create_shared_stack_opnd(profile.induction.check, s0);
                private_copy = create_shared_stack_opnd(profile.induction.check, s2);

                /* Load check value from thread's private stack */
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_ldr,
                                        opnd_create_reg(reg_with_same_width(s3, private_copy)),
                                        private_copy));
                /* Store to shared stack */
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_str,
                                        shared_var,
                                        opnd_create_reg(reg_with_same_width(s3, shared_var))));
            }
        }
    }
}

static void inline
emit_add_offset_to_all_induction(EMIT_CONTEXT, JVar slice, int tid)
{
    int i;
    int64_t offset;
    JVar offset_var;

    if(slice.type == JVAR_REGISTER) {
        /* Save register slice to spill slot and restore afresh each time 
            (since it's probably a scratch register so its value will probably be stomped) */
        INSERT(bb, trigger,
            instr_create_1dst_1src(drcontext,
                                OP_str,
                                OPND_CREATE_MEM64(TLS, LOCAL_SLOT2_OFFSET),
                                opnd_create_reg(s2)));
    }
    for (i=0; i<loop->var_count; i++) {
        JVarProfile *profile = loop->variables + i;

        if (profile->type == INDUCTION_PROFILE) {
            //s3 = stride * slice * tid
            JVar stride = profile->induction.stride;

            //get operand of the variable
            if (slice.type == JVAR_CONSTANT) {
                //offset for the given thread
                offset = slice.value * tid;
                /* case 1: if the stride is constant */
                if (profile->induction.stride.type == JVAR_CONSTANT) {

                    offset *= profile->induction.stride.value;
                    offset_var.type = JVAR_CONSTANT;
                    offset_var.value = offset;
                }
                /* case 2: if the stride is a register */
                else if (profile->induction.stride.type == JVAR_REGISTER) {

                    /* offset_reg (s3) = stride_reg * slice_imm */

                    /* move offset (slice_immed) into s3 as AArch64 has no immediate multiply */
                    insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s3), (ptr_int_t)offset);
                    /* mul rd, rm, rn 
                        is alias of 
                        madd rd, rm, rn, xzr */
                    INSERT(bb, trigger,
                           instr_create_1dst_3src(drcontext,
                                                OP_madd,
                                                opnd_create_reg(s3),
                                                opnd_create_reg(profile->induction.stride.value),
                                                opnd_create_reg(s3),
                                                opnd_create_reg(DR_REG_XZR)));
                    offset_var.type = JVAR_REGISTER;
                    offset_var.value = s3;
                }
                /* case 3: if the stride is in a local stack */
                else if (profile->induction.stride.type == JVAR_STACK || profile->induction.stride.type == JVAR_STACKFRAME) {
                    opnd_t stride_stk = create_shared_stack_opnd(profile->induction.stride, s0);

                    /* load the stride into s3 */
                    INSERT(bb, trigger,
                           instr_create_1dst_1src(drcontext,
                                               OP_ldr,
                                               opnd_create_reg(reg_with_same_width(s3, stride_stk)),
                                               stride_stk));
                    /* s3 = s3 * offset */
                    /* mov offset into s2 - have assumed s2 is not used */
                    insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s2), (ptr_int_t)offset);
                    INSERT(bb, trigger,
                           instr_create_1dst_3src(drcontext,
                                                OP_madd,
                                                opnd_create_reg(s3),
                                                opnd_create_reg(s3),
                                                opnd_create_reg(s2),
                                                opnd_create_reg(DR_REG_XZR)));
                    offset_var.type = JVAR_REGISTER;
                    offset_var.value = s3;
                }
            } else if (slice.type == JVAR_REGISTER) {

                /* Restore slice afresh from spill slot */
                INSERT(bb, trigger,
                    instr_create_1dst_1src(drcontext,
                                        OP_ldr,
                                        opnd_create_reg(slice.value),
                                        OPND_CREATE_MEM64(TLS, LOCAL_SLOT2_OFFSET)));

                /* case 1: if the stride is constant */
                if (profile->induction.stride.type == JVAR_CONSTANT) {
                    offset = profile->induction.stride.value * tid;

                    insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s3), (ptr_int_t)offset);
                    /* offset_reg = slice_reg * offset */
                    INSERT(bb, trigger,
                           instr_create_1dst_3src(drcontext,
                                                OP_madd,
                                                opnd_create_reg(s3),
                                                opnd_create_reg(slice.value),
                                                opnd_create_reg(s3),
                                                opnd_create_reg(DR_REG_XZR)));
                    offset_var.type = JVAR_REGISTER;
                    offset_var.value = s3;
                }
                /* case 2: if the stride is a register */
                else if (profile->induction.stride.type == JVAR_REGISTER) {
                    /* offset = stride * tid */
                    insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s3), (ptr_int_t)tid);
                    INSERT(bb, trigger,
                            instr_create_1dst_3src(drcontext,
                                                OP_madd,
                                                opnd_create_reg(s3),
                                                opnd_create_reg(profile->induction.stride.value),
                                                opnd_create_reg(s3),
                                                opnd_create_reg(DR_REG_XZR)));
                    /* offset = s3 * slice_reg */
                    INSERT(bb, trigger,
                           instr_create_1dst_3src(drcontext,
                                            OP_madd,
                                            opnd_create_reg(s3),
                                            opnd_create_reg(s3),
                                            opnd_create_reg(slice.value),
                                            opnd_create_reg(DR_REG_XZR)));
                    offset_var.type = JVAR_REGISTER;
                    offset_var.value = s3;
                }
                /* case 3: if the stride is in a local stack */
                else if (profile->induction.stride.type == JVAR_STACK || profile->induction.stride.type == JVAR_STACKFRAME) {
                    opnd_t stride_stk = create_shared_stack_opnd(profile->induction.stride, s0);
                    insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s3), (ptr_int_t)tid);
                    /* offset = slice * tid */
                    INSERT(bb, trigger,
                        instr_create_1dst_3src(drcontext,
                                            OP_madd,
                                            opnd_create_reg(s3),
                                            opnd_create_reg(slice.value),
                                            opnd_create_reg(s3),
                                            opnd_create_reg(DR_REG_XZR)));

                    /* load stride_stk into s2 */
                    INSERT(bb, trigger,
                        instr_create_1dst_1src(drcontext,
                                                OP_ldr,
                                                opnd_create_reg(reg_with_same_width(s2, stride_stk)),
                                                stride_stk));
                    /* offset = offset * stride_stk */
                    INSERT(bb, trigger,
                        instr_create_1dst_3src(drcontext,
                                            OP_madd,
                                            opnd_create_reg(s3),
                                            opnd_create_reg(s3),
                                            opnd_create_reg(s2),
                                            opnd_create_reg(DR_REG_XZR)));

                    offset_var.type = JVAR_REGISTER;
                    offset_var.value = s3;
                }
            }
            //slice variable can't be the stack variable.
            /* Now the calculated offset is in variable offset_var */
            if (profile->induction.op == UPDATE_ADD) {
                //add to the current variable
                emit_add_janus_var(emit_context, profile->var, offset_var, s2, s3);
            } else if (profile->induction.op == UPDATE_SUB) {
                DR_ASSERT("induction sub not implemented");
            }
        }
    }
    if(slice.type == JVAR_REGISTER) {
        /* Restore slice afresh from spill slot (since the following function,
             emit_update_loop_bound, relies on slice in s2) */
        INSERT(bb, trigger,
            instr_create_1dst_1src(drcontext,
                                OP_ldr,
                                opnd_create_reg(s2),
                                OPND_CREATE_MEM64(TLS, LOCAL_SLOT2_OFFSET)));
    }
}


static void inline
create_janus_var_from_reg(JVar *var, reg_id_t reg)
{
    var->type = JVAR_REGISTER;
    var->value = reg;
}

void
emit_divide_block_iteration(EMIT_CONTEXT, JVar lower, JVar upper, JVar stride)
{
    JVar s2_var, s3_var;
    create_janus_var_from_reg(&s2_var, s2);
    create_janus_var_from_reg(&s3_var, s3);

    /* Assume that both s2 and s3 are free */
    //Get stride to s2

    emit_move_janus_var(emit_context, s2_var, stride, s2);
    //Get thread_count to s3
    insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s3), (ptr_int_t)rsched_info.number_of_threads);

    //mul stride, thread_count -> s2
    INSERT(bb, trigger,
        instr_create_1dst_3src(drcontext,
                            OP_madd,
                            opnd_create_reg(s2),
                            opnd_create_reg(s2),
                            opnd_create_reg(s3),
                            opnd_create_reg(DR_REG_XZR)));
    //Spill s2 to slot 1 (not enough regs)

    INSERT(bb, trigger,
        instr_create_1dst_1src(drcontext,
                            OP_str,
                            OPND_CREATE_MEM64(TLS, LOCAL_SLOT1_OFFSET),
                            opnd_create_reg(s2)));

    //Get upper to s2
    emit_move_janus_var(emit_context, s2_var, upper, s2);
    //Get lower to s3
    emit_move_janus_var(emit_context, s3_var, lower, s3);

    //sub upper, lower -> s3
    INSERT(bb, trigger,
        INSTR_CREATE_sub_shift(drcontext,
                        opnd_create_reg(s3),
                        opnd_create_reg(s2),
                        opnd_create_reg(s3),
                        OPND_CREATE_LSL(),
                        OPND_CREATE_INT(0)));

    //Restore s2 from slot 1
    INSERT(bb, trigger,
        instr_create_1dst_1src(drcontext,
                            OP_ldr,
                            opnd_create_reg(s2),
                            OPND_CREATE_MEM64(TLS, LOCAL_SLOT1_OFFSET)));
    //udiv s3, s2 -> s2

    INSERT(bb, trigger,
        instr_create_1dst_2src(drcontext,
                            OP_udiv,
                            opnd_create_reg(s2),
                            opnd_create_reg(s3),
                            opnd_create_reg(s2)));

    INSERT(bb, trigger,
        instr_create_1dst_1src(drcontext,
                            OP_str,
                            OPND_CREATE_MEM64(TLS, LOCAL_BLOCK_SIZE_OFFSET),
                            opnd_create_reg(s2)));
}

static void inline
emit_update_thread_loop_boundary(EMIT_CONTEXT, JVar var, JVar stride, JVar check, int tid)
{
    JVar newCheck;
    if(check.type == JVAR_CONSTANT){ // For constant check, write to check field in TLS
        newCheck.type = JVAR_MEMORY;
        newCheck.base = TLS;
        newCheck.index = DR_REG_NULL;
        newCheck.scale = 0;
        newCheck.value = LOCAL_CHECK_OFFSET;
        newCheck.size = 8;
    }
    else newCheck = check;

    //calculate the stride * slice(s2)
    if (stride.type == JVAR_CONSTANT) {
        insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(s3), (ptr_int_t)stride.value);
        INSERT(bb, trigger,
            instr_create_1dst_3src(drcontext,
                                OP_madd,
                                opnd_create_reg(s2),
                                opnd_create_reg(s2),
                                opnd_create_reg(s3),
                                opnd_create_reg(DR_REG_XZR)));

    } else if (stride.type == JVAR_REGISTER) {
        INSERT(bb, trigger,
            instr_create_1dst_3src(drcontext,
                                OP_madd,
                                opnd_create_reg(s2),
                                opnd_create_reg(s2),
                                opnd_create_reg(stride.value),
                                opnd_create_reg(DR_REG_XZR)));
    } else {
        DR_ASSERT("Stack stride not implemented\n");
    }

    //move the value of induction variable into the boundary variable
    emit_move_janus_var(emit_context, newCheck, var, s3);

    JVar slice;
    slice.type = JVAR_REGISTER;
    slice.value = s2;
    //add the stride to the boundary
    emit_add_janus_var(emit_context, newCheck, slice, s3, s2);
}
