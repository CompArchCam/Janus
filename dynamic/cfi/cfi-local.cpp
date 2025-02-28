/* JANUS Client for secure execution */

/* Header file to implement a JANUS client */
#include "janus_api.h"
#include "dr_api.h"

//#include "drmgr.h"
//#include "drsyms.h"
#include <inttypes.h>
#include <iostream>
#include <cstring>
#include <fstream>
#include <sstream>
#include "cfi.h"
#include "instrumentation.h"
#include "hashTable.h"
#include "symTable.h"
using namespace std;

//execution mode
#define DYN_ONLY_MODE 0
#define HYBRID_MODE 1
#define STAT_ONLY_MODE 0
#define FORWARD_CFI 0
#define BACKWARD_CFI 1
#define DEBUG_VERBOSE 0

#define VERBOSE_ERROR
#define ERROR_THRESHOLD 10
#define BLUE "\e[34m"
#define RED "\e[31m"
#define BOLD "\e[1m"
#define GREEN "\e[32m"
#define RESET "\e[0m"
#define BIN_32BIT 1
#define BIN_64BIT 0
#define TAB_MAX_SIZE 1024
#define MAX_STR_LEN 256
#define MAX_MODULES_ALLOWED 16
# define MAX_SYM_RESULT 256
//#define KEYBASE 0x400000
//For 32-bit
#define KEYBASE 0x8048000
char name[MAX_SYM_RESULT];
char file[MAX_SYM_RESULT];

int vdso_id=-1;
int BB_count = 0;
const char* main_module;
uint64_t error_counter=0;
bool monitor_enable = true;
//bool monitor_enable = false;
bool flag_live_on = false;
bool reg_live_on = false;
bool start_BB = false;
app_pc orig_main;

static dr_emit_flags_t event_basic_block(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating);

static void generate_trace_events(JANUS_CONTEXT);
static void generate_security_events(JANUS_CONTEXT, stack_thread_t* local);
static void generate_events_by_rule(JANUS_CONTEXT, instr_t *instr, stack_thread_t* local);

stack_thread_t *shadowstack;
static void verify_return_target(JANUS_CONTEXT, instr_t* trigger, uint64_t bitmask_flags, uint64_t bitmask_reg, stack_thread_t * local);
static void store_return_target(JANUS_CONTEXT, instr_t* trigger, uint64_t bitmask_flags, uint64_t bitmask_reg, stack_thread_t * local);
static void verify_jmp_target(JANUS_CONTEXT, instr_t* trigger, uint64_t bitmask_flags, uint64_t bitmask_reg);
static void verify_call_target(JANUS_CONTEXT, instr_t* trigger, uint64_t bitmask_flags, uint64_t bitmask_reg);
static void unwind_longjmp(JANUS_CONTEXT, instr_t* trigger, uint64_t bitmask_flags, uint64_t bitmask_reg);

HashTable *hashTable_IC[MAX_MODULES_ALLOWED];            //indirect call
HashTable *hashTable_IJ[MAX_MODULES_ALLOWED];            //indirect jump targets which are func entries
HashTable *hashTable_PLT[MAX_MODULES_ALLOWED];           //PLT targets
OuterHashTable *hashTable_IJF[MAX_MODULES_ALLOWED];           //indirect jump targets which are not func entries
Symdata SymTable[MAX_MODULES_ALLOWED];
#define R1 DR_REG_XDI
#define R2 DR_REG_XSI
#define R3 DR_REG_XCX
#define R4 DR_REG_XDX
#define OFFSET_STACK_TOP offsetof(stack_thread_t, top)
/*------------------------------------------------------------------------------*/
/*------------------------------ Utility Routines ------------------------------*/
/*------------------------------------------------------------------------------*/
char* get_binfile_name(string filepath){
    // Make a copy of the string to avoid modifying const data
    char* filepathCopy = new char[filepath.length() + 1];
    strcpy(filepathCopy, filepath.c_str());

    // Returns first token
    char* token = strtok(filepathCopy, "/");

    char *filename = token;
    // Keep printing tokens while one of the
    // delimiters present in str.
    while (token != NULL)
    {
        token = strtok(NULL, "/");
        if(token == NULL)
            break;
        else
            filename = token;
    }
    delete[] filepathCopy;
    return filename;
}
/*------------------------------------------------------------------------------*/
/*-----------------Instrumentation Call Back Routines---------------------------*/
/*------------------------------------------------------------------------------*/
static void print_BB(uintptr_t BBaddr){
    //cout<<"BB:"<<hex<<BBaddr<<endl;
    //if(!start_BB) return;
    int id;
    for (id = 0; id < nmodules; ++id) {
        if (dr_module_contains_addr(loaded_modules[id],(app_pc)BBaddr)) {
            cout<<"BB: "<<hex<<(uintptr_t)BBaddr<<" real:"<<(PCAddress)BBaddr-(PCAddress)loaded_modules[id]->start<<" from "<<loaded_modules[id]->full_path<<endl;;
            break;
        }
    }
    //if(BB_count++ > 400 || error_counter >=10 ) exit(0);
}
static void print_instr(uintptr_t pc){
    cout<<"instr: "<<hex<<pc<<endl;
}
static void print_stack_1(uintptr_t addr){
    cout<<"VERIFY base addr: "<<hex<<addr<<endl;
}
static void print_stack_2(uintptr_t addr){
    cout<<"STORE base addr: "<<hex<<addr<<endl;
}
    
