/* JANUS Client for secure execution */

/* Header file to implement a JANUS client */
#include "janus_api.h"
#include "dr_api.h"
#include <inttypes.h>
#include <iostream>
#include "JanusGuard.h"
using namespace std;
#define REG_S1 DR_REG_RSI
#define REG_S2 DR_REG_RDI


uint64_t ptr_size=0;
int emulate_pc;

guard_t command_buffer[MAX_ALLOWED_COMMAND];
guard_t *commands;

static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating);

void guard_execute(int command_size);
static void generate_security_events(JANUS_CONTEXT);
static void generate_events_by_rule(JANUS_CONTEXT, instr_t *instr);

static void record_size(uint64_t size);
static void record_base_pointer(uint64_t base);
static void reg_table_copy(reg_id_t dest, reg_id_t src);
static void reg_mem_table_copy(reg_id_t src);
static void mem_reg_table_copy(reg_id_t dest);
static void bound_check_event(JANUS_CONTEXT, instr_t *instr);
static void reg_table_event(JANUS_CONTEXT,instr_t *instr);

/* CM_handler*/
void copy_reg_table(reg_id_t src, reg_id_t dest, uint64_t id);
static void mem_reg_event(JANUS_CONTEXT, instr_t *instr, bool mem_to_reg);
void check_overflow_reg(reg_id_t dest, addr_t address, uint64_t id);
void check_overflow_mem(addr_t key, addr_t address, uint64_t id);
void write_to_mem(reg_id_t src, addr_t address, uint64_t id);


DR_EXPORT void 
dr_init(client_id_t id)
{
#ifdef JANUS_VERBOSE
    dr_fprintf(STDOUT,"\n---------------------------------------------------------------\n");
    dr_fprintf(STDOUT,"               Janus Secure Execution\n");
    dr_fprintf(STDOUT,"---------------------------------------------------------------\n\n");
#endif
    /* Register event callbacks. */
    dr_register_bb_event(event_basic_block); 

    /* Initialise janus components */
    janus_init(id);

    if(rsched_info.mode != JSECURE) {
        dr_fprintf(STDOUT,"Static rules not intended for %s!\n",print_janus_mode(rsched_info.mode));
        return;
    }
    commands = command_buffer;
    mlc_base_counter=0;
    mlc_size_counter=0;
#ifdef JANUS_VERBOSE
    dr_fprintf(STDOUT,"Dynamorio client initialised\n");
#endif
}


/* Main execution loop: this will be executed at every initial encounter of new basic block */
static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating)
{
    //get current basic block starting address
    PCAddress bbAddr = (PCAddress)dr_fragment_app_pc(tag);
    //lookup in the hashtable to check if there is any rule attached to the block
    RRule *rule = get_static_rule(bbAddr);
    if (rule)
        generate_security_events(janus_context);
    return DR_EMIT_DEFAULT;
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

    /* clear emulate pc */
    emulate_pc = 0;

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
#ifdef PLANNER_VERBOSE
                thread_print_rule(0, rule);
#endif
                generate_events_by_rule(janus_context, instr);
            } else
                break;
            rule = rule->next;
        }
    }

    /* Generate a clean call at the end of block to execute from the command buffer */
    dr_insert_clean_call(drcontext, bb, last, (void *)guard_execute, false, 1, OPND_CREATE_INT32(emulate_pc));
}

