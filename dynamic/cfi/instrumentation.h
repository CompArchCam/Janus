#ifndef _INSTRUMENT_ROUTINES_CFI_
#define _INSTRUMENT_ROUTINES_CFI_
#endif
#include "janus_api.h"
#include "dr_api.h"


extern uint64_t error_counter;


bool inRegSet(uint64_t bits, uint32_t reg);

#define CACHE_LINE_WIDTH 64

#define REG_S1 DR_REG_RSI
#define REG_S2 DR_REG_RDI
/*
DR_REG_NULL 0
DR_REG_RAX 1
DR_REG_RCX 2
DR_REG_RDX 3
DR_REG_RBX 4
DR_REG_RSP 5
DR_REG_RBP 6
DR_REG_RSI 7
DR_REG_RDI 8
DR_REG_R8 9
DR_REG_R9 10
DR_REG_R10 11
DR_REG_R11 12
DR_REG_R12 13
DR_REG_R13 14
DR_REG_R14 15
DR_REG_R15 16

//32 bit
DR_REG_EAX 17
..
..
DR_REG_R15D 32

//16 bit
DR_REG_AX   33
..
..
DR_REG_R15W 48
*/

#define INSERT_CLEAN_CALL(A, args...)   dr_insert_clean_call(drcontext,bb,get_trigger_instruction(bb,rule),(void *)(A), false, args)
#define SAVE_REG(reg, slot)             dr_save_reg(drcontext,bb,trigger, reg, slot)
#define RESTORE_REG(reg, slot)          dr_restore_reg(drcontext,bb,trigger, reg, slot)
#define SAVE_ARITH_FLAGS(slot)          dr_save_arith_flags(drcontext,bb,trigger, slot)
#define RESTORE_ARITH_FLAGS(slot)       dr_restore_arith_flags(drcontext,bb,trigger, slot)

#define SAVE_REG_SET_STACK(bitmask_reg)     SAVE_REG(DR_REG_RSP,SPILL_SLOT_12);\
                                            SAVE_REG(DR_REG_RBP,SPILL_SLOT_13);
#define RESTORE_REG_SET_STACK(bitmask_reg)  RESTORE_REG(DR_REG_RSP,SPILL_SLOT_12);\
                                            RESTORE_REG(DR_REG_RBP,SPILL_SLOT_13);
#define SAVE_CALLEE_REG_SET1(bitmask_reg)   if(inRegSet(bitmask_reg,7)) SAVE_REG(DR_REG_RSI,SPILL_SLOT_2);\
                                            if(inRegSet(bitmask_reg,8)) SAVE_REG(DR_REG_RDI,SPILL_SLOT_3);\
                                            if(inRegSet(bitmask_reg,11)) SAVE_REG(DR_REG_R10,SPILL_SLOT_4);\
                                            if(inRegSet(bitmask_reg,12)) SAVE_REG(DR_REG_R11,SPILL_SLOT_5);\
                                            if(inRegSet(bitmask_reg,9)) SAVE_REG(DR_REG_R8,SPILL_SLOT_6);\
                                            if(inRegSet(bitmask_reg,10)) SAVE_REG(DR_REG_R9,SPILL_SLOT_7)

/* RAX, RCX, RDX - caller saved*/
#define SAVE_CALLEE_REG_SET2(bitmask_reg)    if(inRegSet(bitmask_reg,2)) SAVE_REG(DR_REG_RCX,SPILL_SLOT_9);\
                                             if(inRegSet(bitmask_reg,3)) SAVE_REG(DR_REG_RDX,SPILL_SLOT_10)

#define SAVE_CALLEE_REG_SET2_RAX(bitmask_reg)  SAVE_REG(DR_REG_RAX,SPILL_SLOT_9);\
                                              if(inRegSet(bitmask_reg,2)) SAVE_REG(DR_REG_RCX,SPILL_SLOT_10);\
                                              if(inRegSet(bitmask_reg,3)) SAVE_REG(DR_REG_RDX,SPILL_SLOT_11)

#define RESTORE_CALLEE_REG_SET1(bitmask_reg) if(inRegSet(bitmask_reg,7)) RESTORE_REG(DR_REG_RSI,SPILL_SLOT_2);\
                                            if(inRegSet(bitmask_reg,8)) RESTORE_REG(DR_REG_RDI,SPILL_SLOT_3);\
                                            if(inRegSet(bitmask_reg,11)) RESTORE_REG(DR_REG_R10,SPILL_SLOT_4);\
                                            if(inRegSet(bitmask_reg,12)) RESTORE_REG(DR_REG_R11,SPILL_SLOT_5);\
                                            if(inRegSet(bitmask_reg,9)) RESTORE_REG(DR_REG_R8,SPILL_SLOT_6);\
                                            if(inRegSet(bitmask_reg,10)) RESTORE_REG(DR_REG_R9,SPILL_SLOT_7)