static void print_store_tgt(uintptr_t pc, uintptr_t ret_pc, int top, uintptr_t addr){
     cout<<"STORE "<<hex<<ret_pc<<" by instr:"<<pc<<" at top = "<<dec<<top<<" addr: "<<hex<<addr<<endl;
}
static void print_verify_tgt(uintptr_t pc, uintptr_t ret_pc/*R3*/, int top/*R2*/, uintptr_t act_pc/*R4*/, uintptr_t base/*R1*/){
     cout<<"VERIFY "<<hex<<ret_pc<<" by instr:"<<pc<<" when top = "<<dec<<top<<" and addr on stack: "<<hex<<act_pc<<endl;
     uintptr_t address = base + (top*4);
     int * value_ptr = (int*) address;
     cout<<"actual value at stack top: "<<top<<" is"<<(*value_ptr)<<" and addr: "<<(uintptr_t)value_ptr<<endl;
}
void enable_monitoring(app_pc main_pc){
    cout<<"ENABLED..."<<endl;
    dr_mcontext_t mc = { sizeof(mc), DR_MC_ALL };
    dr_get_mcontext(dr_get_current_drcontext(), &mc);
    monitor_enable = 1;
    dr_flush_region(NULL, ~0UL ); //flush all the code
    mc.pc = main_pc;
    dr_redirect_execution(&mc);
}
void disable_monitoring(app_pc main_pc){
    cout<<"DISABLED..."<<endl;
    dr_mcontext_t mc = { sizeof(mc), DR_MC_ALL };
    dr_get_mcontext(dr_get_current_drcontext(), &mc);
    monitor_enable = 0;
    dr_flush_region(NULL, ~0UL ); //flush all the code
    mc.pc = main_pc;
    dr_redirect_execution(&mc);
}
static void verify_return_target(JANUS_CONTEXT, instr_t *instr,uint64_t bitmask_flag, uint64_t bitmask_reg, stack_thread_t *local){
    /*MOV %gs:108, %ecx # Copy SSP into ECX
MOV (%esp), %edx  # Copy ret. addr. into EDX
non_match:
    CMP $0, (%ecx)# Is shadow stack empty?
    JZ abort
    ADD $4, %ecx  # Increment our copy of SSP
    CMP %edx, -4(%ecx) # Check ret. addr.
    JNZ non_match # Loop until match
    MOV %ecx, %gs:108 # Synchronize SSP
    RET
    abort:
    HLT*/
 //load local->stack[local->top] into a reg

        /* local->stack[local->top]
             local->stack address in a register create_rel_addr(r1), (load) local->top in a reg(r2). then use
             then r1 - 4*r2;
             then r2-;
             and save back in reg r2
             mov (%esp), %edx => return address
          Label non_match:
             cmp $(0), $r2      //check if r2 of top is now 0
             JZ error
             add 
             cmp %edx, value in (r1+4*r2)
             JNZ non_match
             mov r2 back in local->top
             JUMP restore
          error:
             inc error
          restore: restore valye

        */
    instr_t *meta_instr;
    instr_t *restore_label, *unwind_label, *error_label;
    //stack_thread_t * local = (stack_thread_t*)dr_get_tls_field(dr_get_current_drcontext());
    restore_label = INSTR_CREATE_label(drcontext);
    unwind_label = INSTR_CREATE_label(drcontext);
    error_label = INSTR_CREATE_label(drcontext);
    /*** save context*****/
    dr_save_reg(drcontext, bb, instr,R1, SPILL_SLOT_2);
    dr_save_reg(drcontext, bb, instr, R2, SPILL_SLOT_3);
    dr_save_reg(drcontext, bb, instr, R3, SPILL_SLOT_4);
    dr_save_reg(drcontext, bb, instr, R4, SPILL_SLOT_6);
    if(bitmask_flag)
        dr_save_arith_flags(drcontext, bb, instr, SPILL_SLOT_5);

    /*** store value in stack ***/
    //1. load value at local in R1, this value would be the memory address of local->stack.
    PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_imm(drcontext,
                opnd_create_reg(R1),
                    OPND_CREATE_INT32(&(local->stack))
                       ));
    //2. load the value of top in R2
    PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_ld(drcontext,
                    opnd_create_reg(R2),
                         OPND_CREATE_MEM32(R1, OFFSET_STACK_TOP)
                        ));
    //laod return address at [rsp] at R3
    PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_ld(drcontext,
                    opnd_create_reg(R3),
                             OPND_CREATE_MEM32(DR_REG_XSP, 0)));
    //unwind:
    instrlist_meta_preinsert(bb, instr, unwind_label);
    //compare if top == 0 (stack empty)
    meta_instr = INSTR_CREATE_cmp(drcontext,opnd_create_reg(R2),OPND_CREATE_INT32(0));
    instrlist_meta_preinsert(bb, instr,meta_instr);


    meta_instr = INSTR_CREATE_jcc(drcontext,OP_jz,opnd_create_instr(error_label));
    instrlist_meta_preinsert(bb, instr, meta_instr);

    //top--,
    meta_instr = XINST_CREATE_sub_s(drcontext,opnd_create_reg(R2),OPND_CREATE_INT32(1));
    instrlist_meta_preinsert(bb, instr, meta_instr);


    meta_instr = INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(R4), opnd_create_base_disp( R1, R2, 4, 0,OPSZ_4));
    instrlist_meta_preinsert(bb, instr, meta_instr);
    
    //compare [rsp] with stack[top+4], if not equal, then jump to loop
    meta_instr = INSTR_CREATE_cmp(drcontext,opnd_create_reg(R3),opnd_create_reg(R4));
    instrlist_meta_preinsert(bb, instr,meta_instr);
    
    meta_instr = INSTR_CREATE_jcc(drcontext,OP_jne,opnd_create_instr(unwind_label));
    instrlist_meta_preinsert(bb, instr, meta_instr);
    
    meta_instr = INSTR_CREATE_jmp(drcontext, opnd_create_instr(restore_label));
    instrlist_meta_preinsert(bb, instr, meta_instr);


    instrlist_meta_preinsert(bb, instr, error_label);
   //  increment error counter
    meta_instr = INSTR_CREATE_inc(drcontext,OPND_CREATE_ABSMEM((byte *)&error_counter, OPSZ_4));
    instrlist_meta_preinsert(bb, instr, meta_instr);
    

    //insert label for flag/reg restore instructions
    instrlist_meta_preinsert(bb, instr, restore_label);
    
    //restore value of top
    PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_st(drcontext,
                               OPND_CREATE_MEM32(R1, OFFSET_STACK_TOP),
                                opnd_create_reg(R2)));

    if(bitmask_flag)
        dr_restore_arith_flags(drcontext, bb, instr, SPILL_SLOT_5);
    dr_restore_reg(drcontext, bb, instr, R4, SPILL_SLOT_6);
    dr_restore_reg(drcontext, bb, instr, R3, SPILL_SLOT_4);
    dr_restore_reg(drcontext, bb, instr, R2, SPILL_SLOT_3);
    dr_restore_reg(drcontext, bb, instr, R1, SPILL_SLOT_2);

}
static void store_return_target(JANUS_CONTEXT, instr_t *instr,uint64_t bitmask_flag, uint64_t bitmask_reg, stack_thread_t *local){
    instr_t *meta_instr;
    app_pc instr_pc = instr_get_app_pc(instr);
    app_pc ret_pc = instr_pc + instr_length(drcontext, instr);
    instr_t *restore_label, *error_label;
    //stack_thread_t * local = (stack_thread_t*)dr_get_tls_field(dr_get_current_drcontext());

    /*--------- saving RSI/RDI registers -----------------*/
    dr_save_reg(drcontext, bb, instr, R1, SPILL_SLOT_2);
    dr_save_reg(drcontext, bb, instr, R2, SPILL_SLOT_3);
    dr_save_reg(drcontext, bb, instr, R4, SPILL_SLOT_6);
   
    restore_label = INSTR_CREATE_label(drcontext);
    error_label = INSTR_CREATE_label(drcontext);

    /*SUB $4, %gs:108 # Decrement SSP
    MOV %gs:108, %eax # Copy SSP into EAX
    MOV (%esp), %ecx # Copy instr_pc + size into ecx
    MOV %ecx, (%eax) # shadow stack via ECX
      */  
        /* local->stack[local->top]
             local->stack address in a register create_rel_addr(r1), (load) local->top in a reg(r2). then use
             then r1+ 4*r2;
             then r2++;
             and save back in reg r2
        */

    //1. load value at local in R1, this value would be the memory address of local->stack.
    PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_imm(drcontext,
                opnd_create_reg(R1),
                    OPND_CREATE_INT32((&local->stack)))
                       );
    //2. load the value of top in R2
    PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_ld(drcontext,
                    opnd_create_reg(R2),
                         OPND_CREATE_MEM32(R1, OFFSET_STACK_TOP)
                        ));
    //2. store the value of instr_pc+ size into local->stack[local->top]
    PRE_INSERT(bb, instr,
        INSTR_CREATE_lea(drcontext,
                            opnd_create_reg(R4),
                         opnd_create_base_disp(R1, R2, 4, 0, OPSZ_lea)
                            //OPND_CREATE_INTPTR(ret_pc)
                            ));
    PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_st(drcontext,
                         opnd_create_base_disp(R1, R2, 4, 0, OPSZ_4),
                            //OPND_CREATE_INTPTR(ret_pc)
                            OPND_CREATE_INT32(ret_pc)
                            ));

    //increment top (without affecting status flags)
    meta_instr = XINST_CREATE_add(drcontext,opnd_create_reg(R2), OPND_CREATE_INT32(1));
    instrlist_meta_preinsert(bb, instr, meta_instr);
    
    //2. store the incremented value of top
    PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM32(R1, OFFSET_STACK_TOP),
                            opnd_create_reg(R2)
                        ));
    
        
    //insert label for flag/reg restore instructions
    instrlist_meta_preinsert(bb, instr, restore_label);

    dr_restore_reg(drcontext, bb, instr, R1, SPILL_SLOT_2);
    dr_restore_reg(drcontext, bb, instr, R2, SPILL_SLOT_3);
    dr_restore_reg(drcontext, bb, instr, R4, SPILL_SLOT_6);
}
//TODO: for dynamic only version, we need to check the return address, and then check if the difference between it and the top of the stack address is <= 8


