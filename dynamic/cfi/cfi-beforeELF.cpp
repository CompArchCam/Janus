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
int vdso_id=-1;
int BB_count = 0;
const char* main_module;
uint64_t error_counter=0;
uint64_t push_count =0;
uint64_t pop_count =0;
bool monitor_enable = false;
bool flag_live_on = false;
bool reg_live_on = false;
bool start_BB = false;

stack_thread_t *shadowstack;
static void verify_return_target(JANUS_CONTEXT, instr_t* trigger, uint64_t bitmask_flags, uint64_t bitmask_reg);
static void store_return_target(JANUS_CONTEXT, instr_t* trigger, uint64_t bitmask_flags, uint64_t bitmask_reg);
static void verify_jmp_target(JANUS_CONTEXT, instr_t* trigger, uint64_t bitmask_flags, uint64_t bitmask_reg);
static void verify_call_target(JANUS_CONTEXT, instr_t* trigger, uint64_t bitmask_flags, uint64_t bitmask_reg);
static void unwind_longjmp(JANUS_CONTEXT, instr_t* trigger, uint64_t bitmask_flags, uint64_t bitmask_reg);
//std::set<string> include_mod = {"perlbench_base.gcc55-O3"};
#define MAX_STR_LEN 256
typedef struct p_memdata{
    addr_t                      lea;
    addr_t                      base;
    uint64_t                    bound;
    p_memdata() {clear();}
    void                        clear() {base=0x0; bound=0;};
} memdata;
uint64_t regs[8];

std::map<int, memdata> mem_stack;
#define MAX_MODULES_ALLOWED 16
HashTable *hashTable_IC[MAX_MODULES_ALLOWED];            //indirect call
HashTable *hashTable_IJ[MAX_MODULES_ALLOWED];            //indirect jump targets which are func entries
HashTable *hashTable_PLT[MAX_MODULES_ALLOWED];           //PLT targets
OuterHashTable *hashTable_IJF[MAX_MODULES_ALLOWED];           //indirect jump targets which are not func entries
Symdata SymTable[MAX_MODULES_ALLOWED];


//void push(void *addr)
void store_return_target_clean(uintptr_t addr/*, uintptr_t instr_pc*/)
{
    stack_thread_t *local  = (stack_thread_t*)dr_get_tls_field(dr_get_current_drcontext());
    local->stack[local->top] = addr;
    local->top++;
#if DEBUG_VERBOSE
    cout<<"pushing addr: "<<BOLD<<BLUE<<hex<<addr<<RESET<<" on stack by Call "<<instr_pc<<" stacktop = "<<local->top<<endl;
#endif
}

void verify_longjmp_target_clean(uintptr_t rtn_addr_on_stack/*, uintptr_t instr_pc*/)
{
#if DEBUG_VERBOSE
    cout<<"looking for Return addr: "<<hex<<GREEN<<BOLD<<rtn_addr_on_stack<<RESET<<" by "<<instr_pc<<endl;
#endif
    stack_thread_t *local = (stack_thread_t*)dr_get_tls_field(dr_get_current_drcontext());
    //assert(local->top > 0);
    if(local->top <=0) {
        error_counter++; 
#if DEBUG_VERBOSE
        cout<<RED<<BOLD<<"Error "<<error_counter<<": Return addr not matched: "<<BOLD<<hex<<rtn_addr_on_stack<<RESET<<endl;
#endif
        return;
    }
    while(local->top){
        local->top--;
#if DEBUG_VERBOSE
        cout<<"popped addr "<<BOLD<<local->stack[local->top]<<RESET<<" from stack"<<" stacktop = "<<local->top<<endl;
#endif
        if(rtn_addr_on_stack == local->stack[local->top]) return;
    }
    error_counter++;
#if DEBUG_VERBOSE
    cout<<RED<<BOLD<<"Error "<<error_counter<<": Return addr not matched: "<<hex<<rtn_addr_on_stack<<RESET<<endl;
#endif
}
void verify_call_target_clean(uintptr_t pc, uintptr_t call_target, int id){
     if(lookup(hashTable_IC[id], call_target) == -1){

         cout<<"instr: "<<hex<<pc<<"  call target: "<<hex<<call_target<<" not valid"<<endl;
     }
     else{
         cout<<"call target success. pc: "<<hex<<pc<<" target: "<<call_target<<endl;
     }
     
}
void verify_jmp_target_clean(uintptr_t pc, uintptr_t jmp_target, int id){
     if(lookup(hashTable_IJ[id], jmp_target) == -1){ 
         cout<<"instr: "<<hex<<pc<<"  jmp target: "<<hex<<jmp_target<<" not valid"<<endl;
     }
     else{
         cout<<"jmp target success. pc: "<<hex<<pc<<" target: "<<jmp_target<<endl;
     }
     
}


