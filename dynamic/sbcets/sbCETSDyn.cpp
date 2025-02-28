/* JANUS Client for secure execution */

/* Header file to implement a JANUS client */
#include "janus_api.h"
#include "dr_api.h"

//#include "drmgr.h"
#include "drsyms.h"
//#include "drwrap.h"
#include <inttypes.h>
#include <iostream>
#include <cstring>
#include "sbCETSDyn.h"
#include "instrumentation.h"
using namespace std;

//execution mode
#define DYN_ONLY_MODE 0
#define HYBRID_MODE 0
#define STAT_ONLY_MODE 1

#define WRAP 0

#define VERBOSE_ERROR
#define ERROR_THRESHOLD 10
int vdso_id=-1;
const char* main_module;
uint64_t error_counter=0;
uint64_t push_count =0;
uint64_t pop_count =0;
bool monitor_enable = true;
bool flag_live_on = true;
bool reg_live_on = true;
//std::set<string> include_mod = {"perlbench_base.gcc55-O3"};
#define MAX_STR_LEN 256
std::map<int, bool> canRestore;
typedef struct p_memdata{
    addr_t                      lea;
    addr_t                      base;
    uint64_t                    bound;
    p_memdata() {clear();}
    void                        clear() {base=0x0; bound=0;};
} memdata;
uint64_t regs[8];

std::map<int, memdata> mem_stack;

static void
exit_summary(void *drcontext) {
    if(error_counter)
       cout<<"\033[31m" <<"Total overflow error: "<< "\033[0m" <<error_counter<<endl;
   else
       cout<<"\033[32m"<<"Total overflow error: "<<"\033[0m" <<error_counter<<endl;
     //  drwrap_exit();

}
static void print_BB(uint64_t BBaddr){
    cout<<"instr:"<<hex<<BBaddr<<endl;
}
char* get_binfile_name(string filepath){
    // Returns first token
    char *token = strtok(const_cast<char*>(filepath.c_str()), "/");
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
    return filename;
}
#if WRAP && DYN_ONLY
static void *wrap_lock;

static
void pre_record(void *wrapcxt, OUT void **user_data){
        dr_mutex_lock(wrap_lock);
        uint64_t arg = (uint64_t)drwrap_get_arg(wrapcxt, 0);
        cout<<"SIZE: "<<arg<<endl;
        dr_mutex_unlock(wrap_lock);
}
static
void post_record(void *wrapcxt, void *user_data){
    cout<<"EXIT FROM MALLOC"<<endl;
}
#endif
static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
#if WRAP && DYN_ONLY
                  bool for_trace, bool translating, OUT void **user_data);
#else
                  bool for_trace, bool translating);
#endif

static void generate_trace_events(JANUS_CONTEXT);
static void generate_security_events(JANUS_CONTEXT);
static void generate_events_by_rule(JANUS_CONTEXT, instr_t *instr);

void print_reg_table();
void print_mem_table();
void print_pp_stack();
void print_reg_bounds();
void print_mem_bounds();

struct pp_data{
    int type;
    uint64_t val;
    bool has_bounds;
    pp_data() {}
    pp_data(const pp_data& A) : type(A.type), val(A.val), has_bounds(A.has_bounds) {}
};
typedef struct pp_data stack_op;
std::stack<stack_op> pp_stack;
enum {
   REG=0,
   ABS,
   CONST,
   GLOBAL,
   MEM,
   STACKVAR
};
std::stack<uint64_t> stackargs;
std::stack<int> stype;
std::stack<int> sval;
std::stack<int> pval;
app_pc orig_main;


/***************----------------GLOBAL ----------------*********************/