/*------------------------------------------------------------------------------*/
/*----------------------  Symbol and HashTable Routines-------------------------*/
/*------------------------------------------------------------------------------*/
static void loadSymbolsAndHashTables(const char * binName, int id){

    //STEP 1: read symbols from ELF

    std::ifstream binFile(binName, std::ios::binary | std::ios::ate);

    if (!binFile.is_open()) {
        std::cerr << "Error opening the file" <<binName<< std::endl;
        return;
    }
    uint32_t fileSize = binFile.tellg();

    cout<<"Reading file \""<<binName<<"\" size: "<<fileSize<<" bytes."<<endl;

    uint8_t *filebuffer = new uint8_t[fileSize + 2048];
    binFile.seekg (0, ios::beg);
    binFile.read((char *)filebuffer,fileSize);
    if(strncmp((char *)filebuffer,ELFMAG,4)==0) {
        switch(filebuffer[EI_CLASS]) {
            case ELFCLASS32:
                parseELF32(filebuffer);
            break;
            case ELFCLASS64:
                parseELF64(filebuffer);
            break;
        }
    }


    //STEP 2: read binary addresses and build hashtables

     binFile.seekg (0, ios::beg);
     //set the stream to read binary
     binFile >> std::noskipws;

    uintptr_t address;
    initHashTable(hashTable_IC[id], TAB_MAX_SIZE);
    initHashTable(hashTable_IJ[id], TAB_MAX_SIZE);
    while (binFile.read(reinterpret_cast<char*>(&address), sizeof(address))) {
        // Display the hex address
         //if (jc->functionMap.count(address)){
              insert(hashTable_IC[id], address, 1);
              insert(hashTable_IJ[id], address, 1);
         //}
        // Slide the cursor back by 3 bytes
        binFile.seekg(-3, std::ios::cur);
    }

    // Close the file
    binFile.close();
}
static void loadHashTables(const char * binname, int id, uintptr_t base){
  int size;
  std::ifstream infile;
  char  base_name[MAX_STR_LEN];
  char  ic_target[MAX_STR_LEN];
  char  ij_target[MAX_STR_LEN];
  char  ijf_target[MAX_STR_LEN];
  char  plt_target[MAX_STR_LEN];
  char  isym_file[MAX_STR_LEN];
  char  esym_file[MAX_STR_LEN];
  
  strcpy(base_name,rs_dir);
  strcat(base_name,binname);

  strcpy(ic_target,base_name);
  strcat(ic_target,"_ic.txt");

  strcpy(ij_target,base_name);
  strcat(ij_target,"_ij.txt");

  strcpy(plt_target,base_name);
  strcat(plt_target,"_plt.txt");

  strcpy(ijf_target,base_name);
  strcat(ijf_target,"_ijf.txt");
  
  strcpy(ijf_target,base_name);
  strcat(ijf_target,"_ijf.txt");

  strcpy(isym_file,base_name);
  strcat(isym_file,"_isym.txt");

  strcpy(esym_file,base_name);
  strcat(esym_file,"_esym.txt");
  
  /*indirect call target*/
  infile.open(ic_target, std::ios::in);
  std::string str;
  std::getline(infile, str);
  size = std::stoul(str);
  uintptr_t addr;
  initHashTable(hashTable_IC[id], size);
  while (std::getline(infile, str)) {
      addr = std::stoul(str) + base;
      insert(hashTable_IC[id], addr, 1);
  }

  /*indirect jump target*/
  initHashTable(hashTable_IJ[id], size);
  infile.open(ij_target, std::ios::in);
  while (std::getline(infile, str)) {
      addr = std::stoul(str) + base;
      insert(hashTable_IJ[id], addr, 1);
  }
  /*plt call target*/
  initHashTable(hashTable_PLT[id], size);
  infile.open(plt_target, std::ios::in);
  while (std::getline(infile, str)) {
      addr = std::stoul(str) + base;
      insert(hashTable_PLT[id], addr, 1);
  }

  /*--- load indirect jump targets within the funciton --- */
  infile.open(ijf_target, std::ios::in);
  while (std::getline(infile, str)) {
        unsigned long funcAddr = std::stoul(str);
        //create new entry, and add following entries according to that
        insertIJ(hashTable_IJF[id], funcAddr);
        HashTable* innerTable = getInnerTableForKey(hashTable_IJF[id], funcAddr);
        while(std::getline(infile, str)){    
             if (str.find('\t') != std::string::npos) break;
             else{
                 addr = base + std::stoul(str);
                  insert(innerTable, addr, 1);
             }
        }
  }
  //read imported symbols
  //TODO: for dynamically loaded libraries.  
  infile.open(isym_file, std::ios::in);
  while (std::getline(infile, str)) {
        SymTable[id].importedSyms.insert(str.c_str());
  }
  //read exported symbols
  infile.open(esym_file, std::ios::in);
  while (std::getline(infile, str)) {
        std::istringstream iss(str);
        uintptr_t address;
        std::string symName;
        // Read the first token (address)
        iss >> std::dec >> address;
        address +=  base;
        // Read the second token (const char*)
        std::getline(iss >> std::ws, symName, '\t');
        SymTable[id].exportedSyms[address] =  symName.c_str();
    }


}
/*------------------------------------------------------------------------------*/
/*----------------------  Translation Analysis Routines-------------------------*/
/*------------------------------------------------------------------------------*/
static void
generate_dynamic_events(JANUS_CONTEXT, stack_thread_t *local){


    instr_t     *instr, *last;
    app_pc      current_pc, first_pc, call_pc;

    opnd_t src, dest;
    int num_srcs, num_dsts, i;
    instr_t *first_instr = instrlist_first_app(bb);
    first_pc = instr_get_app_pc(first_instr);
    last = instrlist_last_app(bb);
    if(instr_is_call(last)){
#if DEBUG_VERBOSE
         cout<<"CAll came for BB:"<<(uintptr_t)first_pc<<endl;
#endif
        store_return_target(janus_context, last,  1/*bitmask_flags*/, 0/*bitmask_reg*/, local);
    }
    else if(instr_is_return(last)){
         verify_return_target(janus_context, last,  1/*bitmask_flags*/, 0/*bitmask_reg*/, local);
    }
    return;
}


