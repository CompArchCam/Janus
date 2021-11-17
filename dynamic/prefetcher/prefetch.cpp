/* JANUS Client for binary optimiser */

#define CACHE_LINE_WIDTH 0x100
#define CODE_SNIPPET_MAX_COUNT 16

#define JANUS_DYNAMIC_ADAPT

/* Header file to implement a JANUS client */
#include "janus_api.h"
#include "dr_tools.h"
#include "OPT_rule_structs.h"
#include "instructions.h"
#include "printer.h"
#include <new>

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
    extern "C" {
#endif

hashtable_t savedClones;

//inserts the sequence of instructions in the linked list ruleList having the same pc
//returns a pointer to the first rule that has a different address from the first one in the list
static RRule* prefetch_sequence_gen(void *drcontext, instrlist_t *bb, PCAddress bbAddr, RRule *ruleList, instr_t *instr, bool for_trace);

static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating);

static void event_exit(void);

/** Frees the cloned instr_t pointed to by ptr */
static void clone_free(void *ptr);

/** Frees the thread-local custom stack pointed to by ptr */
static void stack_free(void *ptr);

void allocate_stack(void *drcontext);
void free_allocated_stack(void *drcontext);

static dr_signal_action_t sig_handler(void *drcontext, dr_siginfo_t *siginfo);

#define CUSTOM_STACK_SIZE 1024
hashtable_t custom_stacks;

hashtable_t contexts;

byte ___b[8] = {
    0x66, 0x4c, 0x0f, 0x6e, 0xf8, 0xcc, 0xcc, 0xc3,
};
void* foo_addr;
void *
generate_runtime_code()
{
    void* code_cache = dr_nonheap_alloc(64,
                                         DR_MEMPROT_READ  |
                                         DR_MEMPROT_WRITE |
                                         DR_MEMPROT_EXEC);

    for (int i=0; i<8; i++) {
        ((byte*)code_cache)[i] = ___b[i];
    }


    /* Set memory as just +rx. */
    dr_memory_protect(code_cache, 64,
                    DR_MEMPROT_READ | DR_MEMPROT_WRITE | DR_MEMPROT_EXEC);

    return code_cache;
}

DR_EXPORT void 
dr_init(client_id_t id)
{
#ifdef JANUS_VERBOSE
    dr_fprintf(STDOUT,"\n---------------------------------------------------------------\n");
    dr_fprintf(STDOUT,"               Guided Automatic Binary Optimiser\n");
    dr_fprintf(STDOUT,"---------------------------------------------------------------\n\n");
#endif
    /* Register event callbacks. */
    dr_register_bb_event(event_basic_block);

    dr_register_signal_event(sig_handler);

    /* Initialise JANUS components */
    janus_init(id);

    foo_addr = generate_runtime_code();

    if(ginfo.mode != JOPT) {
        dr_fprintf(STDOUT,"Static rules not intended for %s!\n", print_janus_mode(ginfo.mode));
        return;
    }

    hashtable_init_ex(&savedClones, 8, HASH_INTPTR, false, false, clone_free, NULL, NULL);
    hashtable_init_ex(&custom_stacks, 8, HASH_INTPTR, false, false, stack_free, NULL, NULL);
    hashtable_init_ex(&contexts, 8, HASH_INTPTR, false, false, NULL, NULL, NULL);
    //LOGFILE = dr_open_file("bblog.txt", DR_FILE_WRITE_OVERWRITE);
    
    dr_register_exit_event(event_exit);

    dr_register_thread_init_event(allocate_stack);
    dr_register_thread_exit_event(free_allocated_stack);

#ifdef JANUS_VERBOSE
    dr_fprintf(STDOUT,"Dynamorio client initialised\n");
#endif
}

//TODO check whether this is a fault in a prefetch sequence or not
static dr_signal_action_t sig_handler(void *drcontext, dr_siginfo_t *siginfo) {
    if (siginfo->sig == 11) {
        fprintf(stderr, "Signal 11 at %p\n", siginfo->raw_mcontext->pc);
        while (*((unsigned int*)siginfo->raw_mcontext->pc) != 0x90909090) siginfo->raw_mcontext->pc++;
        //fprintf(stderr, "Resuming at %llx\n", siginfo->raw_mcontext->pc);
        return DR_SIGNAL_SUPPRESS;
    }
    fprintf(stderr, "Signal %d received and delivered.\n", siginfo->sig);
    return DR_SIGNAL_DELIVER;
}