//TODO: for dynamic only version, we need to check the return address, and then check if the difference between it and the top of the stack address is <= 8

static void
exit_summary(void *drcontext) {
       cout<<"Total overflow error: "<<dec<<error_counter<<endl;

}
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
    //if(BB_count++ >= 100 && error_counter) exit(0);
}
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
static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating);

static void generate_trace_events(JANUS_CONTEXT);
static void generate_security_events(JANUS_CONTEXT);
static void generate_events_by_rule(JANUS_CONTEXT, instr_t *instr);

void on_thread(void *drcontext)
{
    stack_thread_t *local = (stack_thread_t *)malloc(sizeof(stack_thread_t));
    memset(local, 0, sizeof(stack_thread_t));
    assert(local != NULL);
    local->top = 0;
    dr_set_tls_field(drcontext, local);
}

void on_thread_exit(void *drcontext)
{
    free((stack_thread_t*)dr_get_tls_field(drcontext));
    if(error_counter)
       cout<<"\033[31m" <<"Total overflow error: "<< "\033[0m" <<dec<<error_counter<<endl;
   else
       cout<<"\033[32m"<<"Total overflow error: "<<"\033[0m" <<dec<<error_counter<<endl;
}
enum {
   REG=0,
   ABS,
   CONST,
   GLOBAL,
   MEM,
   STACKVAR
};
app_pc orig_main;


static void loadHashTables(const char * binname, int id){
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
  initHashTable(hashTable_IC[id], size);

  while (std::getline(infile, str)) {
      insert(hashTable_IC[id], std::stoul(str), 1);
  }

  /*indirect jump target*/
  initHashTable(hashTable_IJ[id], size);
  infile.open(ij_target, std::ios::in);
  while (std::getline(infile, str)) {
      insert(hashTable_IJ[id], std::stoul(str), 1);
  }
  /*plt call target*/
  initHashTable(hashTable_PLT[id], size);
  infile.open(plt_target, std::ios::in);
  while (std::getline(infile, str)) {
      insert(hashTable_PLT[id], std::stoul(str), 1);
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
             else
                  insert(innerTable, std::stoul(str), 1);
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
        // Read the second token (const char*)
        std::getline(iss >> std::ws, symName, '\t');
        SymTable[id].exportedSyms[address] =  symName.c_str();
    }


}
/**************************************************************************/
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
    if(strcmp(dr_module_preferred_name(info), "ld-linux.so.2") == 0){
        dr_module_set_should_instrument(info->handle, false);
        load_schedule = false;
    }
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
#if FORWARD_CFI
        loadHashTables(binfile, nmodules );
#endif
        nmodules++;
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
            cout<<"loading rules for: "<<binfilepath<<endl;
            load_static_rules_security(binfilepath, info);
        }
    }
#endif
}


DR_EXPORT void 
dr_init(client_id_t id)
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

#ifdef SB_VERBOSE_DETAIL
    cout<<"Entered JANUS Dynamic"<<endl;
#endif
    
#ifdef JANUS_VERBOSE
    dr_fprintf(STDOUT,"Dynamorio client initialised\n");
#endif
}

