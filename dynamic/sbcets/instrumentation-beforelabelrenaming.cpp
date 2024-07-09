#include<map>
#include<unordered_map>
#include "instrumentation.h"

#define SB_VERBOSE_DETAIL
#define REG_MIN DR_REG_RAX
//#define REG_MAX DR_REG_BND3
#define REG_MAX DR_REG_XMM0
int ptr_size = 0;
uint64_t ptr_base = 0;
uint64_t tmp_base = 0;
uint64_t LEAddr = 0;
uint64_t ptr_to_free = 0;
typedef std::unordered_map<int, pairdata> map_pair_int;
typedef std::unordered_map<uint64_t, pairdata> map_pair_uint64;
typedef std::unordered_map<uint64_t, int> map_int;
#define DEFAULT_SET     0xFF0FC7 // xmm0-7, RAX, RCX, RDX, RSI, RDI, R8, R9, R10,R11

std::unordered_map<int, pairdata>      reg_table;  //use unordered_map for faster lookup. 
std::unordered_map<uint64_t, pairdata>        memory_table;
std::unordered_map<int, pairdata>      split_reg_table;  //use unordered_map for faster lookup. 
std::unordered_map<uint64_t, pairdata>        split_memory_table;
std::unordered_map<int, pairdata>      reg_bounds;  //use unordered_map for faster lookup. 
std::unordered_map<uint64_t, pairdata>        mem_bounds;
std::unordered_map<uint64_t, int>                 freed;
std::stack<pairdata>       shadow_stack;
map_pair_int* reg_table_ptr = &reg_table;
map_int* freed_ptr = &freed;
//to avoid cache line matches
typedef struct state{    
    uint64_t xmm0;  uint64_t xmm0u; uint64_t _xmm0[6];
    uint64_t xmm1;  uint64_t xmm1u; uint64_t _xmm1[6];
    uint64_t xmm2;  uint64_t xmm2u; uint64_t _xmm2[6];
    uint64_t xmm3;  uint64_t xmm3u; uint64_t _xmm3[6];
    uint64_t xmm4;  uint64_t xmm4u; uint64_t _xmm4[6];
    uint64_t xmm5;  uint64_t xmm5u; uint64_t _xmm5[6];
    uint64_t xmm6;  uint64_t xmm6u; uint64_t _xmm6[6];
    uint64_t xmm7;  uint64_t xmm7u; uint64_t _xmm7[6];
    uint64_t xmm8;  uint64_t xmm8u; uint64_t _xmm8[6];
    uint64_t xmm9;  uint64_t xmm9u; uint64_t _xmm9[6];
    uint64_t xmm10;  uint64_t xmm10u; uint64_t _xmm10[6];
    uint64_t xmm11;  uint64_t xmm11u; uint64_t _xmm11[6];
    uint64_t xmm12;  uint64_t xmm12u; uint64_t _xmm12[6];
    uint64_t xmm13;  uint64_t xmm13u; uint64_t _xmm13[6];
    uint64_t xmm14;  uint64_t xmm14u; uint64_t _xmm14[6];
    uint64_t xmm15;  uint64_t xmm15u; uint64_t _xmm15[6];
    uint64_t rflags; uint64_t _rflags[7];
} xmm_reg_states;
void mapSet0(unordered_map<uint64_t,int> *m, uint64_t k, int v){
    (*m)[k]=v;
}
int mapGet0(unordered_map<uint64_t,int> *m, uint64_t k){
    return (*m)[k];
}
int* mapGet0ref(unordered_map<uint64_t,int> *m, uint64_t k){
    return &((*m)[k]);
}
unordered_map<uint64_t, int>::iterator mapFind0(unordered_map<uint64_t,int> *m, uint64_t k){
    return (*m).find(k);
}
size_t  mapCount0(unordered_map<uint64_t,int> *m, uint64_t k){
    return (*m).count(k);
}
void mapSet1(unordered_map<int,uint64_t> *m, int k, uint64_t v){
    (*m)[k]=v;
}
/*uint64_t mapGet1(unordered_map<int,uint64_t> *m, int k){
    return (*m)[k];
}*/
pairdata* mapGet1(unordered_map<int,pairdata> *m, int k){
    return &((*m)[k]);
}
pairdata* mapGet_test(unordered_map<int,pairdata> *m){
   cout<<"TESTING"<<endl;
   cout<<"val: "<<(*m)[1].first<<endl;
   return &((*m)[1]);
}
pairdata* mapGet2(unordered_map<uint64_t,pairdata> *m, uint64_t k){
    return &((*m)[k]);
}
pairdata mapGet2val(unordered_map<uint64_t,pairdata> *m, uint64_t k){
    return (*m)[k];
}
std::unordered_map<int, uint64_t>::iterator mapFind1(unordered_map<int,uint64_t> *m, int k){
    return (*m).find(k);
}
size_t  mapCount1(unordered_map<int,uint64_t> *m, int k){
    return (*m).count(k);
}

void pairSetValues(pairdata *p, uint64_t v1, uint64_t v2){
    (*p).first=v1;
}
void setRegValues(int k, uint64_t v1, uint64_t v2){
    reg_table[k].first =  v1;
    reg_table[k].second =  v2;
    return;
}
void copyRegValues(int src, int dest){
    reg_table[dest].first =  reg_table[src].first;
    reg_table[dest].second =  reg_table[src].second;
}
void copyRegToMem(int k, uint64_t addr){
    memory_table[addr].first =  reg_table[k].first;
    memory_table[addr].second =  reg_table[k].second;
}
void copyMemToReg(int k, uint64_t addr){
    reg_table[k].first =  memory_table[addr].first;
    reg_table[k].second =  memory_table[addr].second;
}
void setFreedValue(uint64_t addr){
    freed[addr] = 1;
}
void resetFreedValue(uint64_t addr){
   freed[addr] = 0;
}
int getFreedValue(uint64_t addr){
    return freed[addr];
}
void pairSetFirst(pairdata *p, uint64_t v){
    (*p).first=v;
}
void pairSetSecond(pairdata *p, uint64_t v){
    (*p).second=v;
}
void print(){
   cout<<"base: "<<reg_table[1].first<<endl;
   cout<<"bound: "<<reg_table[1].second<<endl;
}
uint64_t pairGetFirst(pairdata *p){
    return (*p).first;
}
uint64_t pairGetSecond(pairdata *p){
    return (*p).second;
}
unordered_map<int, pairdata>::iterator find_key(unordered_map<int, pairdata> &m, int k) {
    return m.find(k);
}
unordered_map<int, pairdata>::iterator get_iterator(unordered_map<int, pairdata> &m, int k) {
    return m.begin();
}
char is_key_found_regtable(int k) {
    auto it = reg_table.find(k);
    if(it == reg_table.end()){
        //cout<<"KEY NOT FOUND: "<<k<<endl;
        return 0;
    }
    else{
        return 1;
    }
}
char is_key_found_freed(uint64_t k) {
    auto it = freed.find(k);
    if(it == freed.end())
        return 0;
    else
        return 1;
}
char is_key_found_memtable(uint64_t k) {
    auto it = memory_table.find(k);
    if(it == memory_table.end())
        return 0;
    else
        return 1;
}

void map_erase_entry_regtab(int k){
      reg_table.erase(k);
}
void map_erase_entry_split_regtab(int k){
      split_reg_table.erase(k);
}
void map_erase_entry_memtab(uint64_t k){
      memory_table.erase(k);
}
void printBits(uint64_t num){
    uint64_t unit=1;
    uint64_t size = sizeof(num)*8;
    uint64_t maxPow = unit<<(size-1);
    int i=0,j;
    for(;i<size;++i){
         // print last bit and shift left.
        printf("%u",!!(num&maxPow));
        num = num<<1;
    }
    printf("\n");
}
bool inRegSet(uint64_t bits, uint32_t reg)
{
    if(reg >= 77) reg = reg - 60; //for XMM registers
    if((bits >> (reg-1)) & 1){
        return true;
    }
    if(bits == 0 || bits == 1){
        return true;
    }
    return false;
}
int default_gpr(uint32_t reg){
   if((1ULL<<(reg-1)) & 0x0FF7) {/*cout <<"reg " <<reg<<" in default set"<<endl;*/ return true;}
   else
       return false;

}
int find_free_reg(uint64_t bits){
    for (int reg = 2; reg < 16; reg++){
        if(reg == DR_REG_RSP || reg == DR_REG_RBP || default_gpr(reg) ) continue;
        if( ! ((bits >> (reg-1)) &  1)){
            return reg;
        }
    }
    return DR_REG_NULL;

}
bool inDefaultSaveSet(uint32_t reg){
   if((1ULL<<(reg-1)) & 0xFF0FC7) {/*cout <<"reg " <<reg<<" in default set"<<endl;*/ return true;}
   else
       return false;

}