static inline void findInstr(app_pc &addr, instr_t *&currentInstr) {
    while (instr_get_app_pc(currentInstr) != addr) currentInstr = instr_get_next(currentInstr);
}

/* Main execution loop: this will be executed at every initial encounter of new basic block
 * After modification, the code is bufferred and reused */
static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    //fprintf(stderr, "Before translation: \n");
    //instrlist_disassemble(drcontext, (app_pc)tag, bb, STDERR);
    //dr_flush_file(STDERR);
    app_pc rule_pc;
    //get current basic block starting address
    PCAddress bbAddr = (PCAddress)dr_fragment_app_pc(tag);

    RRule *curr = get_static_rule(bbAddr);

    //if it is a normal basic block, then omit it.
    if(curr == NULL) return DR_EMIT_DEFAULT;

    //Scan for and handle clone instructions
    instr_t *currentInstr = instrlist_first(bb);
    for (RRule *currh = curr; currh; currh = currh->next) {
        if (currh->opcode != OPT_CLONE_INSTR) continue;

        rule_pc = (app_pc)currh->pc;
        if (rule_pc < instr_get_app_pc(currentInstr)) {
            fprintf(stderr, "BB did not contain address (too small).\n");
            break;
        }
        /*if (rule_pc > instr_get_app_pc(instrlist_last(bb))) {
            fprintf(stderr, "BB did not contain address (too large).\n");
            break;
        }*/

        findInstr(rule_pc, currentInstr);

        //clone and save instruction to cloned pool (to be used when we hit address cloneAddr)
        //fprintf(stderr, "clone created\n");

        //use dr's own allocation mechanism
        OPT_CLONE_INSTR_rule *h = (OPT_CLONE_INSTR_rule*)
                                     dr_global_alloc(sizeof(OPT_CLONE_INSTR_rule));
        h = new (h) OPT_CLONE_INSTR_rule(*currh);

        instr_t *instr;
        if (h->fixedRegisters) {
            instr = instr_clone(drcontext, currentInstr);
            instr_set_meta_no_translation(instr);
        } else instr = clone_with_regs(drcontext, currentInstr, h->opnd);

        instr_make_persistent(drcontext, instr);
        h->ptr = (void*) instr;

        //Link h into the correct place in the chain
        OPT_CLONE_INSTR_rule* curr =
                    (OPT_CLONE_INSTR_rule*)hashtable_lookup(&savedClones, (void*) h->cloneAddr);
        OPT_CLONE_INSTR_rule* prev = NULL;
        while ((curr) && ((curr->ID) < (h->ID))) {
            prev = curr;
            curr = (OPT_CLONE_INSTR_rule*)curr->next;
        }

        if (curr && ((curr->ID) == (h->ID))) {
            //duplicate -> do not save

            instr_destroy(drcontext, instr);
            dr_global_free((void*)h, sizeof(OPT_CLONE_INSTR_rule));
            break;
        } else {
            hashtable_add(&contexts, (void*)h, drcontext);

            h->next = curr;
            if (prev) prev->next = h;
                 else hashtable_add_replace(&savedClones, (void*)h->cloneAddr, (void*)h);
        }
    }

    //Process normal rules
    currentInstr = instrlist_first(bb);
    while (curr) {
        rule_pc = (app_pc)curr->pc;
        if (rule_pc < instr_get_app_pc(currentInstr)) {
            fprintf(stderr, "BB did not contain address. %p %p %lx\n", rule_pc, tag, curr->block_address);
            break;
        }
        /*if (rule_pc > instr_get_app_pc(instrlist_last(bb))) {
            fprintf(stderr, "BB did not contain address (too large).\n");
            break;
        }*/
        findInstr(rule_pc, currentInstr);
        curr = prefetch_sequence_gen(drcontext, bb, bbAddr, curr, currentInstr, for_trace);
    }

    //fprintf(stderr, "After translation: \n");
    //instrlist_disassemble(drcontext, (app_pc)tag, bb, STDERR);
    //dr_flush_file(STDERR);

    return DR_EMIT_DEFAULT;
}