# define MAX_SYM_RESULT 256
char name[MAX_SYM_RESULT];
char file[MAX_SYM_RESULT];
#if 0
static char * get_func_name(app_pc addr){

    drsym_error_t symres;
    drsym_info_t sym;
    module_data_t *data;
    char * func_name;
    data = dr_lookup_module(addr);
    if (data !=  NULL) {
        sym.struct_size = sizeof(sym);
        sym.name = name;
        sym.name_size = MAX_SYM_RESULT;
        sym.file = file;
        sym.file_size = MAXIMUM_PATH;

        symres = drsym_lookup_address(data->full_path, addr - data->start, &sym,
                                      DRSYM_DEFAULT_FLAGS);
        //cout<<"symres return: "<<symres<<endl;
        if (symres == DRSYM_SUCCESS || symres == DRSYM_ERROR_LINE_NOT_AVAILABLE) {
            func_name = sym.name ; 
        }
    }
    dr_free_module_data(data);
    return func_name;
}
#endif
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
static void
generate_dynamic_events(JANUS_CONTEXT){


    instr_t     *instr, *last;
    app_pc      current_pc, first_pc, call_pc;

    opnd_t src, dest;
    int num_srcs, num_dsts, i;
    instr_t *first_instr = instrlist_first_app(bb);
    first_pc = instr_get_app_pc(first_instr);
    last = instrlist_last_app(bb);
    //TODO: get malloc, calloc and free to instrument them
    if(instr_is_call(last)){
         //uintptr_t trg_addr = (uintptr_t)opnd_get_pc(instr_get_target(last));
         /*app_pc trg_addr = opnd_get_pc(instr_get_target(last));
         if((uintptr_t)trg_addr != 0){
             cout<<"instr: "<<hex<<(uintptr_t)instr_get_app_pc(last)<<endl;
             //cout<<" target: "<<get_func_name(trg_addr)<<endl;
         }*/
         call_pc = instr_get_app_pc(last);
#if DEBUG_VERBOSE
         cout<<"CAll came for BB:"<<(uintptr_t)first_pc<<endl;
#endif
         int size = instr_length(drcontext, last);
         dr_insert_clean_call(drcontext, bb, last, (void *)store_return_target_clean, false, 2, OPND_CREATE_INTPTR((PCAddress)call_pc + size), OPND_CREATE_INTPTR(call_pc));
    }
    else if(instr_is_return(last)){
         //uintptr_t trg_addr = (uintptr_t)opnd_get_pc(instr_get_target(last));
         app_pc trg_addr = opnd_get_pc(instr_get_target(last));
#if DEBUG_VERBOSE
         cout<<"Rtn came for BB:"<<(uintptr_t)first_pc<<endl;
#endif
         dr_insert_clean_call(drcontext, bb, last, (void *)verify_longjmp_target_clean, false, 2, OPND_CREATE_MEMPTR(DR_REG_XSP,0), OPND_CREATE_INTPTR(instr_get_app_pc(last)));

    }
    return;
}


/* Main execution loop: this will be executed at every initial encounter of new basic block */
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
#if STAT_ONLY_MODE || HYBRID_MODE
    rule = get_static_rule_security(bbAddr);
    if (rule){
        if(rule->opcode != NO_RULE){
            generate_security_events(janus_context);
        }
    }
    //generate_trace_events(janus_context);
#if HYBRID_MODE && !STAT_ONLY_MODE
    else if(monitor_enable){                                //dynamically generated code, or not seen statically (e.g. vdso)
        generate_dynamic_events(janus_context);
    }
#endif
#endif
#if DYN_ONLY_MODE && !STAT_ONLY_MODE && !HYBRID_MODE
    if(bbAddr == (PCAddress)orig_main && !monitor_enable){
         dr_insert_clean_call(drcontext, bb, instrlist_first_app(bb), (void *)enable_monitoring, false, 1, OPND_CREATE_INT64(instr_get_app_pc(instrlist_first_app(bb))));
    }
    if(monitor_enable){
        generate_dynamic_events(janus_context);
    }
