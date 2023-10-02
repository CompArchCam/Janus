/* JANUS Client for secure execution */

/* Header file to implement a JANUS client */
#include "janus_api.h"
#include "dr_api.h"
# include "drsyms.h"
#include <inttypes.h>
#include <iostream>
#include <cstring>
#include <string>
#include<setjmp.h>
#include "asan.h"

#define DEF_REG_S1 DR_REG_RSI //default register 1
#define DEF_REG_S2 DR_REG_RDI //default register 2
//#define DEBUG_VERBOSE

//execution mode
//settings for saving arithmetic flags
#define HYBRID_MODE 1
#define STAT_ONLY_MODE 0
#define DYN_ONLY_MODE 0 

#define EFLAGS_PUSH_POP 0
#define EFLAGS_DR_SAVE 1
#define EFLAGS_SETO 0


#define MAX_STR_LEN 256
#define MAX_MODULES 1024
#define POISON_CLEANCALL 0
#define MAX_STACK_DEPTH 30
# define MAX_SYM_RESULT 256

using namespace std;

char name[MAX_SYM_RESULT];
char file[MAX_SYM_RESULT];
const char* app_name;
const char* main_module;
typedef uintptr_t uptr;
typedef uint64_t u64;
typedef uint8_t u8;
typedef int8_t s8;
bool flag_live_on = true;
bool reg_live_on = true;
int modNo = -1;
int emulate_pc;
uint64_t error_counter;
uint32_t monitor_enable = 0;
uint32_t counter = 0;
guard_t instbuff[MAX_BB_SIZE];
guard_t *iList;
FILE* ofile;
std::map<opnd_size_t, int> op_size={
    {OPSZ_1, 1},
    {OPSZ_2, 2},
    {OPSZ_4, 4},
    {OPSZ_8, 8},
    {OPSZ_16, 16}
};
uint64_t addrstack[MAX_STACK_DEPTH];
int top = 0;
uint64_t canarySlot;
std::stack<addr_t> canarySlots;
std::stack<int> Slots;
int stackTop=0;
std::set<string> exclude_mod = {"libc.so.6","libclang_rt.asan-x86_64.so"};
struct reg_slots{
    uint64_t slot1; 
    uint64_t slot2;
};
struct reg_slots regs;
app_pc orig_main;


/*-------------------- ASAN Configurations for reference --------*/
#define kLowMemEnd 0x00007fff7fff
#define kLowShadowBeg 0x00007fff8000
#define kLowShadowEnd 0x00008fff6fff
#define kHighMemBeg 0x10007fff8000
#define kHighMemEnd 0x7fffffffffff
#define kHighShadowBeg 0x02008fff7000
#define kHighShadowEnd 0x10007fff7fff


#define SHADOW_SCALE 3                        //should be 3
#define SHADOW_GRANULARITY (1ULL << SHADOW_SCALE)
#define SHADOW_OFFSET 0x7fff8000                //Linux/x86_64
#define MEM_TO_SHADOW(mem) (((mem) >> SHADOW_SCALE) + (SHADOW_OFFSET))


static const u64 kFreeBSD_ShadowOffset32 = 1ULL << 30;  // 0x40000000
static const u64 kFreeBSD_ShadowOffset64 = 1ULL << 46;  // 0x400000000000
static const u64 kDefaultShadowOffset32 = 1ULL << 29;   // 0x20000000
static const u64 kDefaultShadowOffset64 = 1ULL << 44;   // 0x100000000000
uint64_t instrument_count=0;
int count_BB = 0;
static void
exit_summary(void *drcontext) {
   cout<<"Total overflow error: "<<dec<<error_counter<<endl;
   for (uint32_t i = 0; i < nmodules; ++i) {
     dr_free_module_data(loaded_modules[i]);
  }
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
void check_mem_rw_access(int id, int size);
static void mem_rw_event_flags(JANUS_CONTEXT, instr_t *instr, uint64_t bitmask_flag, uint64_t bitmask_reg);
static bool load_mem_addr(JANUS_CONTEXT, instr_t *instr, uint64_t bitmask_flag, uint64_t bitmask_reg);


void unpoison_longjmp_clean(jmp_buf jmp_buffer, uintptr_t curr_sp);

#if POISON_CLEANCALL
void poison_canary(void);
void unpoison_canary(void);
void store_canary(int id);
#else
static void unpoison_canary(JANUS_CONTEXT, instr_t *instr, uint64_t bitmask_flag, uint64_t bitmask_reg);
static void poison_canary(JANUS_CONTEXT, instr_t *instr, uint64_t bitmask_flag, uint64_t bitmask_reg);
static void store_canary(JANUS_CONTEXT, instr_t *instr, uint64_t bitmask_flag, uint64_t bitmask_reg);
#endif
static void poison_canary_single(JANUS_CONTEXT, instr_t *instr, uint64_t bitmask_flag, uint64_t bitmask_reg);
static void unpoison_canary_memaddr(JANUS_CONTEXT, instr_t *instr, uint64_t bitmask_flag, uint64_t bitmask_reg);
static void unpoison_longjmp(JANUS_CONTEXT, instr_t *instr, uint64_t bitmask_flag, uint64_t bitmask_reg);

static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating);

static void generate_asan_events(JANUS_CONTEXT);
static void generate_dynamic_events(JANUS_CONTEXT);
static void generate_events_by_rule(JANUS_CONTEXT, instr_t *instr);
bool enable_BB = true;
bool enable_instr = false;
void print_instr(uintptr_t current_pc){
     printf("executed instr: %p\n", current_pc);
}
static void
event_module_load(void *drcontext, const module_data_t *info, bool loaded){
    char  filepath[MAX_STR_LEN];
    char  binfilepath[MAX_STR_LEN];
    bool load_schedule = true;
    bool rules_found = false;

#ifdef DEBUG_VERBOSE
    dr_fprintf(STDOUT, " full_name %s \n", info->full_path);
    dr_fprintf(STDOUT, " module_name %s \n", dr_module_preferred_name(info));
    dr_fprintf(STDOUT, " entry" PFX "\n", info->entry_point);
    dr_fprintf(STDOUT, " start" PFX "\n", info->start);
#endif
    /*only check for main binary for dynamic only. TODO: we need to remove this for evaluation that includes all binaries*/
    /*if(strcmp(dr_module_preferred_name(info), main_module) != 0){ 
        dr_module_set_should_instrument(info->handle, false);
        load_schedule = false;
    }*/
    if(exclude_mod.count(dr_module_preferred_name(info))){ 
        dr_module_set_should_instrument(info->handle, false);
        load_schedule = false;
    }

    loaded_modules[nmodules] = dr_copy_module_data(info);
    nmodules++;
    
#if DYN_ONLY_MODE
    /*get the start address of main function. Note: for static or hybrid versions, we will have a rewrite rule for this*/
    if(strcmp(dr_module_preferred_name(info), main_module) == 0){ 
        char* MAIN = "main";
        size_t offs;
        if (drsym_lookup_symbol(info->full_path, MAIN, &offs, DRSYM_DEMANGLE) == DRSYM_SUCCESS) {
            orig_main = offs + info->start;
        }
    }
#endif
#if !DYN_ONLY_MODE
    /*for static and hybrid versions, load the rewrite schedules*/
    if(load_schedule && strcmp(dr_module_preferred_name(info), "linux-vdso.so.1") != 0 ){
        strcpy(filepath, info->full_path);
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
        if(rules_found){
            load_static_rules_security(binfilepath, info);
       }
    }
#endif
}

