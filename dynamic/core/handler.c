//  JANUS Handlers are operative functions that modifies code
//  All handlers here are generic to modification not specifically to parallelisation
//  Created by Kevin Zhou on 17/12/2014.
//
#include "handler.h"

/* Global JANUS shared information */
RSchedInfo rsched_info;

//private code caches
static CCache call_func_code_cache;

//storage to buffer all code 

/* -------------------------------------------------------------------------------
 *                              Dynamic Code Caches
 * ------------------------------------------------------------------------------*/
//register your code cache compiler in this function
void 
register_code_cache_compiler(void (*func)())
{

}

//all compile all registered code caches
void 
compile_all_code_caches()
{

}

/* -------------------------------------------------------------------------------
 *                              Handler Utilities
 * ------------------------------------------------------------------------------*/

//return the instr_t structure specified by the static rules
instr_t *get_trigger_instruction(instrlist_t *bb, RRule *rule)
{
    instr_t *trigger = instrlist_first_app(bb);
    app_pc pc = (app_pc)rule->pc;
    
    /* Find the trigger instruction with specified pc. */
    while (instr_get_app_pc(trigger) != pc) {
        trigger = instr_get_next_app(trigger);
        if(trigger==NULL) {
            printf("In block %lx:\n",rule->block_address);
            printf("JANUS couldn't find the specified address %lx at runtime!\n",(PCAddress)pc);
            instrlist_disassemble(NULL, 0, bb, STDOUT);
            printf("\n");
            return NULL;
        }
    }
    return trigger;
}

void load_effective_address(void *drcontext, instrlist_t *bb, instr_t *trigger, opnd_t memref, reg_id_t scratch1, reg_id_t scratch2)
{
#ifdef JANUS_X86
    if (opnd_is_far_base_disp(memref) &&
        opnd_get_segment(memref) != DR_SEG_ES &&
        opnd_get_segment(memref) != DR_SEG_DS) {
        /* get segment base into scratch, then add to memref base and lea */
        instr_t *near_in_dst = NULL;
        if (opnd_uses_reg(memref, scratch2) ||
            (opnd_get_base(memref) != DR_REG_NULL &&
             opnd_get_index(memref) != DR_REG_NULL)) {
                /* have to take two steps */
                opnd_set_size(&memref, OPSZ_lea);
                near_in_dst = INSTR_CREATE_lea(drcontext, opnd_create_reg(scratch1), memref);
                PRE_INSERT(bb, trigger, near_in_dst);
            }
        if (!dr_insert_get_seg_base(drcontext, bb, trigger, opnd_get_segment(memref),
                                    scratch2)) {
            if (near_in_dst != NULL) {
                instrlist_remove(bb, near_in_dst);
                instr_destroy(drcontext, near_in_dst);
            }
            return;
        }
        if (near_in_dst != NULL) {
            PRE_INSERT(bb, trigger,
                       INSTR_CREATE_lea(drcontext, opnd_create_reg(scratch1),
                                        opnd_create_base_disp(scratch1, scratch2, 1, 0, OPSZ_lea)));
        } else {
            reg_id_t base = opnd_get_base(memref);
            reg_id_t index = opnd_get_index(memref);
            int scale = opnd_get_scale(memref);
            int disp = opnd_get_disp(memref);
            if (opnd_get_base(memref) == DR_REG_NULL) {
                base = scratch2;
            } else if (opnd_get_index(memref) == DR_REG_NULL) {
                index = scratch2;
                scale = 1;
            } else {
                //DR_ASSERT(false, "memaddr internal error");
            }
            PRE_INSERT(bb, trigger,
                       INSTR_CREATE_lea(drcontext, opnd_create_reg(scratch1),
                                        opnd_create_base_disp(base, index, scale, disp,
                                                              OPSZ_lea)));
        }
    } else if (opnd_is_base_disp(memref)) {
        /* special handling for xlat instr, [%ebx,%al]
         * - save %eax
         * - movzx %al => %eax
         * - lea [%ebx, %eax] => dst
         * - restore %eax
         */
        byte is_xlat = 0;
        if (opnd_get_index(memref) == DR_REG_AL) {
            is_xlat = 1;
            if (scratch2 != DR_REG_XAX && scratch1 != DR_REG_XAX) {
                /* we do not have to save xax if it is saved by caller */
                PRE_INSERT(bb, trigger,
                           INSTR_CREATE_mov_ld(drcontext,
                                               opnd_create_reg(scratch2),
                                               opnd_create_reg(DR_REG_XAX)));
            }
            PRE_INSERT(bb, trigger,
                       INSTR_CREATE_movzx(drcontext, opnd_create_reg(DR_REG_XAX),
                                          opnd_create_reg(DR_REG_AL)));
            memref = opnd_create_base_disp(DR_REG_XBX, DR_REG_XAX, 1, 0,
                                           OPSZ_lea);
        }
        /* lea [ref] => reg */
        opnd_set_size(&memref, OPSZ_lea);
        PRE_INSERT(bb, trigger,
                   INSTR_CREATE_lea(drcontext, opnd_create_reg(scratch1), memref));
        if (is_xlat && scratch2 != DR_REG_XAX && scratch1 != DR_REG_XAX) {
            PRE_INSERT(bb, trigger,
                       INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(DR_REG_XAX),
                                           opnd_create_reg(scratch2)));
        }
    } else if (IF_X64(opnd_is_rel_addr(memref) ||) opnd_is_abs_addr(memref)) {
        /* mov addr => reg */
        PRE_INSERT(bb, trigger,
                   INSTR_CREATE_mov_imm(drcontext, opnd_create_reg(scratch1),
                                        OPND_CREATE_INTPTR(opnd_get_addr(memref))));
    } else {
        /* unhandled memory reference */
        return;
    }