#endif
    return DR_EMIT_DEFAULT;
}
void enable_instrument(){
    cout<<hex<<" pc: 0x490318"<<endl;
    exit(0);
    //instrument = true;
}
static void
generate_security_events(JANUS_CONTEXT)
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
                //cout<<"instr: "<<hex<<rule->pc<<" rule: "<<print_rule_opcode(rule->opcode)<<endl;
                generate_events_by_rule(janus_context, instr);
            } else
                break;
            rule = rule->next;
        }
    }
}
static void
generate_events_by_rule(JANUS_CONTEXT, instr_t *instr){
    
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
            //store_return_target(janus_context, trigger, flag_live_on ? rule->reg0 : 1/*bitmask_flags*/, reg_live_on? rule->reg1 : 0/*bitmask_reg*/);
#if DEBUG_VERBOSE
                    for (id = 0; id < nmodules; ++id) {
                        if (dr_module_contains_addr(loaded_modules[id],instr_get_app_pc(instr))) {
                            cout<<"Call instr: "<<hex<<(PCAddress)instr_get_app_pc(instr)<<" is number "<<(PCAddress)instr_get_app_pc(instr)-(PCAddress)loaded_modules[id]->start<<" from "<<loaded_modules[id]->full_path<<endl;;
                            cout<<endl;

                            break;
                        }
                    }
#endif
                //dr_insert_clean_call(drcontext, bb, trigger, (void *)store_return_target_clean, false, 2, OPND_CREATE_INTPTR((PCAddress)instr_get_app_pc(instr) + rule->reg2), OPND_CREATE_INTPTR(instr_get_app_pc(trigger)));
                dr_insert_clean_call(drcontext, bb, trigger, (void *)store_return_target_clean, false, 1, OPND_CREATE_INTPTR((PCAddress)instr_get_app_pc(instr) + rule->reg2)/*, OPND_CREATE_INTPTR(instr_get_app_pc(trigger))*/);
            }
	break;
        case SAVE_MAIN_ENTRY:
               if(monitor_enable){
                dr_insert_clean_call(drcontext, bb, trigger, (void *)store_return_target_clean, false, 1, OPND_CREATE_INTPTR(rule->reg2) /*OPND_CREATE_INTPTR(instr_get_app_pc(trigger))*/);
               }

        break;
	case VERIFY_RETURN_TARGET:
            if(monitor_enable){
#if DEBUG_VERBOSE
                for (id = 0; id < nmodules; ++id) {
                    if (dr_module_contains_addr(loaded_modules[id],instr_get_app_pc(instr))) {
                        cout<<"Return instr: "<<hex<<(PCAddress)instr_get_app_pc(instr)<<" is number "<<(PCAddress)instr_get_app_pc(instr)-(PCAddress)loaded_modules[id]->start<<" from "<<loaded_modules[id]->full_path<<endl;;
                        break;
                    }
                }
#endif
                dr_insert_clean_call(drcontext, bb, trigger, (void *)verify_longjmp_target_clean, false, 1, OPND_CREATE_MEMPTR(DR_REG_XSP,0)/*, OPND_CREATE_INTPTR(instr_get_app_pc(trigger))*/);
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
            //verify_jmp_target(janus_context, trigger, flag_live_on ? rule->reg0 : 1/*bitmask_flags*/, reg_live_on? rule->reg1 : 0/*bitmask_reg*/);
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
         case ENABLE_MONITORING:
             if(!monitor_enable){ //only do it first time
                 dr_insert_clean_call(drcontext, bb, instr, (void *)enable_monitoring, false, 1, OPND_CREATE_INTPTR(instr_get_app_pc(instr)));
             }
         break;
         case DISABLE_MONITORING:
                 dr_insert_clean_call(drcontext, bb, instr, (void *)disable_monitoring, false, 1, OPND_CREATE_INTPTR(instr_get_app_pc(instr)));
         break;
         

        default:
                //fprintf(stderr,"In basic block 0x%lx static rule not recognised %d\n",bbAddr,rule_opcode);
            break;
        }
    }
    /*------------------------------------------------------------------------------*/
    /*--------------------------Translation Analysis Routines-----------------------*/
    /*------------------------------------------------------------------------------*/

    /*------------------------------------------------------------------------------*/
    /*-----------------Instrumentation Call Back Routines---------------------------*/
    /*------------------------------------------------------------------------------*/