static void
generate_events_by_rule(JANUS_CONTEXT, instr_t *instr){
    
    RuleOp rule_opcode = rule->opcode;
    
    instr_t *trigger;
    switch (rule_opcode) {
	case APP_SPLIT_BLOCK:
	    split_block_handler(janus_context);
	return;
	
	case BND_CHECK: /* look up for particular base address*/
            bound_check_event(janus_context, instr);
            //trigger= get_trigger_instruction(bb,rule);
	    //dr_insert_clean_call(drcontext, bb, trigger, (void *)bound_check_event, false, 1, instr);
            emulate_pc++;
	break;
	case BND_RECORD_BASE:
            trigger= get_trigger_instruction(bb,rule);
	    dr_insert_clean_call(drcontext, bb, trigger, (void *)record_base_pointer, false, 1, opnd_create_reg((reg_id_t)DR_REG_RAX));
	break;
	case BND_RECORD_SIZE:
            trigger= get_trigger_instruction(bb,rule);
	   dr_insert_clean_call(drcontext, bb, trigger, (void *)record_size, false, 1, opnd_create_reg((reg_id_t)DR_REG_RDI));
	break;
        case TABLE_REG_REG_COPY:
            reg_table_event(janus_context, instr);
            emulate_pc++;
        break;
        case TABLE_MEM_REG_LOAD:
             mem_reg_event(janus_context, instr, true);
             emulate_pc++;
        break;
        case TABLE_REG_MEM_STORE:
            mem_reg_event(janus_context, instr, false);
            emulate_pc++;
        break;
        case TABLE_VALUE_MEM:
            PRE_INSERT(bb, instr,
                INSTR_CREATE_mov_st(drcontext,
                                    opnd_create_rel_addr(&(commands[emulate_pc].opcode), OPSZ_2),
                                    OPND_CREATE_INT16(CM_VALUE_MEM)));
            emulate_pc++;
        break;
        case TABLE_VALUE_REG:
            PRE_INSERT(bb, instr,
                INSTR_CREATE_mov_st(drcontext,
                                    opnd_create_rel_addr(&(commands[emulate_pc].opcode), OPSZ_2),
                                    OPND_CREATE_INT16(CM_VALUE_REG)));
            emulate_pc++;
        break;
	default:
	    //fprintf(stderr,"In basic block 0x%lx static rule not recognised %d\n",bbAddr,rule_opcode);
	break;
    }
}
void record_base_pointer(uint64_t base){
   
   mlc_base_counter++;
   //assert(mlc_base_counter== mlc_size_counter && "Mismatched malloc parameters");
   //printf("base pointer: %#08" PRIx64 "\n", base);
   reg_table[(reg_id_t)DR_REG_RAX].base =  base;
   reg_table[(reg_id_t)DR_REG_RAX].bound = (base!=0x0)? base+ptr_size: 0x0;

}
void record_size(uint64_t size){

   mlc_size_counter++;
   ptr_size= size;

}
/*void check_bound(addr_t ptr_addr, uint64_t size){
  if(lookup_table.find(ptr_addr)!= lookup_table.end()){
     uintptr_t ptr_value =*(uintptr_t)ptr_addr; //get the current value of the pointer, pointers are 64bit values so shall we use uint64_t?
     if( ptr_value < (uint64_t)lookup_table[ptr_address].base) || ( ptr_value+size > (uint64_t)lookup_table[ptr_addr].bound)){
     	cerr<<"ERROR:out of bounds access"<<endl;
	abort();
     }
  }
}*/
static void mem_reg_event(JANUS_CONTEXT, instr_t *instr, bool mem_to_reg){
    addr_t addr;
    opnd_t mem_operand, reg_operand; 
    int i, num_srcs, num_dsts, size;
    //reg_id_t address_reg;
    guard_opcode_t opcode;
   
    num_srcs = instr_num_srcs(instr); 
    num_dsts = instr_num_dsts(instr); 
    
    for(i=0; i< num_srcs; i++){
       opnd_t operand1 = instr_get_src(instr, i);
       if( mem_to_reg && opnd_is_memory_reference(operand1)){
          mem_operand = operand1;
          //size= opnd_get_size(src);
       }else
          reg_operand =operand1;
    }
    for(i=0; i< num_dsts; i++){
       opnd_t operand2 = instr_get_dst(instr, i);
       if(mem_to_reg && opnd_is_reg(operand2)){
          reg_operand = operand2;
         // size= opnd_get_size(dest);
       }else
          mem_operand = operand2;
    }
    dr_save_reg(drcontext, bb, instr, REG_S1, SPILL_SLOT_2);
    dr_save_reg(drcontext, bb, instr, REG_S2, SPILL_SLOT_3);
    
    load_effective_address(drcontext, bb, instr, mem_operand, REG_S1, REG_S2);
    
    reg_id_t reg_id = opnd_get_reg(reg_operand);
    opcode = mem_to_reg ? CM_MEM_REG_LOAD : CM_REG_MEM_STORE;
   
    //cout<<"REG_DEST: "<<get_register_name(reg_id)<<endl; 
    
    PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_st(drcontext,
                opnd_create_rel_addr(&(commands[emulate_pc].opcode), OPSZ_2),
                OPND_CREATE_INT16(opcode)));

    PRE_INSERT(bb, instr,
            INSTR_CREATE_mov_st(drcontext,
              opnd_create_rel_addr(&commands[emulate_pc].id, OPSZ_8),
              OPND_CREATE_INT32(rule->reg0)));

    if(mem_to_reg){
        PRE_INSERT(bb, instr,
            INSTR_CREATE_mov_st(drcontext,
                                opnd_create_rel_addr(&(commands[emulate_pc].dest_reg), OPSZ_2),
                                OPND_CREATE_INT16(reg_id)));

        //get the memory address which is being loaded
        PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_st(drcontext,
                opnd_create_rel_addr(&(commands[emulate_pc].ptr_addr), OPSZ_8),
                opnd_create_reg(REG_S1)));
        
        
        //get the value loaded into reg from memory
        /*POST_INSERT(bb, instr,
        INSTR_CREATE_mov_st(drcontext,
                opnd_create_rel_addr(&(commands[emulate_pc].addr), OPSZ_8),
                opnd_create_reg(reg_id)));*/
        PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_ld(drcontext, 
                opnd_create_reg(REG_S2),
                    OPND_CREATE_MEM64(REG_S1, 0)));
        
        PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_st(drcontext,
                opnd_create_rel_addr(&(commands[emulate_pc].addr), OPSZ_8),
                opnd_create_reg(REG_S2)));

    }else{
        PRE_INSERT(bb, instr,
            INSTR_CREATE_mov_st(drcontext,
                                opnd_create_rel_addr(&(commands[emulate_pc].src_reg), OPSZ_2),
                                OPND_CREATE_INT16(reg_id)));

        /*we only need the memory address where the value is being written to*/ 
        PRE_INSERT(bb, instr,
            INSTR_CREATE_mov_st(drcontext,
                    opnd_create_rel_addr(&(commands[emulate_pc].addr), OPSZ_8),
                    opnd_create_reg(REG_S1)));
    }
    dr_restore_reg(drcontext, bb, instr, REG_S1, SPILL_SLOT_2);
    dr_restore_reg(drcontext, bb, instr, REG_S2, SPILL_SLOT_3);
}
static void bound_check_event(JANUS_CONTEXT, instr_t *instr){
   opnd_t dest; 
    int i, num_dsts, size;
    reg_id_t address_reg;
    num_dsts = instr_num_dsts(instr); 
    instr_t *trigger;
    for(i=0; i< num_dsts; i++){
       opnd_t operand2 = instr_get_dst(instr, i);
       if(opnd_is_memory_reference(operand2)){
          dest = operand2;
          size= opnd_get_size(dest);
       }
    }
    for(int i=0; i<opnd_num_regs_used(dest); i++){
         address_reg  =  opnd_get_reg_used(dest, i);
    }
    dr_save_reg(drcontext, bb, instr, REG_S1, SPILL_SLOT_2);
    dr_save_reg(drcontext, bb, instr, REG_S2, SPILL_SLOT_3);
    
    load_effective_address(drcontext, bb, instr,dest, REG_S1, REG_S2);
    PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_rel_addr(&(commands[emulate_pc].opcode), OPSZ_2),
                            OPND_CREATE_INT16(CM_BND_CHECK)));
    PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_rel_addr(&(commands[emulate_pc].dest_reg), OPSZ_2),
                            OPND_CREATE_INT16(address_reg)));
    // Store address
    PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_rel_addr(&(commands[emulate_pc].addr), OPSZ_8),
                            opnd_create_reg(REG_S1)));
    
    PRE_INSERT(bb, instr,
            INSTR_CREATE_mov_st(drcontext,
              opnd_create_rel_addr(&commands[emulate_pc].id, OPSZ_8),
              OPND_CREATE_INT32(rule->reg0)));
   
    dr_restore_reg(drcontext, bb, instr, REG_S1, SPILL_SLOT_2);
    dr_restore_reg(drcontext, bb, instr, REG_S2, SPILL_SLOT_3);
}
static void reg_table_event(JANUS_CONTEXT, instr_t *instr){
    opnd_t src, dest;
    int num_srcs, num_dsts, i;
    uint64_t size;
    
    num_srcs = instr_num_srcs(instr);
    num_dsts = instr_num_dsts(instr);

    for(i=0; i< num_srcs; i++){
       opnd_t operand1 = instr_get_src(instr, i);
       if(opnd_is_reg(operand1)){
          src = operand1;
          size= opnd_get_size(src);
       }
    }
    for(i=0; i< num_dsts; i++){
       opnd_t operand2 = instr_get_dst(instr, i);
       if(opnd_is_reg(operand2)){
          dest = operand2;
          size= opnd_get_size(dest);
       }
    }
    PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_rel_addr(&(commands[emulate_pc].opcode), OPSZ_2),
                            OPND_CREATE_INT16(CM_REG_REG_COPY)));
    PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_rel_addr(&(commands[emulate_pc].src_reg), OPSZ_2),
                            OPND_CREATE_INT16(opnd_get_reg(src))));
    PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_st(drcontext,
                            opnd_create_rel_addr(&(commands[emulate_pc].dest_reg), OPSZ_2),
                            OPND_CREATE_INT16(opnd_get_reg(dest))));
    
    PRE_INSERT(bb, instr,
            INSTR_CREATE_mov_st(drcontext,
              opnd_create_rel_addr(&commands[emulate_pc].id, OPSZ_8),
              OPND_CREATE_INT32(rule->reg0)));
}
void guard_execute(int command_size){

    guard_opcode_t op;
    for (int i=0; i<command_size; i++)
    {
        guard_t &command = commands[i];
        op = (guard_opcode_t)command.opcode;

        switch (op) {
            case CM_RECORD_BASE:
               mlc_base_counter++;
               assert(mlc_base_counter== mlc_size_counter && "Mismatched malloc parameters");
               printf("base pointer: %#08" PRIx64 "\n", command.base);
               reg_table[(reg_id_t)DR_REG_RAX].base =  command.base;
               reg_table[(reg_id_t)DR_REG_RAX].bound = (command.base!=0x0)? (command.base)+ptr_size: 0x0;
            break;
            case CM_RECORD_BOUND:
               mlc_size_counter++;
               ptr_size= command.bound;
            break;
            case CM_BND_CHECK:
               check_overflow_reg(command.dest_reg, command.addr, command.id);
            break;
            case CM_REG_REG_COPY:
                copy_reg_table(command.src_reg, command.dest_reg, command.id);
            break;
            case CM_MEM_REG_LOAD:
               check_overflow_mem(command.ptr_addr, command.addr, command.id);
            break;
            case CM_REG_MEM_STORE:
                write_to_mem(command.src_reg, command.addr, command.id);
            break;
            case CM_VALUE_MEM:
            break;
            case CM_VALUE_REG:
            break;
            default:
                printf("Dynamic operation not recognised %d\n", op);
            break;
        }
    }
    /* Reset pc */
   // emulate_pc = 0;
}
void copy_reg_table(reg_id_t src, reg_id_t dest, uint64_t id){
    auto it = reg_table.find(src);
    if(it != reg_table.end()){
        reg_table[dest].base = it->second.base;
        reg_table[dest].bound= it->second.bound;
    }
}
void check_overflow_reg(reg_id_t dest, addr_t address, uint64_t id){
     auto it= reg_table.find(dest);
     if(it != reg_table.end()){
         if(!(reg_table[dest].base <= address && address < (reg_table[dest].bound)))
             cout<<"Overflow!!"<<endl;
     }
}
void check_overflow_mem(addr_t key, addr_t address, uint64_t id){
     auto it= memory_table.find(key);
     if(it != memory_table.end()){
         if(!(memory_table[key].base <= address && address < (memory_table[key].bound)))
             cout<<"Overflow!!"<<endl;
     }
}
void write_to_mem(reg_id_t src, addr_t address, uint64_t id){
      auto it = reg_table.find(src);
      if(it != reg_table.end()){
          memory_table[address].base = reg_table[src].base;
          memory_table[address].bound = reg_table[src].bound;
      }
}