static void
generate_security_events(JANUS_CONTEXT, stack_thread_t * local)
{
    int         offset;
    int         id = 0;
    app_pc      current_pc;
    int         skip = 0;
    instr_t     *instr, *last = NULL;
    int         mode;

    /* Iterate through each original instruction in the block
     * generate dynamic inline instructions that emit commands in the command buffer */
    for (instr = instrlist_first_app(bb);
         instr != NULL;
         instr = instr_get_next_app(instr))
    {

        current_pc = instr_get_app_pc(instr);
        /* Firstly, check whether this instruction is attached to static rules */
        while (rule) {
            if ((app_pc)rule->pc == current_pc) {
                generate_events_by_rule(janus_context, instr, local);
            } else
                break;
            rule = rule->next;
        }
    }
}
static void
generate_events_by_rule(JANUS_CONTEXT, instr_t *instr, stack_thread_t * local){
    
    RuleOp rule_opcode = rule->opcode;
    
    instr_t *trigger = get_trigger_instruction(bb,rule);
    reg_id_t dest, src;
    opnd_t src_opnd, target_opnd;
    int id = -1;
    int matched = -1;
    switch (rule_opcode) {
        //TODO: move bound check after the dereference instruction. get the metadata from lea of mem-operand, get value from val in reg, remove reg from reg table. and then check overflow in memory table, not reg table	
        /*----------- RECORD MALLOC DATA ----------*/
#if BACKWARD_CFI
	case SAVE_RETURN_TARGET:
           if(monitor_enable){
                store_return_target(janus_context, trigger,  1/*bitmask_flags*/,  0/*bitmask_reg*/, local);
            }
	break;
	case VERIFY_RETURN_TARGET:
            if(monitor_enable){
                verify_return_target(janus_context, trigger,  1/*bitmask_flags*/, 0/*bitmask_reg*/, local);
            }
        //verify_return_target(janus_context, trigger , flag_live_on ? rule->reg0 : 1/*bitmask_flags*/, reg_live_on ? rule->reg1 : 0/*bitmask_reg*/);
	break;
#endif
#if 0
	case HANDLE_CXX_EX:
            unwind_stack_cxx(janus_context, trigger, flag_live_on ? rule->reg0 : 1/*bitmask_flags*/, reg_live_on ? rule->reg1 : 0/*bitmask_reg*/);
	break;
#endif
        
#if FORWARD_CFI
        case VERIFY_CALL_TARGET_INTER:
            //verify_call_target(janus_context, trigger, flag_live_on ? rule->reg0 : 1/*bitmask_flags*/, reg_live_on ? rule->reg1 : 0/*bitmask_reg*/ );
            //DR_ASSERT(instr_is_call_indirect(instr));
            target_opnd = instr_get_target(instr);
            //DR_ASSERT(opnd_is_reg(target_opnd) || opnd_is_memory_reference(target_opnd));
            for (id = 0; id < nmodules; ++id) {
                if (dr_module_contains_addr(loaded_modules[id],instr_get_app_pc(instr))) {
                    matched = id;
                    break;
                }
            }
            dr_insert_clean_call(drcontext, bb, trigger, (void *)verify_call_target_clean, false, 3, OPND_CREATE_INTPTR(instr_get_app_pc(trigger)), target_opnd, OPND_CREATE_INT32(matched));

        case VERIFY_CALL_TARGET_INTRA:
            //verify_call_target(janus_context, trigger, flag_live_on ? rule->reg0 : 1/*bitmask_flags*/, reg_live_on ? rule->reg1 : 0/*bitmask_reg*/ );
            DR_ASSERT(instr_is_call_indirect(instr));
            target_opnd = instr_get_target(instr);
            DR_ASSERT(opnd_is_reg(target_opnd) || opnd_is_memory_reference(target_opnd));
            for (id = 0; id < nmodules; ++id) {
                if (dr_module_contains_addr(loaded_modules[id],instr_get_app_pc(instr))) {
                    matched = id;
                    break;
                }
            }
            dr_insert_clean_call(drcontext, bb, trigger, (void *)verify_call_target_clean, false, 3, OPND_CREATE_INTPTR(instr_get_app_pc(trigger)), target_opnd, OPND_CREATE_INT32(matched));
	break;
	case VERIFY_JMP_TARGET:
            target_opnd = instr_get_target(instr);
            DR_ASSERT(opnd_is_reg(target_opnd) || opnd_is_memory_reference(target_opnd));
            for (id = 0; id < nmodules; ++id) {
                if (dr_module_contains_addr(loaded_modules[id],instr_get_app_pc(instr))) {
                    matched = id;
                    break;
                }
            }
            dr_insert_clean_call(drcontext, bb, trigger, (void *)verify_jmp_target_clean, false, 3, OPND_CREATE_INTPTR(instr_get_app_pc(trigger)), target_opnd, OPND_CREATE_INT32(matched));
	break;
#endif
#if 0
        case SAVE_AT_ENTRY:
             if(reg_live_on){
                     int alive = 0;
                     int slot = 0;
                     uint64_t bitmask_reg = rule->reg0;
                     for(int id= DR_REG_RCX; id< DR_REG_R12; id++){ //starting from id DR_REG_RCX, to DR_REG_11
                         alive = (bitmask_reg >> id-1 ) & 1;
                         if(alive){
                             PRE_INSERT(bb, trigger,
                            INSTR_CREATE_mov_st(drcontext,
                                            opnd_create_rel_addr(&(regs[slot]), OPSZ_8),
                                            opnd_create_reg(id)));
                                  slot++;
                         }
                     }
             }
        break;
        case RESTORE_AT_EXIT:
             if(reg_live_on){
                 int alive = 0;
                 int slot = 0;
                 uint64_t bitmask_reg = rule->reg0;
                 for(int id= DR_REG_RCX; id< DR_REG_R12; id++){ //starting from id DR_REG_RCX, to DR_REG_11
                     alive = (bitmask_reg >> id-1) & 1;
                     if(alive){
                         PRE_INSERT(bb, trigger,
                         INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(id),
                                opnd_create_rel_addr(&(regs[slot]), OPSZ_8)));
                          slot++;
                     }
                                         
                 }
             }
         break;
#endif
#if 0
         case ENABLE_MONITORING:
             if(!monitor_enable){ //only do it first time
                 dr_insert_clean_call(drcontext, bb, instr, (void *)enable_monitoring, false, 1, OPND_CREATE_INTPTR(instr_get_app_pc(instr)));
             }
         break;
         case DISABLE_MONITORING:
                 dr_insert_clean_call(drcontext, bb, instr, (void *)disable_monitoring, false, 1, OPND_CREATE_INTPTR(instr_get_app_pc(instr)));
         break;
#endif    

        default:
                //fprintf(stderr,"In basic block 0x%lx static rule not recognised %d\n",bbAddr,rule_opcode);
            break;
        }
    }
