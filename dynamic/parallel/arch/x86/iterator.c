#include "iterator.h"
#include "jthread.h"
#include "control.h"
#include "code_gen.h"

#ifdef JANUS_X86
    #include "janus_x86.h"
#endif

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

/* Move loop upper bound expressions/variables into rax to be divided */
static void inline
emit_prepare_loop_upper_bound_in_rax(EMIT_CONTEXT, JVar bound, JVar stride);

/* Move loop loer bound expressions/variables into rax to be divided */
static void inline
emit_prepare_loop_lower_bound_in_rdx(EMIT_CONTEXT, JVar bound);

/* Move the initial value of induction variable to private copy (STACK only) */
static void inline
emit_privatise_stack_induction_variables(EMIT_CONTEXT);

/* Calculate the schedule dynamically */
static void inline
emit_divide_block_iteration(EMIT_CONTEXT, JVar var, JVar stride, int tid,instr_t *parent_skip);

/* Update the loop boundary for each thread */
static void inline
emit_update_thread_loop_boundary(EMIT_CONTEXT, JVar var, JVar init, JVar stride, JVar check, int tid);

/** \brief Emit induction variable initializations for variables for a given loop */
void emit_init_loop_variables(EMIT_CONTEXT, int tid)
{
    int i;

    /* Load shared stack pointer into s0 for later use */
    if (loop->header->useStack) {
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(s0),
                                opnd_create_rel_addr(&(shared->stack_ptr), OPSZ_8)));
    }

    /* Now initialise each variable */
    for (i=0; i<loop->var_count; i++) {
        emit_init_variable(emit_context, tid, loop->variables + i);
    }
}

void
emit_merge_loop_variables(EMIT_CONTEXT)
{
    instr_t *skip = INSTR_CREATE_label(drcontext);
    /* corner case: main thread does not need to merge in dynamic single threaded mode */
    INSERT(bb, trigger,
        INSTR_CREATE_cmp(drcontext,
                         OPND_CREATE_MEM32(TLS, LOCAL_FLAG_SEQ_OFFSET),
                         OPND_CREATE_INT32(1)));
    PRE_INSERT(bb, trigger,
       INSTR_CREATE_jcc(drcontext, OP_jz, opnd_create_instr(skip)));

    if (loop->schedule == PARA_DOALL_BLOCK) {
        //currently simply restore the value from the last thread to the first thread
        emit_restore_from_private_register_bank(emit_context, loop->header->registerToMerge, 0, rsched_info.number_of_threads-1);
        if (loop->header->registerToConditionalMerge)
            emit_conditional_merge_loop_variables(emit_context, 0);
    }

    PRE_INSERT(bb, trigger, skip);
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
    if (loop->schedule == PARA_DOALL_BLOCK)
        emit_init_induction_variable_block(emit_context, tid, profile);
    else if (loop->schedule == PARA_DOALL_CYCLIC_CHUNK)
        emit_init_induction_variable_cyclic(emit_context, tid, profile);
    else{
            DR_ASSERT_MSG(false, "Unknown loop schedule type in emit_init_induction_variable!\n");
    }
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

    instr_t *skip_label = INSTR_CREATE_label(drcontext);

    /* If the stride, init and check are all immediate constants,
    the constant loop upper bounds per thread have been already updated by modifying the cmp instruction in loop_update_boundary_handler
    So no need to do anything with [TLS, LOCAL_CHECK_OFFSET] */

    if (init.type == JVAR_CONSTANT &&
        stride.type == JVAR_CONSTANT &&
        check.type == JVAR_CONSTANT) {
        //for the main thread, you don't have to do anything
        if (tid == 0) return;
        //then we can simply JIT the value
        int64_t slice = (check.value - init.value) / stride.value;
        /* get slice per thread */
        slice = (slice+1) / rsched_info.number_of_threads;

        slice_var.type = JVAR_CONSTANT;
        slice_var.value = slice;
        slice_var.size = profile->var.size;
        /* add offset to all variables for this loop */
        emit_add_offset_to_all_induction(emit_context, slice_var, tid);
        return;
    }

    /* For the rest of the cases, move upper and lower bound to RAX and RDX
     * We have to use divide operation to get the actual dynamic iteration count
     * the divide instruction requires RAX and RDX to be free */

    if (TLS == DR_REG_RAX || TLS == DR_REG_RDX)
    {
        DR_ASSERT_MSG(false, "Janus Error: Conflict uses of TLS, not yet implemented\n");
        exit(-1);
    }

    /* spill RAX and RDX */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(TLS, LOCAL_SLOT1_OFFSET),
                            opnd_create_reg(DR_REG_RAX)));
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(TLS, LOCAL_SLOT2_OFFSET),
                            opnd_create_reg(DR_REG_RDX)));

    /* privatise stack variables */
    emit_privatise_stack_induction_variables(emit_context);

    /* For the rest of cases, prepare loop bounds for division */
    emit_prepare_loop_upper_bound_in_rax(emit_context, check, stride);
    emit_prepare_loop_lower_bound_in_rdx(emit_context, var);

    /* emit code for block division and multiplication of thread id.
     * Input: RDX <- (runtime start_value, lower bound)
     * Input: RAX <- (runtime end value, upper bound)
     * Output: S2 -> number of iteration per thread */
    emit_divide_block_iteration(emit_context, var, stride, tid, skip_label);

    slice_var.type = JVAR_REGISTER;
    slice_var.value = s2;
    slice_var.size = profile->var.size;

    /* Add to all induction variables based on the slice count */
    if (tid != 0)
        emit_add_offset_to_all_induction(emit_context, slice_var, tid);

    /* Update the boundary of the current thread */
    //for the last thread, keep the original loop boundary
    if (tid != rsched_info.number_of_threads - 1)
        emit_update_thread_loop_boundary(emit_context, var, var, stride, check, tid);

    INSERT(bb, trigger, skip_label);
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
        if (profile.type == INDUCTION_PROFILE && profile.var.type == JVAR_STACK) {
            shared_var = create_shared_stack_opnd(profile.var, s0);
            private_copy = create_opnd(profile.var);
            /* Load induction initial value from shared stack */
            INSERT(bb, trigger,
                INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(reg_with_same_width(DR_REG_RAX, shared_var)),
                                    shared_var));
            /* Store to private copy */
            INSERT(bb, trigger,
                INSTR_CREATE_mov_st(drcontext, private_copy, opnd_create_reg(reg_with_same_width(DR_REG_RAX, private_copy))));

        }
    }
}