inline static void opt_save_reg(void *drcontext, instrlist_t *bb, instr_t *instr,
                                                                    OPT_RESTORE_REG_rule *h) {
    //if (h->store > 0) {
        createMoveInstruction(drcontext, bb, instr, (int32_t)h->store, h->reg);
    /*} else {
        //instr_t *instrr = INSTR_CREATE_pinsrq(drcontext, opnd_create_reg(DR_REG_XMM15), opnd_create_reg(h->reg), opnd_create_imm0);
        int m = 15+h->store;
        ((byte*)foo_addr)[3] = 0x7e;
        ((byte*)foo_addr)[4] = 0x80 + (h->reg-DR_REG_RAX) + ((m&0x7)<<3);
        ((byte*)foo_addr)[1] = 0x4c | ((m&0x8)>>1);

        instr_t *instrr = INSTR_CREATE_call_ind(drcontext, opnd_create_rel_addr(&foo_addr,OPSZ_PTR));

        instrlist_meta_preinsert(bb, instr, INSTR_CREATE_int3(drcontext));
        instrlist_meta_preinsert(bb, instr, instrr);
    }*/
}

inline static void opt_restore_reg(void *drcontext, instrlist_t *bb, instr_t *instr,
                                                                    OPT_RESTORE_REG_rule *h) {
    //createMoveInstruction(drcontext, bb, instr, h->reg, (int32_t)h->store);
    //instr_t *instrr = INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(h->reg), opnd_create_reg(DR_REG_XMM15));
    //instrlist_meta_preinsert(bb, instr, instrr);
    //if (h->store > 0) {
        createMoveInstruction(drcontext, bb, instr, h->reg, (int32_t)h->store);
    /*} else {
        //instr_t *instrr = INSTR_CREATE_pinsrq(drcontext, opnd_create_reg(DR_REG_XMM15), opnd_create_reg(h->reg), opnd_create_imm0);
        int m = 15+h->store;
        ((byte*)foo_addr)[3] = 0x6e;
        ((byte*)foo_addr)[4] = 0x80 + (h->reg-DR_REG_RAX) + ((m&0x7)<<3);
        ((byte*)foo_addr)[1] = 0x4c | ((m&0x8)>>1);

        instr_t *instrr = INSTR_CREATE_call_ind(drcontext, opnd_create_rel_addr(&foo_addr,OPSZ_PTR));

        instrlist_meta_preinsert(bb, instr, INSTR_CREATE_int3(drcontext));
        instrlist_meta_preinsert(bb, instr, instrr);
    }*/
}

inline static void opt_save_flags(void *drcontext, instrlist_t *bb, instr_t *where,
                                                                    OPT_RESTORE_FLAGS_rule *h) {
    //fprintf(stderr, "FLAGS SAVED...\n");
    instr_t *instr = INSTR_CREATE_pushf(drcontext);
    //instr_set_meta_no_translation(instr);
    instrlist_meta_preinsert(bb, where, instr);

    //FIXME XXX slot choice was arbitrary
    //dr_save_arith_flags(drcontext, bb, where, SPILL_SLOT_8);
}

inline static void opt_restore_flags(void *drcontext, instrlist_t *bb, instr_t *where,
                                                                    OPT_RESTORE_FLAGS_rule *h) {
    instr_t *instr = INSTR_CREATE_popf(drcontext);
    instr_set_meta_no_translation(instr);
    instrlist_meta_preinsert(bb, where, instr);

    //dr_restore_arith_flags(drcontext, bb, where, SPILL_SLOT_8);
}

inline static void opt_prefetch_handler(void *drcontext, instrlist_t *bb, instr_t *where,
                                                                const OPT_PREFETCH_rule &h)
{
    opnd_t opnd = opnd_from_GVar(h.opnd);
    opnd_set_size(&opnd, OPSZ_prefetch);
    //fprintf(stderr, "Prefetch operand: ");
    //print_janus_variable(h.opnd);
    

    instr_t *instr = INSTR_CREATE_prefetcht0(drcontext, opnd);
    //instr_set_meta(instr);
    instrlist_meta_preinsert(bb, where, instr);
}