static void
init_summary(void *drcontext) {
    error_counter = 0;
}
/* Client_handler*/
DR_EXPORT void 
dr_init(client_id_t id)
{
#ifdef JANUS_VERBOSE
    dr_fprintf(STDOUT,"\n---------------------------------------------------------------\n");
    dr_fprintf(STDOUT,"               Janus Secure Execution\n");
    dr_fprintf(STDOUT,"---------------------------------------------------------------\n\n");
#endif
    set_client_mode((JMode)JASAN_LIVE); //TODO: JASAN has multiple modes
    app_name = dr_get_application_name();
    module_data_t *main = dr_get_main_module();
    main_module = dr_module_preferred_name(main);
    dr_free_module_data(main);
    /*Initialise symbol library*/
    if (drsym_init(0) != DRSYM_SUCCESS) {
        printf("WARNING: unable to initialize symbol translation\n");
    }
    /* Register event callbacks. */
    dr_register_bb_event(event_basic_block); 
    dr_register_thread_init_event(init_summary);
    dr_register_thread_exit_event(exit_summary);
    dr_register_module_load_event(event_module_load);
    /* Initialise janus components */
    janus_init_asan(id);
    
    cout<<"\033[32m"<<"ENTERNED JANUS for  "<<print_janus_mode((JMode)get_client_mode())<<endl;
#if STAT_ONLY_MODE
    cout<<"MODE is STATIC ONLY with flag_liveness="<<flag_live_on<<" reg_liveness="<<reg_live_on<<"\033[0m"<<endl;
#endif
#if HYBRID_MODE
    cout<<"MODE is HYBRID with flag_liveness="<<flag_live_on<<" reg_liveness="<<reg_live_on<<"\033[0m"<<endl;
#endif
#if DYN_ONLY_MODE
    cout<<"MODE is DYN_ONLY"<<"\033[0m"<<endl;
#endif
    iList = instbuff;
#ifdef JANUS_VERBOSE
    dr_fprintf(STDOUT,"Dynamorio client initialised\n");
#endif
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

/* Main execution loop: this will be executed at every initial encounter of new basic block */
static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating)
{
    //get current basic block starting address
    PCAddress bbAddr = (PCAddress)dr_fragment_app_pc(tag);
    RRule *rule;
#if STAT_ONLY_MODE || HYBRID_MODE
    //lookup in the hashtable to check if there is any rule attached to the block
    rule = get_static_rule_security(bbAddr);
    if (rule){                      //statically seen basicblock
        if(rule->opcode != NO_RULE){ //ignore bb with null rules (including standard lib)
            generate_asan_events(janus_context); 
        }
    }
#if HYBRID_MODE
    else if(monitor_enable){                                //dynamically generated code, or not seen statically (e.g. vdso)
        generate_dynamic_events(janus_context);
    }
#endif
#endif

#if DYN_ONLY_MODE
    if(bbAddr == (PCAddress)orig_main && !monitor_enable){
        dr_insert_clean_call(drcontext, bb, instrlist_first_app(bb), (void *)enable_monitoring, false, 1, OPND_CREATE_INT64(instr_get_app_pc(instrlist_first_app(bb))));
    }
    if(monitor_enable){
       generate_dynamic_events(janus_context);
    }
#endif
    return DR_EMIT_DEFAULT;
}
static void 
generate_dynamic_events(JANUS_CONTEXT){
   
    emulate_pc=0;

    instr_t     *instr, *last = NULL;
    app_pc      current_pc, first_pc, last_pc;
    instr_t *mem_instr[MAX_BB_SIZE];
    instr_t *first_instr = instrlist_first_app(bb);
    first_pc = instr_get_app_pc(first_instr);
    last = instrlist_last_app(bb);
    last_pc = instr_get_app_pc(last);
    int mem_count  = 0;
    for (instr = first_instr;
         instr != NULL;
         instr = instr_get_next_app(instr))
    {
        int opcode = instr_get_opcode(instr);

        //if(opcode  == OP_call || opcode  == OP_callq){
        if(instr_is_call(instr)){  
            app_pc trg_addr = opnd_get_pc(instr_get_target(instr));
            if((uintptr_t)trg_addr == 0x4024c0) //TODO:longjmp, works only for 400 perlbench
            {
               unpoison_longjmp(janus_context, instr, 1, 1);
            }
        }
        else{
            bool reads_mem = instr_reads_memory(instr);
            bool writes_mem = instr_writes_memory(instr);
            if((reads_mem || writes_mem) && (!instr_is_cti(instr) && opcode != OP_push && opcode != OP_pop)){ //excluding push, jmp and returns
                if(writes_mem){
                     if(!opnd_is_far_base_disp(instr_get_dst(instr,0))){
                        mem_instr[mem_count++] = instr;
                     }
                }
                if(reads_mem){/*check for stack canaries*/ 
                    opnd_t src0 = instr_get_src(instr,0);
                    if(!opnd_is_far_base_disp(instr_get_src(instr,0))){
                        mem_instr[mem_count++] = instr;
                    }
                    else if(opnd_is_far_base_disp(src0) && opnd_get_segment(src0) == DR_SEG_FS && opnd_get_disp(src0) == 0x28){
                       if(opcode == OP_mov_ld){
                           //store canary before the second instruction, poison it after the second instruction.
                           //mov    %fs:0x28,%rax  -> canary load instruction
                           //mov    %rax,+0x8(%rsp)-> move canary to current stack frame. store -0x8(rbp) as the canary location
                           instr_t* next = instr_get_next_app(instr);
                           if(next != NULL){
                               store_canary(janus_context, next, 1, 1);
                               instr_t* next_to_next = instr_get_next_app(next);
                               if(next_to_next != NULL){
                                 poison_canary_single(janus_context, next_to_next, 1, 1);
                               }
                           }
                       }
                       else if(opcode == OP_xor){
                          //UNPOISON:
                          //mov    0x8(%rsp),%rdi            //unpoison before this instruction to allow canary check in subseq instr
                          //xor    %fs:0x28,%rdi             //checking if canary value has been modified
                           instr_t* prev = instr_get_prev_app(instr);
                           if(prev != NULL){
                             unpoison_canary_memaddr(janus_context, prev, 1, 1);
                           }
                       }
                    }
                }
            }
        }
    }
    //check for memory errors
    //doing this is two rounds, so that we can check for memory on stack after we have first unpoisoned it
    for (int i=0; i< mem_count; i++)
    {
        mem_rw_event_flags(janus_context, mem_instr[i], 1, 1);
    }
    return;
}
static void
generate_asan_events(JANUS_CONTEXT)
{
    app_pc      current_pc;
    instr_t     *instr, *last = NULL;
    instr_t *trigger;
    instr_t *first_instr = instrlist_first_app(bb);

    app_pc first_pc = instr_get_app_pc(first_instr);

    /* Iterate through each original instruction in the block
     * generate dynamic inline instructions that emit commands in the command buffer */
    for (instr = instrlist_first_app(bb);
         instr != NULL;
         instr = instr_get_next_app(instr))
    {
    
        current_pc = instr_get_app_pc(instr);
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
     instr_t *trigger= get_trigger_instruction(bb,rule);
    instr_t *first_instr = instrlist_first_app(bb);
    app_pc first_pc = instr_get_app_pc(first_instr);
    app_pc current_pc = current_pc = instr_get_app_pc(instr);
     switch (rule_opcode) {
        case MEM_R_ACCESS:
        case MEM_W_ACCESS:
        case MEM_RW_ACCESS:
             if(monitor_enable){
                 mem_rw_event_flags(janus_context, instr, flag_live_on ? rule->reg0 : 1/*bitmask_flags*/, reg_live_on ? rule->reg1 : 1/*bitmask_reg*/);
             }
        break;
        case SAVE_AT_ENTRY: 
            if(monitor_enable){
                if(rule->reg0)//save_rdi = 1
                    PRE_INSERT(bb, trigger,
                        INSTR_CREATE_mov_st(drcontext,
                                        opnd_create_rel_addr(&(regs.slot1), OPSZ_8),
                                        opnd_create_reg(DR_REG_RDI)));
                if(rule->reg1)//save_rsi = 1
                    PRE_INSERT(bb, trigger,
                        INSTR_CREATE_mov_st(drcontext,
                                    opnd_create_rel_addr(&(regs.slot2), OPSZ_8),
                                    opnd_create_reg(DR_REG_RSI)));
            }
        break;
        case RESTORE_AT_EXIT:
            if(monitor_enable){
                if(rule->reg0)//save_rdi = 1
                    PRE_INSERT(bb, trigger,
                        INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(DR_REG_RDI),
                                            opnd_create_rel_addr(&(regs.slot1), OPSZ_8)));
                if(rule->reg1)//save_rsi = 1
                    PRE_INSERT(bb, trigger,
                        INSTR_CREATE_mov_ld(drcontext,
                                        opnd_create_reg(DR_REG_RSI),
                                    opnd_create_rel_addr(&(regs.slot2), OPSZ_8)));
            }
        break;
        case PROF_START:
            //dr_insert_clean_call(drcontext, bb, instr, (void *)print_instr, false, 1, OPND_CREATE_INTPTR(instr_get_app_pc(instr)));
        break;
        case STORE_CANARY_SLOT:
             if(monitor_enable){
#if POISON_CLEANCALL
                 load_mem_addr(janus_context, instr, 1, 0);
                 dr_insert_clean_call(drcontext, bb, instr, (void *)store_canary, false, 1, OPND_CREATE_INT64(emulate_pc));
                 emulate_pc++;
#else
                 store_canary(janus_context, instr, flag_live_on ? rule->reg0 : 1, reg_live_on ? rule->reg1: 0);
#endif
             }
         break;
         case POISON_CANARY_SLOT:
             if(monitor_enable){
#if POISON_CLEANCALL
                 dr_insert_clean_call(drcontext, bb, instr, (void *)poison_canary, false, 0);
                 emulate_pc++;
#else
                poison_canary_single(janus_context, instr, flag_live_on ? rule->reg0: 1, reg_live_on ? rule->reg1: 0);
#endif
             }
         break;
         case UNPOISON_CANARY_SLOT:
             if(monitor_enable){
#if POISON_CLEANCALL
                 dr_insert_clean_call(drcontext, bb, instr, (void *)unpoison_canary, false, 0);
                 emulate_pc++;
#else
                 unpoison_canary_memaddr(janus_context, instr, flag_live_on ? rule->reg0 :  1, reg_live_on ? rule->reg1 : 0);

#endif
             }
         break;
         case UNPOISON_CANARY_LONGJMP:
              if(monitor_enable){
                  unpoison_longjmp(janus_context, instr, flag_live_on ? rule->reg0 : 1, reg_live_on ? rule->reg1 : 0);
              }
              
         break;
         case ENABLE_MONITORING:
             if(!monitor_enable){ //only do it first time
                 dr_insert_clean_call(drcontext, bb, instr, (void *)enable_monitoring, false, 1, OPND_CREATE_INT64(instr_get_app_pc(instr)));
             }
         break;
        default:
	     // fprintf(stderr,"In basic block 0x%lx static rule not recognised %d\n",dr_fragment_app_pc(tag),rule_opcode);
	break;
    }
}
/*------------------------------------------------------------------------------*/
/*--------------------------Translation Analysis Routines-----------------------*/
/*------------------------------------------------------------------------------*/
// Typical shadow mapping on Linux/x86_64(oslo, riga, sofia) with SHADOW_OFFSET == 0x00007fff8000:
// || `[0x10007fff8000, 0x7fffffffffff]` || HighMem    ||
// || `[0x02008fff7000, 0x10007fff7fff]` || HighShadow ||
// || `[0x00008fff7000, 0x02008fff6fff]` || ShadowGap  ||
// || `[0x00007fff8000, 0x00008fff6fff]` || LowShadow  ||
// || `[0x000000000000, 0x00007fff7fff]` || LowMem     ||
//
/*MemToShadow(shadow): 0x00008fff7000 0x000091ff6dff 0x004091ff6e00 0x02008fff6fff
redzone=16
max_redzone=2048
quarantine_size_mb=256M
thread_local_quarantine_size_kb=1024K
malloc_context_size=30
SHADOW_SCALE: 3
SHADOW_GRANULARITY: 8
SHADOW_OFFSET: 0x7fff8000*/