static void inline
emit_add_offset_to_all_induction(EMIT_CONTEXT, JVar slice, int tid)
{
    int i;
    int64_t offset;
    JVar offset_var;

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
                    reg_id_t stride_reg = profile->induction.stride.value;

                    if (stride_reg == s0) {
                        INSERT(bb, trigger,
                               INSTR_CREATE_imul_imm(drcontext,
                                                     opnd_create_reg(s3),
                                                     OPND_CREATE_MEM64(TLS, LOCAL_S0_OFFSET),
                                                     OPND_CREATE_INT32(offset)));
                    } else if (stride_reg == s1) {
                        INSERT(bb, trigger,
                               INSTR_CREATE_imul_imm(drcontext,
                                                     opnd_create_reg(s3),
                                                     OPND_CREATE_MEM64(TLS, LOCAL_S1_OFFSET),
                                                     OPND_CREATE_INT32(offset)));
                    } else if (stride_reg == s2) {
                        INSERT(bb, trigger,
                               INSTR_CREATE_imul_imm(drcontext,
                                                     opnd_create_reg(s3),
                                                     OPND_CREATE_MEM64(TLS, LOCAL_S2_OFFSET),
                                                     OPND_CREATE_INT32(offset)));
                    } else if (stride_reg == s3) {
                        INSERT(bb, trigger,
                               INSTR_CREATE_imul_imm(drcontext,
                                                     opnd_create_reg(s3),
                                                     OPND_CREATE_MEM64(TLS, LOCAL_S3_OFFSET),
                                                     OPND_CREATE_INT32(offset)));
                    } else {
                        INSERT(bb, trigger,
                               INSTR_CREATE_imul_imm(drcontext,
                                                     opnd_create_reg(s3),
                                                     opnd_create_reg(stride_reg),
                                                     OPND_CREATE_INT32(offset)));
                    }

                    offset_var.type = JVAR_REGISTER;
                    offset_var.value = s3;
                }
                /* case 3: if the stride is in a local stack */
                else if (profile->induction.stride.type == JVAR_STACK) {
                    /* load the stride into s3 */
                    INSERT(bb, trigger,
                           INSTR_CREATE_mov_ld(drcontext,
                                               opnd_create_reg(s3),
                                               create_shared_stack_opnd(profile->induction.stride, s0)));
                    /* s3 = s3 * offset */
                    INSERT(bb, trigger,
                           INSTR_CREATE_imul_imm(drcontext,
                                                 opnd_create_reg(s3),
                                                 opnd_create_reg(s3),
                                                 OPND_CREATE_INT32(offset)));
                    offset_var.type = JVAR_REGISTER;
                    offset_var.value = s3;
                }
            } else if (slice.type == JVAR_REGISTER) {
                /* case 1: if the stride is constant */
                if (profile->induction.stride.type == JVAR_CONSTANT) {
                    offset = profile->induction.stride.value * tid;
                    /* offset_reg = slice_reg * offset */
                    INSERT(bb, trigger,
                           INSTR_CREATE_imul_imm(drcontext,
                                                 opnd_create_reg(s3),
                                                 opnd_create_reg(slice.value),
                                                 OPND_CREATE_INT32(offset)));
                    offset_var.type = JVAR_REGISTER;
                    offset_var.value = s3;
                }
                /* case 2: if the stride is a register */
                else if (profile->induction.stride.type == JVAR_REGISTER) {
                    /* offset_reg (s3) = stride_reg * slice_imm */
                    reg_id_t stride_reg = profile->induction.stride.value;

                    if (stride_reg == s0) {
                        INSERT(bb, trigger,
                               INSTR_CREATE_imul_imm(drcontext,
                                                     opnd_create_reg(s3),
                                                     OPND_CREATE_MEM64(TLS, LOCAL_S0_OFFSET),
                                                     OPND_CREATE_INT32(tid)));
                    } else if (stride_reg == s1) {
                        INSERT(bb, trigger,
                               INSTR_CREATE_imul_imm(drcontext,
                                                     opnd_create_reg(s3),
                                                     OPND_CREATE_MEM64(TLS, LOCAL_S1_OFFSET),
                                                     OPND_CREATE_INT32(tid)));
                    } else if (stride_reg == s2) {
                        INSERT(bb, trigger,
                               INSTR_CREATE_imul_imm(drcontext,
                                                     opnd_create_reg(s3),
                                                     OPND_CREATE_MEM64(TLS, LOCAL_S2_OFFSET),
                                                     OPND_CREATE_INT32(tid)));
                    } else if (stride_reg == s3) {
                        INSERT(bb, trigger,
                               INSTR_CREATE_imul_imm(drcontext,
                                                     opnd_create_reg(s3),
                                                     OPND_CREATE_MEM64(TLS, LOCAL_S3_OFFSET),
                                                     OPND_CREATE_INT32(tid)));
                    } else {
                        /* offset = stride * tid */
                        INSERT(bb, trigger,
                               INSTR_CREATE_imul_imm(drcontext,
                                                     opnd_create_reg(s3),
                                                     opnd_create_reg(stride_reg),
                                                     OPND_CREATE_INT32(tid)));
                    }

                    /* offset = s2 * slice_reg */
                    INSERT(bb, trigger,
                           INSTR_CREATE_imul(drcontext,
                                            opnd_create_reg(s3),
                                            opnd_create_reg(slice.value)));
                    offset_var.type = JVAR_REGISTER;
                    offset_var.value = s3;
                }
                /* case 3: if the stride is in a local stack */
                else if (profile->induction.stride.type == JVAR_STACK) {
                    /* offset = slice * tid */
                    INSERT(bb, trigger,
                           INSTR_CREATE_imul_imm(drcontext,
                                                 opnd_create_reg(s3),
                                                 opnd_create_reg(slice.value),
                                                 OPND_CREATE_INT32(tid)));
                    /* offset = offset * stride_stk */
                    INSERT(bb, trigger,
                           INSTR_CREATE_imul(drcontext,
                                            opnd_create_reg(s3),
                                            create_shared_stack_opnd(profile->induction.stride, s0)));
                    offset_var.type = JVAR_REGISTER;
                    offset_var.value = s3;
                }
            }
            //slice variable can't be the stack variable.
            /* Now the calculated offset is in variable offset_var */
            if (profile->induction.op == UPDATE_ADD) {
                //add to the current variable
                emit_add_janus_var(emit_context, profile->var, offset_var, s3, DR_REG_NULL);
            }
        }
    }
}