inline static void opt_insert_mov_handler(void *drcontext, instrlist_t *bb, instr_t *instr,
                                                                const OPT_INSERT_MOV_rule &h)
{
    createMoveInstruction(drcontext, bb, instr, (int32_t)h.dst, (int32_t)h.src);
}

inline static void opt_insert_mem_mov_handler(void *drcontext, instrlist_t *bb, instr_t *where,
                                                                const OPT_INSERT_MEM_MOV_rule &h)
{
    //fprintf(stderr,"mem_mov (%d)\n", h.dst);
    instr_t *instr;
    if (h.src.type == VAR_IMM) {
        instr = INSTR_CREATE_mov_imm(drcontext, opnd_create_reg(h.dst),
                                                            opnd_from_GVar(h.src));
    } else {
        instr = INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(h.dst),
                                                            opnd_from_GVar(h.src));
        ////instr_set_translation(instr, instr_get_app_pc(where));
    }
    //instr_set_meta(instr);
    instrlist_meta_preinsert(bb, where, instr);
}

inline static void opt_insert_add_handler(void *drcontext, instrlist_t *bb, instr_t *where,
                                                                const OPT_INSERT_ADD_rule &h)
{
    //fprintf(stderr,"add\n");
    instr_t *instr = INSTR_CREATE_add(drcontext, opnd_create_reg(h.dst),
                                                        opnd_regOrImm(h.source_opnd_type, h.src));
    //instr_set_meta(instr);
    instrlist_meta_preinsert(bb, where, instr);
}

inline static void opt_insert_sub_handler(void *drcontext, instrlist_t *bb, instr_t *where,
                                                                const OPT_INSERT_SUB_rule &h)
{
    //fprintf(stderr,"sub\n");
    instr_t *instr = INSTR_CREATE_sub(drcontext, opnd_create_reg(h.dst),
                                                        opnd_regOrImm(h.source_opnd_type, h.src));
    //instr_set_meta(instr);
    instrlist_meta_preinsert(bb, where, instr);
}

inline static void opt_insert_imul_handler(void *drcontext, instrlist_t *bb, instr_t *where,
                                                                const OPT_INSERT_IMUL_rule &h)
{
    //fprintf(stderr,"imul\n");
    instr_t *instr = INSTR_CREATE_imul_imm(drcontext, opnd_create_reg(h.dst),
                                    opnd_create_reg(h.dst), opnd_create_immed_int((int32_t)h.src, OPSZ_4));
    //instr_set_meta(instr);
    instrlist_meta_preinsert(bb, where, instr);
}

inline static void opt_insert_clone(void *drcontext, instrlist_t *bb, instr_t *where,
                                                                OPT_CLONE_INSTR_rule *h)
{
    instr_t *instr = instr_clone(drcontext, (instr_t*)h->ptr);
    ////instr_set_translation(instr, instr_get_app_pc(where));
    instrlist_meta_preinsert(bb, where, instr);
}

//exchanges the application's stack pointer and the custom stack pointer
inline static void load_custom_rsp(void *drcontext, instrlist_t *bb, instr_t *where,
                                                                                void* tls_stack) {
    //save original RSP
    createMoveInstruction(drcontext, bb, where, -1, DR_REG_RSP);
    //load custom stack pointer into RSP
    instr_t *instr = INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(DR_REG_RSP),
                                                    opnd_create_rel_addr(tls_stack, OPSZ_8));
    instrlist_meta_preinsert(bb, where, instr);
}

//exchanges the custom stack pointer and the application's stack pointer
inline static void save_custom_rsp(void *drcontext, instrlist_t *bb, instr_t *where,
                                                                                void* tls_stack) {
    //store the new custom stack pointer
    instr_t *instr = INSTR_CREATE_mov_st(drcontext, opnd_create_rel_addr(tls_stack, OPSZ_8),
                                                    opnd_create_reg(DR_REG_RSP));
    instrlist_meta_preinsert(bb, where, instr);
    //restore original RSP
    createMoveInstruction(drcontext, bb, where, DR_REG_RSP, -1);
}