static void verify_return_target(JANUS_CONTEXT, instr_t* trigger, uint64_t bitmask_flags, uint64_t bitmask_reg){
}
static void store_return_target(JANUS_CONTEXT, instr_t* trigger, uint64_t bitmask_flags, uint64_t bitmask_reg){
}
static void verify_jmp_target(JANUS_CONTEXT, instr_t* trigger, uint64_t bitmask_flags, uint64_t bitmask_reg){
}
static void verify_call_target(JANUS_CONTEXT, instr_t* trigger, uint64_t bitmask_flags, uint64_t bitmask_reg){
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
static void unwind_longjmp(JANUS_CONTEXT, instr_t* trigger, uint64_t bitmask_flags, uint64_t bitmask_reg){
    instr_t *meta_instr;
    
    instr_t *unpoison_stack_label;
    /*--------- saving RSI/RDI registers -----------------*/
    if(inRegSet(bitmask_reg,DR_REG_R8)) dr_save_reg(drcontext, bb,trigger, DR_REG_R8, SPILL_SLOT_2);
    if(inRegSet(bitmask_reg,DR_REG_R9)) dr_save_reg(drcontext, bb,trigger, DR_REG_R9, SPILL_SLOT_3);
    if(inRegSet(bitmask_reg,DR_REG_RDI)) dr_save_reg(drcontext, bb,trigger, DR_REG_RDI, SPILL_SLOT_5);

    if(bitmask_flags) 
        dr_save_arith_flags(drcontext, bb,trigger, SPILL_SLOT_4);
    
#if 0
    //load address of longjmp buffer.
    meta_instr = INSTR_CREATE_mov_ld(drcontext,opnd_create_reg(DR_REG_R8), OPND_CREATE_MEM64(DR_REG_RDI, 0x30));
    instrlist_meta_preinsert(bb,trigger,meta_instr);


    meta_instr = INSTR_CREATE_ror(drcontext,opnd_create_reg(DR_REG_R8), OPND_CREATE_INT8(0x11));
    instrlist_meta_preinsert(bb,trigger,meta_instr);
    
    dr_insert_get_seg_base(drcontext, bb,trigger, (reg_id_t)DR_SEG_FS, (reg_id_t)DR_REG_RDI);

    
    meta_instr = INSTR_CREATE_xor(drcontext, opnd_create_reg(DR_REG_R8), OPND_CREATE_MEM64(DR_REG_RDI, 0x30));
    instrlist_meta_preinsert(bb,trigger,meta_instr);
    
    
    /* ----- UNPOISON_STACK ------- */
    unpoison_stack_label = INSTR_CREATE_label(drcontext);
    instrlist_meta_preinsert(bb,trigger, unpoison_stack_label);


    //unpoison shadow value 
    meta_instr = XINST_CREATE_move(drcontext,opnd_create_reg(DR_REG_R9),opnd_create_reg(DR_REG_R8));
    instrlist_meta_preinsert(bb,trigger,meta_instr);

    meta_instr = INSTR_CREATE_shr(drcontext,opnd_create_reg(DR_REG_R9),OPND_CREATE_INT8(3));
    instrlist_meta_preinsert(bb,trigger, meta_instr);
    
    meta_instr = INSTR_CREATE_mov_imm(drcontext, opnd_create_reg(DR_REG_RDI), OPND_CREATE_INT64(0x0));
    instrlist_meta_preinsert(bb,trigger, meta_instr);
    
    meta_instr = INSTR_CREATE_mov_st(drcontext, OPND_CREATE_MEM8(DR_REG_R9,SHADOW_OFFSET), opnd_create_reg(DR_REG_DIL));
    instrlist_meta_preinsert(bb,trigger, meta_instr);
    
    //sub r8,8 and cmp rsp, r8
    meta_instr = XINST_CREATE_sub(drcontext,opnd_create_reg(DR_REG_R8),OPND_CREATE_INT64(8));
    instrlist_meta_preinsert(bb,trigger, meta_instr);
    

    meta_instr = INSTR_CREATE_cmp(drcontext,opnd_create_reg(DR_REG_RSP),opnd_create_reg(DR_REG_R8));
    instrlist_meta_preinsert(bb,trigger,meta_instr);
    

    meta_instr = INSTR_CREATE_jcc(drcontext,OP_jne,opnd_create_instr(unpoison_stack_label));
    instrlist_meta_preinsert(bb,trigger, meta_instr);
#endif
    /*--------- restoring RSI/RDI registers -----------------*/
    if(bitmask_flags)
        dr_restore_arith_flags(drcontext, bb,trigger, SPILL_SLOT_4);
    if(inRegSet(bitmask_reg,DR_REG_R8)) dr_restore_reg(drcontext, bb,trigger, DR_REG_R8, SPILL_SLOT_2);
    if(inRegSet(bitmask_reg,DR_REG_R9)) dr_restore_reg(drcontext, bb,trigger, DR_REG_R9, SPILL_SLOT_3);
    if(inRegSet(bitmask_reg,DR_REG_RDI)) dr_restore_reg(drcontext, bb,trigger, DR_REG_RDI, SPILL_SLOT_5);
}

#if 0
void print_reg_bounds(){
    cout<<"*-----------REG BOUNDS-----------*"<<endl;
    for(auto it: reg_bounds){
        cout<<hex<<it.first<<"\t"<<hex<<it.second.base<<"\t"<<hex<<it.second.bound<<endl;
    }
    cout<<endl;
}
void print_mem_bounds(){
    cout<<"*-----------REG TABLE-----------*"<<endl;
    for(auto it: reg_table){
        cout<<hex<<it.first<<"\t"<<hex<<it.second.base<<"\t"<<hex<<it.second.bound<<endl;
    }
    cout<<endl;
}
void print_pp_stack(){
    cout<<"*-----------PP_STACK-----------*"<<endl;
    /*for(auto it: pp_stack){
        cout<<dec<<it.type<<"\t"<<dec<<it.val<<"\t"<<dec<<it.has_bounds<<endl;
    }*/
    cout<<endl;
}
#endif