static void inline emit_prepare_loop_upper_bound_in_rax(EMIT_CONTEXT, JVar bound, JVar stride)
{
    opnd_t shared_loop_bound;
    opnd_t private_loop_bound;

    /* Step 1: load and privatise loop bound value */
    if (bound.type == JVAR_STACK) {
        shared_loop_bound = create_shared_stack_opnd(bound, s0);
        private_loop_bound = create_opnd(bound);

        /* Load the upper bound into RAX */
        if (opnd_get_size(shared_loop_bound) == OPSZ_8){
            INSERT(bb, trigger,
                INSTR_CREATE_mov_ld(drcontext,
                                    opnd_create_reg(DR_REG_RAX),
                                    shared_loop_bound));
        }
        else {
            //If the stack loc is not 64 byte size, move with sign extension to accomodat negative numbers
            INSERT(bb, trigger,
                INSTR_CREATE_movsxd(drcontext,
                                    opnd_create_reg(DR_REG_RAX),
                                    shared_loop_bound));
        }

        /* Move to private loop bound */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                private_loop_bound,
                                opnd_create_reg(reg_with_same_width(DR_REG_RAX, private_loop_bound))));

    } else if (bound.type == JVAR_REGISTER) {
        reg_id_t bound_reg = bound.value;
        /* Load loop bound into rax */
        if (bound_reg != DR_REG_RAX) {
            /* bound value is spill in private storage */
            if (bound_reg == s0) {
                INSERT(bb, trigger,
                    INSTR_CREATE_mov_ld(drcontext,
                                        opnd_create_reg(DR_REG_RAX),
                                        OPND_CREATE_MEM64(TLS,0)));
            } else if (bound_reg == s1) {
                INSERT(bb, trigger,
                    INSTR_CREATE_mov_ld(drcontext,
                                        opnd_create_reg(DR_REG_RAX),
                                        OPND_CREATE_MEM64(TLS,8)));
            } else if (bound_reg == s2) {
                INSERT(bb, trigger,
                    INSTR_CREATE_mov_ld(drcontext,
                                        opnd_create_reg(DR_REG_RAX),
                                        OPND_CREATE_MEM64(TLS,16)));
            } else if (bound_reg == s3) {
                INSERT(bb, trigger,
                    INSTR_CREATE_mov_ld(drcontext,
                                        opnd_create_reg(DR_REG_RAX),
                                        OPND_CREATE_MEM64(TLS,24)));
            } else {
                /* Move bound register to rax */
                INSERT(bb, trigger,
                    INSTR_CREATE_mov_ld(drcontext,
                                        opnd_create_reg(DR_REG_RAX),
                                        opnd_create_reg(bound_reg)));
            }
        }
    } else if (bound.type == JVAR_CONSTANT) {
        /* Load the immediate value into RAX */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_imm(drcontext,
                                opnd_create_reg(DR_REG_RAX),
                                    OPND_CREATE_INT64(bound.value+stride.value)));  
                                    //We need bound+stride, because the static analyzer emitted "check" value (for constants) is 
                                    //actually one stride lower than the upper bound we require here

        uint32_t jccOpcode = loop->header->jumpInstructionOpcode;
        if  ((loop->header->jumpingGoesBack && 
            (jccOpcode == X86_INS_JLE || jccOpcode == X86_INS_JBE || jccOpcode == X86_INS_JGE || jccOpcode == X86_INS_JBE)) || 
            (!loop->header->jumpingGoesBack && 
            (jccOpcode == X86_INS_JL || jccOpcode == X86_INS_JB || jccOpcode == X86_INS_JG || jccOpcode == X86_INS_JB))){
            //If the loop has a JLE instruction (and we jump back to loop start),
            //the upper bound needs to be further incremented, because we assume it to be 
            //one stride past the last executed induction variable value
            //(If we have a JG, but we jump to exit the loop, the same logic applies)
            dr_printf("Loop has JLE!\n");
            INSERT(bb, trigger, 
                INSTR_CREATE_add(drcontext,
                                    opnd_create_reg(DR_REG_EAX),
                                    OPND_CREATE_INT32(stride.value)));
        }
    }
    else if (bound.type == JVAR_MEMORY || bound.type == JVAR_ABSOLUTE){
        dr_printf("Memory bound type -- may be bugs!\n");
        shared_loop_bound = create_opnd(bound); //Read the memory location value

        /* Load the upper bound into RAX */
        if (opnd_get_size(shared_loop_bound) == OPSZ_8){
            INSERT(bb, trigger,
                INSTR_CREATE_mov_ld(drcontext,
                                    opnd_create_reg(DR_REG_RAX),
                                    shared_loop_bound));
        }
        else {
            //If the stack loc is not 64 byte size, move with sign extension to accomodate negative numbers
            INSERT(bb, trigger,
                INSTR_CREATE_movsxd(drcontext,
                                    opnd_create_reg(DR_REG_RAX),
                                    shared_loop_bound));
        }
    }
    else
    {
        DR_ASSERT_MSG(false, "Induction variable loop upper bound not implemented");
        exit(-1);
    }
}