int get_64bit(int id){
    if(id>16 && id<=32){ //32 bit version, EAX -- R15D
        return id-16;
    }
    else if(id>32 && id<=48){ //16 bit versions, AX --- R15W
        return id-32;
    }
    else if(id>48 && id<=52){   //8 bit version, AL, CL, DL, BL
        return id-48;
    }
    else if(id >52 && id<=56){ //8 bit version AH, CH, DH, BH
        return id-52;
    }
    else if(id>56 && id<=68){  //8 bit version low, R8L, R9L.... SIL, DIL
         return id-48;
    }
    return id;  
}
int get_32bit(int id){
    if(1<=id && id<=16) //64 bit version
        return id+16;
    else if(16<id && id<=32) //32 bit version
        return id;
    else if(32<id && id<=48) //16 bit version
        return id-16;
    return id;
}
int get_16bit(int id){
    if(1<=id && id<=16) //64 bit version
        return id+32;
    else if(16<id && id<=32) //32 bit version
        return id+16;
    else if(32<id && id<=48) //16 bit version
        return id;
    return id;
}
void printval(uint64_t val1, uint64_t val2){
   cout<<"val1: "<<hex<<val1<<" val2: "<<val2<<endl;
}
bool is_64bit(int id){
    if(1<=id && id<=16)
        return true;
    return false;
}
bool is_32bit(int id){
    if(16<id && id<=32)
        return true;
    return false;
}
bool is_16bit(reg_id_t id){
    if(32<id && id<=48)
        return true;
    return false;
}
void print_size(){
  cout<<"PTR SIZE: "<<ptr_size<<endl;
}
void print_base(){
  cout<<"PTR BASE: "<<hex<<ptr_base<<endl;
  cout<<"freed[base] "<<freed[ptr_base]<<endl;
  cout<<"reg_table[1].base = "<<reg_table[1].first<<endl;
  cout<<"reg_table[1].bound = "<<reg_table[1].second<<endl;
}
void print_base_2(uint64_t instr){
  cout<<"instr: "<<hex<< instr<<" PTR BASE: "<<hex<<ptr_base<<"PTR SIZE: "<<dec<<ptr_size/4<<endl;
}

xmm_reg_states spill_slots_xmm;
void save_xmm_reg(JANUS_CONTEXT, instr_t *trigger, uint64_t simd_mask, int s0){
    reg_id_t reg;
    int i, cacheline;

    PCAddress slots_addr = (PCAddress)(&(spill_slots_xmm));
    /* load the shared register to s0 */
    //cout<<"INSTR:"<<hex<<(uintptr_t)instr_get_app_pc(trigger)<<dec<<" s0: "<<s0<<endl;
    instrlist_meta_preinsert(bb, trigger,
        INSTR_CREATE_mov_imm(drcontext,
                             opnd_create_reg(s0),       //s0 is the free register
                             OPND_CREATE_INTPTR(slots_addr)));
    
   //for (reg=DR_REG_XMM0; reg<=DR_REG_XMM15; reg++) {
    for (reg=DR_REG_XMM0; reg<=DR_REG_XMM7; reg++) {
        i = reg - DR_REG_XMM0;
        cacheline = i;
        if (inRegSet(simd_mask,reg)) {
       //     cout<<"SAVING XMM"<<i<<endl;
            PRE_INSERT(bb, trigger,
                INSTR_CREATE_movdqu(drcontext,
                                    opnd_create_base_disp(s0, DR_REG_NULL, 0,
                                  cacheline*CACHE_LINE_WIDTH, OPSZ_16),
                                    opnd_create_reg(reg)));
        }
    }
}
void restore_xmm_reg(JANUS_CONTEXT, instr_t *trigger, uint64_t simd_mask, int s0){
    reg_id_t reg;
    int i, cacheline;

    PCAddress slots_addr = (PCAddress)(&(spill_slots_xmm));
        /* load the shared register bank to s0 */
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_mov_imm(drcontext,
                             opnd_create_reg(s0),       /*s0 is the free register*/
                             OPND_CREATE_INTPTR(slots_addr)));
    //for (reg=DR_REG_XMM0; reg<=DR_REG_XMM15; reg++) {
    for (reg=DR_REG_XMM0; reg<=DR_REG_XMM7; reg++) {
        i = reg - DR_REG_XMM0;
        cacheline = i;
        if (inRegSet(simd_mask, reg)) {
     //       cout<<"instr: "<<hex<<(uintptr_t)instr_get_app_pc(trigger)<<" bitmask: "<<simd_mask<<endl;
      //      cout<<"RESTORE XMM"<<i<<endl;
            PRE_INSERT(bb, trigger,
                INSTR_CREATE_movdqu(drcontext,
                                    opnd_create_reg(reg),
                                    opnd_create_base_disp(s0, DR_REG_NULL, 0,
                                  cacheline*CACHE_LINE_WIDTH, OPSZ_16)));
        }
    }
}