/*  mov 0x30(%rdi), %r8
    ror $0x11, %r8
    xor %fs:0x30, %r8

    .CFI_LONGJMP_labelxx:
    movq %r8, %r9
    shrq $3, %r9
    movb $0, {off}(r9)
    subq $8, %r8
    cmp %r8, %rsp
    jne .CFI_LONGJMP_labelxx
*/
/*------------------------------------------------------------------------------*/
/*----------------------  DR Event Call Back Routines---------------------------*/
/*------------------------------------------------------------------------------*/
static void
exit_summary(void *drcontext) {
       cout<<"Total overflow error: "<<dec<<error_counter<<endl;
}
void on_thread(void *drcontext)
{
    stack_thread_t *local_stack = (stack_thread_t *)malloc(sizeof(stack_thread_t));
    memset(local_stack, 0, sizeof(stack_thread_t));
    assert(local_stack != NULL);
    local_stack->top = 0;
    dr_set_tls_field(drcontext, local_stack);
}

void on_thread_exit(void *drcontext)
{
    free((stack_thread_t*)dr_get_tls_field(drcontext));
    if(error_counter)
       cout<<"\033[31m" <<"Total overflow error: "<< "\033[0m" <<dec<<error_counter<<endl;
   else
       cout<<"\033[32m"<<"Total overflow error: "<<"\033[0m" <<dec<<error_counter<<endl;
}
static void
event_module_load(void *drcontext, const module_data_t *info, bool loaded){
    char  filepath[MAX_STR_LEN];
    char  binfilepath[MAX_STR_LEN];
    bool load_schedule = true;
    bool rules_found = false;
#if DEBUG_VERBOSE
    if(loaded == true)
        cout<<"module loaded"<<endl;
    dr_fprintf(STDOUT, " full_name %s \n", info->full_path);
    dr_fprintf(STDOUT, " module_name %s \n", dr_module_preferred_name(info));
    dr_fprintf(STDOUT, " entry" PFX "\n", info->entry_point);
    dr_fprintf(STDOUT, " start" PFX "\n", info->start);
#endif
    //if(strcmp(dr_module_preferred_name(info),"libquadmath.so.0") == 0) cout<<"Loaded: libquadmaths"<<endl;
    /*if(strcmp(dr_module_preferred_name(info), "ld-linux.so.2") == 0){
        dr_module_set_should_instrument(info->handle, false);
        load_schedule = false;
    }*/
    loaded_modules[nmodules] = dr_copy_module_data(info);
    //if(strcmp(dr_module_preferred_name(info), "linux-vdso.so.1") == 0) {vdso_id=nmodules-1 ; return;}
    if(strcmp(dr_module_preferred_name(info), "linux-gate.so.1") == 0) {vdso_id=nmodules-1 ; return;}
#if DYN_ONLY_MODE
    if(strcmp(dr_module_preferred_name(info), main_module) == 0){
        char* MAIN = "main";
        //orig_main = (app_pc)dr_get_proc_address((module_handle_t)info->start, MAIN);
        size_t offs;
        if (drsym_lookup_symbol(info->full_path, MAIN, &offs, DRSYM_DEMANGLE) == DRSYM_SUCCESS) {
            orig_main = offs + info->start;
        }
    }
#endif
#if !DYN_ONLY_MODE
    if(load_schedule && strcmp(dr_module_preferred_name(info), "linux-gate.so.1") != 0){
        char * binfile = get_binfile_name(info->full_path);
        strcpy(filepath, info->full_path);
        //char * binfile = const_cast<char*>(dr_module_preferred_name(info));
        //char * binfile = get_binfile_name(info->full_path);
        strcpy(binfilepath,rs_dir);
        strcat(binfilepath,binfile);
        strcat(binfilepath,".jrs");
#if DEBUG_VERBOSE
        printf("binfilepath: %s\n", binfilepath);
#endif
        //strcat(filepath, ".jrs");
        FILE *file = fopen(binfilepath, "r");
        if(file != NULL) {
            rules_found=true;
        }
    //fclose(file);
        if(rules_found){
#if FORWARD_CFI
            if(info->start == KEYBASE)
                base = 0;
            else
                base = info->entry_point;
            loadHashTables(binfile, nmodules, base);
#endif
            nmodules++;
            //cout<<"loading rules for: "<<binfilepath<<endl;
            load_static_rules_security(binfilepath, info);
        }
        else{ /* rules not found, analyze dynamically */
#if FORWARD_CFI
            loadSymbolsAndHashtables(binfiles, nmodules);
#endif
            nmodules++;
        }
    }
#endif
}