static void inline
emit_prepare_loop_lower_bound_in_rdx(EMIT_CONTEXT, JVar bound)
{
    opnd_t lower_bound = create_opnd(bound);

    if (bound.type == JVAR_STACK) {
        /* Load the lower bound into RDX */
        if (opnd_get_size(lower_bound) == OPSZ_8){
            INSERT(bb, trigger,
                INSTR_CREATE_mov_ld(drcontext,
                                    opnd_create_reg(DR_REG_RDX),
                                    lower_bound));
        }
        else {
            //If the stack loc is not 64 byte size, move with sign extension to accomodate negative numbers
            INSERT(bb, trigger,
                INSTR_CREATE_movsxd(drcontext,
                                    opnd_create_reg(DR_REG_RDX),
                                    lower_bound));
        }
    } else if (bound.type == JVAR_REGISTER) {
        reg_id_t bound_reg = bound.value;
        /* bound value is spill in private storage */
        if (bound_reg == s0) {
            INSERT(bb, trigger,
                INSTR_CREATE_mov_ld(drcontext,
                                    opnd_create_reg(DR_REG_RDX),
                                    OPND_CREATE_MEM64(TLS,0)));
        } else if (bound_reg == s1) {
            INSERT(bb, trigger,
                INSTR_CREATE_mov_ld(drcontext,
                                    opnd_create_reg(DR_REG_RDX),
                                    OPND_CREATE_MEM64(TLS,8)));
        } else if (bound_reg == s2) {
            INSERT(bb, trigger,
                INSTR_CREATE_mov_ld(drcontext,
                                    opnd_create_reg(DR_REG_RDX),
                                    OPND_CREATE_MEM64(TLS,16)));
        } else if (bound_reg == s3) {
            INSERT(bb, trigger,
                INSTR_CREATE_mov_ld(drcontext,
                                    opnd_create_reg(DR_REG_RDX),
                                    OPND_CREATE_MEM64(TLS,24)));
        } else if (bound_reg == DR_REG_RAX){
            INSERT(bb, trigger,
                INSTR_CREATE_mov_ld(drcontext,
                                    opnd_create_reg(DR_REG_RDX),
                                    OPND_CREATE_MEM64(TLS, LOCAL_SLOT1_OFFSET)));
        } else {
            /* Move bound register to rax */
            INSERT(bb, trigger,
                INSTR_CREATE_mov_ld(drcontext,
                                    opnd_create_reg(DR_REG_RDX),
                                    opnd_create_reg(bound_reg)));
        }
    } else if (bound.type == JVAR_CONSTANT) {
        /* Load the immediate value into RAX */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_imm(drcontext,
                                opnd_create_reg(DR_REG_RDX),
                                OPND_CREATE_INT64(bound.value)));
    }
    else
    {
        DR_ASSERT_MSG(false, "Induction variable loop lower bound not implemented");
        exit(-1);
    }
}

