#include <iostream>
#include <cassert>
#include "dr_api.h"
#include "util.h"
using namespace std;
uint64_t vars[20];
uint64_t var_count=0;
void print(uint64_t x){
  printf("%ld\n",x);
}
void print(uint32_t x){
  printf("%d\n",x);
}
void print(int x){
  printf("%d\n",x);
}
void print(char* x){

  printf("%s\n",x);
}

opnd_t get_src1(instr_t *instr){
   assert(instr_num_srcs(instr) > 0 );
   return instr_get_src(instr,0);
}
opnd_t get_src2(instr_t *instr){
   assert(instr_num_srcs(instr) > 1 );
   return instr_get_src(instr,1);
}

app_pc get_nextaddr_full(void *dr_context, instr_t *instr){
    app_pc src_inst = instr_get_app_pc(instr);
    app_pc fall = (app_pc)decode_next_pc(dr_context, (byte *)src_inst);
    return fall;
}
opnd_t get_target_addr_indcall(instr_t * instr){
   opnd_t target_opnd = instr_get_target(instr);
   DR_ASSERT(opnd_is_reg(target_opnd) || opnd_is_memory_reference(target_opnd));
   reg_id_t target = opnd_get_reg(instr_get_target(instr));
   return target_opnd;
}
uintptr_t get_target_addr_call(instr_t * instr){
    uintptr_t target;
    opnd_t target_opnd = instr_get_target(instr);
    DR_ASSERT(instr_is_call(instr) || instr_is_ubr(instr));
    if (opnd_is_pc(instr_get_target(instr))) {
        /*if (opnd_is_far_pc(instr_get_target(instr))) {
        }*/
        target = (ptr_uint_t)opnd_get_pc(instr_get_target(instr));
    }/* else if (opnd_is_instr(instr_get_target(instr))) {
        instr_t *tgt = opnd_get_instr(instr_get_target(instr));
        target = (ptr_uint_t)instr_get_translation(tgt);
        if (opnd_is_far_instr(instr_get_target(instr))) {
        }
    }*/
    return target;
}

opnd_t get_dest(instr_t *instr){
   assert(instr_num_dsts(instr) > 0 );
   return instr_get_dst(instr,0);
}
bool mem_to_reg(instr_t *instr, opnd_t opnd){
    int num_dsts, num_srcs;
    num_dsts = instr_num_dsts(instr);
    for(int i=0; i< num_dsts; i++){
        opnd_t dst_opnd = instr_get_dst(instr, i);
        if(opnd_is_memory_reference(dst_opnd))
            return false;
    }
    return true;
}

opnd_t get_mem_opnd(instr_t *instr){
    int num_dsts, num_srcs;
    opnd_t mem_opnd;
    num_dsts = instr_num_dsts(instr);
    for(int i=0; i< num_dsts; i++){
        opnd_t dst_opnd = instr_get_dst(instr, i);
        if(opnd_is_memory_reference(dst_opnd))
            mem_opnd = dst_opnd;
    }
    num_srcs = instr_num_srcs(instr);
    for(int i=0; i< num_srcs; i++){
        opnd_t src_opnd = instr_get_src(instr, i);
        if(opnd_is_memory_reference(src_opnd))
            mem_opnd = src_opnd;
    }
    return mem_opnd;
}

uint64_t get_value_exp(dr_mcontext_t *mc, void *dr_context, instrlist_t *bb, opnd_t opnd, instr_t * instr){
  if(opnd_is_reg(opnd)){
     auto val = reg_get_value(opnd_get_reg(opnd), mc);
     return val;
 }
  else if (opnd_is_memory_reference(opnd)){
     dr_save_reg(dr_context, bb, instr, DR_REG_RSI, SPILL_SLOT_2);
     dr_save_reg(dr_context, bb, instr, DR_REG_RDI, SPILL_SLOT_3);
     load_effective_address(dr_context, bb, instr, opnd, DR_REG_RSI, DR_REG_RDI);
     auto val = reg_get_value(DR_REG_RSI, mc);
     dr_save_reg(dr_context, bb, instr, DR_REG_RSI, SPILL_SLOT_2);
     dr_save_reg(dr_context, bb, instr, DR_REG_RDI, SPILL_SLOT_3);
     return val;
 }
}