#endif
}

/**
 * Jump to the given code cache pc, putting the address to jump back to in
 * a register.  Inserts the code after the appPrev instruction and before the
 * appNext instruction, with the translation set to the address of appPrev.
 * Note that this handler has to use private stack
 */
void
call_code_cache_handler(void *drcontext, instrlist_t *bb,
                        instr_t *trigger, CCache code_cache)
{
#ifdef JANUS_X86
    PRE_INSERT(bb,trigger,INSTR_CREATE_call(drcontext, opnd_create_pc(code_cache)));
#endif
}

/**
 * Jump to the given code cache pc, putting the address to jump back to in
 * a register.  Inserts the code after the appPrev instruction and before the
 * appNext instruction, with the translation set to the address of appPrev.
 */
void
jump_to_code_cache(void *drcontext, void *tag, instrlist_t *bb,
                   instr_t *appPrev, instr_t *appNext, CCache code_cache)
{
#ifdef JANUS_X86
    instr_t *instr;
    app_pc translation;

    /* The translation for the instructions. */
    translation = instr_get_app_pc(appPrev);

    /* Put the address to jump back to in a register. */
    instr = INSTR_CREATE_mov_imm(drcontext, opnd_create_reg(DR_REG_R14),
                               OPND_CREATE_INTPTR(instr_get_app_pc(appNext)));
    PRE_INSERT(bb,appNext,instr);

    /* Create the jump. */
    instr = INSTR_CREATE_jmp(drcontext, opnd_create_pc(code_cache));
    instr_set_translation(instr, translation);
    instrlist_preinsert(bb, appNext, instr);
#endif
    /* For debugging. */
    //instrlist_disassemble(drcontext, tag, bb, STDERR);
}