#define RESTORE_CALLEE_REG_SET2(bitmask_reg)  if(inRegSet(bitmask_reg,2)) RESTORE_REG(DR_REG_RCX,SPILL_SLOT_9);\
                                             if(inRegSet(bitmask_reg,3)) RESTORE_REG(DR_REG_RDX,SPILL_SLOT_10)

#define RESTORE_CALLEE_REG_SET2_RAX(bitmask_reg)   RESTORE_REG(DR_REG_RAX,SPILL_SLOT_9);\
                                                 if(inRegSet(bitmask_reg,2)) RESTORE_REG(DR_REG_RCX,SPILL_SLOT_10);\
                                                 if(inRegSet(bitmask_reg,3)) RESTORE_REG(DR_REG_RDX,SPILL_SLOT_11)

#define LABEL(x)         instrlist_meta_preinsert(bb,trigger, x)           
#define PREINSERT(instr)        instrlist_meta_preinsert(bb, trigger, instr)
#define INSERT_add(dst, src) PREINSERT( XINST_CREATE_add(drcontext,dst, src))
#define INSERT_sub(dst, src) PREINSERT( XINST_CREATE_sub(drcontext,dst, src))
#define INSERT_imul(dst, src) PREINSERT( INSTR_CREATE_imul(drcontext,dst, src))
#define INSERT_xor(dst, src) PREINSERT( INSTR_CREATE_xor(drcontext,dst, src))
#define INSERT_load(reg, mem) PREINSERT( XINST_CREATE_load(drcontext,reg, mem))
#define INSERT_store( mem, reg) PREINSERT( XINST_CREATE_store(drcontext,mem, reg))
#define INSERT_load_int(reg, i) PREINSERT( XINST_CREATE_load_int(drcontext,reg, i))
#define INSERT_move(dst, src) PREINSERT( XINST_CREATE_move(drcontext,dst, src)) //reg -> reg
#define INSERT_movsxd(dst, src) PREINSERT( INSTR_CREATE_movsxd(drcontext,dst, src))
#define INSERT_test(s1, s2) PREINSERT( INSTR_CREATE_test(drcontext, s1, s2))
#define INSERT_cmp(s1, s2) PREINSERT( XINST_CREATE_cmp(drcontext, s1, s2))
#define INSERT_push(opnd1) PREINSERT( INSTR_CREATE_push(drcontext,opnd1))
#define INSERT_pop(opnd1) PREINSERT( INSTR_CREATE_pop(drcontext,opnd1))
#define INSERT_call(target) PREINSERT( XINST_CREATE_call(drcontext,target))
#define INSERT_jz(target) PREINSERT( XINST_CREATE_jump_cond(drcontext,DR_PRED_Z, opnd_create_instr(target)))
#define INSERT_jnz(target) PREINSERT( XINST_CREATE_jump_cond(drcontext, DR_PRED_NZ, opnd_create_instr(target)))
#define INSERT_jb(target) PREINSERT( XINST_CREATE_jump_cond(drcontext, DR_PRED_B, opnd_create_instr(target)))
#define INSERT_jbe(target) PREINSERT( XINST_CREATE_jump_cond(drcontext, DR_PRED_BE, opnd_create_instr(target)))
#define INSERT_jump(target) PREINSERT( XINST_CREATE_jump(drcontext, opnd_create_instr(target)))
#define INCREMENT_error_counter      INSERT_load(opnd_create_reg(DR_REG_EAX), OPND_CREATE_ABSMEM((byte *)&error_counter, OPSZ_4));\
                                     INSERT_add(opnd_create_reg(DR_REG_EAX), OPND_CREATE_INT32(1));\
                                    INSERT_store(OPND_CREATE_ABSMEM((byte *)&error_counter, OPSZ_4), opnd_create_reg(DR_REG_EAX)) 

//#define ERASE_DST_REG_FROM_REGTABLE     INSERT_load(opnd_create_reg(DR_REG_ESI), OPND_CREATE_ABSMEM((byte *)&dest_id, OPSZ_4));\