/*get lea*/
uint64_t get_addr_exp(void *dr_context, instrlist_t *bb, opnd_t opnd, instr_t * instr){
     //assert(opnd_is_memory_reference(opnd) &&  "ERROR: NOT a memory operand");
     uint64_t addr = 0x0;
     if(opnd_is_memory_reference(opnd)){
         dr_save_reg(dr_context, bb, instr, DR_REG_RSI, SPILL_SLOT_2);
         dr_save_reg(dr_context, bb, instr, DR_REG_RDI, SPILL_SLOT_3);
         
         load_effective_address(dr_context, bb, instr, opnd, DR_REG_RSI, DR_REG_RDI);
        
         dr_mcontext_t mc = {sizeof(mc), DR_MC_ALL};
         dr_get_mcontext(dr_context, &mc);
         addr = mc.rsi;
         dr_save_reg(dr_context, bb, instr, DR_REG_RSI, SPILL_SLOT_2);
         dr_save_reg(dr_context, bb, instr, DR_REG_RDI, SPILL_SLOT_3);
     }
     return (uint64_t)addr;
}
void get_addr_full(void *dr_context, instrlist_t *bb, opnd_t opnd, instr_t * instr){
     //assert(opnd_is_memory_reference(opnd) &&  "ERROR: NOT a memory operand");
     dr_save_reg(dr_context, bb, instr, DR_REG_RSI, SPILL_SLOT_2);
     dr_save_reg(dr_context, bb, instr, DR_REG_RDI, SPILL_SLOT_3);
         
     load_effective_address(dr_context, bb, instr, opnd, DR_REG_RSI, DR_REG_RDI);
     PRE_INSERT(bb, instr,
           INSTR_CREATE_mov_imm(dr_context,
                   opnd_create_reg(DR_REG_RDI),
                   OPND_CREATE_INTPTR((uint64_t)&vars[var_count])));
    /* Store address */
    PRE_INSERT(bb, instr,
            INSTR_CREATE_mov_st(dr_context,
                    OPND_CREATE_MEM64(DR_REG_RDI, 0),
                    opnd_create_reg(DR_REG_RSI)));
     var_count++;
     dr_save_reg(dr_context, bb, instr, DR_REG_RSI, SPILL_SLOT_2);
     dr_save_reg(dr_context, bb, instr, DR_REG_RDI, SPILL_SLOT_3);
}
uint64_t get_size(opnd_t opnd){

}

void get_mem_rw_addr_full(void *dr_context, instrlist_t *bb, instr_t * instr){
    int num_dsts, num_srcs;
    opnd_t mem_opnd;
    num_dsts = instr_num_dsts(instr);
    for(int i=0; i< num_dsts; i++){
        opnd_t dst_opnd = instr_get_dst(instr, i);
        if(opnd_is_memory_reference(dst_opnd))
            mem_opnd = dst_opnd;
    }
    num_srcs = instr_num_srcs(instr);
    for(int i=0; i< num_srcs; i++){
        opnd_t src_opnd = instr_get_src(instr, i);
        if(opnd_is_memory_reference(src_opnd))
            mem_opnd = src_opnd;
    }
     dr_save_reg(dr_context, bb, instr, DR_REG_RSI, SPILL_SLOT_2);
     dr_save_reg(dr_context, bb, instr, DR_REG_RDI, SPILL_SLOT_3);
         
     load_effective_address(dr_context, bb, instr, mem_opnd, DR_REG_RSI, DR_REG_RDI);
     PRE_INSERT(bb, instr,
           INSTR_CREATE_mov_imm(dr_context,
                   opnd_create_reg(DR_REG_RDI),
                   OPND_CREATE_INTPTR((uint64_t)&vars[var_count])));
        /* Store address */
    PRE_INSERT(bb, instr,
            INSTR_CREATE_mov_st(dr_context,
                    OPND_CREATE_MEM64(DR_REG_RDI, 0),
                    opnd_create_reg(DR_REG_RSI)));
     var_count++;
     dr_save_reg(dr_context, bb, instr, DR_REG_RSI, SPILL_SLOT_2);
     dr_save_reg(dr_context, bb, instr, DR_REG_RDI, SPILL_SLOT_3);
}
uint64_t
is_type_opnd(opnd_t opnd, int type){
  bool rtn_val = false;
  switch(type){
      case JVAR_MEMORY:
          rtn_val= opnd_is_memory_reference(opnd);
          break;
      case JVAR_REGISTER:
          rtn_val = opnd_is_reg(opnd);
          break;
      case JVAR_CONSTANT:
          rtn_val = opnd_is_immed(opnd);
          break;
      default:
          cout<<"INVALID type"<<endl;
      break;
  }
  if(rtn_val == false) return 0;
  else return 1;
}
