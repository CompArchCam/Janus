#include <iostream>
#include <fstream>
#include <cassert>
#include "dr_api.h"
#include "util.h"
using namespace std;
/*
    -1: 0 destination, 1 source
    0: 1 destination, 1 source, being the same
    1: 1 destination, 1 source, being different
    2: 1 destination, 2 sources, s * d -> s
    3: 2 destinations, 2 sources
*/
enum INSTR_OPND{
    ZERO_DST_ONE_SRC = -1,
    ZERO_DST_TWO_SRC,
    ONE_DST_ONE_SRC_SAME,
    ONE_DST_ONE_SRC_DIFF,
    ONE_DST_TWO_SRC,
    TWO_DST_TWO_SRC
};
std::map<int, int> opcode_ds = {
    {OP_call, TWO_DST_TWO_SRC},
    {OP_ret, ONE_DST_TWO_SRC},
    {OP_add, ONE_DST_TWO_SRC},
    {OP_sub, ONE_DST_TWO_SRC},
    {OP_mov_ld, ONE_DST_ONE_SRC_DIFF},
    {OP_mov_st, ONE_DST_ONE_SRC_DIFF},
    {OP_mov_imm, ONE_DST_ONE_SRC_DIFF},
    {OP_inc, ONE_DST_ONE_SRC_SAME},
    {OP_jmp, ZERO_DST_ONE_SRC},
    {OP_jl, ZERO_DST_ONE_SRC},
    {OP_jle, ZERO_DST_ONE_SRC},
    {OP_jnbe, ZERO_DST_ONE_SRC},
    {OP_jnb, ZERO_DST_ONE_SRC},
    {OP_jnle, ZERO_DST_ONE_SRC},
    {OP_jnl, ZERO_DST_ONE_SRC},
    {OP_jb, ZERO_DST_ONE_SRC},
    {OP_jo, ZERO_DST_ONE_SRC},
    {OP_jno, ZERO_DST_ONE_SRC},
    {OP_jz, ZERO_DST_ONE_SRC},
    {OP_jnz, ZERO_DST_ONE_SRC},
    {OP_js, ZERO_DST_ONE_SRC},
    {OP_jns, ZERO_DST_ONE_SRC},
    {OP_cmp, ZERO_DST_TWO_SRC}
};
uint64_t vars[20];
uint64_t var_count=0;
void* dr_ctext;
void print_u64(uint64_t x){
    cout << x << endl;
}
void print_u32(uint32_t x){
    cout << x << endl;
}
void print_i(int x){
    cout << x << endl;
}
void print_str(char* x){
    cout << x << endl;
}
opnd_t opnd(int x){
    return OPND_CREATE_INT32(x);
}
opnd_t opnd(uint64_t x){
    return OPND_CREATE_INT64(x);
}
opnd_t opnd(uint32_t x){
    return OPND_CREATE_INT32(x);
}
opnd_t opnd(opnd_t x){
    return x;
}
opnd_t opnd(opnd_t x, int offset, int size){
    if(opnd_is_reg(x)){
        reg_id_t reg = opnd_get_reg(x);
        return opnd_create_base_disp(reg, DR_REG_NULL, 0, offset, opnd_size_from_bytes(size));
    }
    return opnd_create_base_disp(DR_REG_NULL, DR_REG_NULL, 0, offset, opnd_size_from_bytes(size));
}
opnd_t opnd(struct instr &x){
    if(x.opcode == OP_LABEL){
        instr_t* label = INSTR_CREATE_label(dr_ctext);
        x.inst = label;
        return opnd_create_instr(label);
    } else {
        return opnd_create_null();
    }
}
opnd_t opnd_null(){
    return opnd_create_null();
}
opnd_t get_src1(instr_t *instr){
   assert(instr_num_srcs(instr) > 0 );
   return instr_get_src(instr,0);
}
opnd_t get_src2(instr_t *instr){
   assert(instr_num_srcs(instr) > 1 );
   return instr_get_src(instr,1);
}
int get_opcode(instr_t *instr){
    return instr_get_opcode(instr);
}
opnd_t transform_opnd(opnd_t opnd, opnd_size_t s){
    if(s == 4)
        return opnd_shrink_to_32_bits(opnd);
    if(s == 2)
        return opnd_shrink_to_16_bits(opnd_shrink_to_32_bits(opnd));
}
void validate_size(struct instr &x){
    if(opnd_is_memory_reference(x.src) || opnd_is_memory_reference(x.dst))
        return;
    if(x.opcode == OP_cmp){
        opnd_size_t size1 = opnd_size_in_bytes(opnd_get_size(x.src));
        opnd_size_t size2 = opnd_size_in_bytes(opnd_get_size(x.src2));
        if(size1 > size2){
            x.src = transform_opnd(x.src, size2);
        }
        if(size1 < size2){
            x.src2 = transform_opnd(x.src2, size1);
        }
    } else {
        opnd_size_t size1 = opnd_size_in_bytes(opnd_get_size(x.src));
        opnd_size_t size2 = opnd_size_in_bytes(opnd_get_size(x.dst));
        if(size1 > size2){
            x.src = transform_opnd(x.src, size2);
        }
        if(size1 < size2){
            x.dst = transform_opnd(x.dst, size1);
        }
        if(!opnd_is_null(x.src2)){
            opnd_size_t size3 = opnd_size_in_bytes(opnd_get_size(x.src2));
            if(size1 == size3)
                return;
            if(size1 > size3){
                x.src = transform_opnd(x.src, size3);
                x.dst = transform_opnd(x.dst, size3);
            }
            if(size1 < size3){
                x.src2 = transform_opnd(x.src2, size1);
            }
        }
    }
}
instr_t *create_instr(void *drcontext, struct instr &x){
    if(x.opcode == OP_LABEL){
        if(x.inst == NULL){
            instr_t* label = INSTR_CREATE_label(drcontext);
            x.inst = label;
            return label;
        } else {
            return x.inst;
        }
    }
    validate_size(x);
    if(x.opcode == OP_call)
        return XINST_CREATE_call(drcontext, x.dst);
    if(x.opcode == OP_ret)
        return XINST_CREATE_return(drcontext);
    if(opcode_ds[x.opcode] == ONE_DST_TWO_SRC){
        if(opnd_is_null(x.src2))
            return instr_create_1dst_2src(drcontext, x.opcode, x.dst, x.src, x.dst);
        else
            return instr_create_1dst_2src(drcontext, x.opcode, x.dst, x.src, x.src2);
    }
    if(opcode_ds[x.opcode] == ONE_DST_ONE_SRC_DIFF)
        return instr_create_1dst_1src(drcontext, x.opcode, x.dst, x.src);
    if(opcode_ds[x.opcode] == ONE_DST_ONE_SRC_SAME)
        return instr_create_1dst_1src(drcontext, x.opcode, x.dst, x.dst);
    if(opcode_ds[x.opcode] == ZERO_DST_ONE_SRC){
        return instr_create_0dst_1src(drcontext, x.opcode, x.src);
    }
    if(opcode_ds[x.opcode] == ZERO_DST_TWO_SRC)
        return instr_create_0dst_2src(drcontext, x.opcode, x.src, x.src2);
}
void replace(void *drcontext, instrlist_t *bb, instr_t *trigger, struct instr x){
    instr_t *new_instr = create_instr(drcontext, x);
    dr_print_instr(drcontext, STDOUT, trigger, "old instruction: ");
    instr_set_translation(new_instr, instr_get_app_pc(trigger));
    instrlist_replace(bb, trigger, new_instr);
    dr_print_instr(drcontext, STDOUT, new_instr, "replaced with: ");
    instr_destroy(drcontext, trigger);
}
void delete_instr(void *drcontext, instrlist_t *bb, instr_t *trigger){
    dr_print_instr(drcontext, STDOUT, trigger, "deleted instruction: ");
    instrlist_remove(bb, trigger);
    instr_destroy(drcontext, trigger);
}
void insert_before(void *drcontext, instrlist_t *bb, instr_t *trigger, struct instr x){
    instr_t *new_instr = create_instr(drcontext, x);
    dr_print_instr(drcontext, STDOUT, new_instr, "preinsert instruction: ");
    instrlist_preinsert(bb, trigger, new_instr);
}
void insert_after(void *drcontext, instrlist_t *bb, instr_t *trigger, struct instr x){
    instr_t *new_instr = create_instr(drcontext, x);
    dr_print_instr(drcontext, STDOUT, new_instr, "postinsert instruction: ");
    instrlist_postinsert(bb, trigger, new_instr);
}
void print_bb(void *drcontext, instrlist_t *bb, instr_t *trigger){
    instrlist_disassemble(drcontext, dr_fragment_app_pc(bb), bb, STDOUT);
}
void clear_bb(void *drcontext, instrlist_t *bb, instr_t *trigger){
    instrlist_clear(drcontext, bb);
}
void clear_bb_but_last(void *drcontext, instrlist_t *bb, instr_t *trigger){
    instr_t *last = instrlist_last(bb);
    last = instr_clone(drcontext, last);
    instrlist_clear(drcontext, bb);
    instrlist_prepend(bb, last);
}
void bb_append(void *drcontext, instrlist_t *bb, struct basicblock &sbb, struct instr &x){
    instr_t *new_instr = create_instr(drcontext, x);
    sbb.instrs.push_back(new_instr);
}
void bb_prepend(void *drcontext, instrlist_t *bb, struct basicblock &sbb, struct instr &x){
    instr_t *new_instr = create_instr(drcontext, x);
    sbb.instrs.push_front(new_instr);
}
void bb_append(void *drcontext, instrlist_t *bb, instr_t *trigger, struct instr x){
    instr_t *new_instr = create_instr(drcontext, x);
    dr_print_instr(drcontext, STDOUT, new_instr, "append instruction: ");
    instrlist_append(bb, new_instr);
}
void bb_prepend(void *drcontext, instrlist_t *bb, instr_t *trigger, struct instr x){
    instr_t *new_instr = create_instr(drcontext, x);
    dr_print_instr(drcontext, STDOUT, new_instr, "prepend instruction: ");
    instrlist_prepend(bb, new_instr);
}
void bb_append(void *drcontext, instrlist_t *bb, instr_t *trigger, struct basicblock sbb){
    for(instr_t *inst : sbb.instrs){
        dr_print_instr(drcontext, STDOUT, inst, "append instruction: ");
        instrlist_append(bb, inst);
    }
}
void bb_prepend(void *drcontext, instrlist_t *bb, instr_t *trigger, struct basicblock sbb){
    list<instr_t*>::reverse_iterator revit;
    for(revit=sbb.instrs.rbegin(); revit!=sbb.instrs.rend(); revit++){
        instrlist_prepend(bb, *revit);
    }
    for(instr_t* inst : sbb.instrs){
        dr_print_instr(drcontext, STDOUT, inst, "prepend instruction: ");
    }
}
void replace_bb(void *drcontext, instrlist_t *bb, instr_t *trigger, struct basicblock sbb){
    clear_bb(drcontext, bb, trigger);
    list<instr_t*>::reverse_iterator revit;
    for(instr_t* inst : sbb.instrs){
        dr_print_instr(drcontext, STDOUT, inst, "prepend instruction: ");
    }
    for(revit=sbb.instrs.rbegin(); revit!=sbb.instrs.rend(); revit++){
        instrlist_prepend(bb, *revit);
    }
}
void replace_bb_but_last(void *drcontext, instrlist_t *bb, instr_t *trigger, struct basicblock sbb){
    clear_bb_but_last(drcontext, bb, trigger);
    list<instr_t*>::reverse_iterator revit;
    for(instr_t* inst : sbb.instrs){
        dr_print_instr(drcontext, STDOUT, inst, "prepend instruction: ");
    }
    for(revit=sbb.instrs.rbegin(); revit!=sbb.instrs.rend(); revit++){
        instrlist_prepend(bb, *revit);
    }
}
void fileOpen(fstream* s, char* name){
    (*s).open(name, ios::out);
}
void fileClose(fstream *s){
    (*s).close();
}
void fileWrite(fstream *s, char* str){
    (*s) << str << endl;
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
uint64_t get_val(void *dr_context, instrlist_t *bb, opnd_t opnd, instr_t * instr){
  if (opnd_is_immed_int(opnd)){
      return opnd_get_immed_int(opnd);
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

void print_test(){
    cout<<"test"<<endl;
}