static inline bool AddrIsInLowMem(uptr a) {
  return a <= kLowMemEnd;
}

static inline bool AddrIsInLowShadow(uptr a) {
  return a >= kLowShadowBeg && a <= kLowShadowEnd;
}

static inline bool AddrIsInHighMem(uptr a) {
  return kHighMemBeg && a >= kHighMemBeg && a <= kHighMemEnd;
}

static inline bool AddrIsInHighShadow(uptr a) {
  return kHighMemBeg && a >= kHighShadowBeg && a <= kHighShadowEnd;
}

static inline bool AddrIsInShadow(uptr a) {
    return AddrIsInLowShadow(a) || AddrIsInHighShadow(a);
}
static inline bool AddrIsInMem(uptr a) {
    return (AddrIsInLowMem(a) || AddrIsInHighMem(a));
}
static inline uptr MemToShadow(uptr p) {
    return MEM_TO_SHADOW(p);
}

static inline bool AddressIsPoisoned(uptr a, int size) {
    if (!AddrIsInMem(a) && !AddrIsInShadow(a))
        return false;
    //const uptr kAccessSize = 1;
    //TODO: use fast check method
    //shadow memory value: k
    //0: all 8 bytes addressable, memory not poisoned
    //1-7: 1-7 bytes addressable, if 8 byte access then whole is poisoned. if < 8 bytes then need to check the address of memory and size of access. if starting byte of mem address + access size > k
    //what about >8 or 16 byte accesses?? need to compute two addresses of shadow memory
    const uptr kAccessSize = size;
    u8 *shadow_address = (u8*)MEM_TO_SHADOW(a); // (mem >> SHADOW_SCALE) + SHADOW_OFFSET
    s8 shadow_value = *shadow_address;
    //k = shadow_address;
    //if(k!=0 && (addr&7) + acsz > k) error;
    if (shadow_value != 0) { //k!=0
        //return true;
        if(kAccessSize == 8 || kAccessSize == 16) return true;
        u8 last_accessed_byte = (a & (SHADOW_GRANULARITY - 1))+ kAccessSize - 1 ; // addr&7 + (acsz-1)
        return (last_accessed_byte >= shadow_value);    //addr&7 + acsz > k+1, address poisoned, else not.
       
    }
    return false;
}
void check_mem_rw_access(int id, int size){
    addr_t LEAddr = iList[id].LEAddr;
    uint64_t *LEAddr_ptr = (uint64_t*)LEAddr;
    bool error = false;
    if (!AddrIsInMem(LEAddr) && !AddrIsInShadow(LEAddr)) return;
    const uptr kAccessSize = size;
    u8 *shadow_address = (u8*)MEM_TO_SHADOW(LEAddr); // (mem >> SHADOW_SCALE) + SHADOW_OFFSET
    s8 shadow_value = *shadow_address;
    if (shadow_value != 0) { //k!=0
        if(kAccessSize == 8 || kAccessSize == 16) error = true;
        else{
            u8 last_accessed_byte = (LEAddr & (SHADOW_GRANULARITY - 1))+ kAccessSize - 1 ; // addr&7 + (acsz-1)
            error = (last_accessed_byte >= shadow_value);    //addr&7 + acsz > k+1, address poisoned, else not.
        }
    }
    if(error){
        error_counter++;
#ifdef DEBUG_VERBOSE
        printf("ERROR: Address Poisoned: %#08" PRIx8 "\n", LEAddr);
#endif
    }
}
/*------------------------------------------------------------------------------*/
/*-----------------Instrumentation Call Back Routines---------------------------*/
/*------------------------------------------------------------------------------*/
#if POISON_CLEANCALL
void store_canary(int id){
    addr_t addr = iList[id].LEAddr;
    canarySlots.push(addr);
#ifdef DEBUG_VERBOSE
   printf("STORE canary: %#08" PRIx8 "\n", addr);
#endif
}
void poison_canary(void){
   assert(canarySlots.size()>0);
   addr_t addr= canarySlots.top();
   u8 *shadow_address = (u8*)MEM_TO_SHADOW(addr);
   *shadow_address = 0xFF;
#ifdef DEBUG_VERBOSE
   printf("POISON shadow canary: %#08" PRIx8 "\n", addr);
#endif
}
void unpoison_canary(void){
   assert(canarySlots.size()>0);
   addr_t canary_addr= canarySlots.top();
  /* addr_t addr= iList[id].LEAddr;
   assert(canary_addr == addr);
   */
   canarySlots.pop();
   u8 *shadow_address = (u8*)MEM_TO_SHADOW(canary_addr);
#ifdef DEBUG_VERBOSE
   printf("UNPOISON shadow canary: %#08" PRIx8 "\n", canary_addr);
#endif
   *shadow_address = 0x0;
}
void unpoison_longjmp_clean(jmp_buf jmp_buffer, uintptr_t curr_sp){
   //for(int i=0; i<8; i++)
   //    cout<<"longjmp val: "<<jmp_buffer[0].__jmpbuf[i]<<endl;
   uint64_t stack_addr = jmp_buffer[0].__jmpbuf[6];
    // Inline assembly to move the value of 'stack_addr' into r8 register and manipulate it
    asm volatile (
        "mov %0, %%r8;"   // Move the value of 'stack_addr' into r8 register
        "ror $0x11, %%r8;" // XOR r8 with 0x11
    //    "xorq %%fs:0x30, %%r8;" // XOR r8 with the value at fs:0x30
        "mov %%r8, %0;"   // Move the value in r8 back to 'stack_addr'
        : "=r" (stack_addr)     // Output operand (modified 'stack_addr' variable)
        : "0" (stack_addr)      // Input operand (original 'stack_addr' value)
        : "%r8"           // Clobbered registers (list any registers modified by the assembly code)
    );

   /*PTR_DEMANGLE();
   while(stack_addr != curr_sp){
       u8 *shadow_address = (u8*)MEM_TO_SHADOW(stack_addr);
       *shadow_address = 0x0;
       stack_addr-= 8;
   }
#ifdef DEBUG_VERBOSE
   printf("UNPOISON shadow canary: %#08" PRIx8 "\n", canary_addr);
#endif*/
}
#else
static void store_canary(JANUS_CONTEXT, instr_t *instr, uint64_t bitmask_flag, uint64_t bitmask_reg){
    opnd_t mem_operand; 
    int i, num_srcs, num_dsts;
    int         mode = 0;
    instr_t *meta_instr;
    num_dsts = instr_num_dsts(instr); 
    for(i=0; i< num_dsts; i++){
       opnd_t operand2 = instr_get_dst(instr, i);
       if(opnd_is_memory_reference(operand2))
          mem_operand = operand2;
    }
    //TODO:first make sure that RSI or RDI are not destination
    /*--------- saving RSI/RDI registers -----------------*/
    if(inRegSet(bitmask_reg,DR_REG_RSI)) dr_save_reg(drcontext, bb, instr, DR_REG_RSI, SPILL_SLOT_2);
    if(inRegSet(bitmask_reg,DR_REG_RDI)) dr_save_reg(drcontext, bb, instr, DR_REG_RDI, SPILL_SLOT_3);

    /*----------- PRE INSERT to save memory address where canary is loaded--------------*/
    //leaq  mem, %rsi.   ,load memory address in rsi
    load_effective_address(drcontext, bb, instr, mem_operand, DR_REG_RSI, DR_REG_RDI);
    PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_rel_addr(&canarySlot, OPSZ_8),
                            opnd_create_reg(DR_REG_RSI)));
    /*--------- restoring RSI/RDI registers -----------------*/
    if(inRegSet(bitmask_reg,DR_REG_RSI)) dr_restore_reg(drcontext, bb, instr, DR_REG_RSI, SPILL_SLOT_2);
    if(inRegSet(bitmask_reg,DR_REG_RSI)) dr_restore_reg(drcontext, bb, instr, DR_REG_RDI, SPILL_SLOT_3);
}