void
emit_divide_block_iteration(EMIT_CONTEXT, JVar var, JVar stride, int tid, instr_t *parent_skip)
{
    instr_t *low_slice_label = INSTR_CREATE_label(drcontext);
    instr_t *skip_label = INSTR_CREATE_label(drcontext);
    /* pick the div_reg to store the quotient */
    reg_id_t div_reg = s2;
    //corner case flag
    bool redirect_div_reg = false;

    /* Step 1: pick divide register */
    /* div reg should not be RAX nor RDX */
    if (div_reg == DR_REG_RAX) {
        //if s2 is rax, then use s3
        if (s3 == DR_REG_RDX) {
            //if s3 is occupied, then use rdi
            INSERT(bb, trigger,
                INSTR_CREATE_mov_st(drcontext,
                                    OPND_CREATE_MEM64(TLS, LOCAL_SLOT3_OFFSET),
                                    opnd_create_reg(DR_REG_RDI)));
            div_reg = DR_REG_RDI;
            redirect_div_reg = true;
        } else
            div_reg = s3;
    } else if (div_reg == DR_REG_RDX) {
        //if s2 is rdx, then use s3
        if (s3 == DR_REG_RAX) {
            //if s3 is rax, then use rdi
            INSERT(bb, trigger,
                INSTR_CREATE_mov_st(drcontext,
                                    OPND_CREATE_MEM64(TLS, LOCAL_SLOT3_OFFSET),
                                    opnd_create_reg(DR_REG_RDI)));
            div_reg = DR_REG_RDI;
            redirect_div_reg = true;
        } else
            div_reg = s3;
    }
    div_reg = reg_64_to_32(div_reg);

    /* Step 2: perform division */
    /* slice = (upper(RAX) - lower(RDX)) / (thread_count * stride)
     * if stride is constant, then we can generate a quick division 
     * if stride is non-constant, we use proper division */

    /* rax = rax - rdx; get total blocks */
    INSERT(bb, trigger,
        INSTR_CREATE_sub(drcontext,
                         opnd_create_reg(DR_REG_RAX),
                         opnd_create_reg(DR_REG_RDX)));

    /* Corner case: if total_blocks < nums_threads
     * where not enough threads could be scheduled
     * all parallelising threads need to jump back to thread pool
     * only the main thread proceeds */

    if (stride.type != JVAR_CONSTANT){
        DR_ASSERT_MSG(false, "Error: non constant stride not supported in emit_divide_block_iteration");
    }
    INSERT(bb, trigger,
        INSTR_CREATE_cmp(drcontext,
                         opnd_create_reg(DR_REG_RAX),
                            OPND_CREATE_INT32(rsched_info.number_of_threads*stride.value)));
    INSERT(bb, trigger,
        INSTR_CREATE_jcc(drcontext, OP_jge,
                         opnd_create_instr(skip_label)));

    if (tid) {
        //INSERT(bb, trigger,
        //    INSTR_CREATE_jmp_ind(drcontext, opnd_create_rel_addr(&(oracle[tid]->gen_code[loop->dynamic_id].thread_loop_finish), OPSZ_8)));
        //since it is in application mode, we can direct jump
        /* For other thread, simply jump back to thread loop finish */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(DR_REG_RAX),
                                OPND_CREATE_MEM64(TLS, LOCAL_SLOT1_OFFSET)));
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(DR_REG_RDX),
                                OPND_CREATE_MEM64(TLS, LOCAL_SLOT2_OFFSET)));

        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(DR_REG_RDI),
                                opnd_create_reg(s1)));
        INSERT(bb, trigger,
            INSTR_CREATE_jmp(drcontext, opnd_create_pc((void *)janus_reenter_thread_pool_app)));
    } else if (rsched_info.number_of_threads > 1) {
        /* For the main thread, simply recover and skip to the end
         * However we need to mark flag that we are in the special mode */
        //PRE_INSERT(bb,trigger,INSTR_CREATE_int1(drcontext));
        //we still need to update boundary in case LOOP_UPDATE_BOUND rule
        /* rax = rax + rdx; recover upper bound */
        INSERT(bb, trigger,
            INSTR_CREATE_add(drcontext,
                             opnd_create_reg(DR_REG_RAX),
                             opnd_create_reg(DR_REG_RDX)));

        uint32_t jccOpcode = loop->header->jumpInstructionOpcode;
        if  ((loop->header->jumpingGoesBack && 
            (jccOpcode == X86_INS_JLE || jccOpcode == X86_INS_JBE || jccOpcode == X86_INS_JGE || jccOpcode == X86_INS_JBE)) || 
            (!loop->header->jumpingGoesBack && 
            (jccOpcode == X86_INS_JL || jccOpcode == X86_INS_JB || jccOpcode == X86_INS_JG || jccOpcode == X86_INS_JB))){
            //If our loop has a JLE instruction, RAX currently stores the value of one stride past what actually gets executed
            //(This is because we use that to compute number of iterations)
            //However, for JLE the check value must be exactly the last value with which the loop gets run
            //So we subtract one stride
            //(If we have a JG, a jump means we exit the loop, so same logic applies)
            INSERT(bb, trigger, 
                INSTR_CREATE_sub(drcontext,
                                    opnd_create_reg(DR_REG_RAX),
                                    OPND_CREATE_INT32(stride.value)));
        }
        /* save [TLS, LOCAL_CHECK] */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                OPND_CREATE_MEM64(TLS, LOCAL_CHECK_OFFSET),
                                opnd_create_reg(DR_REG_RAX)));

        //set local->sequential_execution = 1;
        INSERT(bb, trigger,
            INSTR_CREATE_xor(drcontext,
                             opnd_create_reg(DR_REG_RAX),
                             opnd_create_reg(DR_REG_RAX)));
        INSERT(bb, trigger,
            INSTR_CREATE_inc(drcontext,
                             opnd_create_reg(DR_REG_RAX)));
        /* save [TLS, LOCAL_EXIT] */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_st(drcontext,
                                OPND_CREATE_MEM64(TLS, LOCAL_FLAG_SEQ_OFFSET),
                                opnd_create_reg(DR_REG_RAX)));

        /* recover rax rdx */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(DR_REG_RAX),
                                OPND_CREATE_MEM64(TLS, LOCAL_SLOT1_OFFSET)));
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(DR_REG_RDX),
                                OPND_CREATE_MEM64(TLS, LOCAL_SLOT2_OFFSET)));

        //for main thread, simply skip to the parent
        INSERT(bb, trigger,
            INSTR_CREATE_jmp(drcontext, opnd_create_instr(parent_skip)));
    }

    /* Corner case: skip the corner case */
    INSERT(bb, trigger, skip_label);

    /* clear rdx */
    INSERT(bb, trigger,
       INSTR_CREATE_xor(drcontext,
                        opnd_create_reg(DR_REG_RDX),
                        opnd_create_reg(DR_REG_RDX)));

    if (stride.type == JVAR_CONSTANT) {
        INSERT(bb, trigger,
                    INSTR_CREATE_mov_imm(drcontext,
                                         opnd_create_reg(div_reg),
                                         OPND_CREATE_INT32(stride.value * rsched_info.number_of_threads)));
    } else if (stride.type == JVAR_REGISTER) {
        reg_id_t stride_reg = stride.value;
        if (div_reg != stride_reg) {
            //load stride reg into div reg
            if (stride_reg != DR_REG_RDX)
                INSERT(bb, trigger,
                            INSTR_CREATE_mov_ld(drcontext,
                                                opnd_create_reg(div_reg),
                                                opnd_create_reg(reg_with_same_width(stride_reg, opnd_create_reg(div_reg)))));
            else
                //corner case: load rdx from spill slot
                dr_restore_reg(drcontext, bb, trigger, reg_32_to_64(div_reg), SPILL_SLOT_3);
        }

    } else if (stride.type == JVAR_STACK) {
        DR_ASSERT("Induction variable stack stride not implemented");
    }

    /* rax = rax / div_reg */
    INSERT(bb, trigger,
                   INSTR_CREATE_div_4(drcontext,
                                      opnd_create_reg(div_reg)));
    /* clear remainder */
    INSERT(bb, trigger,
       INSTR_CREATE_xor(drcontext,
                        opnd_create_reg(DR_REG_RDX),
                        opnd_create_reg(DR_REG_RDX)));

    /* rax = rax / thread_count */
    if (rsched_info.number_of_threads != 1 && stride.type != JVAR_CONSTANT) {
        INSERT(bb, trigger,
            INSTR_CREATE_mov_imm(drcontext,
                                 opnd_create_reg(div_reg),
                                 OPND_CREATE_INT32(rsched_info.number_of_threads)));
        INSERT(bb, trigger,
               INSTR_CREATE_div_4(drcontext,
                                  opnd_create_reg(div_reg)));
    }

    if (redirect_div_reg) {
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(DR_REG_RDI),
                                OPND_CREATE_MEM64(TLS, LOCAL_SLOT3_OFFSET)));
    }
    INSERT(bb, trigger,
        INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(DR_REG_RDX),
                            OPND_CREATE_MEM64(TLS, LOCAL_SLOT2_OFFSET)));

    /* Now results are in rax, move to s2 */
    if (s2 != DR_REG_RAX) {
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(s2),
                                opnd_create_reg(DR_REG_RAX)));
        /* Restore rax */
        INSERT(bb, trigger,
            INSTR_CREATE_mov_ld(drcontext,
                                opnd_create_reg(DR_REG_RAX),
                                OPND_CREATE_MEM64(TLS, LOCAL_SLOT1_OFFSET)));
    }

    /* save the results into local.block */
    INSERT(bb, trigger,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(TLS, LOCAL_BLOCK_SIZE_OFFSET),
                            opnd_create_reg(s2)));
}