static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating)
{
    uint64_t num_instructions = 0;
    //get current basic block starting address
    PCAddress bbAddr = (PCAddress)dr_fragment_app_pc(tag);
    //if(!start_BB && bbAddr == 0x804841a) 
      //  dr_insert_clean_call(drcontext, bb, instrlist_first_app(bb), (void *)enable, false, 0);
#if DEBUG_VERBOSE
    //dr_insert_clean_call(drcontext, bb, instrlist_first_app(bb), (void *)print_BB, false, 1, OPND_CREATE_INTPTR(bbAddr));
#endif
     
    //lookup in the hashtable to check if there is any rule attached to the block
    RRule *rule;
    stack_thread_t *local = (stack_thread_t*)dr_get_tls_field(dr_get_current_drcontext());
#if STAT_ONLY_MODE || HYBRID_MODE
    rule = get_static_rule_security(bbAddr);
    if (rule){
        if(rule->opcode != NO_RULE){
            generate_security_events(janus_context, local);
        }
    }
    //generate_trace_events(janus_context);
#if HYBRID_MODE && !STAT_ONLY_MODE
    else if(monitor_enable){                                //dynamically generated code, or not seen statically (e.g. vdso)
        generate_dynamic_events(janus_context, local);
    }
#endif
#endif
#if DYN_ONLY_MODE && !STAT_ONLY_MODE && !HYBRID_MODE
    if(bbAddr == (PCAddress)orig_main && !monitor_enable){
         dr_insert_clean_call(drcontext, bb, instrlist_first_app(bb), (void *)enable_monitoring, false, 1, OPND_CREATE_INT64(instr_get_app_pc(instrlist_first_app(bb))));
    }
    if(monitor_enable){
        generate_dynamic_events(janus_context, local);
    }
#endif
    return DR_EMIT_DEFAULT;
}