/*void check_lea_global(int id, int dest, uint64_t base, uint64_t size, uint64_t instr){
    addr_t LEAddr = iList[id].LEAddr;
    int dest_reg = get_64bit(dest);
    if(memory_table.find(LEAddr) != memory_table.end()){
       reg_table[dest_reg].base = memory_table[LEAddr].base;
       reg_table[dest_reg].bound = memory_table[LEAddr].bound;
    }
    else{
        reg_table[dest_reg].base = base;
        reg_table[dest_reg].bound= base+size;
    }

}*/
/**************************************************************************/
static void
event_module_load(void *drcontext, const module_data_t *info, bool loaded){
    char  filepath[MAX_STR_LEN];
    char  binfilepath[MAX_STR_LEN];
    bool load_schedule = true;
    bool rules_found = false;
#ifdef DEBUG_VERBOSE
    if(loaded == true)
        cout<<"module loaded"<<endl;
    dr_fprintf(STDOUT, " full_name %s \n", info->full_path);
    dr_fprintf(STDOUT, " module_name %s \n", dr_module_preferred_name(info));
    dr_fprintf(STDOUT, " entry" PFX "\n", info->entry_point);
    dr_fprintf(STDOUT, " start" PFX "\n", info->start);
#endif
#if WRAP
    app_pc rewrap = (app_pc)dr_get_proc_address(info->handle, "malloc");

   
    if (rewrap != NULL) {
            dr_fprintf(STDERR, "<wrapped malloc @" PFX "> from routine %s\n", rewrap, dr_module_preferred_name(info));
            bool wrapped1 = false;
            wrapped1 = drwrap_wrap(rewrap, pre_record, post_record);
            DR_ASSERT(wrapped1);

    }
#endif
    if(strcmp(dr_module_preferred_name(info), main_module) != 0){
        dr_module_set_should_instrument(info->handle, false);
        load_schedule = false;
    }
    loaded_modules[nmodules] = dr_copy_module_data(info);
    nmodules++;
    if(strcmp(dr_module_preferred_name(info), "linux-vdso.so.1") == 0) {vdso_id=nmodules-1 ; return;}
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
    if(load_schedule && strcmp(dr_module_preferred_name(info), "linux-vdso.so.1") != 0){
        strcpy(filepath, info->full_path);
        //char * binfile = const_cast<char*>(dr_module_preferred_name(info));
        char * binfile = get_binfile_name(info->full_path);
        strcpy(binfilepath,rs_dir);
        strcat(binfilepath,binfile);
        strcat(binfilepath,".jrs");
#ifdef DEBUG_VERBOSE
        printf("binfilepath: %s\n", binfilepath);
#endif
        //strcat(filepath, ".jrs");
        FILE *file = fopen(binfilepath, "r");
        if(file != NULL) {
            rules_found=true;
        }
    //fclose(file);
        if(rules_found){
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
    dr_fprintf(STDOUT,"               Janus Secure Execution\n");
    dr_fprintf(STDOUT,"---------------------------------------------------------------\n\n");
#endif
#if WRAP && DYN_ONLY
    drmgr_init();
    drwrap_init();
    //set_client_mode((JMode)JSBCETS_LIVE);
#endif
    set_client_mode((JMode)JSBCETS_LIVE);
    module_data_t *main = dr_get_main_module();
    main_module = dr_module_preferred_name(main);
    /*Initialise symbol library*/
    if (drsym_init(0) != DRSYM_SUCCESS) {
            printf("WARNING: unable to initialize symbol translation\n");
    }
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
#if WRAP && DYN_ONLY
    drmgr_register_bb_instrumentation_event(event_basic_block, NULL, NULL); 
    drmgr_register_thread_exit_event(exit_summary);
    drmgr_register_module_load_event(event_module_load);
#else
    dr_register_bb_event(event_basic_block); 
    dr_register_thread_exit_event(exit_summary);
    dr_register_module_load_event(event_module_load);
#endif
    /* Initialise janus components */
    //janus_init(id);

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
void enable_monitoring(app_pc main_pc){
    cout<<"ENABLED..."<<endl;
    dr_mcontext_t mc = { sizeof(mc), DR_MC_ALL };
    dr_get_mcontext(dr_get_current_drcontext(), &mc);
    monitor_enable = 1;
    dr_flush_region(NULL, ~0UL ); //flush all the code
    mc.pc = main_pc;
    dr_redirect_execution(&mc);
}
static void
generate_dynamic_events(JANUS_CONTEXT){


    instr_t     *instr, *last;
    app_pc      current_pc, first_pc;

    opnd_t src, dest;
    int num_srcs, num_dsts, i;
    instr_t *first_instr = instrlist_first_app(bb);
    first_pc = instr_get_app_pc(first_instr);
    last = instrlist_last_app(bb);
    //TODO: get malloc, calloc and free to instrument them
    if(instr_is_call(last)){
         //uintptr_t trg_addr = (uintptr_t)opnd_get_pc(instr_get_target(last));
         app_pc trg_addr = opnd_get_pc(instr_get_target(last));
         if((uintptr_t)trg_addr != 0){
             cout<<"instr: "<<hex<<(uintptr_t)instr_get_app_pc(last)<<endl;
             cout<<" target: "<<get_func_name(trg_addr)<<endl;
         }
    }
    for (instr = first_instr;
         instr != NULL;
         instr = instr_get_next_app(instr))
    { 
        int mem_mode = 0; 
        if(instr_reads_memory(instr)) mem_mode+=1;
        if(instr_writes_memory(instr)) mem_mode+=2;
        
        num_srcs = instr_num_srcs(instr);
        num_dsts = instr_num_dsts(instr);

        for(i=0; i< num_srcs; i++){
           opnd_t src = instr_get_src(instr, i);
        }
        for(i=0; i< num_dsts; i++){
           opnd_t dest = instr_get_dst(instr, i);
        }
        int opcode = instr_get_opcode(instr);
//malloc, free
        if(mem_mode && opcode != OP_push && opcode != OP_push_imm && opcode != OP_pop && opcode != OP_cmp && !instr_is_cti(instr)){
            if(mem_mode == 1){// reads_memory
#if DEBUG_VERBOSE
                      cout<<endl<<"MEM MODE 1: "<<hex<<uintptr_t(instr_get_app_pc(instr))<<endl;
                      instr_disassemble(drcontext, instr, STDOUT);
                      cout<<endl;
#endif
                  if(instr_is_mov(instr)){ // dest is reg
                      //if (opnd_is_base_disp(src)){
                      if (opnd_is_base_disp(src) || !opnd_is_abs_addr(src)){
                          if(opnd_get_base(src) != DR_REG_NULL){
                        //  TABLE_MEM_REG_LOAD
                             check_deref_mem_load(janus_context, instr, 1/*bitmask_flags*/, 0/*bitmask_reg*/ , opnd_get_reg(dest)/*dest_id*/,opnd_get_base(src) /*base_reg_id*/ , MEM_REF /*mem reference*/);
                          }
                      }
                      else if(opnd_is_abs_addr(src)){ // Returns true iff opnd is a (near or far) absolute address operand. Returns true for both base-disp operands with no base or index and 64-bit non-base-disp absolute address operand
                         // ABS_MEM_REG_LOAD
                         check_deref_mem_load(janus_context, instr, 1/*bitmask_flags*/, 0/*bitmask_reg*/, opnd_get_reg(dest)/*dest_id*/, 0 /*base_id*/, ABS_MEM /*abs_mem*/ );
                      }
                  }
                  else if (opcode == OP_add || opcode == OP_sub || opcode == OP_mul || opcode == OP_div){
                      if(opnd_is_base_disp(src) && opnd_get_base(src) != DR_REG_NULL){
                          //ARITH_MEM_REG_LOAD
                          check_deref_mem_load(janus_context, instr, 1/*bitmask_flags*/, 0/*bitmask_reg*/, opnd_get_reg(dest), opnd_get_base(src), ARITH_MEM);
                      }

                  }
            }
            else if(mem_mode == 2){ //writes memory
#if DEBUG_VERBOSE
                      cout<<endl<<"MEM MODE 2: "<<hex<<uintptr_t(instr_get_app_pc(instr))<<endl;
                      instr_disassemble(drcontext, instr, STDOUT);
                      cout<<endl;
#endif
                if(instr_is_mov(instr)){//OP_mov_st, OP_mov_ld, OP_mov_imm, OP_mov_seg, or OP_mov_priv
                    if (opnd_is_base_disp(dest)){ 
                        if (opnd_is_reg(src)){
                                //TABLE_REG_MEM_STORE
                                check_deref_mem_store(janus_context, instr, 1/*bitmask_flags*/, 0/*bitmask_reg*/, opnd_get_reg(src) /*src_id*/, opnd_get_base(dest) /*base_reg_id*/, MEM_REF_STORE);
                        }
                        else if(opnd_is_immed(src)){
                                //TABLE_VALUE_MEM
                                check_deref_mem_store(janus_context, instr, 1/*bitmask_flags*/, 0/*bitmask_reg*/, 0 /*src_id*/, opnd_get_base(dest), CONST_MEM_STORE);
                        }
                    }
                    else if(opnd_is_abs_addr(dest)){
                        if (opnd_is_reg(src)){
                            //ABS_REG_MEM_STORE
                            check_deref_mem_store(janus_context, instr, 1/*bitmask_flags*/, 0/*bitmask_reg*/, opnd_get_reg(src)/*src_id*/, 0 /*base_reg_id*/,ABS_MEM_STORE);
                        }
                        else if(opnd_is_immed(src)){
                            //ABS_VALUE_MEM
                            check_deref_mem_store(janus_context, instr, 1/*bitmask_flags*/, 0/*bitmask_reg*/, 0, 0,  CONST_ABS_MEM_STORE);
                        }
                    }
                }
                else if(opcode == OP_add || opcode == OP_sub || opcode == OP_mul || opcode == OP_div){
                   reg_id_t dest_base = opnd_get_base(dest);
                   if((opnd_is_reg(src) || opnd_is_immed(src)) && dest_base!= DR_REG_NULL){
                      // ARITH_REG_MEM_STORE,  ARITH_VALUE_MEM
                        check_deref_mem_store(janus_context, instr, 1/*bitmask_flags*/, 0/*bitmask_reg*/, opnd_get_reg(src), dest_base ,ARITH_MEM_STORE);
                   }

                }

                        
            }
            else{ //both reads and writes
#if DEBUG_VERBOSE
              cout<<endl<<"MEM MODE 3: "<<hex<<uintptr_t(instr_get_app_pc(instr))<<endl;
              instr_disassemble(drcontext, instr, STDOUT);
              cout<<endl;
#endif
            }
        }//instr_has_rel_addr_reference()
        else if(instr_is_mov(instr)){
             if (opnd_is_reg(dest)){
                if(opnd_is_reg(src)){ 
                    //TABLE_REG_REG_COPY
                    copy_reg_table(janus_context, instr, 1/*bitmask_flags*/,0/*bitmask_reg*/, opnd_get_reg(src), opnd_get_reg(dest));
                }
                else if(opnd_is_immed(src)){
                    //TABLE_VALUE_REG
                     reg_id_t dest_id = get_64bit(opnd_get_reg(dest));
                     remove_reg_table(janus_context, instr, dest_id, 1/*bitmask_flags*/, 0/*bitmask_regs*/);
                }
             }
        }
        else if( opcode == OP_lea){
            //TODO:if base is SP, check_lea_stack
            //else
            check_lea_mem(janus_context, instr, 1/*bitmask_flags*/, 0/*bitmask_reg*/);
        }
    }
    return;
}


/* Main execution loop: this will be executed at every initial encounter of new basic block */
static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
#if WRAP && DYN_ONLY
                  bool for_trace, bool translating, OUT void **user_data)
#else
                  bool for_trace, bool translating)
#endif
{
    uint64_t num_instructions = 0;
    //get current basic block starting address
    PCAddress bbAddr = (PCAddress)dr_fragment_app_pc(tag);
     /* count the number of instructions in this block */
     //cout<<"bbAddr: "<<hex<<bbAddr<<" bbsize= " <<dec<<num_instructions<<" fragment size: "<<dr_fragment_size(drcontext,tag)<<endl; 

     
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
bool instrument = true;
int ins_count = 0;
void trace_instr(uintptr_t pc, uintptr_t bb){
   if(instrument){
       cout<<dec<<ins_count<<" pc: "<<hex<<(uintptr_t)pc<<" BB: "<<bb<<endl;
       //ins_count++;
       //if(ins_count>50) instrument = false;
   }
}
void enable_instrument(){
    instrument = false;
    cout<<dec<<ins_count<<" STOP:"<<endl;
    cout<<hex<<" pc: 0x490318"<<endl;
    exit(0);
    //instrument = true;
}
void copy_reg_table_clean(int src, int dest){
    auto it = reg_table.find(src);
    if(it !=  reg_table.end()){
        reg_table[dest].first = it->second.first;
        reg_table[dest].second = it->second.second;
    }
}
static void
generate_trace_events(JANUS_CONTEXT){
    app_pc      current_pc;
    instr_t     *instr, *last = NULL;

    for (instr = instrlist_first_app(bb);
         instr != NULL;
         instr = instr_get_next_app(instr))
    {
        current_pc = instr_get_app_pc(instr);
        dr_insert_clean_call(drcontext, bb, instr, (void*)trace_instr, false, 2, OPND_CREATE_INTPTR(current_pc),OPND_CREATE_INTPTR(dr_fragment_app_pc(tag) ));
    }

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
    opnd_t src_opnd;
    switch (rule_opcode) {
        //TODO: move bound check after the dereference instruction. get the metadata from lea of mem-operand, get value from val in reg, remove reg from reg table. and then check overflow in memory table, not reg table	
        /*----------- RECORD MALLOC DATA ----------*/
	case BND_RECORD_SIZE_MALLOC:
            record_size_malloc(janus_context, trigger);
	break;
	case BND_RECORD_SIZE_CALLOC:
            record_size_calloc(janus_context, trigger);
	break;
        case MONITOR_FREE_CALL:
            monitor_free_call(janus_context, trigger, flag_live_on ? rule->reg0 : 1/*bitmask_flags*/, reg_live_on ? rule->reg1 : 0/*bitmask_reg*/ );
	break;
	case BND_RECORD_BASE:
            record_base_pointer(janus_context, trigger, flag_live_on ? rule->reg0 : 1/*bitmask_flags*/, reg_live_on? rule->reg1 : 0/*bitmask_reg*/);
	break;
        /*-----------REG -> REG COPY ----------*/
        case TABLE_REG_REG_COPY:
            copy_reg_table(janus_context, trigger, flag_live_on ? rule->reg0 : 1/*bitmask_flags*/,reg_live_on? rule->reg1 : 0/*bitmask_reg*/, rule->reg2 /*src_id*/, rule->reg3 /*dest_id*/);
        break;
        case BND_REMOVE_RAX: //TODO: delete RAX from reg table on function calls        
            remove_reg_table(janus_context, trigger, flag_live_on ? rule->reg0 : 1/*bitmask_flags*/,reg_live_on ? rule->reg1 : 0/*bitmask_regs*/ , (reg_id_t)DR_REG_RAX);
	break;
        /*---------- MEM -> REG LOAD---------- */
        case TABLE_MEM_REG_LOAD:
             check_deref_mem_load(janus_context, trigger, flag_live_on ? rule->reg0 : 1/*bitmask_flags*/, reg_live_on ? rule->reg1 : 0/*bitmask_reg*/ , rule->reg2 /*dest_id*/, rule->reg3/*base reg*/,MEM_REF /*mem reference*/);
        break;
        case ABS_MEM_REG_LOAD:
             check_deref_mem_load(janus_context, trigger,flag_live_on ? rule->reg0 : 1/*bitmask_flags*/, reg_live_on ? rule->reg1 : 0/*bitmask_reg*/, rule->reg2 /*dest_id*/, 0/*base_id*/,ABS_MEM /*abs_mem*/ );
        break;
        case ARITH_MEM_REG_LOAD:
            check_deref_mem_load(janus_context, trigger, flag_live_on ? rule->reg0 : 1/*bitmask_flags*/, reg_live_on ? rule->reg1 : 0/*bitmask_reg*/, rule->reg2/*dest_id*/, rule->reg3 /*mem base*/,ARITH_MEM);
        break;
        case TABLE_VALUE_REG:
            remove_reg_table(janus_context, trigger, flag_live_on ? rule->reg0 : 1/*bitmask_flags*/, reg_live_on ? rule->reg1 : 0/*bitmask_regs*/, rule->reg2/*dest_id*/);
                //TODO: either remove reg from reg table. what if we are copying abs address value? 
        break;
                //TODO: stack memory to registr
        
        case TABLE_VALUE_MEM:
            check_deref_mem_store(janus_context, trigger, flag_live_on ? rule->reg0 : 1/*bitmask_flags*/, reg_live_on ? rule->reg1 : 0/*bitmask_reg*/, 0 /*src id*/, rule->reg3 /*base reg*/ ,CONST_MEM_STORE);
        break;
        /*----------- REG/VAL -> MEM/ABS STORE---------- */
        case TABLE_REG_MEM_STORE:
            check_deref_mem_store(janus_context, trigger, flag_live_on ? rule->reg0 : 1/*bitmask_flags*/, reg_live_on ? rule->reg1 : 0/*bitmask_reg*/, rule->reg2 /*src_id*/, rule->reg3 /*base reg*/, MEM_REF_STORE);
        break;

        case ABS_REG_MEM_STORE: //Absolute or PC-relative address?
            check_deref_mem_store(janus_context, trigger, flag_live_on ? rule->reg0 : 1/*bitmask_flags*/, reg_live_on ? rule->reg1 : 0/*bitmask_reg*/, rule->reg2/*src_reg*/,0 /*base reg*/, ABS_MEM_STORE);
        break;
        case ABS_VALUE_MEM:
            check_deref_mem_store(janus_context, trigger, flag_live_on ? rule->reg0 : 1/*bitmask_flags*/, reg_live_on ? rule->reg1 : 0/*bitmask_reg*/, 0,0, CONST_ABS_MEM_STORE);
        break;
        case ARITH_REG_MEM_STORE:
            check_deref_mem_store(janus_context, trigger, flag_live_on ? rule->reg0 : 1/*bitmask_flags*/, reg_live_on ? rule->reg1 : 0/*bitmask_reg*/, rule->reg2 /*src id*/, rule->reg3 /*base reg*/ ,ARITH_MEM_STORE);
        break;
        case ARITH_VALUE_MEM:
            check_deref_mem_store(janus_context, trigger, flag_live_on ? rule->reg0 : 1/*bitmask_flags*/, reg_live_on ? rule->reg1 : 0/*bitmask_reg*/, 0 /*src id*/, rule->reg3 /*base reg*/,ARITH_MEM_STORE);
        break;
        case LEA_COPY_BASE:
            check_lea_mem(janus_context, trigger, flag_live_on ? rule->reg0 : 1/*bitmask_flags*/, reg_live_on ? rule->reg1 : 0/*bitmask_reg*/);
        break;
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
            /*----- Load from Global Memory ------- */
#if 0
            case GLOBAL_MEM_REG_LOAD:
                dest = get_64bit(opnd_get_reg(instr_get_dst(instr,0)));
                check_deref_global_mem_load(janus_context, trigger, flag_live_on ? rule->reg0 : 1/*bitmask_flags*/, reg_live_on ? rule->reg1 : 0/*bitmask_reg*/,  dest, rule->reg2/*global_base*/, rule->reg3/*global_bounds*/, MEM_REF);
            break;
            case GLOBAL_ARITH_MEM_REG_LOAD:
                dest = get_64bit(opnd_get_reg(instr_get_dst(instr,0)));
                check_deref_global_mem_load(janus_context, trigger, flag_live_on ? rule->reg0 : 1/*bitmask_flags*/, reg_live_on ? rule->reg1 : 0/*bitmask_reg*/,  dest, rule->reg2 /*global_base*/, rule->reg3 /*global_bounds*/, ARITH_MEM);
            break;
            case GLOBAL_VALUE_REG:
            case GLOBAL_LEA_COPY_BASE:
                dest = get_64bit(opnd_get_reg(instr_get_dst(instr,0)));
                check_deref_global_mem_load(janus_context, trigger, flag_live_on ? rule->reg0 : 1/*bitmask_flags*/, reg_live_on ? rule->reg1 : 0/*bitmask_reg*/,  dest, rule->reg2/*global_base*/, rule->reg3 /*global_bounds*/, GLOBALVAL_TO_REG);
            break;
            case ABS_GLOBAL_MEM_REG_LOAD:
                dest = get_64bit(opnd_get_reg(instr_get_dst(instr,0)));
                check_deref_global_mem_load(janus_context, trigger, flag_live_on ? rule->reg0 : 1/*bitmask_flags*/, reg_live_on ? rule->reg1 : 0/*bitmask_reg*/,  dest, rule->reg2/*global_base*/, rule->reg3 /*global_bounds*/, ABS_GLOBALMEM_TO_REG);
            break;
            /*--- Store to Global Memory ------*/
            case GLOBAL_REG_MEM_STORE:
                src = get_64bit(opnd_get_reg(instr_get_src(instr,0)));
                check_deref_global_mem_store(janus_context, trigger, flag_live_on ? rule->reg0 : 1/*bitmask_flags*/, reg_live_on ? rule->reg1 : 0/*bitmask_reg*/, src/*src_id*/, rule->reg2 /*global base*/, rule->reg3 /*global_bounds*/, MEM_REF_STORE);
            break;
            case GLOBAL_TABLE_VALUE_MEM:
            case GLOBAL_ARITH_VALUE_MEM:
            case GLOBAL_ARITH_REG_MEM_STORE:
                src = get_64bit(opnd_get_reg(instr_get_src(instr,0)));     //TODO: need to make sure it is not memory
                check_deref_global_mem_store(janus_context, trigger, flag_live_on ? rule->reg0 : 1/*bitmask_flags*/, reg_live_on ? rule->reg1 : 0/*bitmask_reg*/, src/*src_id*/, rule->reg2 /*global base*/, rule->reg3 /*global_bounds*/, ARITH_MEM_STORE);
            break;

            case POP_STACK:
            break;
            case PUSH_STACK:
            break;
            case POP_STACK_REG:
            break;
            case PUSH_STACK_REG:
            break;
            case PUSH_REG:
            break;
            case POP_REG:
            break;
            case PUSH_GLOBAL:
            break;
            case PUSH_CONSTANT:
            break;
            case PUSH_MEM:
            break;
            case PUSH_ABS:
            break;
            case PUSH_STACK_VAR:
            break;
            case LEA_COPY_STACK_BASE:
                trigger= get_trigger_instruction(bb,rule);
                //check_lea_stack(janus_context, trigger, reg_live_on ? rule->reg1 : 0/*bitmask_reg*/);
            break;
#endif
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


    /****----------- utilities- copy base/bound to reg table ----****/

    /****----------- Record heap allocation Malloc/Calloc base ----****/
    /****----------- Record heap allocation Malloc/Calloc size----****/
    /****------------------Load Dereference Check - MEM -> REG-----------****/
    /****------------------Load Dereference Check - ABS MEM -> REG-----------****/
    /****------------------Store Dereference Check REG ->MEM STORE------------****/
    /****------------------Store Dereference Check REG ->ABS MEM STORE------------****/
    /****------------------Store Dereference Check CONST VAL->MEM STORE------------****/
    /****------------------Store Dereference Check CONST VAL->ABS MEM STORE------------****/
    /****-------------------------- Remove RAX from Reg table ----------- -----------****/
    /****------------------------ Monitor Free Calls -------------------------****/

    /****------------------------ Monitor Stack Buffer -------------------------****/


    /****------------------------ LEA instructions ---------------------****/

    /****------------------------ print tables -------------------------****/
    void print_reg_table(){
        cout<<"*-----------REG TABLE-----------*"<<endl;
        for(auto it: reg_table){
            cout<<hex<<it.first<<"\t"<<hex<<it.second.first<<"\t"<<hex<<it.second.second<<endl;
        }
        cout<<endl;
}
void print_mem_table(){
    cout<<"*----------MEM BOUNDS-----------*"<<endl;
    for(auto it: memory_table){
        cout<<hex<<it.first<<"\t"<<hex<<it.second.first<<"\t"<<hex<<it.second.second<<endl;
    }
    cout<<endl;
        cout<<"*-----------REG TABLE-----------*"<<endl;
        for(auto it: reg_table){
            cout<<hex<<it.first<<"\t"<<hex<<it.second.first<<"\t"<<hex<<it.second.second<<endl;
        }
        cout<<endl;
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