static void opt_save_reg_to_stack_handler(void *drcontext, instrlist_t *bb, instr_t *where,
                                           const OPT_SAVE_REG_TO_STACK_rule &h, void* tls_stack) {
    /*//increment RSP
    instr_t *instr = INSTR_CREATE_sub(drcontext, opnd_create_reg(DR_REG_RSP),
                                                 opnd_create_immed_int(8, OPSZ_4));
    instrlist_meta_preinsert(bb, where, instr);
    //store the required register
    instr = INSTR_CREATE_mov_st(drcontext,
                            opnd_create_base_disp(DR_REG_RSP, DR_REG_NULL, 1, 0, OPSZ_8),
                            opnd_create_reg(h.reg)
                               );*/

    instr_t *instr = INSTR_CREATE_push(drcontext, opnd_create_reg(h.reg));
    instrlist_meta_preinsert(bb, where, instr);
}

static void opt_restore_reg_from_stack_handler(void *drcontext, instrlist_t *bb,
                      instr_t *where, const OPT_RESTORE_REG_FROM_STACK_rule &h, void* tls_stack) {
    /*//load the required register
    instr_t *instr = INSTR_CREATE_mov_ld(drcontext,
                            opnd_create_reg(h.reg),
                            opnd_create_base_disp(DR_REG_RSP, DR_REG_NULL, 1, 0, OPSZ_8)
                               );
    instrlist_meta_preinsert(bb, where, instr);
    //decrement RSP
    instr = INSTR_CREATE_add(drcontext, opnd_create_reg(DR_REG_RSP),
                                        opnd_create_immed_int(8, OPSZ_4));*/

    instr_t *instr = INSTR_CREATE_pop(drcontext, opnd_create_reg(h.reg));
    instrlist_meta_preinsert(bb, where, instr);
}