/*#define ERASE_dst_reg_from_reg_table(dest_id)     INSERT_load(opnd_create_reg(DR_REG_ESI), OPND_CREATE_INT32(dest_id));\
                                        INSERT_load_int(opnd_create_reg(DR_REG_RDI), OPND_CREATE_INT64((uint64_t)&reg_table));\
                                        INSERT_call(opnd_create_pc((byte *)&map_erase_entry_regtab))
#define ERASE_dst_reg_from_split_reg_table(dest_id)     INSERT_load(opnd_create_reg(DR_REG_ESI), OPND_CREATE_ABSMEM((byte *)&dest_id, OPSZ_4));\
                                        INSERT_load_int(opnd_create_reg(DR_REG_RDI), OPND_CREATE_INT64((uint64_t)&split_reg_table));\
                                        INSERT_call(opnd_create_pc((byte *)&map_erase_entry_regtab))
*/
#define ERASE_dst_reg_from_reg_table(dest_id)     INSERT_load_int(opnd_create_reg(DR_REG_EDI), OPND_CREATE_INT32(dest_id));\
                                        INSERT_call(opnd_create_pc((byte *)&map_erase_entry_regtab))
#define ERASE_dst_reg_from_split_reg_table(dest_id)     INSERT_load_int(opnd_create_reg(DR_REG_EDI), OPND_CREATE_INT32(dest_id));\
                                        INSERT_call(opnd_create_pc((byte *)&map_erase_entry_split_regtab))

/*#define ERASE_lea_addr_from_memory_table     INSERT_load(opnd_create_reg(DR_REG_ESI), OPND_CREATE_ABSMEM((byte *)&LEAddr, OPSZ_8));\
                                        INSERT_load_int(opnd_create_reg(DR_REG_RDI), OPND_CREATE_INT64((uint64_t)&memory_table));\
                                        INSERT_call(opnd_create_pc((byte *)&map_erase_entry_memtab))
*/
#define ERASE_lea_addr_from_memory_table     INSERT_load(opnd_create_reg(DR_REG_RDI), OPND_CREATE_ABSMEM((byte *)&LEAddr, OPSZ_8));\
                                        INSERT_call(opnd_create_pc((byte *)&map_erase_entry_memtab))
#define JUMP_to_label(target)           INSERT_jump(target)
/*#define LOAD_EFFECTIVE_ADDR_LEADDR(mem_operand)      SAVE_REG(REG_S1, SPILL_SLOT_2);\
                                        SAVE_REG(REG_S2, SPILL_SLOT_3);\
                                        load_effective_address(drcontext, bb, trigger, mem_operand, REG_S1, REG_S2);\
                                        INSERT_store(OPND_CREATE_ABSMEM((byte *)&LEAddr, OPSZ_8), opnd_create_reg(REG_S1));\
                                        RESTORE_REG(REG_S1, SPILL_SLOT_2);\
                                        RESTORE_REG(REG_S2, SPILL_SLOT_3)  
*/
#define LOAD_EFFECTIVE_ADDR_LEADDR(mem_operand)    load_effective_address(drcontext, bb, trigger, mem_operand, DR_REG_RSI, DR_REG_RDI);\
                                        INSERT_store(OPND_CREATE_ABSMEM((byte *)&LEAddr, OPSZ_8), opnd_create_reg(DR_REG_RSI));
#define    SAVE_CALLEE_REG_SET(bitmask_reg)         SAVE_REG(DR_REG_RAX,SPILL_SLOT_2);\
                                        if(inRegSet(bitmask_reg,7)) SAVE_REG(DR_REG_RSI,SPILL_SLOT_3);\
                                        if(inRegSet(bitmask_reg,8)) SAVE_REG(DR_REG_RDI,SPILL_SLOT_4);\
                                        if(inRegSet(bitmask_reg,11)) SAVE_REG(DR_REG_R10,SPILL_SLOT_5);\
                                        if(inRegSet(bitmask_reg,12)) SAVE_REG(DR_REG_R11,SPILL_SLOT_6);\
                                        if(inRegSet(bitmask_reg,16)) SAVE_REG(DR_REG_R15,SPILL_SLOT_7);\
                                        if(inRegSet(bitmask_reg,9)) SAVE_REG(DR_REG_R8,SPILL_SLOT_8);\
                                        if(inRegSet(bitmask_reg,10)) SAVE_REG(DR_REG_R9,SPILL_SLOT_9);\
                                        if(inRegSet(bitmask_reg,2)) SAVE_REG(DR_REG_RCX,SPILL_SLOT_10);\
                                        if(inRegSet(bitmask_reg,3)) SAVE_REG(DR_REG_RDX,SPILL_SLOT_11)  