#ifdef JANUS_AARCH64
void insert_mov_immed_ptrsz(void *dcontext, instrlist_t *bb, instr_t *trigger, opnd_t dst, ptr_int_t val)
{
    int i;
    instr_t *mov;

    mov = INSTR_CREATE_movz(dcontext, dst,
                            OPND_CREATE_INT16(val & 0xffff), OPND_CREATE_INT8(0));
    PRE_INSERT(bb, trigger, mov);

    for (i = 1; i < 4; i++) {
        if (((val >> (16 * i)) & 0xffff) != 0) {
            /* movk x(dst), #(val >> sh & 0xffff), lsl #(sh) */
            mov = INSTR_CREATE_movk(dcontext, dst,
                                    OPND_CREATE_INT16((val >> (16 * i)) & 0xffff),
                                    OPND_CREATE_INT8(i * 16));
            PRE_INSERT(bb, trigger, mov);
        }
    }
}
#endif

/* Register 0 needs to hold the jump back address */
void 
insert_function_call_as_application(JANUS_CONTEXT, void *func)
{
#ifdef JANUS_X86
    instr_t *trigger = get_trigger_instruction(bb,rule);
    instr_t *first = trigger;
    instr_t *instr, *prev_instr;
    instr_t *label = INSTR_CREATE_label(drcontext);
    app_pc pc = (app_pc)rule->pc;

    prev_instr = instr_get_prev(trigger);
    
    //PRE_INSERT(bb,trigger,INSTR_CREATE_int1(drcontext));

    /* Put the address of the function in register r15. */
    instr = INSTR_CREATE_mov_imm(drcontext, opnd_create_reg(DR_REG_R15),
                                 OPND_CREATE_INTPTR(func));
    PRE_INSERT(bb, trigger, instr);

    /* Put the address of jump back to in a register. */
    instr = INSTR_CREATE_mov_imm(drcontext, opnd_create_reg(DR_REG_R14),
                               OPND_CREATE_INTPTR(pc));    
    PRE_INSERT(bb, trigger, instr);

    /* Create the jump. */
    instr = INSTR_CREATE_jmp(drcontext, opnd_create_pc(call_func_code_cache));
    instr_set_translation(instr, instr_get_app_pc(prev_instr));
    instrlist_preinsert(bb, trigger, instr);
#elif JANUS_AARCH64
    instr_t *trigger = get_trigger_instruction(bb,rule);
    instr_t *prev_instr = trigger;
    trigger = instr_get_next(trigger);
    instr_t *instr;
    //instr_t *instr = instr_create_0dst_1src(drcontext, OP_brk, OPND_CREATE_INT16(0));
    //PRE_INSERT(bb, trigger, instr);

    /* Put the address of the function in register x20 */
    insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(DR_REG_X20), (ptr_int_t)func);

    /* Put the address of the return address in register x21 */
    insert_mov_immed_ptrsz(drcontext, bb, trigger, opnd_create_reg(DR_REG_X21), (ptr_int_t)rule->pc);

    /* Perform an application call and force dynamoRIO to translate the "call_func_code_cache" */
    instr = instr_create_0dst_1src(drcontext, OP_b, opnd_create_pc(call_func_code_cache));
    instr_set_translation(instr, instr_get_app_pc(prev_instr));
    instrlist_preinsert(bb, trigger, instr);
#endif
}

/**
 * Create the code cache containing instructions for calling a function defined
 * within the client.  The address of the function to call will be stored in
 * register r15 and all arguments will already have been put in the correct
 * registers or on the stack.  To get back to DR's code cache, the address to
 * jump back to will be in register rbx.
 */