static void unpoison_canary(JANUS_CONTEXT, instr_t *instr, uint64_t bitmask_flag, uint64_t bitmask_reg){
    instr_t *meta_instr;
    
    /*--------- saving RSI/RDI registers -----------------*/
    if(inRegSet(bitmask_reg,DR_REG_RSI)) dr_save_reg(drcontext, bb, instr, DR_REG_RSI, SPILL_SLOT_2);
    if(inRegSet(bitmask_reg,DR_REG_RDI)) dr_save_reg(drcontext, bb, instr, DR_REG_RDI, SPILL_SLOT_3);
    if(inRegSet(bitmask_reg,DR_REG_RCX)) dr_save_reg(drcontext, bb, instr, DR_REG_RCX, SPILL_SLOT_5);
    if(inRegSet(bitmask_reg,DR_REG_RDX)) dr_save_reg(drcontext, bb, instr, DR_REG_RDX, SPILL_SLOT_6);

    /*----------- Calculate memory address --------------*/
    if(bitmask_flag) 
        dr_save_arith_flags(drcontext, bb, instr, SPILL_SLOT_4);
    
    /* mov &addrstack[0] -> rdx
       load &top -> rcx
       mov [rdx, rcx, 8] -> rsi
       unposion on rsi addr*/
    if(inRegSet(bitmask_reg,DR_REG_RAX)) dr_save_reg(drcontext, bb, instr, DR_REG_RAX, SPILL_SLOT_7);   //save arith flags

    
    meta_instr = INSTR_CREATE_mov_imm(drcontext,opnd_create_reg(DR_REG_RDX),  OPND_CREATE_INTPTR(&addrstack[0]));
    instrlist_meta_preinsert(bb, instr, meta_instr);

    meta_instr = INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(DR_REG_EAX),  opnd_create_rel_addr(&top, OPSZ_4));
    instrlist_meta_preinsert(bb, instr, meta_instr);
    
    meta_instr = XINST_CREATE_sub_s(drcontext,opnd_create_reg(DR_REG_EAX),OPND_CREATE_INT32(1));
    instrlist_meta_preinsert(bb, instr, meta_instr);
   
   //movslq eax -> rcx, doubleword to quadword 
    meta_instr = INSTR_CREATE_movsxd(drcontext, opnd_create_reg(DR_REG_RCX),  opnd_create_reg(DR_REG_EAX));
    instrlist_meta_preinsert(bb, instr, meta_instr);

    meta_instr = INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(DR_REG_RSI),  opnd_create_base_disp(DR_REG_RDX, DR_REG_RCX, 8, 0, OPSZ_8));
    instrlist_meta_preinsert(bb, instr, meta_instr);

    
    //unpoison shadow value 
    meta_instr = INSTR_CREATE_shr(drcontext,opnd_create_reg(DR_REG_RSI),OPND_CREATE_INT8(3));
    instrlist_meta_preinsert(bb, instr, meta_instr);

    meta_instr = INSTR_CREATE_mov_imm(drcontext, opnd_create_reg(DR_REG_RDI), OPND_CREATE_INT64(0x0));
    instrlist_meta_preinsert(bb, instr, meta_instr);
    
    meta_instr = INSTR_CREATE_mov_st(drcontext, OPND_CREATE_MEM8(DR_REG_RSI,SHADOW_OFFSET), opnd_create_reg(DR_REG_DIL));
    instrlist_meta_preinsert(bb, instr, meta_instr);
          
    //top--
    
    meta_instr = INSTR_CREATE_mov_st(drcontext, opnd_create_rel_addr(&top, OPSZ_4), opnd_create_reg(DR_REG_EAX));
    instrlist_meta_preinsert(bb, instr, meta_instr);


    
    if(inRegSet(bitmask_reg, DR_REG_RAX)) dr_restore_reg(drcontext, bb, instr, DR_REG_RAX, SPILL_SLOT_7);//restore arith flags
    /*--------- restoring RSI/RDI registers -----------------*/
    if(bitmask_flag)
        dr_restore_arith_flags(drcontext, bb, instr, SPILL_SLOT_4);                     //restore them from rax current, and fill rax with original
    if(inRegSet(bitmask_reg, DR_REG_RSI))dr_restore_reg(drcontext, bb, instr, DR_REG_RSI, SPILL_SLOT_2);
    if(inRegSet(bitmask_reg, DR_REG_RDI))dr_restore_reg(drcontext, bb, instr, DR_REG_RDI, SPILL_SLOT_3);
    if(inRegSet(bitmask_reg, DR_REG_RCX)) dr_restore_reg(drcontext, bb, instr, DR_REG_RCX, SPILL_SLOT_5);
    if(inRegSet(bitmask_reg, DR_REG_RDX)) dr_restore_reg(drcontext, bb, instr, DR_REG_RDX, SPILL_SLOT_6);
}
static void poison_canary(JANUS_CONTEXT, instr_t *instr, uint64_t bitmask_flag, uint64_t bitmask_reg){
    instr_t *meta_instr;
    /*--------- saving RSI/RDI registers -----------------*/
    if(inRegSet(bitmask_reg,DR_REG_RSI)) dr_save_reg(drcontext, bb, instr, DR_REG_RSI, SPILL_SLOT_2);
    if(inRegSet(bitmask_reg,DR_REG_RDI)) dr_save_reg(drcontext, bb, instr, DR_REG_RDI, SPILL_SLOT_3);
    if(inRegSet(bitmask_reg,DR_REG_RCX)) dr_save_reg(drcontext, bb, instr, DR_REG_RCX, SPILL_SLOT_5);
    if(inRegSet(bitmask_reg,DR_REG_RDX)) dr_save_reg(drcontext, bb, instr, DR_REG_RDX, SPILL_SLOT_6);
    if(bitmask_flag){
        dr_save_arith_flags(drcontext, bb, instr, SPILL_SLOT_4);
    }
    if(inRegSet(bitmask_reg,DR_REG_RAX)) dr_save_reg(drcontext, bb, instr, DR_REG_RAX, SPILL_SLOT_7);
    meta_instr = INSTR_CREATE_mov_ld(drcontext,opnd_create_reg(DR_REG_RSI),  opnd_create_rel_addr(&canarySlot, OPSZ_8));
    instrlist_meta_preinsert(bb, instr, meta_instr);
    
   
    //push on the stack
    meta_instr = INSTR_CREATE_mov_imm(drcontext,opnd_create_reg(DR_REG_RDX),  OPND_CREATE_INTPTR(&addrstack[0]));
    instrlist_meta_preinsert(bb, instr, meta_instr);

    meta_instr = INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(DR_REG_EAX),  opnd_create_rel_addr(&top, OPSZ_4));
    instrlist_meta_preinsert(bb, instr, meta_instr);
    
   //movslq eax -> rcx, doubleword to quadword 
    meta_instr = INSTR_CREATE_movsxd(drcontext, opnd_create_reg(DR_REG_RCX),  opnd_create_reg(DR_REG_EAX));
    instrlist_meta_preinsert(bb, instr, meta_instr);

    meta_instr = INSTR_CREATE_mov_st(drcontext, opnd_create_base_disp(DR_REG_RDX, DR_REG_RCX, 8, 0 , OPSZ_8), opnd_create_reg(DR_REG_RSI));
    instrlist_meta_preinsert(bb, instr, meta_instr);

    //poison shadow value 
    meta_instr = INSTR_CREATE_shr(drcontext,opnd_create_reg(DR_REG_RSI),OPND_CREATE_INT8(3));
    instrlist_meta_preinsert(bb, instr, meta_instr);
    
    meta_instr = INSTR_CREATE_mov_imm(drcontext, opnd_create_reg(DR_REG_RDI), OPND_CREATE_INT64(0xFF));
    instrlist_meta_preinsert(bb, instr, meta_instr);
   
    meta_instr = INSTR_CREATE_mov_st(drcontext, OPND_CREATE_MEM8(DR_REG_RSI,SHADOW_OFFSET),opnd_create_reg(DR_REG_DIL));
    instrlist_meta_preinsert(bb, instr, meta_instr);
   
    //inc top 
    meta_instr = XINST_CREATE_add_s(drcontext,opnd_create_reg(DR_REG_EAX),OPND_CREATE_INT32(1));
    instrlist_meta_preinsert(bb, instr, meta_instr);
    
    meta_instr = INSTR_CREATE_mov_st(drcontext, opnd_create_rel_addr(&top, OPSZ_4), opnd_create_reg(DR_REG_EAX));
    instrlist_meta_preinsert(bb, instr, meta_instr);

   

    if(inRegSet(bitmask_reg,DR_REG_RAX)) dr_restore_reg(drcontext, bb, instr, DR_REG_RAX, SPILL_SLOT_7);
    
    if(bitmask_flag){
        dr_restore_arith_flags(drcontext, bb, instr, SPILL_SLOT_4);
    }
    if(inRegSet(bitmask_reg,DR_REG_RSI)) dr_restore_reg(drcontext, bb, instr, DR_REG_RSI, SPILL_SLOT_2);
    if(inRegSet(bitmask_reg,DR_REG_RDI)) dr_restore_reg(drcontext, bb, instr, DR_REG_RDI, SPILL_SLOT_3);
    if(inRegSet(bitmask_reg,DR_REG_RCX)) dr_restore_reg(drcontext, bb, instr, DR_REG_RCX, SPILL_SLOT_5);
    if(inRegSet(bitmask_reg,DR_REG_RDX)) dr_restore_reg(drcontext, bb, instr, DR_REG_RDX, SPILL_SLOT_6);
}
#endif
static void unpoison_canary_memaddr(JANUS_CONTEXT, instr_t *instr, uint64_t bitmask_flag, uint64_t bitmask_reg){
    opnd_t mem_operand; 
    int i, num_srcs, num_dsts;
    int         mode = 0;
    instr_t *meta_instr;
    num_srcs = instr_num_srcs(instr); 
    for(i=0; i< num_srcs; i++){
       opnd_t operand2 = instr_get_src(instr, i);
       if(opnd_is_memory_reference(operand2))
          mem_operand = operand2;
    }
    
    if(inRegSet(bitmask_reg,DR_REG_RSI)) dr_save_reg(drcontext, bb, instr, DR_REG_RSI, SPILL_SLOT_2);
    if(inRegSet(bitmask_reg,DR_REG_RDI)) dr_save_reg(drcontext, bb, instr, DR_REG_RDI, SPILL_SLOT_3);
    
    load_effective_address(drcontext, bb, instr, mem_operand, DR_REG_RSI, DR_REG_RDI);
    
    if(bitmask_flag) 
        dr_save_arith_flags(drcontext, bb, instr, SPILL_SLOT_4);
    
    meta_instr = INSTR_CREATE_shr(drcontext,opnd_create_reg(DR_REG_RSI),OPND_CREATE_INT8(3));
    instrlist_meta_preinsert(bb, instr, meta_instr);

    meta_instr = INSTR_CREATE_mov_imm(drcontext, opnd_create_reg(DR_REG_RDI), OPND_CREATE_INT64(0x0));
    instrlist_meta_preinsert(bb, instr, meta_instr);
    
    meta_instr = INSTR_CREATE_mov_st(drcontext, OPND_CREATE_MEM8(DR_REG_RSI,SHADOW_OFFSET), opnd_create_reg(DR_REG_DIL));
    instrlist_meta_preinsert(bb, instr, meta_instr);
    
    if(bitmask_flag)
        dr_restore_arith_flags(drcontext, bb, instr, SPILL_SLOT_4);                     //restore them from rax current, and fill rax with original
    
    if(inRegSet(bitmask_reg,DR_REG_RSI)) dr_restore_reg(drcontext, bb, instr, DR_REG_RSI, SPILL_SLOT_2);
    if(inRegSet(bitmask_reg,DR_REG_RDI)) dr_restore_reg(drcontext, bb, instr, DR_REG_RDI, SPILL_SLOT_3);
          
}
/*  mov 0x30(%rdi), %r8
    ror $0x11, %r8
    xor %fs:0x30, %r8

    .ASAN_LONGJMP_labelxx:
    movq %r8, %r9
    shrq $3, %r9
    movb $0, {off}(r9)
    subq $8, %r8
    cmp %r8, %rsp
    jne .ASAN_LONGJMP_labelxx
*/
static void unpoison_longjmp(JANUS_CONTEXT, instr_t *instr, uint64_t bitmask_flag, uint64_t bitmask_reg){
    instr_t *meta_instr;
    
    instr_t *unpoison_stack_label;
    /*--------- saving RSI/RDI registers -----------------*/
    if(inRegSet(bitmask_reg,DR_REG_R8)) dr_save_reg(drcontext, bb, instr, DR_REG_R8, SPILL_SLOT_2);
    if(inRegSet(bitmask_reg,DR_REG_R9)) dr_save_reg(drcontext, bb, instr, DR_REG_R9, SPILL_SLOT_3);
    if(inRegSet(bitmask_reg,DR_REG_RDI)) dr_save_reg(drcontext, bb, instr, DR_REG_RDI, SPILL_SLOT_5);

    if(bitmask_flag) 
        dr_save_arith_flags(drcontext, bb, instr, SPILL_SLOT_4);

    meta_instr = INSTR_CREATE_mov_ld(drcontext,opnd_create_reg(DR_REG_R8), OPND_CREATE_MEM64(DR_REG_RDI, 0x30));
    instrlist_meta_preinsert(bb, instr,meta_instr);


    meta_instr = INSTR_CREATE_ror(drcontext,opnd_create_reg(DR_REG_R8), OPND_CREATE_INT8(0x11));
    instrlist_meta_preinsert(bb, instr,meta_instr);
    
    dr_insert_get_seg_base(drcontext, bb, instr, (reg_id_t)DR_SEG_FS, (reg_id_t)DR_REG_RDI);

    
    meta_instr = INSTR_CREATE_xor(drcontext, opnd_create_reg(DR_REG_R8), OPND_CREATE_MEM64(DR_REG_RDI, 0x30));
    instrlist_meta_preinsert(bb, instr,meta_instr);
    
    
    /* ----- UNPOISON_STACK ------- */
    unpoison_stack_label = INSTR_CREATE_label(drcontext);
    instrlist_meta_preinsert(bb, instr, unpoison_stack_label);


    //unpoison shadow value 
    meta_instr = XINST_CREATE_move(drcontext,opnd_create_reg(DR_REG_R9),opnd_create_reg(DR_REG_R8));
    instrlist_meta_preinsert(bb, instr,meta_instr);

    meta_instr = INSTR_CREATE_shr(drcontext,opnd_create_reg(DR_REG_R9),OPND_CREATE_INT8(3));
    instrlist_meta_preinsert(bb, instr, meta_instr);
    
    meta_instr = INSTR_CREATE_mov_imm(drcontext, opnd_create_reg(DR_REG_RDI), OPND_CREATE_INT64(0x0));
    instrlist_meta_preinsert(bb, instr, meta_instr);
    
    meta_instr = INSTR_CREATE_mov_st(drcontext, OPND_CREATE_MEM8(DR_REG_R9,SHADOW_OFFSET), opnd_create_reg(DR_REG_DIL));
    instrlist_meta_preinsert(bb, instr, meta_instr);
    
    //sub r8,8 and cmp rsp, r8
    meta_instr = XINST_CREATE_sub(drcontext,opnd_create_reg(DR_REG_R8),OPND_CREATE_INT64(8));
    instrlist_meta_preinsert(bb, instr, meta_instr);
    

    meta_instr = INSTR_CREATE_cmp(drcontext,opnd_create_reg(DR_REG_RSP),opnd_create_reg(DR_REG_R8));
    instrlist_meta_preinsert(bb, instr,meta_instr);
    

    meta_instr = INSTR_CREATE_jcc(drcontext,OP_jne,opnd_create_instr(unpoison_stack_label));
    instrlist_meta_preinsert(bb, instr, meta_instr);
    /*--------- restoring RSI/RDI registers -----------------*/
    if(bitmask_flag)
        dr_restore_arith_flags(drcontext, bb, instr, SPILL_SLOT_4);
    if(inRegSet(bitmask_reg,DR_REG_R8)) dr_restore_reg(drcontext, bb, instr, DR_REG_R8, SPILL_SLOT_2);
    if(inRegSet(bitmask_reg,DR_REG_R9)) dr_restore_reg(drcontext, bb, instr, DR_REG_R9, SPILL_SLOT_3);
    if(inRegSet(bitmask_reg,DR_REG_RDI)) dr_restore_reg(drcontext, bb, instr, DR_REG_RDI, SPILL_SLOT_5);
}
static void poison_canary_single(JANUS_CONTEXT, instr_t *instr, uint64_t bitmask_flag, uint64_t bitmask_reg){
    instr_t *meta_instr;
    /*--------- saving RSI/RDI registers -----------------*/
    if(inRegSet(bitmask_reg,DR_REG_RSI)) dr_save_reg(drcontext, bb, instr, DR_REG_RSI, SPILL_SLOT_2);
    if(inRegSet(bitmask_reg,DR_REG_RDI)) dr_save_reg(drcontext, bb, instr, DR_REG_RDI, SPILL_SLOT_3);
    if(bitmask_flag){
        dr_save_arith_flags(drcontext, bb, instr, SPILL_SLOT_4);
    }
    
    meta_instr = INSTR_CREATE_mov_ld(drcontext,opnd_create_reg(DR_REG_RSI),  opnd_create_rel_addr(&canarySlot, OPSZ_8));
    instrlist_meta_preinsert(bb, instr, meta_instr);

    //poison shadow value 
    meta_instr = INSTR_CREATE_shr(drcontext,opnd_create_reg(DR_REG_RSI),OPND_CREATE_INT8(3));
    instrlist_meta_preinsert(bb, instr, meta_instr);
    
    meta_instr = INSTR_CREATE_mov_imm(drcontext, opnd_create_reg(DR_REG_RDI), OPND_CREATE_INT64(0xFF));
    instrlist_meta_preinsert(bb, instr, meta_instr);
   
    meta_instr = INSTR_CREATE_mov_st(drcontext, OPND_CREATE_MEM8(DR_REG_RSI,SHADOW_OFFSET),opnd_create_reg(DR_REG_DIL));
    instrlist_meta_preinsert(bb, instr, meta_instr);
   
   
    if(bitmask_flag){
        dr_restore_arith_flags(drcontext, bb, instr, SPILL_SLOT_4);
    }
    if(inRegSet(bitmask_reg,DR_REG_RSI)) dr_restore_reg(drcontext, bb, instr, DR_REG_RSI, SPILL_SLOT_2);
    if(inRegSet(bitmask_reg,DR_REG_RDI)) dr_restore_reg(drcontext, bb, instr, DR_REG_RDI, SPILL_SLOT_3);
}
static bool load_mem_addr(JANUS_CONTEXT, instr_t *instr, uint64_t bitmask_flag, uint64_t bitmask_reg){

    dr_mcontext_t mc;
    app_pc addr;
    opnd_t mem_operand; 
    int i, num_srcs, num_dsts;
    int         mode = 0;
    opnd_size_t size;
    if(instr_reads_memory(instr)) mode += 1;
    if(instr_writes_memory(instr)) mode += 2;
    if (mode == 0) return false;
    if(instr_is_predicated(instr)) return false;
    if(mode == 1 || mode == 3){ 
        num_srcs = instr_num_srcs(instr); 
        for(i=0; i< num_srcs; i++){
           opnd_t operand1 = instr_get_src(instr, i);
           if(opnd_is_memory_reference(operand1))
              mem_operand = operand1;
        }
    }
    if(mode == 2 || mode == 3){ 
        num_dsts = instr_num_dsts(instr); 
        for(i=0; i< num_dsts; i++){
           opnd_t operand2 = instr_get_dst(instr, i);
           if(opnd_is_memory_reference(operand2))
              mem_operand = operand2;
        }
    }
    if(inRegSet(bitmask_reg,DR_REG_RSI)) dr_save_reg(drcontext, bb, instr, DR_REG_RSI, SPILL_SLOT_2);
    if(inRegSet(bitmask_reg,DR_REG_RDI)) dr_save_reg(drcontext, bb, instr, DR_REG_RDI, SPILL_SLOT_3);
    load_effective_address(drcontext, bb, instr, mem_operand, DR_REG_RSI, DR_REG_RDI);
        int written_offset = 0;
    PRE_INSERT(bb, instr,
               INSTR_CREATE_mov_imm(drcontext,
                                    opnd_create_reg(DR_REG_RDI),
                                    OPND_CREATE_INTPTR((uint64_t)&iList[emulate_pc])));
    PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM64(DR_REG_RDI, written_offset),
                            opnd_create_reg(DR_REG_RSI)));
    if(inRegSet(bitmask_reg,DR_REG_RDI)) dr_restore_reg(drcontext, bb, instr, DR_REG_RDI, SPILL_SLOT_3);
    if(inRegSet(bitmask_reg,DR_REG_RSI)) dr_restore_reg(drcontext, bb, instr, DR_REG_RSI, SPILL_SLOT_2);

   return true;

}
static void mem_rw_event_flags(JANUS_CONTEXT, instr_t *instr, uint64_t bitmask_flag, uint64_t bitmask_reg){
    app_pc addr;
    opnd_t mem_operand; 
    int i, num_srcs, num_dsts;
    int         mode = 0;
    instr_t *meta_instr;
    instr_t *restore_label;
    opnd_size_t size;
    if(instr_reads_memory(instr)) mode += 1;
    if(instr_writes_memory(instr)) mode += 2;
    if (mode == 0) return;
    if(mode == 1 || mode == 3){ //memory read
        num_srcs = instr_num_srcs(instr); 
        for(i=0; i< num_srcs; i++){
           opnd_t operand1 = instr_get_src(instr, i);
           if(opnd_is_memory_reference(operand1))
              mem_operand = operand1;
        }
    }
    if(mode == 2 || mode == 3){ //memory write
        num_dsts = instr_num_dsts(instr); 
        for(i=0; i< num_dsts; i++){
           opnd_t operand2 = instr_get_dst(instr, i);
           if(opnd_is_memory_reference(operand2))
              mem_operand = operand2;
        }
    }
    size = opnd_get_size(mem_operand);

#ifdef VERBOSE_DEBUG
    printf("instr: %#08" PRIx64 "size: %d\n", instr_id, op_size[size]);
#endif 

    if(size != OPSZ_1 && size !=OPSZ_2 && size!=OPSZ_4 && size!=OPSZ_8 && size!= OPSZ_16) return;

    /*--------- saving RSI/RDI registers -----------------*/
    if(inRegSet(bitmask_reg,DR_REG_RSI)) dr_save_reg(drcontext, bb, instr, DR_REG_RSI, SPILL_SLOT_2);
    if(inRegSet(bitmask_reg,DR_REG_RDI)) dr_save_reg(drcontext, bb, instr, DR_REG_RDI, SPILL_SLOT_3);

    /*----------- Calculate memory address --------------*/
    //leaq  mem, %rsi.   ,load memory address in rsi
    load_effective_address(drcontext, bb, instr, mem_operand, DR_REG_RSI, DR_REG_RDI);
    

    /*--------- saving arithmetic flags -----------------*/
    if(bitmask_flag){
#if EFLAGS_DR_SAVE
       dr_save_arith_flags(drcontext, bb, instr, SPILL_SLOT_4);
#elif EFLAGS_PUSH_POP
        meta_instr = INSTR_CREATE_pushf(drcontext); //pop flags into eax 
        instrlist_meta_preinsert(bb, instr,meta_instr);
#elif EFLAGS_SETO
        //if(inRegSet(bitmask,DR_REG_RAX)) dr_save_reg(drcontext, bb, instr, DR_REG_RAX, SPILL_SLOT_5);  //save rax original value      
        dr_save_reg(drcontext, bb, instr, DR_REG_RAX, SPILL_SLOT_5);  //save value of eflags stored      

        meta_instr = INSTR_CREATE_setcc(drcontext, OP_seto, opnd_create_reg(DR_REG_AL)); //seto al 
        instrlist_meta_preinsert(bb, instr,meta_instr);

        meta_instr = INSTR_CREATE_lahf(drcontext); //lahf 
        instrlist_meta_preinsert(bb, instr,meta_instr);

        //meta_instr = INSTR_CREATE_push(drcontext, opnd_create_reg(DR_REG_RAX)); //push eax 
        //instrlist_meta_preinsert(bb, instr,meta_instr);
        
        dr_save_reg(drcontext, bb, instr, DR_REG_RAX, SPILL_SLOT_6);  //save value of eflags stored      
#endif
    }
    /*----------- instrument memory checks ----------------*/
    restore_label = INSTR_CREATE_label(drcontext);
     /*------ sanitization code -------------------*/
    //movq %rsi, %rdi   , move rsi to rdi (now rdi has mem address)
    meta_instr = XINST_CREATE_move(drcontext,opnd_create_reg(DR_REG_RDI),opnd_create_reg(DR_REG_RSI)); 
    instrlist_meta_preinsert(bb, instr,meta_instr);
    //TODO: just for printing 

    //shrq $3, %rdi		    //mem >> SHADOW_SCALE(3) , multiply by 8
    meta_instr = INSTR_CREATE_shr(drcontext,opnd_create_reg(DR_REG_RDI),OPND_CREATE_INT8(3));
    instrlist_meta_preinsert(bb, instr, meta_instr);

    if(size == OPSZ_8 || size == OPSZ_16){
        //cmp *(rdi+offset), 0   ,is  *[(mem>>SHADOW_SCALE) +SHADOW_OFFSET] = 0?
        meta_instr = INSTR_CREATE_cmp(drcontext,OPND_CREATE_MEM8(DR_REG_RDI,SHADOW_OFFSET), OPND_CREATE_INT8(0));
        instrlist_meta_preinsert(bb, instr, meta_instr);

        //je to label //if shadow_value is zero, proceed normal
        meta_instr = INSTR_CREATE_jcc(drcontext,OP_je,opnd_create_instr(restore_label)); 
        instrlist_meta_preinsert(bb, instr,meta_instr);

    }
    else{
         //movb *(rdi+offset), dil //
         meta_instr = XINST_CREATE_load_1byte(drcontext,opnd_create_reg(DR_REG_DIL),OPND_CREATE_MEM8(DR_REG_RDI,SHADOW_OFFSET));
        instrlist_meta_preinsert(bb, instr, meta_instr);

        //testb dil, dil , check if value in dil = 0 (set ZF if AND operation results in zero)
         meta_instr = INSTR_CREATE_test(drcontext,opnd_create_reg(DR_REG_DIL),opnd_create_reg(DR_REG_DIL));
        instrlist_meta_preinsert(bb, instr, meta_instr);


        //je label, if yes, proceed normal (tests the ZF, if set then jump]
        meta_instr = INSTR_CREATE_jcc(drcontext,OP_je,opnd_create_instr(restore_label)); 
        instrlist_meta_preinsert(bb, instr, meta_instr);
      
    }
    if(size == OPSZ_1 || size == OPSZ_2 || size == OPSZ_4) {//if size != 8 or 16
        //andl 7, esi,  if shadow value not zero, mem & (SHADOW_GRAN-1) =>esi
        meta_instr = INSTR_CREATE_and(drcontext,opnd_create_reg(DR_REG_ESI),OPND_CREATE_INT32(7));
        instrlist_meta_preinsert(bb, instr, meta_instr);
        
        if(size == OPSZ_4){
           //addl 3, esi,
           meta_instr = XINST_CREATE_add_s(drcontext,opnd_create_reg(DR_REG_ESI),OPND_CREATE_INT32(3));
           instrlist_meta_preinsert(bb, instr, meta_instr);
        }
        if(size == OPSZ_2){
            //inc esi   //( mem & (SHADOW_GRAN-1)) + 1 (which is acsz-1)
           meta_instr = XINST_CREATE_add_s(drcontext,opnd_create_reg(DR_REG_ESI), OPND_CREATE_INT32(1));
           instrlist_meta_preinsert(bb, instr, meta_instr);
        }
        // movsbl %dil, %edi	    ,mov dil (shadow_value to edi)
        meta_instr = INSTR_CREATE_movsx(drcontext,opnd_create_reg(DR_REG_EDI),opnd_create_reg(DR_REG_DIL));
        instrlist_meta_preinsert(bb, instr, meta_instr);

        //cmpl %edi, %esi	     ,if mem &(SHADOW_GRAN - 01) + acsz-1 < shadow_value
        meta_instr = INSTR_CREATE_cmp(drcontext,opnd_create_reg(DR_REG_ESI),opnd_create_reg(DR_REG_EDI));
        instrlist_meta_preinsert(bb, instr,meta_instr);

        //jl label, yes, proceed normal 
        meta_instr = INSTR_CREATE_jcc(drcontext,OP_jl,opnd_create_instr(restore_label));
        instrlist_meta_preinsert(bb, instr, meta_instr);
    }
   //  increment error counter
    meta_instr = INSTR_CREATE_inc(drcontext,OPND_CREATE_ABSMEM((byte *)&error_counter, OPSZ_8));
    instrlist_meta_preinsert(bb, instr, meta_instr);
    
    
    //insert label for flag/reg restore instructions
    instrlist_meta_preinsert(bb, instr, restore_label);
    /*--------- restoring arithmetic flags -----------------*/
    if(bitmask_flag){
#if EFLAGS_DR_SAVE
       dr_restore_arith_flags(drcontext, bb, instr, SPILL_SLOT_4);
#elif EFLAGS_PUSH_POP
        meta_instr = INSTR_CREATE_popf(drcontext); //pop flags into eax 
        instrlist_meta_preinsert(bb, instr,meta_instr);
#elif EFALGS_SETO
        dr_restore_reg(drcontext, bb, instr, DR_REG_RAX, SPILL_SLOT_6);  //restore flags from spill slot into rax    
        meta_instr = XINST_CREATE_add_s(drcontext,opnd_create_reg(DR_REG_AL),OPND_CREATE_INT8(127));
        instrlist_meta_preinsert(bb, instr,meta_instr);

        meta_instr = INSTR_CREATE_sahf(drcontext); //sahf 
        instrlist_meta_preinsert(bb, instr,meta_instr);
        
        //if(inRegSet(bitmask,DR_REG_RAX)) dr_restore_reg(drcontext, bb, instr, DR_REG_RAX, SPILL_SLOT_5);  //restore original value of rax      
        dr_restore_reg(drcontext, bb, instr, DR_REG_RAX, SPILL_SLOT_5);  //restore flags from spill slot into rax    
#endif
    }
    /*--------- restoring RSI/RDI registers -----------------*/
    if(inRegSet(bitmask_reg,DR_REG_RDI)) dr_restore_reg(drcontext, bb, instr, DR_REG_RDI, SPILL_SLOT_3);
    if(inRegSet(bitmask_reg,DR_REG_RSI)) dr_restore_reg(drcontext, bb, instr, DR_REG_RSI, SPILL_SLOT_2);
    
}