inline static RRule* prefetch_sequence_gen(void *drcontext, instrlist_t *bb, PCAddress bbAddr,
                                             RRule *rules, instr_t *instr, bool for_trace) {
    PCAddress rule_pc = rules->pc;

    thread_id_t tid = dr_get_thread_id(drcontext);
    void *tls_stack = hashtable_lookup(&custom_stacks, (void*)(long)tid);

    //instrlist_meta_preinsert(bb, instr, INSTR_CREATE_int3(drcontext));

    RRule *stack_rules = rules;
    while (rules && ((rules->opcode == OPT_SAVE_REG_TO_STACK)
        || (rules->opcode == OPT_RESTORE_REG_FROM_STACK))) rules = rules->next;

    RRule *restore_rules = rules;
    while (rules && (rules->pc == rule_pc)
                && ((rules->opcode == OPT_RESTORE_REG) || (rules->opcode == OPT_RESTORE_FLAGS)))
                                                                              rules = rules->next;
    //assert(rules->pc == restore_rules->pc); //there must be at least one real rule at this pc

    //Handle OPT_SAVE_REG_TO_STACK rules
    if (stack_rules && (stack_rules->opcode == OPT_SAVE_REG_TO_STACK)) {
        load_custom_rsp(drcontext, bb, instr, tls_stack);

        while (stack_rules && (stack_rules->opcode == OPT_SAVE_REG_TO_STACK)) {
            OPT_SAVE_REG_TO_STACK_rule h = OPT_SAVE_REG_TO_STACK_rule(*stack_rules);
            opt_save_reg_to_stack_handler(drcontext, bb, instr, h, tls_stack);

            stack_rules = stack_rules->next;
        }

        save_custom_rsp(drcontext, bb, instr, tls_stack);
    }

    while (rules && (rules->pc == rule_pc) && (rules->opcode == OPT_CLONE_INSTR))
                                                                            rules = rules->next;

    if (rules && (rules->pc == rule_pc)) { //if this place contains a prefetch sequence
                 //rather than just OPT_SAVE_REG_TO_STACK and OPT_RESTORE_REG_FROM_STACK rules

    #ifdef JANUS_DYNAMIC_ADAPT
        //If this is not for trace, then do not insert prefetch sequence
        if (!for_trace) {
            while (rules && (rules->pc == rule_pc) && (rules->opcode != OPT_RESTORE_REG_FROM_STACK))
                rules = rules->next;
        } else {
    #endif

    //Get the list of cloned instructions
    OPT_CLONE_INSTR_rule *cloneRule = (OPT_CLONE_INSTR_rule*) hashtable_lookup(&savedClones,
                                                                                (void*)rules->pc);

    //Now go through restore rules (save)
    OPT_HINT *rrules = NULL;
    OPT_HINT *rrule = NULL;

    for (RRule *rh = restore_rules; rh; rh = rh->next) {
        OPT_HINT *nh;

        if (rh->opcode == OPT_RESTORE_REG) {
            nh = (OPT_HINT*)dr_thread_alloc(drcontext, sizeof(OPT_RESTORE_REG_rule));
            nh = new (nh) OPT_RESTORE_REG_rule(*rh);
            opt_save_reg(drcontext, bb, instr, (OPT_RESTORE_REG_rule*)nh);
        } else if (rh->opcode ==  OPT_RESTORE_FLAGS) {
            nh = (OPT_HINT*)dr_thread_alloc(drcontext, sizeof(OPT_RESTORE_FLAGS_rule));
            nh = new (nh) OPT_RESTORE_FLAGS_rule(*rh);
            opt_save_flags(drcontext, bb, instr, (OPT_RESTORE_FLAGS_rule*)nh);
        } else break; //not a restore rule

        if (rrule) {
            rrule = rrule->next = nh;
        } else {
            rrules = rrule = nh;
        }
        nh->next = NULL;
    }

    //Actual prefetch sequence
    for (; rules && (rules->pc == rule_pc) && (rules->opcode != OPT_RESTORE_REG_FROM_STACK);
                                                                            rules = rules->next) {
        //Handle any (all) clone instructions at this position in the chain
        while (cloneRule && (cloneRule->ID < ((uint16_t)rules->ureg0.down))) {
            //fprintf(stderr, "ID: %d\n", cloneRule->ID);
            //fprintf(stderr, "%d (cloned)\n", cloneRule->opcode);
            opt_insert_clone(drcontext, bb, instr, cloneRule);
            cloneRule = (OPT_CLONE_INSTR_rule*)cloneRule->next;
        }

        //fprintf(stderr, "%d\n", h->opcode);
        //fprintf(stderr, "ID: %d\n", (int)((uint16_t)h->ureg0.down));
        switch (rules->opcode) {
            case OPT_PREFETCH: {
                opt_prefetch_handler(drcontext, bb, instr, OPT_PREFETCH_rule(*rules));
            } break;
            case OPT_INSERT_MOV: {
                opt_insert_mov_handler(drcontext, bb, instr, OPT_INSERT_MOV_rule(*rules));
            } break;
            case OPT_INSERT_MEM_MOV: {
                opt_insert_mem_mov_handler(drcontext, bb, instr, OPT_INSERT_MEM_MOV_rule(*rules));
            } break;
            case OPT_INSERT_ADD: {
                opt_insert_add_handler(drcontext, bb, instr, OPT_INSERT_ADD_rule(*rules));
            } break;
            case OPT_INSERT_SUB: {
                opt_insert_sub_handler(drcontext, bb, instr, OPT_INSERT_SUB_rule(*rules));
            } break;
            case OPT_INSERT_IMUL: {
                opt_insert_imul_handler(drcontext, bb, instr, OPT_INSERT_IMUL_rule(*rules));
            } break;
            case OPT_CLONE_INSTR:
                break;
            default:
                fprintf(stderr,"In basic block 0x%lx static rule not recognised %d\n", bbAddr,
                                                                                   rules->opcode);
                break;
        }
    }

    //Reverse rrules so that pop instructions from the stack are correct
    OPT_HINT *curr = rrules, *prev = NULL;
    while (curr) {
        OPT_HINT *next = curr->next;
        curr->next = prev;
        prev = curr;
        curr = next;
    }
    rrules = prev;

    for (int i=0; i<4; i++) {
        instr_t *instrr = INSTR_CREATE_nop(drcontext);
        instrlist_meta_preinsert(bb, instr, instrr);
    }

    //Restore registers&flags that have been saved
    for (OPT_HINT *rh = rrules; rh; rh = rh->next) {
        if (rh->opcode == OPT_RESTORE_REG) {
            opt_restore_reg(drcontext, bb, instr, (OPT_RESTORE_REG_rule*)rh);
        } else {
            opt_restore_flags(drcontext, bb, instr, (OPT_RESTORE_FLAGS_rule*)rh);
        }
    }

    #ifdef JANUS_DYNAMIC_ADAPT
        }//endif (for_trace)
    #endif
    }//endif (rules)

    //Handle OPT_RESTORE_REG_FROM_STACK rules
    if (stack_rules && (stack_rules->opcode == OPT_RESTORE_REG_FROM_STACK)) {
        load_custom_rsp(drcontext, bb, instr, tls_stack);

        while (stack_rules && (stack_rules->opcode == OPT_RESTORE_REG_FROM_STACK)) {
            OPT_RESTORE_REG_FROM_STACK_rule h = OPT_RESTORE_REG_FROM_STACK_rule(*stack_rules);
            opt_restore_reg_from_stack_handler(drcontext, bb, instr, h, tls_stack);

            stack_rules = stack_rules->next;
        }

        save_custom_rsp(drcontext, bb, instr, tls_stack);
    }

    return rules;

    /*

    //free restore rules
    OPT_HINT *rh = rrules;
    while (rh) {
        OPT_HINT *prev = rh;
        rh = rh->next;

        dr_thread_free(drcontext, (void*)prev, prev->opcode==OPT_RESTORE_REG ? 
                                                                sizeof(OPT_RESTORE_REG_rule)
                                                              : sizeof(OPT_RESTORE_FLAGS_rule));
    }

    //free clone rules
    hashtable_remove(&savedClones, (void*)rules->pc);*/

    //fprintf(stderr, "/Prefetch sequence \n");
}