/*------------------------------------------------------------------------------*/
/*--------------------------------- DR MAIN ----- ------------------------------*/
/*------------------------------------------------------------------------------*/
DR_EXPORT void dr_init(client_id_t id)
{
#ifdef JANUS_VERBOSE
    dr_fprintf(STDOUT,"\n---------------------------------------------------------------\n");
    dr_fprintf(STDOUT,"               Janus Secure Execution --- Control Flow Integrity\n");
    dr_fprintf(STDOUT,"---------------------------------------------------------------\n\n");
#endif
    set_client_mode((JMode)JCFI);
    module_data_t *main = dr_get_main_module();
    main_module = dr_module_preferred_name(main);
#if DYN_ONLY_MODE
    /*Initialise symbol library*/
    if (drsym_init(0) != DRSYM_SUCCESS) {
            printf("WARNING: unable to initialize symbol translation\n");
    }
#endif
#if !DYN_ONLY_MODE
    janus_init_asan(id);

    cout<<"\033[32m"<<"ENTERNED JANUS: mode is set to "<<print_janus_mode((JMode)get_client_mode())<<endl; 
#endif
    
#if STAT_ONLY_MODE
    cout<<"MODE is STATIC ONLY with flag_liveness="<<flag_live_on<<" reg_liveness="<<reg_live_on<<"\033[0m"<<endl;
#endif
#if HYBRID_MODE
    cout<<"MODE is HYBRID with flag_liveness="<<flag_live_on<<" reg_liveness="<<reg_live_on<<"\033[0m"<<endl;
#endif
#if DYN_ONLY_MODE
    cout<<"MODE is DYN_ONLY"<<"\033[0m"<<endl;
#endif
    /* Register event callbacks. */
       // Initialize each hash table in the array

#if FORWARD_CFI
    for (int i = 0; i < MAX_MODULES_ALLOWED; i++) {
        hashTable_IC[i] = (HashTable *)malloc(sizeof(HashTable));
        hashTable_IJ[i] = (HashTable *)malloc(sizeof(HashTable));
        hashTable_PLT[i] = (HashTable *)malloc(sizeof(HashTable));
    }
#endif

    dr_register_thread_init_event(on_thread);
    dr_register_bb_event(event_basic_block); 
    dr_register_thread_exit_event(on_thread_exit);
    dr_register_module_load_event(event_module_load);

#ifdef JANUS_VERBOSE
    dr_fprintf(STDOUT,"Dynamorio client initialised\n");
#endif
}
