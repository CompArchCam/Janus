//  JANUS Handlers are operative functions that modifies code
//  All handlers here are generic to modification not specifically to parallelisation
//  Created by Kevin Zhou on 17/12/2014.
//

#ifndef __JANUS_HANDLER__
#define __JANUS_HANDLER__

#include "janus.h"
#include "dr_api.h"

#ifdef __cplusplus
extern "C" {
#endif
/* -------------------------------------------------------------------------------
 *                              Binary Rewrite Macros
 * ------------------------------------------------------------------------------*/

//shortcut for passing parameters, please use the name if you use this macro
#define JANUS_CONTEXT                    void *drcontext, instrlist_t *bb, RRule *rule, void *tag
#define janus_context                    drcontext,bb,rule,tag

//modification macro: a default insert is in meta mode
#define PRE_INSERT                      instrlist_meta_preinsert
#define POST_INSERT                     instrlist_meta_postinsert
#define APPEND							instrlist_append
#define INSERT  						instrlist_preinsert
#define REPLACE                         instrlist_replace
#define DELETE                          instrlist_delete

//function call macro, the last name indicates the number of arguments
#define insert_call(A,args...)          dr_insert_clean_call(drcontext,bb,get_trigger_instruction(bb,rule),(void *)(A), false, args)
#define insert_call_with_pc(A)          insert_call(A,1,OPND_CREATE_INTPTR(rule->pc))

#ifdef JANUS_AARCH64
#define INSERT_DEBUG PRE_INSERT(bb,trigger,instr_create_0dst_1src(drcontext, OP_brk, OPND_CREATE_INT16(0)))
#endif

//Memory operations
#define MEM_READ 						1
#define MEM_WRITE 						2
#define MEM_READ_AND_WRITE 				3

//Debugging
//dump register file content
//Note that it is only valid in the handler code
#define PRINT_REG(A) 					dr_insert_clean_call(drcontext,bb,trigger,print_debug_reg, false, 1, opnd_create_reg(A))
#define DUMP_REGS 						dr_insert_clean_call(drcontext,bb,trigger,dump_registers, false, 0);

#ifndef PAGE_SIZE
#define PAGE_SIZE 						4096
#endif
typedef app_pc							CCache;
/* -------------------------------------------------------------------------------
 *                              Dynamic Code Caches
 * ------------------------------------------------------------------------------*/
//register your code cache compiler in this function
void 
register_code_cache_compiler(void (*func)());

//all compile all registered code caches
void 
compile_all_code_caches();

void 
create_call_func_code_cache();

/* -------------------------------------------------------------------------------
 *                              Handler Utilities
 * ------------------------------------------------------------------------------*/
//return the instr_t structure specified by the static rules
instr_t *
get_trigger_instruction(instrlist_t *bb, RRule *rule);

//To perform this call, make sure that the call_function code cache is ready
//And save R15 and R14 in advance
void 
insert_function_call_as_application(JANUS_CONTEXT, void *func);

//load effective address from given memory operand
void 
load_effective_address(void *drcontext, instrlist_t *bb,
					   instr_t *trigger, opnd_t memref,
					   reg_id_t scratch1, reg_id_t scratch2);

void
call_code_cache_handler(void *drcontext, instrlist_t *bb,
                        instr_t *trigger, CCache code_cache);

void
jump_to_code_cache(void *drcontext, void *tag, instrlist_t *bb, instr_t *appPrev, instr_t *appNext, CCache code_cache);

void 
prepare_for_call(void *drcontext, instrlist_t *bb, instr_t *probe);

void 
cleanup_after_call(void *drcontext, instrlist_t *bb, instr_t *probe);

void
delete_instr_handler(JANUS_CONTEXT);

void
split_block_handler(JANUS_CONTEXT);

#ifdef JANUS_AARCH64
void insert_mov_immed_ptrsz(void *dcontext, instrlist_t *bb, instr_t *trigger, opnd_t dst, ptr_int_t val);
#endif
/* -------------------------------------------------------------------------------
 *                              Function Utilities
 * ------------------------------------------------------------------------------*/
//dump all architectural registers
void dump_registers();
void print_debug_reg(uint64_t reg);
#ifdef __cplusplus
}
#endif
#endif