void create_call_func_code_cache(void)
{
    void *drcontext;
    instrlist_t *bb;
    instr_t *jmp_back;
    instr_t *instr;
    byte *end;
    app_pc code_cache;
    /* Allocate memory for the code cache. */
    drcontext  = dr_get_current_drcontext();
    code_cache = dr_nonheap_alloc(PAGE_SIZE, 
                                            DR_MEMPROT_READ  |
                                            DR_MEMPROT_WRITE |
                                            DR_MEMPROT_EXEC);
    bb = instrlist_create(drcontext);
#ifdef JANUS_X86
    /**
    * Simply call the client function (that will execute as app code) whose
    * address is in register r15.
    */
    /* Jump back to DR's code cache. */
    jmp_back = INSTR_CREATE_jmp_ind(drcontext, opnd_create_reg(DR_REG_R14));
    instrlist_append(bb, jmp_back);

    prepare_for_call(drcontext,bb,jmp_back);
    
    /* Call client function in r15. */
    instr = INSTR_CREATE_call_ind(drcontext, opnd_create_reg(DR_REG_R15));
    instrlist_preinsert(bb, jmp_back, instr);

    cleanup_after_call(drcontext,bb,jmp_back);
#elif JANUS_AARCH64
    instr_t *debug = instr_create_0dst_1src(drcontext, OP_brk, OPND_CREATE_INT16(0));
    instrlist_append(bb, debug);
    //instr_t *debug1 = instr_create_0dst_1src(drcontext, OP_brk, OPND_CREATE_INT16(0));
    //instrlist_preinsert(bb, debug, debug1);

    /* sub sp, sp, #16 */
    instr = XINST_CREATE_sub(drcontext,
                             opnd_create_reg(DR_REG_XSP),
                             OPND_CREATE_INT(16));
    instrlist_preinsert(bb, debug, instr);


    /* X20 stores the function and X21 stores the return address, push them on the stack before calling actual call */
    /* Todo: make the post decrement work */
    /* stp x20, x21, [rsp] */
    instr = INSTR_CREATE_stp(drcontext,
                             opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, OPSZ_16),
                             opnd_create_reg(DR_REG_X20),
                             opnd_create_reg(DR_REG_X21));
    instrlist_preinsert(bb, debug, instr);

    /* save all the caller-saved registers */
    prepare_for_call(drcontext, bb, debug);

    /* call the actual function */
    instr = instr_create_0dst_1src(drcontext, OP_blr, opnd_create_reg(DR_REG_X20));
    instrlist_preinsert(bb, debug, instr);

    /* restore all the caller-saved registers */
    cleanup_after_call(drcontext, bb, debug);

   /* ldp, x20, x21, [rsp] */
    instr = INSTR_CREATE_ldp(drcontext,
                             opnd_create_reg(DR_REG_X20),
                             opnd_create_reg(DR_REG_X21),
                             opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, OPSZ_16));
    instrlist_preinsert(bb, debug, instr);

    /* Function returns, resume stack */
    /* add sp, sp, #16 */
    instr = XINST_CREATE_add(drcontext,
                             opnd_create_reg(DR_REG_XSP),
                             OPND_CREATE_INT(16));
    instrlist_preinsert(bb, debug, instr);
 
    //debugging instruction
    //instrlist_preinsert(bb, debug, instr_create_0dst_1src(drcontext, OP_brk, OPND_CREATE_INT16(0)));

    instr = instr_create_0dst_1src(drcontext, OP_br, opnd_create_reg(DR_REG_X21));
    instrlist_preinsert(bb, debug, instr);
#endif
    /* Encodes the instructions into memory and then cleans up. */
    end = instrlist_encode(drcontext, bb, code_cache, false);
    DR_ASSERT((end - code_cache) < PAGE_SIZE);
    //instrlist_disassemble(drcontext, code_cache, bb, STDERR);
    instrlist_clear_and_destroy(drcontext, bb);

    /* Set memory as just +rx. */
    dr_memory_protect(code_cache, PAGE_SIZE,
                    DR_MEMPROT_READ | DR_MEMPROT_EXEC);

    call_func_code_cache = code_cache;
}