//XXX For some reason DynamoRIO (debug build only) sometimes complains about a memory leak before calling event_exit
static void clone_free(void *ptr) {
    //fprintf(stderr, "clone freed (ptr=%lld)\n", ptr);
    void* drcontext = hashtable_lookup(&contexts, ptr);

    OPT_CLONE_INSTR_rule *rule = (OPT_CLONE_INSTR_rule*) ptr;
    if (rule->next) clone_free((void*)rule->next);
    instr_destroy(drcontext, (instr_t*) rule->ptr);
    dr_global_free(ptr, sizeof(OPT_CLONE_INSTR_rule));

    //fprintf(stderr, "success\n");
}

static void stack_free(void *ptr) {
    if (!ptr) return;
    fprintf(stderr,"Freeing custom stack at %p, size: %d\n", ptr, CUSTOM_STACK_SIZE*8);
    dr_thread_free(hashtable_lookup(&contexts, ptr), ptr, CUSTOM_STACK_SIZE * 8);
    hashtable_remove(&contexts, ptr);
    fprintf(stderr,"Freed.\n");
}

void allocate_stack(void *drcontext) {
    fprintf(stderr,"allocate_stack for thread %d\n", dr_get_thread_id(drcontext));
    void *stack = dr_thread_alloc(drcontext, CUSTOM_STACK_SIZE * 8);
    fprintf(stderr,"Allocated custom stack at %p, size: %d\n", stack, CUSTOM_STACK_SIZE*8);
    hashtable_add(&custom_stacks, (void*)(long)dr_get_thread_id(drcontext), stack);
    hashtable_add(&contexts, stack, drcontext);

    //set up stack pointer (0'th entry)
    *((void**)stack) = (void*)(((int*)stack) + CUSTOM_STACK_SIZE);
}

void free_allocated_stack(void *drcontext) {
    fprintf(stderr,"free_allocated_stack for thread %d\n", dr_get_thread_id(drcontext));
    hashtable_remove(&custom_stacks, (void*)(long)dr_get_thread_id(drcontext));
}

static void event_exit(void) {
    fprintf(stderr, "Exit event...\n");
    front_end_exit();
    hashtable_delete(&savedClones);
    hashtable_delete(&custom_stacks);
    hashtable_delete(&contexts);
    //dr_close_file(LOGFILE);
}

#ifdef __cplusplus
    }
#endif