#define   RESTORE_CALLEE_REG_SET(bitmask_reg)        RESTORE_REG(DR_REG_RAX,SPILL_SLOT_2);\
                                        if(inRegSet(bitmask_reg,7)) RESTORE_REG(DR_REG_RSI,SPILL_SLOT_3);\
                                        if(inRegSet(bitmask_reg,8)) RESTORE_REG(DR_REG_RDI,SPILL_SLOT_4);\
                                        if(inRegSet(bitmask_reg,11)) RESTORE_REG(DR_REG_R10,SPILL_SLOT_5);\
                                        if(inRegSet(bitmask_reg,12)) RESTORE_REG(DR_REG_R11,SPILL_SLOT_6);\
                                        if(inRegSet(bitmask_reg,16)) RESTORE_REG(DR_REG_R15,SPILL_SLOT_7);\
                                        if(inRegSet(bitmask_reg,9)) RESTORE_REG(DR_REG_R8,SPILL_SLOT_8);\
                                        if(inRegSet(bitmask_reg,10)) RESTORE_REG(DR_REG_R9,SPILL_SLOT_9);\
                                        if(inRegSet(bitmask_reg,2)) RESTORE_REG(DR_REG_RCX,SPILL_SLOT_10);\
                                        if(inRegSet(bitmask_reg,3)) RESTORE_REG(DR_REG_RDX,SPILL_SLOT_11)

#define MAX_STACK_DEPTH 512
typedef struct{
    uintptr_t stack[MAX_STACK_DEPTH];
    int top;
} stack_thread_t;
extern stack_thread_t *shadowstack;

int get_64bit(int id);
int get_32bit(int id);
int get_16bit(int id);
bool is_64bit(int id);
bool is_32bit(int id);
bool is_16bit(reg_id_t id);
void save_xmm_reg(JANUS_CONTEXT, instr_t *trigger, uint64_t simd_mask, int s0);
void restore_xmm_reg(JANUS_CONTEXT, instr_t *trigger, uint64_t simd_mask, int s0);
void record_base_pointer(JANUS_CONTEXT, instr_t *trigger, uint64_t bitmask_flags, uint64_t bitmask_reg);
void record_size_malloc(JANUS_CONTEXT, instr_t *trigger);
void copy_reg_table(JANUS_CONTEXT, instr_t* trigger, uint64_t bitmask_flags, uint64_t bitmask_reg, int src_id, int dest_id);
//void copy_reg_table(JANUS_CONTEXT, instr_t* trigger, uint64_t bitmask_flags, uint64_t bitmask_reg);
void monitor_free_call(JANUS_CONTEXT, instr_t *trigger,uint64_t bitmask_flags, uint64_t bitmask_reg);
void record_size_calloc(JANUS_CONTEXT, instr_t *instr);
//void check_deref_abs_load(JANUS_CONTEXT, instr_t *trigger, uint64_t bitmask_flags, uint64_t bitmask_reg);
void remove_reg_table(JANUS_CONTEXT, instr_t *trigger, uint64_t bitmask_flags,uint64_t bitmask_reg, int dest_id );
void copy_global_bounds(JANUS_CONTEXT, instr_t *trigger, int dest_id, uint64_t base/*base*/, uint64_t total_bound/*size*/, uint64_t bitmask_reg);
void check_deref_mem_load(JANUS_CONTEXT, instr_t *trigger, uint64_t bitmask_flags, uint64_t bitmask_reg, int dest_id, int base_reg_id, int mem_access);
void check_deref_mem_store(JANUS_CONTEXT, instr_t* trigger, uint64_t bitmask_flags, uint64_t bitmask_reg,int src_id, int base_reg_id, int mem_access/*MEM_REF, CONST_MEM, ABS_MEM, CONST_ABS_MEM, ARITH_MEM*/);
//void check_deref_mem_store(JANUS_CONTEXT, instr_t* trigger, uint64_t bitmask_flags, uint64_t bitmask_reg);
void check_lea_mem(JANUS_CONTEXT, instr_t *trigger, uint64_t bitmask_flags, uint64_t bitmask_reg);
void check_lea_stack(JANUS_CONTEXT, instr_t *trigger, uint64_t bitmask_flags, uint64_t bitmask_reg);
void check_deref_global_mem_load(JANUS_CONTEXT, instr_t *trigger, uint64_t bitmask_flags, uint64_t bitmask_reg, int dest_id, uint64_t global_base, uint64_t global_bound, int mem_access);
void check_deref_global_mem_store(JANUS_CONTEXT, instr_t* trigger, uint64_t bitmask_flags, uint64_t bitmask_reg,int src_id, uint64_t global_base, uint64_t global_bound, int mem_access/*MEM_REF, CONST_MEM, ABS_MEM, CONST_ABS_MEM, ARITH_MEM*/);