//BND_RECORD_SIZE_MALLOC
void record_size_malloc(JANUS_CONTEXT, instr_t *trigger){
    //cout<<"pointer addr of reg_table: "<<&reg_table<<endl;
    SAVE_REG(DR_REG_RAX,SPILL_SLOT_1);
    INSERT_move(opnd_create_reg(DR_REG_EAX), opnd_create_reg(DR_REG_EDI));
    INSERT_store(OPND_CREATE_ABSMEM((byte *)&ptr_size, OPSZ_4), opnd_create_reg(DR_REG_EAX));
    RESTORE_REG(DR_REG_RAX,SPILL_SLOT_1);
}
//BND_RECORD_SIZE_CALLOC
void record_size_calloc(JANUS_CONTEXT, instr_t *trigger){
    SAVE_REG(DR_REG_RAX,SPILL_SLOT_1);
    //TODO: check if RDI and RSI
    INSERT_move(opnd_create_reg(DR_REG_EAX), opnd_create_reg(DR_REG_EDI));
    INSERT_imul(opnd_create_reg(DR_REG_EAX), opnd_create_reg(DR_REG_ESI));
    INSERT_store(OPND_CREATE_ABSMEM((byte *)&ptr_size, OPSZ_4), opnd_create_reg(DR_REG_EAX));
    RESTORE_REG(DR_REG_RAX,SPILL_SLOT_1);
}
//BND_RECORD_BASE
void record_base_pointer(JANUS_CONTEXT, instr_t *trigger, uint64_t bitmask_flags, uint64_t bitmask_reg){
    instr_t *LBB34_2 = INSTR_CREATE_label(drcontext);
    SAVE_REG(DR_REG_RAX,SPILL_SLOT_1);
    SAVE_CALLEE_REG_SET1(bitmask_reg);
    SAVE_CALLEE_REG_SET2(bitmask_reg);
    if(bitmask_flags)
    {
        SAVE_ARITH_FLAGS(SPILL_SLOT_11);
        SAVE_REG(DR_REG_RAX,SPILL_SLOT_11);  
      //now we have RAX in SLOT_1 AND ARITH FLAGS IN SLOT_11, i also need RAX value that was used.
        RESTORE_REG(DR_REG_RAX,SPILL_SLOT_1); // 
    }
    //1. pairSetFirst(mapGet1(&reg_table,rax_reg_id),ptr_base); OR reg_table[rax].first = ptr_base;
    //reg_table[rax].first = ptr_base; reg_table[rax].second = ptr_base+ ptr_size;

    INSERT_store(OPND_CREATE_ABSMEM((byte *)&ptr_base, OPSZ_8), opnd_create_reg(DR_REG_RAX));
    INSERT_load_int( opnd_create_reg(DR_REG_EDI), OPND_CREATE_INT32((reg_id_t)DR_REG_RAX));
    INSERT_load( opnd_create_reg(DR_REG_RSI), OPND_CREATE_ABSMEM((byte *)&ptr_base, OPSZ_8));
    INSERT_load( opnd_create_reg(DR_REG_RDX), OPND_CREATE_ABSMEM((byte *)&ptr_size, OPSZ_8));
    INSERT_add(opnd_create_reg(DR_REG_RDX), opnd_create_reg(DR_REG_RSI));
    INSERT_call(opnd_create_pc((app_pc)(void *)&setRegValues));
  
    //freed[ptr_base] = 0;
    INSERT_load( opnd_create_reg(DR_REG_RDI), OPND_CREATE_ABSMEM((byte *)&ptr_base, OPSZ_8));
    INSERT_call(opnd_create_pc((app_pc)(void *)&resetFreedValue));
    
    INSERT_LABEL(LBB34_2);
   //INSERT_CLEAN_CALL(print_base_2,1, OPND_CREATE_INT64(instr_get_app_pc(trigger)));
    RESTORE_REG(DR_REG_RAX,SPILL_SLOT_1);
    RESTORE_CALLEE_REG_SET1(bitmask_reg);
    RESTORE_CALLEE_REG_SET2(bitmask_reg);
    if(bitmask_flags)
    {
        RESTORE_REG(DR_REG_RAX,SPILL_SLOT_11);
        RESTORE_ARITH_FLAGS(SPILL_SLOT_11);
        RESTORE_REG(DR_REG_RAX,SPILL_SLOT_1);
    }


}
//MONITOR_FREE_CALL
void monitor_free_call(JANUS_CONTEXT, instr_t *trigger,uint64_t bitmask_flags, uint64_t bitmask_reg){
    instr_t *LBB38_1 = INSTR_CREATE_label(drcontext);
    instr_t *LBB38_5 = INSTR_CREATE_label(drcontext);
    instr_t *LBB38_7 = INSTR_CREATE_label(drcontext);
    //saving in slots 1-10, arith flags in 11
    SAVE_REG(DR_REG_RAX,SPILL_SLOT_1);
    SAVE_CALLEE_REG_SET1(bitmask_reg);
    SAVE_CALLEE_REG_SET2(bitmask_reg);
    if(bitmask_flags){
        SAVE_ARITH_FLAGS(SPILL_SLOT_11);
        SAVE_REG(DR_REG_RAX,SPILL_SLOT_11);
        RESTORE_REG(DR_REG_RAX,SPILL_SLOT_1);
    }
    INSERT_store(OPND_CREATE_ABSMEM((byte *)&ptr_to_free, OPSZ_8), opnd_create_reg(DR_REG_RDI));
    INSERT_call(opnd_create_pc((byte *)&is_key_found_freed));
    INSERT_test( opnd_create_reg(DR_REG_AL),opnd_create_reg(DR_REG_AL));
    INSERT_jnz(LBB38_1);
    JUMP_to_label(LBB38_5);

    INSERT_LABEL(LBB38_1);
    INSERT_load( opnd_create_reg(DR_REG_RDI), OPND_CREATE_ABSMEM((byte *)&ptr_to_free, OPSZ_8));
    INSERT_call(opnd_create_pc((byte *)&getFreedValue));
    INSERT_cmp(opnd_create_reg(DR_REG_EAX),OPND_CREATE_INT32(1));
    INSERT_jnz(LBB38_5);
    
    //TODO: if(doublefree), only pass the value, not the memory address
    //INSERT_cmp(OPND_CREATE_ABSMEM((byte *)&double_free, OPSZ_4),OPND_CREATE_INT32(0));

    INCREMENT_error_counter;
    JUMP_to_label(LBB38_7);
    
    INSERT_LABEL(LBB38_5);
    INSERT_load( opnd_create_reg(DR_REG_RDI), OPND_CREATE_ABSMEM((byte *)&ptr_to_free, OPSZ_8));
    INSERT_call(opnd_create_pc((byte *)&setFreedValue));
    
    INSERT_LABEL(LBB38_7);
    RESTORE_REG(DR_REG_RAX,SPILL_SLOT_1);
    RESTORE_CALLEE_REG_SET1(bitmask_reg);
    RESTORE_CALLEE_REG_SET2(bitmask_reg);
    if(bitmask_flags){
        RESTORE_REG(DR_REG_RAX,SPILL_SLOT_11);
        RESTORE_ARITH_FLAGS(SPILL_SLOT_11);
        RESTORE_REG(DR_REG_RAX,SPILL_SLOT_1);
    }
}
std::map<int, int> save_restore_set;