void prepare_for_call(void *drcontext, instrlist_t *bb, instr_t *probe)
{
    instr_t *instr;
#ifdef JANUS_X86
    instr = INSTR_CREATE_pushf(drcontext);
    PRE_INSERT(bb,probe,instr);
    instr = INSTR_CREATE_push(drcontext,opnd_create_reg(DR_REG_R11));
    PRE_INSERT(bb,probe,instr);
    instr = INSTR_CREATE_push(drcontext,opnd_create_reg(DR_REG_R10));
    PRE_INSERT(bb,probe,instr);
    instr = INSTR_CREATE_push(drcontext,opnd_create_reg(DR_REG_R9));
    PRE_INSERT(bb,probe,instr);
    instr = INSTR_CREATE_push(drcontext,opnd_create_reg(DR_REG_R8));
    PRE_INSERT(bb,probe,instr);
    instr = INSTR_CREATE_push(drcontext,opnd_create_reg(DR_REG_RDI));
    PRE_INSERT(bb,probe,instr);
    instr = INSTR_CREATE_push(drcontext,opnd_create_reg(DR_REG_RSI));
    PRE_INSERT(bb,probe,instr);
    instr = INSTR_CREATE_push(drcontext,opnd_create_reg(DR_REG_RDX));
    PRE_INSERT(bb,probe,instr);
    instr = INSTR_CREATE_push(drcontext,opnd_create_reg(DR_REG_RCX));
    PRE_INSERT(bb,probe,instr);
    instr = INSTR_CREATE_push(drcontext,opnd_create_reg(DR_REG_RAX));
    PRE_INSERT(bb,probe,instr);
#elif JANUS_AARCH64

    //instr = instr_create_0dst_1src(drcontext, OP_brk, OPND_CREATE_INT16(0));
    //PRE_INSERT(bb, probe, instr);


    int i;
    /* sub sp, sp, #8*22 */
    instr = XINST_CREATE_sub(drcontext,
                             opnd_create_reg(DR_REG_XSP),
                             OPND_CREATE_INT(8*22));
    instrlist_preinsert(bb, probe, instr);

    for (i = 0; i <20 ; i += 2) {
        PRE_INSERT(bb, probe,
            INSTR_CREATE_stp(drcontext,
                             opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0,
                                                   i*8, OPSZ_16),
                             opnd_create_reg(DR_REG_X0 + i),
                             opnd_create_reg(DR_REG_X0 + i + 1)));
    }


    PRE_INSERT(bb, probe,
        INSTR_CREATE_stp(drcontext,
                         opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0,
                                               20*8, OPSZ_16),
                         opnd_create_reg(DR_REG_X29),
                         opnd_create_reg(DR_REG_X30)));
#endif
}

void cleanup_after_call(void *drcontext, instrlist_t *bb, instr_t *probe)
{
    instr_t *instr;
#ifdef JANUS_X86
    
    instr = INSTR_CREATE_pop(drcontext,opnd_create_reg(DR_REG_RAX));
    PRE_INSERT(bb,probe,instr);
    instr = INSTR_CREATE_pop(drcontext,opnd_create_reg(DR_REG_RCX));
    PRE_INSERT(bb,probe,instr);
    instr = INSTR_CREATE_pop(drcontext,opnd_create_reg(DR_REG_RDX));
    PRE_INSERT(bb,probe,instr);
    instr = INSTR_CREATE_pop(drcontext,opnd_create_reg(DR_REG_RSI));
    PRE_INSERT(bb,probe,instr);
    instr = INSTR_CREATE_pop(drcontext,opnd_create_reg(DR_REG_RDI));
    PRE_INSERT(bb,probe,instr);
    instr = INSTR_CREATE_pop(drcontext,opnd_create_reg(DR_REG_R8));
    PRE_INSERT(bb,probe,instr);
    instr = INSTR_CREATE_pop(drcontext,opnd_create_reg(DR_REG_R9));
    PRE_INSERT(bb,probe,instr);
    instr = INSTR_CREATE_pop(drcontext,opnd_create_reg(DR_REG_R10));
    PRE_INSERT(bb,probe,instr);
    instr = INSTR_CREATE_pop(drcontext,opnd_create_reg(DR_REG_R11));
    PRE_INSERT(bb,probe,instr);
    instr = INSTR_CREATE_popf(drcontext);
    PRE_INSERT(bb,probe,instr);
#elif JANUS_AARCH64
    int i; 
   for (i = 0; i <20 ; i += 2) {
        PRE_INSERT(bb, probe,
            INSTR_CREATE_ldp(drcontext,
                             opnd_create_reg(DR_REG_X0 + i),
                             opnd_create_reg(DR_REG_X0 + i + 1),
                             opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0,
                                                   i*8, OPSZ_16)));
                             }
    PRE_INSERT(bb, probe,
        INSTR_CREATE_ldp(drcontext,
                         opnd_create_reg(DR_REG_X29),
                         opnd_create_reg(DR_REG_X30),
                         opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0,
                                               20*8, OPSZ_16)));
    /* add sp, sp, #8*22 */
    instr = XINST_CREATE_add(drcontext,
                             opnd_create_reg(DR_REG_XSP),
                             OPND_CREATE_INT(8*22));
    instrlist_preinsert(bb, probe, instr);