static void inline
emit_update_thread_loop_boundary(EMIT_CONTEXT, JVar var, JVar init, JVar stride, JVar check, int tid)
{
    //Loop slice should be stored in s2 before calling this
    //move the value of induction variable "var" to the check register
    if (check.type == JVAR_REGISTER) {
        //we need to avoid the case where check register is a scratch register
        JVar replace;
        replace.type = JVAR_MEMORY;
        replace.base = s1;
        replace.index = DR_REG_NULL;
        replace.scale = 1;
        replace.size = check.size;
        if (check.value == s0){
            replace.value = LOCAL_S0_OFFSET;
            emit_move_janus_var(emit_context, replace, var, s3);
        }
        else if (check.value == s1)
        {
            replace.value = LOCAL_S1_OFFSET;
            emit_move_janus_var(emit_context, replace, var, s3);
        } else if (check.type == JVAR_REGISTER && check.value == s2) {
            replace.value = LOCAL_S2_OFFSET;
            emit_move_janus_var(emit_context, replace, var, s3);
        } else if (check.type == JVAR_REGISTER && check.value == s3) {
            replace.value = LOCAL_S3_OFFSET;
            emit_move_janus_var(emit_context, replace, var, s3);
        } else
            emit_move_janus_var(emit_context, check, var, s3);
    }
    else if (check.type == JVAR_CONSTANT || check.type == JVAR_MEMORY || check.type == JVAR_ABSOLUTE)
    {
        //if check is a fixed constant or a memory location, we can move the induction value to the [TLS, LOCAL_CHECK_OFFSET]
        //Because the PARA_LOOP_UPDATE_BOUNDS rule has changed the cmp instruction to use [TLS, LOCAL_CHECK_OFFSET] instead
        JVar replace;
        replace.type = JVAR_MEMORY;
        replace.base = s1;
        replace.index = DR_REG_NULL;
        replace.scale = 1;
        replace.size = var.size;
        replace.value = LOCAL_CHECK_OFFSET;
        emit_move_janus_var(emit_context, replace, var, s3);
    }
    else if (check.type == JVAR_STACK)
    {
        //for stacks, simply move
        emit_move_janus_var(emit_context, check, var, s3);
    }
    else{
        DR_ASSERT_MSG(false, "Unrecognized check variable type in emit_update_thread_loop_boundary\n");
    }

    //calculate the stride * slice(s2)
    if (stride.type == JVAR_CONSTANT) {
        INSERT(bb, trigger,
               INSTR_CREATE_imul_imm(drcontext,
                                     opnd_create_reg(s2),
                                     opnd_create_reg(s2),
                                     OPND_CREATE_INT32(stride.value)));
        uint32_t jccOpcode = loop->header->jumpInstructionOpcode;
        if  ((loop->header->jumpingGoesBack && 
            (jccOpcode == X86_INS_JLE || jccOpcode == X86_INS_JBE || jccOpcode == X86_INS_JGE || jccOpcode == X86_INS_JBE)) || 
            (!loop->header->jumpingGoesBack && 
            (jccOpcode == X86_INS_JL || jccOpcode == X86_INS_JB || jccOpcode == X86_INS_JG || jccOpcode == X86_INS_JB))){
            //If our loop has a JLE (as opposed to a JNE) instruction, we must increase the check value 
            //by one stride less for each thread
            //Same logic applies for a JG
            INSERT(bb, trigger, 
                INSTR_CREATE_sub(drcontext,
                                        opnd_create_reg(s2),
                                        OPND_CREATE_INT32(stride.value)));
        }
                                     
    }
    else if (stride.type == JVAR_REGISTER)
    {
        INSERT(bb, trigger,
               INSTR_CREATE_imul(drcontext,
                                 opnd_create_reg(s2),
                                 OPND_CREATE_INT32(stride.value)));
    }
    else
    {
        DR_ASSERT_MSG(false, "Stack stride not implemented\n");
    }

    JVar slice;
    slice.type = JVAR_REGISTER;
    slice.value = s2;
    //add the stride to the boundary
    if (check.type == JVAR_REGISTER) {
        //we need to avoid the case where check register is a scratch register
        JVar replace;
        replace.type = JVAR_MEMORY;
        replace.base = s1;
        replace.index = DR_REG_NULL;
        replace.scale = 1;
        replace.size = check.size;
        if (check.value == s0){
            replace.value = LOCAL_S0_OFFSET;
            emit_add_janus_var(emit_context, replace, slice, s3, DR_REG_NULL);
        }
        else if (check.value == s1)
        {
            replace.value = LOCAL_S1_OFFSET;
            emit_add_janus_var(emit_context, replace, slice, s3, DR_REG_NULL);
        } else if (check.type == JVAR_REGISTER && check.value == s2) {
            replace.value = LOCAL_S2_OFFSET;
            emit_add_janus_var(emit_context, replace, slice, s3, DR_REG_NULL);
        } else if (check.type == JVAR_REGISTER && check.value == s3) {
            replace.value = LOCAL_S3_OFFSET;
            emit_add_janus_var(emit_context, replace, slice, s3, DR_REG_NULL);
        } else
            emit_add_janus_var(emit_context, check, slice, s3, DR_REG_NULL);
    }
    else if (check.type == JVAR_CONSTANT || check.type == JVAR_MEMORY || check.type == JVAR_ABSOLUTE)
    {
        //if check is a fixed constant or memory location, we can now add one slice to the induction variable value in [TLS, LOCAL_CHECK_OFFSET]
        JVar replace;
        replace.type = JVAR_MEMORY;
        replace.base = s1;
        replace.index = DR_REG_NULL;
        replace.scale = 1;
        replace.size = var.size;
        replace.value = LOCAL_CHECK_OFFSET;
        emit_add_janus_var(emit_context, replace, slice, s3, DR_REG_NULL);
    }
    else if (check.type == JVAR_STACK)
    {
        //for stacks, simply move
        emit_add_janus_var(emit_context, check, slice, s3, DR_REG_NULL);
    }
    else{
        DR_ASSERT_MSG(false, "Unrecognized check variable type in emit_update_thread_loop_boundary\n");
    }
}