void copy_reg_table(JANUS_CONTEXT, instr_t* trigger, uint64_t bitmask_flags, uint64_t bitmask_reg, int src_id, int dest_id){
    app_pc pc = instr_get_app_pc(trigger);
    instr_t *LBB33_2 = INSTR_CREATE_label(drcontext);
    instr_t *LBB33_1 = INSTR_CREATE_label(drcontext);
    instr_t *LBB33_3 = INSTR_CREATE_label(drcontext);
    SAVE_REG(DR_REG_RAX,SPILL_SLOT_1);          //RAX in SPILL_SLOT_9
    SAVE_CALLEE_REG_SET1(bitmask_reg);
   // SAVE_CALLEE_REG_SET2_RAX(bitmask_reg);
    SAVE_CALLEE_REG_SET2(bitmask_reg);
    if(bitmask_flags){
        SAVE_ARITH_FLAGS(SPILL_SLOT_12); //
        SAVE_REG(DR_REG_RAX,SPILL_SLOT_12); //arith flags in SPLILL_SLOT_12
  //      RESTORE_REG(DR_REG_RAX,SPILL_SLOT_9);
    }
    int free_reg = find_free_reg(bitmask_reg);
    //save_xmm_reg(janus_context, trigger, bitmask_reg, free_reg);
    //if( is_key_found(reg_table, src))
    // reg_table[dest].base = it->second.base; OR reg_table[dest].base = reg_table[src].base;
    INSERT_load_int( opnd_create_reg(DR_REG_EDI), OPND_CREATE_INT32(src_id));
    INSERT_call(opnd_create_pc((byte *)&is_key_found_regtable));
    INSERT_test( opnd_create_reg(DR_REG_AL),opnd_create_reg(DR_REG_AL));
    INSERT_jnz(LBB33_1); //src key is found
    //JUMP_to_label(LBB33_3); //TODO: LBB33_2, src key not found
    JUMP_to_label(LBB33_2); //src key not found

    INSERT_LABEL(LBB33_1);
    if(dest_id != src_id){
        INSERT_load_int( opnd_create_reg(DR_REG_ESI), OPND_CREATE_INT32(dest_id));
        INSERT_load_int( opnd_create_reg(DR_REG_EDI), OPND_CREATE_INT32(src_id));
        INSERT_call(opnd_create_pc((byte *)&copyRegValues));
        JUMP_to_label(LBB33_3);
    }
    //erase dest from reg
    //else. map_erase_entry(&reg_table, dest);
    //else. map_erase_entry(&split_reg_table, dest);
    INSERT_LABEL(LBB33_2);
    ERASE_dst_reg_from_reg_table(dest_id);
   // ERASE_dst_reg_from_split_reg_table(dest_id);
    //restore context
    INSERT_LABEL(LBB33_3);
   // RESTORE_REG(DR_REG_RSP,SPILL_SLOT_1);
    RESTORE_CALLEE_REG_SET1(bitmask_reg);
    RESTORE_CALLEE_REG_SET2(bitmask_reg);
    //RESTORE_CALLEE_REG_SET2_RAX(bitmask_reg);
    //RESTORE_XMM_SET(bitmask_reg);
   // restore_xmm_reg(janus_context, trigger, bitmask_reg, free_reg);
    if(bitmask_flags){
        RESTORE_REG(DR_REG_RAX,SPILL_SLOT_12); //
        RESTORE_ARITH_FLAGS(SPILL_SLOT_12); //SPILL_SLOT_12 -> arith flags, and arith flags to RAX
   //     RESTORE_REG(DR_REG_RAX,SPILL_SLOT_9);
    }
    RESTORE_REG(DR_REG_RAX,SPILL_SLOT_1);
}
void check_deref_mem_load(JANUS_CONTEXT, instr_t *trigger, uint64_t bitmask_flags, uint64_t bitmask_reg, int dest_id, int base_reg_id, int mem_access){

    opnd_t mem_operand;
    int i, num_srcs;
    
    num_srcs = instr_num_srcs(trigger);
    for(i=0; i< num_srcs; i++){
       opnd_t operand1 = instr_get_src(trigger, i);
       if(opnd_is_memory_reference(operand1))
          mem_operand = operand1;
    }
    instr_t *LBB51_1 = INSTR_CREATE_label(drcontext);
    instr_t *LBB51_3 = INSTR_CREATE_label(drcontext);
    instr_t *LBB51_5 = INSTR_CREATE_label(drcontext);
    instr_t *LBB51_6 = INSTR_CREATE_label(drcontext);
    instr_t *LBB51_7 = INSTR_CREATE_label(drcontext);
    instr_t *LBB51_8 = INSTR_CREATE_label(drcontext);
    instr_t *LBB51_12 = INSTR_CREATE_label(drcontext);
    instr_t *LBB51_14 = INSTR_CREATE_label(drcontext);
    instr_t *LBB51_17 = INSTR_CREATE_label(drcontext);
    instr_t *LBB51_18 = INSTR_CREATE_label(drcontext);
    instr_t *LBB51_19 = INSTR_CREATE_label(drcontext);
    instr_t *LBB51_20 = INSTR_CREATE_label(drcontext);
    instr_t *LBB51_22 = INSTR_CREATE_label(drcontext);
    instr_t *LBB51_30 = INSTR_CREATE_label(drcontext);
   
    //LOAD_EFFECTIVE_ADDR_LEADDR(mem_operand);
    //START. save reg, flags and increment SP

    SAVE_REG(DR_REG_RSP,SPILL_SLOT_1);
    SAVE_CALLEE_REG_SET1(bitmask_reg);
    SAVE_CALLEE_REG_SET2_RAX(bitmask_reg);
    if(bitmask_flags){
        SAVE_ARITH_FLAGS(SPILL_SLOT_12);
        SAVE_REG(DR_REG_RAX,SPILL_SLOT_12);
        RESTORE_REG(DR_REG_RAX,SPILL_SLOT_9);
    }
    INSERT_sub(opnd_create_reg(DR_REG_RSP), OPND_CREATE_INT32(24));

    LOAD_EFFECTIVE_ADDR_LEADDR(mem_operand);            //puts address in RSI register, and then in LEAddr
    //A. if(is_key_found(memory_table, LEAddr))
    INSERT_load(opnd_create_reg(DR_REG_RDI), OPND_CREATE_ABSMEM((byte *)&LEAddr, OPSZ_8));
    INSERT_call(opnd_create_pc((byte *)&is_key_found_memtable));
    INSERT_test(opnd_create_reg(DR_REG_AL), opnd_create_reg(DR_REG_AL));
    INSERT_jnz(LBB51_1); //if key found (result of AND is not zero)
    if(mem_access == ABS_MEM)
        JUMP_to_label(LBB51_19);
    else{
        if(base_reg_id != DR_REG_NULL)
            JUMP_to_label(LBB51_7);
        else
            JUMP_to_label(LBB51_19);
    }
    // A1. base = memory_table[LEAddr].base; OR base = pairGetFirst(mapGet2(&memory_table, LEAddr));
    //LBB51_1:  
    INSERT_LABEL(LBB51_1);
    INSERT_load(opnd_create_reg(DR_REG_RSI), OPND_CREATE_ABSMEM((byte *)&LEAddr, OPSZ_8));
    INSERT_load_int(opnd_create_reg(DR_REG_RDI), OPND_CREATE_INT64((uint64_t)&memory_table));
    INSERT_call(opnd_create_pc((byte *)&mapGet2));   //memory_table[LEAddr]
    INSERT_move(opnd_create_reg(DR_REG_RDI), opnd_create_reg(DR_REG_RAX));
    SAVE_REG(DR_REG_RDI,SPILL_SLOT_13);
    INSERT_call(opnd_create_pc((byte *)&pairGetFirst)); //memory_table[LEAddr].base
    INSERT_store(OPND_CREATE_MEM64(DR_REG_RSP, 16), opnd_create_reg(DR_REG_RAX));
    // A2. bound = memory_table[LEAddr].bound;  OR bound = pairGetSecond(mapGet2(&memory_table, LEAddr));
    RESTORE_REG(DR_REG_RDI,SPILL_SLOT_13);
    INSERT_call(opnd_create_pc((byte *)&pairGetSecond)); ////memory_table[LEAddr].bound
    INSERT_store(OPND_CREATE_MEM64(DR_REG_RSP, 8), opnd_create_reg(DR_REG_RAX));
    //A3.1. if(is_key_found(freed, base) && mapGet0(&freed, base) == 1)
    INSERT_load(opnd_create_reg(DR_REG_RDI), OPND_CREATE_MEM64(DR_REG_RSP, 16));
    INSERT_call(opnd_create_pc((byte *)&is_key_found_freed));
    INSERT_test(opnd_create_reg(DR_REG_AL),opnd_create_reg(DR_REG_AL));
    INSERT_jnz(LBB51_3);
    
    if(mem_access == ARITH_MEM)
        JUMP_to_label(LBB51_19); //remove dest reg
    else 
        JUMP_to_label(LBB51_5);
    //if mapGet0(&freed, base) == 1
    //LBB51_3:
    
    INSERT_LABEL(LBB51_3);
    INSERT_load(opnd_create_reg(DR_REG_RDI), OPND_CREATE_MEM64(DR_REG_RSP, 16));
    INSERT_call(opnd_create_pc((byte *)&getFreedValue)); //freed[base]
    INSERT_cmp(opnd_create_reg(DR_REG_EAX),OPND_CREATE_INT32(1)); //if both are same i.e. freed=1, zf=1 increment error
    if(mem_access == ARITH_MEM)
        INSERT_jnz(LBB51_19);
    else
        INSERT_jnz(LBB51_5);
    INCREMENT_error_counter;
    if(mem_access == ARITH_MEM)
        JUMP_to_label(LBB51_19); //remove dest reg
    //LBB51_5:
    INSERT_LABEL(LBB51_5);
    INSERT_load_int(opnd_create_reg(DR_REG_EDI), OPND_CREATE_INT32(dest_id));
    INSERT_load(opnd_create_reg(DR_REG_RSI), OPND_CREATE_MEM64(DR_REG_RSP, 16)); //base
    INSERT_load(opnd_create_reg(DR_REG_RDX), OPND_CREATE_MEM64(DR_REG_RSP, 8));  //bound
    INSERT_call(opnd_create_pc((byte *)&setRegValues));
    JUMP_to_label(LBB51_22);
    //B. else if(is_key_found(reg_table, base_reg_id) && (base_reg_id != DR_REG_RBP && base_reg != DR_REG_RSP)){
    //LBB51_7:
    INSERT_LABEL(LBB51_7);
    INSERT_load_int(opnd_create_reg(DR_REG_EDI), OPND_CREATE_INT32(base_reg_id));
    INSERT_call(opnd_create_pc((byte *)&is_key_found_regtable));
    INSERT_test(opnd_create_reg(DR_REG_AL), opnd_create_reg(DR_REG_AL));
    if(mem_access  == ARITH_MEM)
        INSERT_jnz(LBB51_30);
    else
        INSERT_jnz(LBB51_8);
    JUMP_to_label(LBB51_19);
    //LBB51_8:
    INSERT_LABEL(LBB51_8);
    INSERT_load_int(opnd_create_reg(DR_REG_EAX), OPND_CREATE_INT32(base_reg_id));
    INSERT_cmp(opnd_create_reg(DR_REG_EAX),OPND_CREATE_INT32(DR_REG_RBP));
    INSERT_jz(LBB51_19);
    INSERT_load_int(opnd_create_reg(DR_REG_EAX), OPND_CREATE_INT32(base_reg_id));
    INSERT_cmp(opnd_create_reg(DR_REG_EAX),OPND_CREATE_INT32(DR_REG_RSP));
    INSERT_jz(LBB51_19);
    INSERT_LABEL(LBB51_30);      //NEW for ARITH_MEM
    //B1. base = reg_table[base_reg].base; OR base = pairGetFirst(mapGet1(&reg_table, base_reg));
    INSERT_load_int(opnd_create_reg(DR_REG_ESI), OPND_CREATE_INT32(base_reg_id));
    INSERT_load_int(opnd_create_reg(DR_REG_RDI), OPND_CREATE_INT64((uint64_t)&reg_table));
    INSERT_call(opnd_create_pc((byte *)&mapGet1));
    INSERT_move(opnd_create_reg(DR_REG_RDI), opnd_create_reg(DR_REG_RAX));
    SAVE_REG(DR_REG_RDI,SPILL_SLOT_13);
    INSERT_call(opnd_create_pc((byte *)&pairGetFirst));
    INSERT_store(OPND_CREATE_MEM64(DR_REG_RSP, 16), opnd_create_reg(DR_REG_RAX));
    RESTORE_REG(DR_REG_RDI,SPILL_SLOT_13);
    //B2. bound = reg_table[base_reg].bound; OR bound = pairGetSecond(mapGet1(&reg_table, base_reg));
    INSERT_call(opnd_create_pc((byte *)&pairGetSecond));
    INSERT_store(OPND_CREATE_MEM64(DR_REG_RSP, 8), opnd_create_reg(DR_REG_RAX));
    //B3.1. if(is_key_found(freed, base) && mapGet0(&freed, base) == 1){ error_counter++}
    INSERT_load(opnd_create_reg(DR_REG_RDI), OPND_CREATE_MEM64(DR_REG_RSP, 16));
    INSERT_call(opnd_create_pc((byte *)&is_key_found_freed));
    INSERT_test(opnd_create_reg(DR_REG_AL), opnd_create_reg(DR_REG_AL));
    INSERT_jnz(LBB51_12);
    JUMP_to_label(LBB51_14);
    INSERT_LABEL(LBB51_12);
    INSERT_load(opnd_create_reg(DR_REG_RDI), OPND_CREATE_MEM64(DR_REG_RSP, 16));
    INSERT_call(opnd_create_pc((byte *)&getFreedValue));
    INSERT_cmp(opnd_create_reg(DR_REG_EAX),OPND_CREATE_INT32(1));
    INSERT_jnz(LBB51_14);
    INCREMENT_error_counter;
    INSERT_LABEL(LBB51_14);
    //B4. if( LEAddr < base || LEAddr > bound){ error_counter++}
    INSERT_load(opnd_create_reg(DR_REG_RAX), OPND_CREATE_ABSMEM((byte *)&LEAddr, OPSZ_8));
    INSERT_cmp(opnd_create_reg(DR_REG_RAX),OPND_CREATE_MEM64(DR_REG_RSP, 16));
    INSERT_jb(LBB51_17);
    INSERT_load(opnd_create_reg(DR_REG_RAX), OPND_CREATE_ABSMEM((byte *)&LEAddr, OPSZ_8));
    INSERT_cmp(opnd_create_reg(DR_REG_RAX),OPND_CREATE_MEM64(DR_REG_RSP, 8));
    INSERT_jbe(LBB51_18);
    //LBB51_17:
    INSERT_LABEL(LBB51_17);
    INCREMENT_error_counter;
    //LBB51_18:
    INSERT_LABEL(LBB51_18);
    ERASE_dst_reg_from_reg_table(dest_id);
    ERASE_dst_reg_from_split_reg_table(dest_id);
    JUMP_to_label(LBB51_22);
    //C. else if(is_key_found(reg_table, dest_id)){remove dest_id from reg tables} 
    //LBB51_19:
    INSERT_LABEL(LBB51_19);
    INSERT_load_int(opnd_create_reg(DR_REG_EDI), OPND_CREATE_INT32(dest_id));
    INSERT_call(opnd_create_pc((byte *)&is_key_found_regtable));
    INSERT_test(opnd_create_reg(DR_REG_AL),opnd_create_reg(DR_REG_AL));
    INSERT_jnz(LBB51_20);
    JUMP_to_label(LBB51_22);
    //LBB51_20:
    INSERT_LABEL(LBB51_20);
    ERASE_dst_reg_from_reg_table(dest_id);
    ERASE_dst_reg_from_split_reg_table(dest_id);
    //END. restore SP, registers and flags
    //LBB51_22:

    INSERT_LABEL(LBB51_22); //TODO : redundant
    INSERT_add(opnd_create_reg(DR_REG_RSP), OPND_CREATE_INT32(24));
    RESTORE_REG(DR_REG_RSP,SPILL_SLOT_1);
    RESTORE_CALLEE_REG_SET1(bitmask_reg);
    RESTORE_CALLEE_REG_SET2_RAX(bitmask_reg);
    if(bitmask_flags){
        RESTORE_REG(DR_REG_RAX,SPILL_SLOT_12);
        RESTORE_ARITH_FLAGS(SPILL_SLOT_12);
        RESTORE_REG(DR_REG_RAX,SPILL_SLOT_9);
    }
}
void remove_reg_table(JANUS_CONTEXT, instr_t *trigger,uint64_t bitmask_flags, uint64_t bitmask_reg , int dest_id){
    //START
    SAVE_CALLEE_REG_SET1(bitmask_reg);
    SAVE_REG(DR_REG_RAX,SPILL_SLOT_12);
    //SAVE_REG(DR_REG_RAX,SPILL_SLOT_8);
    SAVE_CALLEE_REG_SET2(bitmask_reg);
    if(bitmask_flags){
        SAVE_ARITH_FLAGS(SPILL_SLOT_11); //arith flags => RAX, RAX => SPILL_SLOT_11
        SAVE_REG(DR_REG_RAX,SPILL_SLOT_11); //arith flags => SPILL_SLOT_11
        RESTORE_REG(DR_REG_RAX,SPILL_SLOT_12);  // now RAX has its original value, plus SPILL_SLOT_12 as well.
    }
    //RESTORE_REG(DR_REG_RAX,SPILL_SLOT_8);
    //A. 
    ERASE_dst_reg_from_reg_table(dest_id);
    ERASE_dst_reg_from_split_reg_table(dest_id);
    
    //END
    RESTORE_REG(DR_REG_RAX,SPILL_SLOT_12);      //original value of RAX§
    //RESTORE_REG(DR_REG_RAX,SPILL_SLOT_8);
    RESTORE_CALLEE_REG_SET2(bitmask_reg);
    if(bitmask_flags){
        RESTORE_REG(DR_REG_RAX,SPILL_SLOT_11); //arith_flags (SPILL_SLOT_11) => RAX
        RESTORE_ARITH_FLAGS(SPILL_SLOT_11);    // RAX(arith_flags original value) => arith_flags, SPILL_SLOT_11 =>rax 
        RESTORE_REG(DR_REG_RAX,SPILL_SLOT_12);  //RAX = orginal value
    }
    RESTORE_CALLEE_REG_SET1(bitmask_reg);
    //RESTORE_REG(DR_REG_RAX,SPILL_SLOT_8);
}
void copy_global_bounds(JANUS_CONTEXT, instr_t *trigger, int dest_id, uint64_t base/*base*/, uint64_t total_bound/*size*/, uint64_t bitmask_reg){
    opnd_t reg_operand, dest;
    int i, num_dsts, size;
    num_dsts = instr_num_dsts(trigger);
    for(i=0; i< num_dsts; i++){
       reg_operand = instr_get_dst(trigger, i);
       if(opnd_is_reg(reg_operand))
          dest = reg_operand;
    }
    dest_id = get_64bit(opnd_get_reg(dest));

    if(inRegSet(bitmask_reg,7)) SAVE_REG(DR_REG_RSI,SPILL_SLOT_1);
    if(inRegSet(bitmask_reg,8)) SAVE_REG(DR_REG_RDI,SPILL_SLOT_2);
    if(inRegSet(bitmask_reg,11)) SAVE_REG(DR_REG_R10,SPILL_SLOT_3);
    if(inRegSet(bitmask_reg,12)) SAVE_REG(DR_REG_R11,SPILL_SLOT_4);
    if(inRegSet(bitmask_reg,16)) SAVE_REG(DR_REG_R15,SPILL_SLOT_5);
    if(inRegSet(bitmask_reg,9)) SAVE_REG(DR_REG_R8,SPILL_SLOT_6);
    if(inRegSet(bitmask_reg,10)) SAVE_REG(DR_REG_R9,SPILL_SLOT_7);
    SAVE_REG(DR_REG_RAX,SPILL_SLOT_8);
    if(inRegSet(bitmask_reg,2)) SAVE_REG(DR_REG_RCX,SPILL_SLOT_9);
    if(inRegSet(bitmask_reg,3)) SAVE_REG(DR_REG_RDX,SPILL_SLOT_10);
    SAVE_ARITH_FLAGS(SPILL_SLOT_11);
    SAVE_REG(DR_REG_RAX,SPILL_SLOT_11);
    RESTORE_REG(DR_REG_RAX,SPILL_SLOT_8);
    INSERT_push(opnd_create_reg(DR_REG_RAX));
    INSERT_load_int(opnd_create_reg(DR_REG_ESI), OPND_CREATE_INT32(dest_id));
    INSERT_load_int(opnd_create_reg(DR_REG_RDI), OPND_CREATE_INT64((uint64_t)&reg_table));
    INSERT_call(opnd_create_pc((byte *)&mapGet1));
    INSERT_move(opnd_create_reg(DR_REG_RDI), opnd_create_reg(DR_REG_RAX));
    INSERT_load_int(opnd_create_reg(DR_REG_RSI), OPND_CREATE_INTPTR(base));
    INSERT_call(opnd_create_pc((byte *)&pairSetFirst));
    INSERT_load_int(opnd_create_reg(DR_REG_ESI), OPND_CREATE_INT32(dest_id));
    INSERT_load_int(opnd_create_reg(DR_REG_RDI), OPND_CREATE_INT64((uint64_t)&reg_table));
    INSERT_call(opnd_create_pc((byte *)&mapGet1));
    INSERT_move(opnd_create_reg(DR_REG_RDI), opnd_create_reg(DR_REG_RAX));
    INSERT_load_int(opnd_create_reg(DR_REG_RSI), OPND_CREATE_INTPTR(total_bound));
    INSERT_call(opnd_create_pc((byte *)&pairSetSecond));
    INSERT_pop(opnd_create_reg(DR_REG_RAX));
    if(inRegSet(bitmask_reg,7)) RESTORE_REG(DR_REG_RSI,SPILL_SLOT_1);
    if(inRegSet(bitmask_reg,8)) RESTORE_REG(DR_REG_RDI,SPILL_SLOT_2);
    if(inRegSet(bitmask_reg,11)) RESTORE_REG(DR_REG_R10,SPILL_SLOT_3);
    if(inRegSet(bitmask_reg,12)) RESTORE_REG(DR_REG_R11,SPILL_SLOT_4);
    if(inRegSet(bitmask_reg,16)) RESTORE_REG(DR_REG_R15,SPILL_SLOT_5);
    if(inRegSet(bitmask_reg,9)) RESTORE_REG(DR_REG_R8,SPILL_SLOT_6);
    if(inRegSet(bitmask_reg,10)) RESTORE_REG(DR_REG_R9,SPILL_SLOT_7);
    RESTORE_REG(DR_REG_RAX,SPILL_SLOT_8);
    if(inRegSet(bitmask_reg,2)) RESTORE_REG(DR_REG_RCX,SPILL_SLOT_9);
    if(inRegSet(bitmask_reg,3)) RESTORE_REG(DR_REG_RDX,SPILL_SLOT_10);
    RESTORE_REG(DR_REG_RAX,SPILL_SLOT_11);
    RESTORE_ARITH_FLAGS(SPILL_SLOT_11);
    RESTORE_REG(DR_REG_RAX,SPILL_SLOT_8);
}
void check_deref_mem_store(JANUS_CONTEXT, instr_t* trigger, uint64_t bitmask_flags, uint64_t bitmask_reg,int src_id, int base_reg_id, int mem_access/*MEM_REF, CONST_MEM, ABS_MEM, CONST_ABS_MEM, ARITH_MEM*/){
    
    opnd_t mem_operand;
    int i, num_dsts;

    num_dsts = instr_num_dsts(trigger);
    for(i=0; i< num_dsts; i++){
       opnd_t operand1 = instr_get_dst(trigger, i);
       if(opnd_is_memory_reference(operand1))
          mem_operand = operand1;
    }


    instr_t *LBB55_1 = INSTR_CREATE_label(drcontext);
    instr_t *LBB55_3 = INSTR_CREATE_label(drcontext);
    instr_t *LBB55_6 = INSTR_CREATE_label(drcontext);
    instr_t *LBB55_7 = INSTR_CREATE_label(drcontext);
    instr_t *LBB55_8 = INSTR_CREATE_label(drcontext);
    instr_t *LBB55_10 = INSTR_CREATE_label(drcontext);
    instr_t *LBB55_13 = INSTR_CREATE_label(drcontext);
    instr_t *LBB55_14 = INSTR_CREATE_label(drcontext);
    instr_t *LBB55_15 = INSTR_CREATE_label(drcontext);
    instr_t *LBB55_17 = INSTR_CREATE_label(drcontext);
    instr_t *LBB55_20 = INSTR_CREATE_label(drcontext);
    instr_t *LBB55_22 = INSTR_CREATE_label(drcontext);
    instr_t *LBB55_26 = INSTR_CREATE_label(drcontext);

    SAVE_REG(DR_REG_RSP,SPILL_SLOT_1);
    SAVE_CALLEE_REG_SET1(bitmask_reg);
    SAVE_CALLEE_REG_SET2_RAX(bitmask_reg);
    if(bitmask_flags){
        SAVE_ARITH_FLAGS(SPILL_SLOT_12);
        SAVE_REG(DR_REG_RAX,SPILL_SLOT_12);
        RESTORE_REG(DR_REG_RAX,SPILL_SLOT_9);
    }
    LOAD_EFFECTIVE_ADDR_LEADDR(mem_operand);
    

    INSERT_sub(opnd_create_reg(DR_REG_RSP), OPND_CREATE_INT32(24));
    
    //if(reg_table.find(src) != reg_table.end())
    if(mem_access == MEM_REF_STORE || mem_access == ABS_MEM_STORE ){
        INSERT_load_int(opnd_create_reg(DR_REG_EDI), OPND_CREATE_INT32(src_id));
        INSERT_call(opnd_create_pc((byte *)&is_key_found_regtable));
        INSERT_test(opnd_create_reg(DR_REG_AL), opnd_create_reg(DR_REG_AL));
        INSERT_jnz(LBB55_1); //if key found (result of AND is not zero)
        JUMP_to_label(LBB55_7); //if key not found
        
        INSERT_LABEL(LBB55_1);
        INSERT_load_int(opnd_create_reg(DR_REG_ESI), OPND_CREATE_INT32(src_id));
        INSERT_load_int(opnd_create_reg(DR_REG_RDI), OPND_CREATE_INT64((uint64_t)&reg_table));
        INSERT_call(opnd_create_pc((byte *)&mapGet1));
        INSERT_move(opnd_create_reg(DR_REG_RDI), opnd_create_reg(DR_REG_RAX));
        SAVE_REG(DR_REG_RDI,SPILL_SLOT_13);
        INSERT_call(opnd_create_pc((byte *)&pairGetFirst));
        INSERT_store(OPND_CREATE_MEM64(DR_REG_RSP, 16), opnd_create_reg(DR_REG_RAX));
        RESTORE_REG(DR_REG_RDI,SPILL_SLOT_13);
        INSERT_call(opnd_create_pc((byte *)&pairGetSecond));
        INSERT_store(OPND_CREATE_MEM64(DR_REG_RSP, 8), opnd_create_reg(DR_REG_RAX));
        
        INSERT_load(opnd_create_reg(DR_REG_RDI), OPND_CREATE_MEM64(DR_REG_RSP, 16));
        INSERT_call(opnd_create_pc((byte *)&is_key_found_freed));
        INSERT_test(opnd_create_reg(DR_REG_AL), opnd_create_reg(DR_REG_AL));
        INSERT_jnz(LBB55_3);
        JUMP_to_label(LBB55_6);
        INSERT_LABEL(LBB55_3);
        INSERT_load(opnd_create_reg(DR_REG_RSI), OPND_CREATE_MEM64(DR_REG_RSP, 16));
        INSERT_call(opnd_create_pc((byte *)&getFreedValue));
        INSERT_cmp(opnd_create_reg(DR_REG_EAX),OPND_CREATE_INT32(1));
        INSERT_jnz(LBB55_6);
        INCREMENT_error_counter;
        // memory_table[LEAddr].base = base; memory_table[LEAddr].bound = bound;
        INSERT_LABEL(LBB55_6);
        INSERT_load(opnd_create_reg(DR_REG_RSI), OPND_CREATE_ABSMEM((byte *)&LEAddr, OPSZ_8));
        INSERT_load_int(opnd_create_reg(DR_REG_RDI), OPND_CREATE_INT64((uint64_t)&memory_table));
        INSERT_call(opnd_create_pc((byte *)&mapGet2));
        INSERT_move(opnd_create_reg(DR_REG_RDI), opnd_create_reg(DR_REG_RAX));
        SAVE_REG(DR_REG_RDI,SPILL_SLOT_13);
        INSERT_load(opnd_create_reg(DR_REG_RSI), OPND_CREATE_MEM64(DR_REG_RSP, 16));
        INSERT_call(opnd_create_pc((byte *)&pairSetFirst));
        RESTORE_REG(DR_REG_RDI,SPILL_SLOT_13);
        INSERT_load(opnd_create_reg(DR_REG_RSI), OPND_CREATE_MEM64(DR_REG_RSP, 8));
        INSERT_call(opnd_create_pc((byte *)&pairSetSecond));
        JUMP_to_label(LBB55_26);
    }
    if( mem_access != ARITH_MEM_STORE ){
        INSERT_LABEL(LBB55_7);
        //if(memory_table.find(LEAddr) != memory_table.end())
        INSERT_load(opnd_create_reg(DR_REG_RDI), OPND_CREATE_ABSMEM((byte *)&LEAddr, OPSZ_8));
        INSERT_call(opnd_create_pc((byte *)&is_key_found_memtable));
        INSERT_test(opnd_create_reg(DR_REG_AL), opnd_create_reg(DR_REG_AL));
        INSERT_jnz(LBB55_8);
        if(mem_access == ABS_MEM_STORE || mem_access == CONST_ABS_MEM_STORE)
            JUMP_to_label(LBB55_26);
        else
            JUMP_to_label(LBB55_14);
        INSERT_LABEL(LBB55_8);
        INSERT_load(opnd_create_reg(DR_REG_RSI), OPND_CREATE_ABSMEM((byte *)&LEAddr, OPSZ_8));
        INSERT_load_int(opnd_create_reg(DR_REG_RDI), OPND_CREATE_INT64((uint64_t)&memory_table));
        INSERT_call(opnd_create_pc((byte *)&mapGet2));
        INSERT_move(opnd_create_reg(DR_REG_RDI), opnd_create_reg(DR_REG_RAX));
        SAVE_REG(DR_REG_RDI,SPILL_SLOT_13);
        INSERT_call(opnd_create_pc((byte *)&pairGetFirst));
        INSERT_store(OPND_CREATE_MEM64(DR_REG_RSP, 16), opnd_create_reg(DR_REG_RAX));
        RESTORE_REG(DR_REG_RDI,SPILL_SLOT_13);
        INSERT_call(opnd_create_pc((byte *)&pairGetSecond));
        INSERT_store(OPND_CREATE_MEM64(DR_REG_RSP, 8), opnd_create_reg(DR_REG_RAX));
        INSERT_load(opnd_create_reg(DR_REG_RDI), OPND_CREATE_MEM64(DR_REG_RSP, 16));
        INSERT_call(opnd_create_pc((byte *)&is_key_found_freed));
        INSERT_test(opnd_create_reg(DR_REG_AL), opnd_create_reg(DR_REG_AL));
        INSERT_jnz(LBB55_10);
        JUMP_to_label(LBB55_13);
        INSERT_LABEL(LBB55_10);
        INSERT_load(opnd_create_reg(DR_REG_RDI), OPND_CREATE_MEM64(DR_REG_RSP, 16));
        INSERT_call(opnd_create_pc((byte *)&getFreedValue));
        INSERT_cmp(opnd_create_reg(DR_REG_EAX),OPND_CREATE_INT32(1));
        INSERT_jnz(LBB55_13);
        INCREMENT_error_counter;
        INSERT_LABEL(LBB55_13);
        ERASE_lea_addr_from_memory_table;
        JUMP_to_label(LBB55_26);
    }
    //if(reg_table.find(base_reg) != reg_table.end())
    INSERT_LABEL(LBB55_14);
    if(base_reg_id != DR_REG_NULL){
        INSERT_load_int(opnd_create_reg(DR_REG_EDI), OPND_CREATE_INT32(base_reg_id));
        INSERT_call(opnd_create_pc((byte *)&is_key_found_regtable));
        INSERT_test(opnd_create_reg(DR_REG_AL), opnd_create_reg(DR_REG_AL));
        INSERT_jnz(LBB55_15);
        JUMP_to_label(LBB55_26);
        INSERT_LABEL(LBB55_15);
        INSERT_load_int(opnd_create_reg(DR_REG_ESI), OPND_CREATE_INT32(base_reg_id));
        INSERT_load_int(opnd_create_reg(DR_REG_RDI), OPND_CREATE_INT64((uint64_t)&reg_table));
        INSERT_call(opnd_create_pc((byte *)&mapGet1));
        INSERT_move(opnd_create_reg(DR_REG_RDI), opnd_create_reg(DR_REG_RAX));
        SAVE_REG(DR_REG_RDI,SPILL_SLOT_13);
        INSERT_call(opnd_create_pc((byte *)&pairGetFirst));
        INSERT_store(OPND_CREATE_MEM64(DR_REG_RSP, 16), opnd_create_reg(DR_REG_RAX));
        RESTORE_REG(DR_REG_RDI,SPILL_SLOT_13);
        INSERT_call(opnd_create_pc((byte *)&pairGetSecond));
        INSERT_store(OPND_CREATE_MEM64(DR_REG_RSP, 8), opnd_create_reg(DR_REG_RAX));
        INSERT_load(opnd_create_reg(DR_REG_RDI), OPND_CREATE_MEM64(DR_REG_RSP, 16));
        INSERT_call(opnd_create_pc((byte *)&is_key_found_freed));
        INSERT_test(opnd_create_reg(DR_REG_AL), opnd_create_reg(DR_REG_AL));
        INSERT_jnz(LBB55_17);
        JUMP_to_label(LBB55_20);
        INSERT_LABEL(LBB55_17);
        INSERT_load(opnd_create_reg(DR_REG_RSI), OPND_CREATE_MEM64(DR_REG_RSP, 16));
        INSERT_call(opnd_create_pc((byte *)&getFreedValue));
        INSERT_cmp(opnd_create_reg(DR_REG_EAX),OPND_CREATE_INT32(1));
        INSERT_jnz(LBB55_20);
        INCREMENT_error_counter;
        INSERT_LABEL(LBB55_20);
        INSERT_load(opnd_create_reg(DR_REG_RAX), OPND_CREATE_ABSMEM((byte *)&LEAddr, OPSZ_8));
        //DOUBLE CHECK
        INSERT_cmp(opnd_create_reg(DR_REG_RAX),OPND_CREATE_MEM64(DR_REG_RSP, 16));
        INSERT_jb(LBB55_22);
        INSERT_load(opnd_create_reg(DR_REG_RAX), OPND_CREATE_ABSMEM((byte *)&LEAddr, OPSZ_8));
        INSERT_cmp(opnd_create_reg(DR_REG_RAX),OPND_CREATE_MEM64(DR_REG_RSP, 8));
        INSERT_jbe(LBB55_26);
        INSERT_LABEL(LBB55_22);
        INCREMENT_error_counter;
    }
    //RESTORE
    INSERT_LABEL(LBB55_26);
    INSERT_add(opnd_create_reg(DR_REG_RSP), OPND_CREATE_INT32(24));
    RESTORE_REG(DR_REG_RSP,SPILL_SLOT_1);
    RESTORE_CALLEE_REG_SET1(bitmask_reg);
    RESTORE_CALLEE_REG_SET2_RAX(bitmask_reg);
    if(bitmask_flags){
        RESTORE_REG(DR_REG_RAX,SPILL_SLOT_12);
        RESTORE_ARITH_FLAGS(SPILL_SLOT_12);
        RESTORE_REG(DR_REG_RAX,SPILL_SLOT_9);
    }
}
void check_lea_mem(JANUS_CONTEXT, instr_t *trigger, uint64_t bitmask_flags, uint64_t bitmask_reg){
    opnd_t mem_operand, reg_operand, dest;
    int i, num_srcs, num_dsts, size;
    guard_opcode_t opcode;
    reg_id_t dest_id, base_reg_id;
    num_srcs = instr_num_srcs(trigger);
    for(i=0; i< num_srcs; i++){
       opnd_t operand1 = instr_get_src(trigger, i);
       if(opnd_is_memory_reference(operand1))
          mem_operand = operand1;
    }
    num_dsts = instr_num_dsts(trigger);
    for(i=0; i< num_dsts; i++){
       reg_operand = instr_get_dst(trigger, i);
       if(opnd_is_reg(reg_operand))
          dest = reg_operand;
    } 
    reg_id_t base = opnd_get_base(mem_operand);
    if(base != DR_REG_NULL)
        base_reg_id = get_64bit(base);

    dest_id = get_64bit(opnd_get_reg(dest));


    instr_t *LBB59_3 = INSTR_CREATE_label(drcontext);
    instr_t *LBB59_4 = INSTR_CREATE_label(drcontext);
    instr_t *LBB59_5 = INSTR_CREATE_label(drcontext);
    instr_t *LBB59_6 = INSTR_CREATE_label(drcontext);
    instr_t *LBB59_7 = INSTR_CREATE_label(drcontext);
    instr_t *LBB59_8 = INSTR_CREATE_label(drcontext);
    instr_t *LBB59_11 = INSTR_CREATE_label(drcontext);
    SAVE_REG(DR_REG_RSP,SPILL_SLOT_1);
    //save full set with RAX in the start
    SAVE_CALLEE_REG_SET(bitmask_reg);
    if(bitmask_flags){
        SAVE_ARITH_FLAGS(SPILL_SLOT_12);
        SAVE_REG(DR_REG_RAX,SPILL_SLOT_12);
        RESTORE_REG(DR_REG_RAX,SPILL_SLOT_2);
    }
    LOAD_EFFECTIVE_ADDR_LEADDR(mem_operand);
    if(base != DR_REG_NULL){
        if(base_reg_id == DR_REG_XBP || base_reg_id == DR_REG_XSP){
            INSERT_load(opnd_create_reg(DR_REG_RDI), OPND_CREATE_ABSMEM((byte *)&LEAddr, OPSZ_8));
            INSERT_call(opnd_create_pc((byte *)&is_key_found_memtable));
            INSERT_test(opnd_create_reg(DR_REG_AL), opnd_create_reg(DR_REG_AL));
            INSERT_jnz(LBB59_3);
            JUMP_to_label(LBB59_4);
            
            INSERT_LABEL(LBB59_3);
            INSERT_load_int(opnd_create_reg(DR_REG_EDI), OPND_CREATE_INT32(dest_id));
            INSERT_load(opnd_create_reg(DR_REG_RSI), OPND_CREATE_ABSMEM((byte *)&LEAddr, OPSZ_8));
            INSERT_call(opnd_create_pc((byte *)&copyMemToReg));
            JUMP_to_label(LBB59_11);
        }
     //   else{
            INSERT_LABEL(LBB59_4);
            INSERT_load_int(opnd_create_reg(DR_REG_EDI), OPND_CREATE_INT32(base_reg_id));
            INSERT_call(opnd_create_pc((byte *)&is_key_found_regtable));
            INSERT_test(opnd_create_reg(DR_REG_AL), opnd_create_reg(DR_REG_AL));
            INSERT_jnz(LBB59_5);
            JUMP_to_label(LBB59_6);

            INSERT_LABEL(LBB59_5);
            INSERT_load_int( opnd_create_reg(DR_REG_ESI), OPND_CREATE_INT32(dest_id));
            INSERT_load_int( opnd_create_reg(DR_REG_EDI), OPND_CREATE_INT32(base_reg_id));
            INSERT_call(opnd_create_pc((byte *)&copyRegValues));
            JUMP_to_label(LBB59_11);
       // }
    }
    INSERT_LABEL(LBB59_6);
    INSERT_load(opnd_create_reg(DR_REG_RDI), OPND_CREATE_ABSMEM((byte *)&LEAddr, OPSZ_8));
    INSERT_call(opnd_create_pc((byte *)&is_key_found_memtable));
    INSERT_test(opnd_create_reg(DR_REG_AL), opnd_create_reg(DR_REG_AL));
    INSERT_jnz(LBB59_7);
    JUMP_to_label(LBB59_8);

    INSERT_LABEL(LBB59_7);
    INSERT_load_int(opnd_create_reg(DR_REG_EDI), OPND_CREATE_INT32(dest_id));
    INSERT_load(opnd_create_reg(DR_REG_RSI), OPND_CREATE_ABSMEM((byte *)&LEAddr, OPSZ_8));
    INSERT_call(opnd_create_pc((byte *)&copyMemToReg));
    JUMP_to_label(LBB59_11);

    INSERT_LABEL(LBB59_8);
    ERASE_dst_reg_from_reg_table(dest_id);
    ERASE_dst_reg_from_split_reg_table(dest_id);

    INSERT_LABEL(LBB59_11);
    RESTORE_REG(DR_REG_RSP,SPILL_SLOT_1);
    RESTORE_CALLEE_REG_SET(bitmask_reg);
    if(bitmask_flags){
        RESTORE_REG(DR_REG_RAX,SPILL_SLOT_12);
        RESTORE_ARITH_FLAGS(SPILL_SLOT_12);
        RESTORE_REG(DR_REG_RAX,SPILL_SLOT_2);
    }
}
void check_lea_stack(JANUS_CONTEXT, instr_t *trigger, uint64_t bitmask_flags,uint64_t bitmask_reg){
    opnd_t reg_operand, dest;
    int i, num_dsts, size;
    reg_id_t dest_id;
    
    num_dsts = instr_num_dsts(trigger);
    for(i=0; i< num_dsts; i++){
       reg_operand = instr_get_dst(trigger, i);
       if(opnd_is_reg(reg_operand))
          dest = reg_operand;
    } 

    dest_id = get_64bit(opnd_get_reg(dest));

    instr_t *LBB60_1 = INSTR_CREATE_label(drcontext);
    instr_t *LBB60_2 = INSTR_CREATE_label(drcontext);
    SAVE_REG(DR_REG_RSP,SPILL_SLOT_1);
    SAVE_CALLEE_REG_SET1(bitmask_reg);
    SAVE_CALLEE_REG_SET2_RAX(bitmask_reg);
    if(bitmask_flags){
        SAVE_ARITH_FLAGS(SPILL_SLOT_12);
        SAVE_REG(DR_REG_RAX,SPILL_SLOT_12);
        RESTORE_REG(DR_REG_RAX,SPILL_SLOT_9);
    }

    INSERT_load_int(opnd_create_reg(DR_REG_EDI), OPND_CREATE_INT32(DR_REG_RSP));
    INSERT_call(opnd_create_pc((byte *)&is_key_found_regtable));
    INSERT_test(opnd_create_reg(DR_REG_AL), opnd_create_reg(DR_REG_AL));
    INSERT_jnz(LBB60_1);
    JUMP_to_label(LBB60_2);

    INSERT_LABEL(LBB60_1);
    INSERT_load_int( opnd_create_reg(DR_REG_ESI), OPND_CREATE_INT32(dest_id));
    INSERT_load_int( opnd_create_reg(DR_REG_EDI), OPND_CREATE_INT32(DR_REG_RSP));
    INSERT_call(opnd_create_pc((byte *)&copyRegValues));

    INSERT_LABEL(LBB60_2);
    RESTORE_REG(DR_REG_RSP,SPILL_SLOT_1);
    RESTORE_CALLEE_REG_SET1(bitmask_reg);
    RESTORE_CALLEE_REG_SET2_RAX(bitmask_reg);
    if(bitmask_flags){
        RESTORE_REG(DR_REG_RAX,SPILL_SLOT_12);
        RESTORE_ARITH_FLAGS(SPILL_SLOT_12);
        RESTORE_REG(DR_REG_RAX,SPILL_SLOT_9);
    }
}