#endif
}

void delete_instr_handler(JANUS_CONTEXT)
{
    instr_t *temp;
    instr_t *trigger = get_trigger_instruction(bb,rule);
    app_pc start_pc = (app_pc)rule->reg0;
    app_pc end_pc = (app_pc)rule->reg1;

    while (1) {
        app_pc pc = instr_get_app_pc(trigger);
        temp = trigger;
        trigger = instr_get_next_app(trigger);
        if (pc >= start_pc && pc <= end_pc) {
            instrlist_remove(bb, temp);
            instr_destroy(drcontext, temp);
        }
        if (pc > end_pc) break;
    }
}

void
split_block_handler(JANUS_CONTEXT)
{
    instr_t *instr, *temp;
    instr_t *trigger = get_trigger_instruction(bb,rule);
#ifdef JANUS_VERBOSE
    printf("Going to split the instruction\n");
    instr_disassemble(drcontext, trigger,STDOUT);
    dr_printf("\n");
#endif
    app_pc target_pc = instr_get_app_pc(trigger);
    app_pc jmp_addr = instr_get_app_pc(instr_get_prev(trigger));

#ifdef JANUS_X86
    /* Create a manual jump to the next address */
    instr = INSTR_CREATE_jmp(drcontext, opnd_create_pc(target_pc));
    instr_set_translation(instr, jmp_addr);
    instrlist_preinsert(bb, trigger, instr);
#elif JANUS_AARCH64

#endif
    /* Remove all subsequent instructions */
    instr = trigger;
    while (instr != NULL) {
        temp = instr;
        instr = instr_get_next(instr);
        instrlist_remove(bb, temp);
        instr_destroy(drcontext, temp);
    }
}

/* -------------------------------------------------------------------------------
 *                              Function Utilities
 * ------------------------------------------------------------------------------*/
void dump_registers() {
#ifdef JANUS_X86
    dr_mcontext_t mc;
    mc.size = sizeof(dr_mcontext_t);
    mc.flags = DR_MC_ALL;

    //copy the machine state to my private copy
    dr_get_mcontext(dr_get_current_drcontext(),&mc);

    printf("\n-----------------------------------------------------\n");
    printf("rax %lx rcx %lx rdx %lx rbx %lx\n",mc.rax,mc.rcx, mc.rdx, mc.rbx);
    printf("rsp %lx rbp %lx rdi %lx rsi %lx\n",mc.rsp,mc.rbp, mc.rdi, mc.rsi);
    printf("r8 %lx r9 %lx r10 %lx r11 %lx\n",mc.r8,mc.r9, mc.r10, mc.r11);
    printf("r12 %lx r13 %lx r14 %lx r15 %lx\n",mc.r12,mc.r13, mc.r14, mc.r15);
    printf("-----------------------------------------------------\n");
#endif
}

void print_debug_reg(uint64_t reg)
{
    printf("%lx\n", reg);
}

