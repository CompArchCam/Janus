#include "sbCETSRule.h"
#include "JanusContext.h"
#include "IO.h"
#include "Arch.h"
#include <algorithm>
#include <regex>

#include <assert.h>
#include <string.h>
#include <unordered_map>
#include <iomanip>
using namespace janus;
using namespace std;

#define OBJECT_SYM 1
//#define DEBUG_DETAIL
//#define DEBUG_DETAIL_2
//#define DEBUG_DETAIL_STACK
/*------------------- Var Declarations --------------------------*/
enum allocType{
    MALLOC=1,
    CALLOC=2
};

std::set<Instruction*> boundCheckSet;
std::set<Instruction*> visitedSet; //same as tableMaintainSet
std::set<VarState*> memoryNodeSet;
std::set<Function*> interProcSet;
std::unordered_map<int, std::set<int>> funcAllocMap;
std::unordered_map<uint64_t, uint32_t> global_sym_table;
int push_count =0;
int pop_count =0;
/*------ Temporal Safety ------*/
int useAfterFree = 1;
int doubleFree = 1;
int temporal_safety = 1;
/*------ Spatial Safety---------*/
int spatial_safety = 1;
int detect_heap_overflows = 1;
int detect_stack_overflows = 1;
int detect_global_overflows = 1;
int trace_func_boundaries = 0;
int trace_stack_args = 0;
int enable_push_pop = 0;
bool has_debug_info = false;
/*-----------------------Function Prototypes ----------------------*/

static bool null_rules = true;
int  is_alloc_call(Instruction *instr);
bool trace_to_alloc(VarState  *vs);
bool trace_to_instr(VarState  *vs, Instruction *instr);
//bool generate_table_main_rule(Instruction *instr);
bool generate_table_main_rule(Instruction *instr, Function &func,  uint64_t bitmask_flags, uint64_t bitmask_regs );
//bool generate_table_main_rule(Instruction *instr, int funcID);
void mark_table_calls(VarState *vs, int funcID);
std::string called_func_name(Instruction *instr);
int node_type(VarState *vs);
void monitor_stack_args(JanusContext *jc);
bool generate_rule_LEA_instr(Instruction *instr, VarState *ip, VarState *op,  uint64_t bitmask_flags, uint64_t bitmask_regs );
bool generate_rule_MOV_instr(Instruction *instr, VarState *ip, VarState *op,  uint64_t bitmask_flags, uint64_t bitmask_regs );
bool generate_rule_ARITH_instr(Instruction *instr, VarState *ip, VarState *op,  uint64_t bitmask_flags, uint64_t bitmask_regs );
bool generate_rule_PUSH_instr(Instruction *instr, VarState *ip,  uint64_t bitmask_flags, uint64_t bitmask_regs );
void print_stack_details(JanusContext *jc);
/* 32-bit code on 32 or 64 bit (eax = 32bit, rax = 64 bit)

  base registers: eax-edx, esp,ebp, esi, edi
  index registers: eax-edx, ebp, esi, edi
  64-bit code on 64-bit x86

  base: GPR  rax-rdx, rsp, rbp, rsi, erdi, r8-r15
  index: same as base

  indirect address: mov 1, (%rax)
  indirect with disp: mov 1, -24(%rbp)
  indirect with displacement and scaled index
  
 JVAR_MEMORY          Generic memory variables (in form: [base+index*scale+disp])
 -0x8(%rbp)             => base = rbp, value = -0x8
 +0x8(%rsp, rax, 4)     => base = rsp, index = rax, scale =4, value =0x8
 +0x8(%rbp, rax, 4)     => base = rbp, index = rax, scale =4, value =0x8
 +0x8(, rax, 4)         => base = 0, index = rax, scale =4, value =0x8
 +0x8(%rax, rcx, 4)     => base = rax, index = rcx, scale =4, value =0x8
 +0x8(%rax, rcx)        => base = rax, index = rcx, scale =1, value =0x8
 (%rax, rcx, 4)         => base = rax, index = rcx, scale =4, value =0x0
 +0x606180(, rcx, 4)    => base = 0,   index = rcx, scale =4, value = 0x606180 //global. static base address


 JVAR_ABSOLUTE        Absolute memory addresses (PC-relative addresses)
 0x200bb5(%rip)         => base = rip, value = 0x200bb5 + pc

 JVAR_STACK            Stack variables (only in form stack with displacement) 
 +0x8(%rsp)             => base = rsp, value = 0x8

 JVAR_POLYNOMIAL        Polynomial variable type (reserved for x86), JVAR_MEM, JVAR_ABSOLUTE or JVAR_STACK used in LEA
 LEA  rbp, [rip + 0x2007be] 

 JVAR_CONSTANT          Immediate value

 */

/*--------------------- Function Definitions --------------------*/


std::string called_func_name(Instruction *instr){
   std::string name = "";
   Function *targetFunc;
   
   if(instr->opcode != Instruction::Call || instr->block == NULL || instr->block->parentFunction == NULL) 
       return name; 
    
    targetFunc = instr->block->parentFunction->calls[instr->id];

    if(targetFunc != NULL)
        return targetFunc->name;

    return name;
}
/*----------------- Routine to add security related rules -----------------------*/
bool insert_security_rule(Instruction *instr, RuleOp ruleID, uint64_t data1, uint64_t data2, uint64_t data3=0, uint64_t data4=0){
    RewriteRule rule;
    BasicBlock *bb = instr->block;
    if(bb == NULL) return false;      //ADDED NEW
    rule = RewriteRule(ruleID, bb->instrs->pc, instr->pc, instr->id);
    rule.reg0 = data1;
    rule.reg1 = data2;
    rule.reg2 = data3;
    rule.reg3 = data4;
    insertRule(0, rule, bb);
    return true;
}
/* ------------------ Check if the instruction is a malloc call ------------------*/
int 
is_alloc_call(Instruction *instr){
   Function *targetFunc; 
   
    std::string name= called_func_name(instr);
    if(!name.compare("malloc@plt")) 
        return MALLOC;
    else if(!name.compare("calloc@plt"))
        return CALLOC;
    return 0;
}
/*------------------ Monitor Calls to free() to check for use-after-free and double free ------*/
void monitor_free_callsite(JanusContext *jc){
    uint64_t bitmask_flags = 0x1; //always live
    uint64_t bitmask_regs = 0x0;
    for(auto &func: jc->functions){
        if ((!func.entry && !func.instrs.size()) ||func.isExternal ) continue; //TODO: do this only if this function has a malloc call
        if(gcc_func.count(func.name)) continue;
        for(auto it: func.calls){
           Instruction *instr = &(func.instrs[it.first]);
           if(instr != NULL && !called_func_name(instr).compare("free@plt")){//calls free function
               if(jc->mode == JSBCETS_LIVE){
                   //TODO: id - 1 or instr->pred->id?? if we insert before the instr, the values of prev instr flags or regs need to be saved? 
                        bitmask_flags = func.liveFlagIn[instr->id].bits;
                        bitmask_regs = func.liveRegIn[instr->id].bits;
                }
               insert_security_rule(instr, MONITOR_FREE_CALL, bitmask_flags,bitmask_regs); 
           }
        }
    }
}
/*------------------ Monitor Calls to malloc()/calloc() to check for heap overflows  ------*/
void monitor_malloc_callsite(JanusContext *jc){
    
    /* Place holder for a static rule for copy */
    RewriteRule rule;
    uint64_t bitmask_flags = 0x1; //always live
    uint64_t bitmask_regs = 0x0;
    
    for (auto &func: jc->functions){
        if ((!func.entry && !func.instrs.size()) || func.isExternal) continue;
        if(gcc_func.count(func.name)) continue;
	for (auto &bb: func.blocks) { //TODO: go through call sites instead of basic blocks
            Instruction *instr = bb.lastInstr();
	    if(instr == NULL) continue;
            int call_type = is_alloc_call(instr);
            if(call_type){
              //func.hasAlloc = true;
              if(call_type == MALLOC)
                //  insert_security_rule(instr, BND_RECORD_SIZE_MALLOC, 0,0);
                  rule = RewriteRule(BND_RECORD_SIZE_MALLOC, bb.instrs->pc, instr->pc, instr->id);
              else if (call_type == CALLOC)
                  //insert_security_rule(instr, BND_RECORD_SIZE_CALLOC, 0,0);
                  rule = RewriteRule(BND_RECORD_SIZE_CALLOC, bb.instrs->pc, instr->pc, instr->id);
              
              insertRule(0, rule, &bb);
              
              if(bb.succ1) {
                  rule = RewriteRule(BND_RECORD_BASE, bb.succ1, PRE_INSERT);
                  int iid = bb.succ1->instrs->id;
                  if(jc->mode == JSBCETS_LIVE){
                        /*bitmask_flags = func.liveFlagOut[instr->id].bits;
                        bitmask_regs = func.liveRegOut[instr->id].bits;
                        */
                        bitmask_flags = func.liveFlagIn[iid].bits;
                        bitmask_regs = func.liveRegIn[iid].bits;
                  }
                  rule.reg0 = bitmask_flags;
                  rule.reg1 = bitmask_regs;

                  insertRule(0, rule, bb.succ1);
              }
              visitedSet.insert(instr); //TODO: why need this??
              funcAllocMap[func.fid].insert(instr->id);
	  }
       }
    }
}
/*--------------------------- Monitor Memory Accesses ---------------------------------*/
void monitor_mem_access(JanusContext *jc){
    RewriteRule rule;
    uint64_t bitmask_flags = 0x1; //always live
    uint64_t bitmask_regs = 0x0;
    for(auto &func: jc->functions){
        if ((!func.entry && !func.instrs.size()) || func.isExternal) continue;
        if(gcc_func.count(func.name)) continue;
#ifdef DEBUG_DETAIL_2
        cout<<"===============Func: "<<func.name<<"================="<<endl;
#endif
        push_count = 0;
        pop_count = 0;
        for(auto &instr : func.instrs){
           if(jc->mode == JSBCETS_LIVE){
                bitmask_flags = func.liveFlagIn[instr.id].bits;
                bitmask_regs = func.liveRegIn[instr.id].bits;
            }
            generate_table_main_rule( &instr, func, bitmask_flags, bitmask_regs);
        }
        //remove RAX from table
        for(auto it: func.calls){
           Instruction *instr = &(func.instrs[it.first]);
           if(instr == NULL || (detect_heap_overflows && funcAllocMap[func.fid].count(instr->id))) continue; //instr is NULL or alloc call
            //Add rules to remove RAX from the table for other function calls.
           if(instr->block && instr->block->succ1) {
              rule = RewriteRule(BND_REMOVE_RAX, instr->block->succ1, PRE_INSERT);
              if(jc->mode == JSBCETS_LIVE){
                    bitmask_flags = func.liveFlagIn[instr->id].bits;
                    bitmask_regs = func.liveRegIn[instr->id].bits;
              }
              rule.reg0 = bitmask_flags;
              rule.reg1 = bitmask_regs;
              insertRule(0, rule, instr->block->succ1);
           }
        }
    }
}
bool generate_rule_MOV_instr(Instruction *instr, VarState *ip, VarState *op,  uint64_t bitmask_flags, uint64_t bitmask_regs ){
    bool rule_applied = true;
    //case 1: DEST: reg    SRC: reg, mem, absoulte addr, const 
    if(op->type == JVAR_REGISTER){
        switch(ip->type){
            case JVAR_REGISTER:
                if(!(op->value == JREG_RBP && ip->value == JREG_RSP)){
                    insert_security_rule(instr, (RuleOp)TABLE_REG_REG_COPY,bitmask_flags,bitmask_regs, ip->value, op->value);    //reg->reg copy
                }
            break;
            case JVAR_MEMORY:                                              //memory->reg load
                if(ip->base){
                    //cout<<hex<<instr->pc<<" "<<*instr<<" dest: "<<op->value<<" base: "<<(int)ip->base<<endl; 
                    //cout<<hex<<instr->pc<<" "<<*instr<<endl; 
                    insert_security_rule(instr, (RuleOp)TABLE_MEM_REG_LOAD,bitmask_flags,bitmask_regs, (int)op->value/*dest_id*/, (int)ip->base/*base reg*/);    
                
                }else{
                  if(detect_global_overflows && has_debug_info){
                     if(global_sym_table.count(ip->value)){
#ifdef DEBUG_DETAIL_2
                            cout<<"GLOBAL_MEM_REG_LOAD - instr: "<<dec<<instr->id<<endl;
#endif
                            insert_security_rule(instr, (RuleOp)GLOBAL_MEM_REG_LOAD, bitmask_flags, bitmask_regs, ip->value, ip->value + global_sym_table[ip->value]); 
                     }
                  }
                }
                /* else if no base, and value not in global table such as 0x0(,rax,4) => base missing, value not in global, but rax is in the table?? here index is rax. add rax, 4 => 0x0(,rax,4)=> ? no it will rather create %(rax) and will be detected. what about mov eax, 0x1,*/ 
            break;
            case JVAR_ABSOLUTE:                                            //abs mem ->reg load
                if(detect_global_overflows && has_debug_info && global_sym_table.count(ip->value)){
                    insert_security_rule(instr, (RuleOp)ABS_GLOBAL_MEM_REG_LOAD, bitmask_flags, bitmask_regs, ip->value, ip->value+global_sym_table[ip->value]);    
                }
                else{
                    insert_security_rule(instr, (RuleOp)ABS_MEM_REG_LOAD,bitmask_flags,bitmask_regs, op->value/*dest_reg*/, 0/*no base*/);    
                }
            break;
            case JVAR_CONSTANT:
                if(detect_global_overflows && has_debug_info && global_sym_table.count(ip->value)){
#ifdef DEBUG_DETAIL_2
                            cout<<"GLOBAL_VALUE_REG - instr: "<<dec<<instr->id<<endl;
#endif
                        insert_security_rule(instr, (RuleOp)GLOBAL_VALUE_REG,bitmask_flags, bitmask_regs, ip->value,ip->value + global_sym_table[ip->value]);    
                    
                }else{
                    insert_security_rule(instr, (RuleOp)TABLE_VALUE_REG,bitmask_flags,bitmask_regs, op->value /*dest_reg*/, 0/*no base*/);    //value->reg //TODO: check if absoulte is in global table??
                }
            break;
            case JVAR_STACK:            //NEED ATTENTION
                insert_security_rule(instr, (RuleOp)TABLE_MEM_REG_LOAD,bitmask_flags,bitmask_regs);    //+0x8(%SP) => reg
            break;
            default:
                //cout<<" Invalid operands: invalid src for MOV to reg"<<endl;
                rule_applied= false;
            break;
        }
    }
    //case 2: DEST: mem, SRC: reg,const 
    else if(op->type == JVAR_MEMORY){
      switch(ip->type){
          case JVAR_REGISTER:
              if(op->base){
                  insert_security_rule(instr, (RuleOp)TABLE_REG_MEM_STORE,bitmask_flags,bitmask_regs, (int)ip->value/*src_reg*/, (int)op->base/*memory base reg*/);  //reg->memory store
              }else{
                  if(detect_global_overflows && has_debug_info){
                      if(global_sym_table.count(op->value)){//TODO: add rules for global table
#ifdef DEBUG_DETAIL_2
                        cout<<"GLOBAL_REG_MEM_STORE - instr: "<<dec<<instr->id<<endl;
#endif
                        insert_security_rule(instr, GLOBAL_REG_MEM_STORE, bitmask_flags, bitmask_regs, op->value, op->value + global_sym_table[op->value]); 
                        //insert_security_rule(instr, MONITOR_GLOBAL_BUFFER, op->value, global_sym_table[op->value]); 
                      }
                  }
              } 
          break;
          case JVAR_CONSTANT:
            if(op->base){
                insert_security_rule(instr, (RuleOp)TABLE_VALUE_MEM,bitmask_flags,bitmask_regs, 0 /*src_id*/, (int)op->base/*memory base*/);  //value->memory
            }else{
                  if(detect_global_overflows && has_debug_info){
                      if(global_sym_table.count(op->value)){//
#ifdef DEBUG_DETAIL_2
                        cout<<"GLOBAL_TABLE_VALUE_MEM - instr: "<<dec<<instr->id<<endl;
#endif
                        insert_security_rule(instr, GLOBAL_TABLE_VALUE_MEM, bitmask_flags, bitmask_regs, op->value, op->value + global_sym_table[op->value]); 
                       } 
                 }
            }
          break;
          case JVAR_STACK:
            rule_applied= false;            //TODO: is this even valid? yes. mostly LEA to load stack address to global memory such as memory => +0x8(%SP).
          break;
          default:
            cout<<"instr:"<<hex<<instr->pc<<" "<<*instr<<endl;
            cout<<" Invalid operands: invalid src for MOV(store) to mem"<<endl;
            rule_applied= false;
           break;
        }
   }
   //case 3: DEST = addr w.r.t. SP, SRC = reg, const
   else if(op->type == JVAR_STACK){ 
          switch(ip->type){
              case JVAR_REGISTER:       //NEED ATTENTION
               // insert_security_rule(instr, (RuleOp)TABLE_REG_MEM_STORE,bitmask_flags,bitmask_regs);  //reg->memory store //mov rax => 0x8(SP)
              break;
              case JVAR_CONSTANT:       //ADDED_NEW
              //  insert_security_rule(instr, (RuleOp)TABLE_VALUE_MEM,bitmask_flags,bitmask_regs);  //reg->memory store //mov rax => 0x8(SP)
                rule_applied= false;            //mov const => 0x8(SP)           
              break;
              default:
                rule_applied= false;
              break;
              }
   }
   //case 4: DEST= absolute addr w.r.t. IP, SRC = reg, const
   else if(op->type == JVAR_ABSOLUTE){ //TODO: add rules here
      switch(ip->type){
          case JVAR_REGISTER:
            insert_security_rule(instr, (RuleOp)ABS_REG_MEM_STORE,bitmask_flags,bitmask_regs, ip->value/*src_reg*/, 0/*memory_base*/);  //reg->memory store
          break;
          case JVAR_CONSTANT:
                insert_security_rule(instr, (RuleOp)ABS_VALUE_MEM,bitmask_flags,bitmask_regs);  //value->memory
          break;
         case JVAR_STACK:
            rule_applied= false;            //TODO: is this even valid?
         break;
          default:
            cout<<" Invalid operands for MOV to absolute addr"<<endl;
            rule_applied= false;
          break;
      }
   }else{
        cout<<" Invalid operands for MOV. dest not valid"<<endl;
        rule_applied= false;
   }
   return rule_applied;
}
/*--------- Generate rules for LEA instructions ----------*/
bool generate_rule_LEA_instr(Instruction *instr, VarState *ip, VarState *op,  uint64_t bitmask_flags, uint64_t bitmask_regs ){
    bool rule_applied = true;
    //case 1: LEA src: JVAR_POLYNOMINAL, dest: reg
    if(ip->type == JVAR_POLYNOMIAL){
        if(ip->base){
            insert_security_rule(instr, (RuleOp)LEA_COPY_BASE,bitmask_flags,bitmask_regs); //do we need use-after-free etc?
        }
        else{ //if no base, do we want to check through global table?
            if(ip->value){  //should we check index?? 
                  if(detect_global_overflows && has_debug_info && global_sym_table.count(ip->value)){//TODO: add rules for global table
#ifdef DEBUG_DETAIL_2
                        cout<<"GLOBAL_LEA_COPY_BASE - instr: "<<dec<<instr->id<<endl;
#endif
                           insert_security_rule(instr, GLOBAL_LEA_COPY_BASE, bitmask_flags, bitmask_regs, ip->value, ip->value + global_sym_table[ip->value]); 
                  }
                  else{
                     rule_applied = false;
                  }
            }
            else{
                 rule_applied = false;
            }
        }
    }
    //case 2: LEA src: JVAR_STACK, dest:reg
    else if(ip->type == JVAR_STACK){    //NEED ATTENTION: would that even be the case with LEA as JVAR_STACK changed to JVAR_POLY
        insert_security_rule(instr, (RuleOp)LEA_COPY_STACK_BASE,bitmask_flags,bitmask_regs); //do we need use-after-free etc?
    }
    return rule_applied;
}
/*--------- Generate rules for ARITH instructions ----------*/
bool generate_rule_ARITH_instr(Instruction *instr, VarState *ip, VarState *op,  uint64_t bitmask_flags, uint64_t bitmask_regs ){
    bool rule_applied = true;
    if(op->type == JVAR_REGISTER){
        switch(ip->type){
            case JVAR_MEMORY: 
                if(ip->base)
                    insert_security_rule(instr, (RuleOp)ARITH_MEM_REG_LOAD,bitmask_flags,bitmask_regs, op->value /*dest_id*/, ip->base /*mem base*/);    
                else{
                  if(detect_global_overflows && has_debug_info){
                     if(global_sym_table.count(ip->value)){
#ifdef DEBUG_DETAIL_2
                        cout<<"GLOBAL_ARITH_MEM_REG_LOAD - instr: "<<dec<<instr->id<<endl;
#endif
                        insert_security_rule(instr, GLOBAL_ARITH_MEM_REG_LOAD, ip->value, ip->value + global_sym_table[ip->value]); 
                     }else
                        rule_applied = false;
                  }else{
                      rule_applied = false;
                  }
                }
            break;
            case JVAR_STACK:    //is that even possible with memory reference?
                rule_applied = false; 
            break;
            case JVAR_ABSOLUTE:         //NEED ATTENTION
                rule_applied = false;
            break;
            default:
                rule_applied = false;
            break;
            //TODO: what about JVAR_STACK 
        }
    }
    else if(op->type == JVAR_MEMORY){
        switch(ip->type){
          case JVAR_REGISTER:
              if(op->base){
                  insert_security_rule(instr, (RuleOp)ARITH_REG_MEM_STORE,bitmask_flags,bitmask_regs, ip->value /*src_reg*/, op->base /*memory base*/);  //reg->memory store
              }else{
                  if(detect_global_overflows && has_debug_info){
                      if(global_sym_table.count(op->value)){
#ifdef DEBUG_DETAIL_2
                        cout<<"GLOBAL_ARITH_REG_MEM_STORE - instr: "<<dec<<instr->id<<endl;
#endif
                        insert_security_rule(instr, GLOBAL_ARITH_REG_MEM_STORE, bitmask_flags, bitmask_regs, op->value, op->value + global_sym_table[op->value]); 
                      }
                  }
              } 
          break;
          case JVAR_CONSTANT:
            if(op->base){
                insert_security_rule(instr, (RuleOp)ARITH_VALUE_MEM,bitmask_flags,bitmask_regs, 0/*src_reg*/, op->base /*mem base*/);  //value->memory
            }
            else{
                  if(detect_global_overflows && has_debug_info){
                      if(global_sym_table.count(op->value)){
#ifdef DEBUG_DETAIL_2
                        cout<<"GLOBAL_ARITH_REG_MEM_STORE - instr: "<<dec<<instr->id<<endl;
#endif
                        insert_security_rule(instr, GLOBAL_ARITH_VALUE_MEM, bitmask_flags, bitmask_regs, op->value, op->value + global_sym_table[op->value]); 
                      }
                  }
             
            }
          break;
          default:
            cout<<" Invalid operands"<<endl;
            rule_applied= false;
           break;
        
        }
    }
    else if(op->type == JVAR_ABSOLUTE){
        rule_applied = false; //is this even possible?
    }
    else if(op->type == JVAR_STACK){
        rule_applied = false; //is this even possible with memory reference? 
    
    }
    return rule_applied;
}

bool generate_rule_PUSH_instr(Instruction *instr, VarState *ip,  uint64_t bitmask_flags, uint64_t bitmask_regs ){
   bool applied = false;
   if(ip->type == JVAR_REGISTER){
        //TODO: see how RSP and RBP are treated differently.
        if(ip->value != JREG_RBP){
            //shall we check if there is any call in that func or push part of entry??
            applied = insert_security_rule(instr, PUSH_REG, 0, 0);
            push_count++;
        }
   }
   else if(ip->type == JVAR_MEMORY){
        if(ip->base){
            applied = insert_security_rule(instr, PUSH_MEM, 0, 0); //is it mostly just stack vars? 
            push_count++;
        }else{
            if(has_debug_info && global_sym_table.count(ip->value)){
                applied = insert_security_rule(instr, PUSH_GLOBAL, ip->value, ip->value + global_sym_table[ip->value]); 
            push_count++;
            }
        }
   }
   else if(ip->type == JVAR_ABSOLUTE){
        applied = insert_security_rule(instr, PUSH_ABS, 0, 0); 
            push_count++;
   }
   else if(ip->type == JVAR_CONSTANT){
      if(has_debug_info && global_sym_table.count(ip->value)){
            applied = insert_security_rule(instr, PUSH_GLOBAL, ip->value, ip->value + global_sym_table[ip->value]); 
            push_count++;
      }
      else{
            applied = insert_security_rule(instr, PUSH_CONSTANT, ip->value, 0); 
            //applied = insert_security_rule(instr, PUSH_CONSTANT, 0, 0); 
            push_count++;
      }
   }
   else if(ip->type == JVAR_STACK){
       //TODO: what to do here?
        applied = insert_security_rule(instr, PUSH_STACK_VAR, 0, 0); 
   }
}
/*Routine to generate table maintenance rules*/
bool generate_table_main_rule(Instruction *instr, Function &func, uint64_t bitmask_flags, uint64_t bitmask_regs ){
    bool rule_applied= true; 
   
    VarState *ip, *op;
    
    // if(visitedSet.count(instr)) return true; //already tracked 
    if(instr->minstr == NULL) return false;
       
    for(auto vs: instr->inputs){

      ip =vs; //assumption: only one i/p //need to make sure these are correct
    }
    for(auto vs: instr->outputs){
      op =vs;                   //TODO: need to make sure these are correct
    }

   //visitedSet.insert(instr);
    //if(instr->minstr->isMOV()){
    if(instr->opcode == Instruction::Mov){
        if(instr->inputs.size() == 0){
           return false;
        }
        else{
            generate_rule_MOV_instr(instr, ip, op, bitmask_flags, bitmask_regs);
        }
    }
    else if(instr->minstr->isLEA()){
        generate_rule_LEA_instr(instr, ip, op, bitmask_flags, bitmask_regs);
    }
    else if(instr->opcode == Instruction::Add || instr->opcode == Instruction::Sub || instr->opcode == Instruction::Mul || instr->opcode == Instruction::Div){
        if(instr->minstr->hasMemoryReference()){
            generate_rule_ARITH_instr(instr, ip, op, bitmask_flags, bitmask_regs);
        }
        else{//
            if(instr->opcode == Instruction::Add && op->type == JVAR_REGISTER && op->value == JREG_RSP){
                    if(ip->type == JVAR_CONSTANT || ip->type == JVAR_REGISTER)
                    {
                        if(func.hasConstantSP_AddSub){ //value is zero, add no rule
                            if(func.hasConstantSP_total){ 
                            }
                        }
                        else{ //AddSub not zero but value to 
                            if(ip->value == func.stackFrameSize){ //or changeSizeAddSub==func.stackFrameSize
                            }
                            else{ //add rule
                                if(ip->type == JVAR_CONSTANT){ //sub rsp, 0x8
                                    int offset = (ip->value)/8;
                                    //pop by that much size,no copying, only remove from tables if added there
                                    rule_applied = insert_security_rule(instr, POP_STACK, offset, 0);
                                }
                                else{ //sub rsp, rax , send reg id
                                    rule_applied = insert_security_rule(instr, POP_STACK_REG, 0, 0);
                                }
                            }
                        }
                    }
            }
            else if(instr->opcode == Instruction::Sub){
                op = instr->outputs[0]; //may not be the case for all SUB. only for sub rsp, 0x8 type
                if(op->type == JVAR_REGISTER && op->value == JREG_RSP)
                {
                    if(ip->type== JVAR_CONSTANT || ip->type == JVAR_REGISTER){
                        if(func.hasConstantSP_AddSub){ //value is zero, add rule
                            if(func.hasConstantSP_total){ 
                            }
                        }
                        else{ //AddSub not zero but value to 
                            //cout<<"instr: "<<hex<<instr->pc<<" add rule here for sub from rsp"<<endl;
                            if(ip->type == JVAR_CONSTANT){ //sub rsp, 0x8
                                //if(ip->value == func.stackFrameSize){ //or changeSizeAddSub==func.stackFrameSize
                                if(func.changeSizeAddSub == func.stackFrameSize){ //or changeSizeAddSub==func.stackFrameSize
                                } //TODO:if by the end of function anything left in size of stackFrameSize, delete
                                else if(func.changeSizeTotal == func.stackFrameSize && ip->value == func.stackFrameSize){
                                }
                                else{
                                int offset = (ip->value)/8;
                                //TODO:what to do in this case?? we may not need a rule, just record it somewhere??
                                rule_applied = insert_security_rule(instr, PUSH_STACK, offset, 0);
                                }
                            }
                            else{ //sub rsp, rax , send reg id
                                rule_applied = insert_security_rule(instr, PUSH_STACK_REG, 0, 0);
                            }
                        }
                    }
                }
            }
        }
    }
    else if(enable_push_pop){
       if(instr->minstr->isPUSH()){
            generate_rule_PUSH_instr(instr, ip, bitmask_flags, bitmask_regs);

        }
        else if(instr->minstr->isPOP()){
        //do i also need to check if push was through add sub,8 so pop does not have to do anything?
            //TODO: see how RSP and RBP are treated differently.
        //shall we see if pop part of termination/endblocks and no call in that function.or init/endblock size = same?
            if(op->value != JREG_RBP){
                rule_applied = insert_security_rule(instr, POP_REG, 0, 0); 
            }
        }
    }
    else{ 
        rule_applied = false;
    }
#ifdef DEBUG_DETAIL_3
    if(rule_applied) cout<<"applied rule for instr: "<<instr->id<<endl;
#endif
    return rule_applied;
}

void load_symbol_table(JanusContext *jc){
    if(!jc->program.hasStaticSymbolTable){
        cout<<"WARNING: Cannot track global buffers. No symbol table found"<<endl;
        return;
    }
    has_debug_info = true;
    cout<<"Loading Symbol Table......"<<endl;
    for(auto &sym : jc->program.symbols){
        if(sym.type == OBJECT_SYM && sym.size > 0 && 
            ((sym.section->name.find(".bss") != std::string::npos) || (sym.section->name.find(".data") != std::string::npos)))
        {
            global_sym_table[sym.startAddr] = sym.size;
#ifdef DEBUG_DETAIL 
            cout<<sym.name<<" \t "<<sym.type<<" \t "<<hex<<sym.startAddr<<" \t "<<sym.size<<" \t "<<sym.section->name<<endl;
#endif
        }
    }
}
void monitor_stack_access(JanusContext *jc){
    
    RewriteRule rule;
    for (auto &func: jc->functions) {
        bool hasStackRules = false;
        bool entryRules = false;
        bool exitRules = false;
        if ((!func.entry && !func.minstrs.size()) || func.isExternal) continue;
        if(gcc_func.count(func.name)) continue;
        BasicBlock *entry = func.entry; 
        //shall i skip external or not executable as well? 
        //if(func.stackFrameSize && func.totalFrameSize && func.hasConstantStackPointer){
        /*-------Step 1: Store stack bounds at function entry and remove at Exit ---------------*/
        if(func.stackFrameSize>0){ //stackFrameSize > 0, insert rule after making stack base i.e. SUB rsp, 0xsize
            for(int i=0; i<entry->size; i++){
               Instruction *instr = &(entry->instrs[i]);
               if(instr->pc == func.prologueEnd){
                    //assert(i+1 < func.entry->size);
                    if(i+1 >= func.entry->size) continue;
                    Instruction *next_instr = &(func.entry->instrs[i+1]);
                    insert_security_rule(next_instr, STORE_STACK_BOUNDS, func.totalFrameSize, func.hasBasePointer ? 1 : 0);
                    break;
               }
            }
            
        }
        else{ //stackFrameSize = 0
             if(func.totalFrameSize == 0){
                if(func.hasBasePointer){ //has BP, insert rule right after mov rsp->rbp
                    for(int i=0; i<entry->size; i++){
                       Instruction *instr = &(entry->instrs[i]);
                       if(instr->minstr->isMakingStackBase()){
                            insert_security_rule(instr, STORE_STACK_BOUNDS, 0, 1);
                            break;
                       }
                    }

                }
                else{ //no BP, insert rule just before first instruction
                    Instruction* instr = &(entry->instrs[0]); 
                    insert_security_rule(instr, STORE_STACK_BOUNDS, func.totalFrameSize, 0);
                }

             }
             else{ //totalFrameSize > 0
                Instruction* instr_rule;
                for(int i=0; i<entry->size; i++){
                   Instruction *instr = &(entry->instrs[i]);
                   if(instr->minstr->isPUSH()){
                     instr_rule = instr; 
                   }
                }
                //assert(instr_rule != NULL);
                if(instr_rule != NULL)
                    insert_security_rule(instr_rule, STORE_STACK_BOUNDS, func.totalFrameSize, func.hasBasePointer);
                 /*if(func.hasBasePointer){//has BP, insert rule after last PUSH?

                 }
                 else{ // no BP, insert rule after last PUSH.
                 }*/
             }
        }
        /* REMOVE STACK BOUNDS is inserted at all exit blocks */
        for (auto retID : func.terminations) {
            BasicBlock &bb = func.blocks[retID];
            Instruction *instr = bb.lastInstr();
            if (instr->opcode == Instruction::Return) {
                //rule = RewriteRule(REMOVE_STACK_BOUNDS, &bb, PRE_INSERT);
                Instruction *e_instr = instr;
                BasicBlock &e_block = bb;
                rule = RewriteRule(REMOVE_STACK_BOUNDS, bb.instrs->pc, instr->pc, instr->id);
                rule.reg0 = func.hasBasePointer ? 1 : 0;
                insertRule(0, rule, &bb); //need to remove these even if we do not monitor stack voilations. that's to ensure that no unnecessary address remains in memory table.
            }
        }


        /* STACK_{SAVE,RESTORE} is inserted {before,after} each call */
        for(auto call: func.calls) {
            Instruction &instr = func.instrs[call.first];
            BasicBlock *block = instr.block;
            if (!block) continue;
            /* SAVE_STACK */
            rule = RewriteRule(SAVE_STACK_BOUNDS, block, POST_INSERT);
            rule.reg0 = func.hasBasePointer? 1 : 0;
            insertRule(0, rule, block);
#ifdef DEBUG_DETAIL
            cout<<"Save Stack Pointer"<<endl;
#endif

            if(block && block->succ1)
                block = block->succ1;
            /* RESOTRE_STACK */
            if(block) {
                rule = RewriteRule(RESTORE_STACK_BOUNDS, block, PRE_INSERT);
                rule.reg0 = func.hasBasePointer? 1 : 0;
                insertRule(0, rule, block);
#ifdef DEBUG_DETAIL
                cout<<"Restore Stack Pointer"<<endl;
#endif
            }
        }
    }
//var.value=offser/disp, var.base, var.index, var.scale

#ifdef DEBUG_DETAIL_STACK
    print_stack_details(jc);
#endif
    if(trace_stack_args){
       monitor_stack_args(jc);
    }
}
void print_stack_details(JanusContext *jc){
    for(auto &func: jc->functions){
        //if ((!func.entry || !func.instrs.size())) continue; 
        cout<<"-----Function: "<<func.name<<" stack details-----"<<endl;
        std::string value;
        value = jc->functions[func.fid].isExternal ? "Yes" : "No";
        cout<<"Is External? "<<value<<endl;
        value = jc->functions[func.fid].isExecutable ? "Yes" : "No";
        cout<<"Is Executable? "<<value<<endl;
        if ((!func.entry || !func.instrs.size())) continue; 
        cout<<"Stack Frame Size: "<<func.stackFrameSize<<endl;
        cout<<"total Frame Size: "<<func.totalFrameSize<<endl;
        value = func.hasBasePointer ? "Yes" : "No";
        cout<<"Has Base Pointer? "<<value<<endl;
        value = func.implicitStack ? "Yes" : "No";
        cout<<"implicit Stack? "<<value<<endl;
        value = func.hasIndirectStackAccesses ? "Yes" : "No";
        cout<<"Indirect Stack Accesses? "<<value<<endl;
        value = func.hasConstantStackPointer ? "Yes" : "No";
        cout<<"has Constant Stack Pointer? "<<value<<endl;
    }

}
void monitor_stack_args(JanusContext *jc){
   bool contains_calls = false;
   for(auto &func: jc->functions){
      if ((!func.entry || !func.instrs.size()) || func.isExternal) continue;
      if(gcc_func.count(func.name)) continue;
      //cout<<"**Caller -- "<<func.name<<endl;
      for(auto &call : func.calls){
          if(call.second == NULL || jc->functions[(call.second)->fid].isExternal) continue; //or Executable?? currently only checking user-defined functions.
          //analyse basic block. look for push and mov to JVAR_STACK (if pushing local vars)
          //cout<<"Callee -- "<<jc->functions[(call.second)->fid].name<<endl;
          Instruction *instr = &(func.instrs[call.first]);
          BasicBlock *block =  instr->block;
          assert(instr->id == block->lastInstr()->id); //assert it belongs to this block, TODO: what if the block starts with call because previous instruction was a CTI?
          int arg_size = 0;
          bool entry_block = false;
          if(block->bid == func.entry->bid) entry_block = true;
          for(int i=0; i< block->size; i++){
              Instruction& instr = block->instrs[i];
              if(entry_block && func.stackFrameSize > 0 && instr.pc <= func.prologueEnd) continue; //skip all the instructions up to sub 0x10, rsp 
              if(instr.minstr->isPUSH()){ //if func.hasConstantStackPointer, skip checking PUSH
                  VarState* ip;
                  for(auto vs: instr.inputs){
                      ip =vs;
                  }
                  if(ip->type == JVAR_REGISTER && ip->value == JREG_RBP) continue; // //TODO: should i see if a pop cancels out a push?
                  arg_size += instr.minstr->operands[0].size;
              }
              else if(instr.minstr->isMOV()){
                  VarState* op, *ip;
                  for(auto vs: instr.outputs){
                      op =vs;
                  }
                  for(auto vs: instr.inputs){
                      ip =vs;
                  }
                  //for movups ptr [rax]<- xmm0 or movups ptr [rsp] <- xmm0, it recognoises 0 outputs 
                  if(instr.minstr->isXMMInstruction() && instr.inputs[0]->type == JVAR_STACK){
                     op = instr.inputs[0];
                  }
                  if(op->type == JVAR_STACK && op->value >= 0){ //positive offset, what about we push wrt to RBP  , if SP not changed, we can only have negative offsets e.g. for red zone. what about then if SP not changed and moved to negative offset?? hopefully will not be the case in caller. also if hasBasePointer = No, but stackSize>0 and we use SP+0x10 index
                     arg_size += instr.minstr->operands[1].size; //src size  
                 } 
             }
          }//end of iteration over instructions in basic block
          if(arg_size > 0){
                //add rule to pass arg_size just before call instr.
               insert_security_rule(instr, PASS_STACK_ARGS, arg_size, 0); 
          }
      }//end of iteration over calls
   }//end of iteration over functions
   //next for each function entry, check if there is any rule there, grab it and add to size, if function hasBasePointer, up to 16 bytes for RBP and Return address. this region is forbidden. allowed will be (RBP+0x10) + (RBP+0x10+size)
}
void check_entry_instructions(JanusContext *jc){
    for(auto &func: jc->functions){
        if ((!func.entry || !func.instrs.size()) || func.name.compare("main")) continue; 
        cout<<"-----Function: "<<func.name<<" entry details-----"<<endl;
        cout<<"entry instructions"<<endl; 
        for(int i=0; i< func.entry->size ; i++){
            cout<<"instr: " <<func.entry->instrs[i]<<endl;
            if(func.entry->instrs[i].minstr->isMoveStackPointer()){
               cout<<"Moving Stack Pointer"<<endl;
            }
        }
        cout<<"exit instructions"<<endl; 
        for (auto retID : func.terminations) {
            BasicBlock &bb = func.blocks[retID];
            for(int i=0; i< bb.size; i++)
                cout<<"instr: " <<bb.instrs[i]<<endl;
        }
    } 
}
static string get_binfile_name(string filepath){
    // Returns first token
    char *token = strtok(const_cast<char*>(filepath.c_str()), "/");
    char *filename = token;
    // Keep printing tokens while one of the delimiters present in str.
    while (token != NULL)
    {
        token = strtok(NULL, "/");
        if(token == NULL)
            break;
        else
            filename = token;
    }
    string finalname =filename;
    //printf("final file: %s\n", filename);
    return finalname;
}
/* Prints the matching line */
static std::string printLine(const std::smatch& m){
        std::string lineMatch;

        lineMatch += "\n";
        lineMatch += m.prefix().str();
        lineMatch += m.str();
        lineMatch += m.suffix().str();
        lineMatch += "\n";
        return (std::move(lineMatch));
}

static string removeZero(string str)
{
    // Count leading zeros
    int i = 0;
    while (str[i] == '0')
        i++;
    // The erase function removes i characters from given index (0 here)
    str.erase(0, i);
    return str;
}
static void search_plt(char * filename) {
        char  pattern_i[] ="@plt";
        bool enable_grep = false;
        std::regex pattern(pattern_i,std::regex_constants::grep);
        char pattern_text[] = "Disassembly of section .text";
        char  pattern_plt[] = "Disassembly of section .plt";
        std::regex pattern_t(const_cast<char*>(pattern_text),std::regex_constants::grep);
        std::regex pattern_plt_t(const_cast<char*>(pattern_plt),std::regex_constants::grep);
        std::ifstream fp(filename);
        if(!fp.is_open()) {
            std::cout << "Error in opening the file \n";
            exit(EXIT_FAILURE);
        }
        std::string line;
        string text_start = "Disassembly of section .text";
        bool found = false;
        while(getline(fp,line)) {
            std::smatch match;
            std::smatch match_t;
            if(!enable_grep)
            {
                if(regex_search(line,match_t,pattern_plt_t)){ 
                    enable_grep= true;
                }
                else{
                    continue;
                }
            }
            if(regex_search(line,match_t,pattern_t)) break;
            
     //       fprintf(stdout,"%s",regex_search(line,match,pattern) ? printLine(match).c_str() :"");
            found = regex_search(line,match,pattern);
            if(found){
                printLine(match).c_str();
                string mstr = string(match.prefix().str()+match.str()+match.suffix().str());
                std::string firstWord = mstr.substr(0, mstr.find(" "));
                firstWord = removeZero(firstWord);
                PCAddress bbAddr = (PCAddress)stoul(firstWord,nullptr, 16);
                if(!rewriteRules[0].ruleMap.count(bbAddr)){ //if bb not found in map, insert empty rule
                    insert_null_rule(bbAddr);
                }
            }
        }
        fp.close();
}
static void
printplt(JanusContext *jc){
     string filename=string(rs_dir + get_binfile_name(jc->name)+ ".s");
     insert_null_rule(jc->program.pltSectionStartAddr);
     search_plt(const_cast<char*>(filename.c_str()));

}
static void analyze_leaf_functions(JanusContext *jc){
//  initially set all the caller-saved registers as live (RDI, RSI, RCD, RDX, R8, R9, R10, R11)

  for(auto &func: jc->functions){
     if ((!func.entry && !func.instrs.size()) || func.isExternal) continue;
     if(gcc_func.count(func.name) || func.name == "_plt") continue;

     //if function has subcalls, skip
     if(func.subCalls.size()  || func.jumpCalls.size() ) continue;
      uint64_t bitmask_reg = 0xFC6; // b'1111 1100 0110
      RegSet WriteSet;
     for(auto &instr: func.instrs){
        for(auto op : instr.outputs){
           if(op->type == JVAR_REGISTER){
                 WriteSet.insert(op->value);
                 bitmask_reg = bitmask_reg & ~(WriteSet.bits); //reset the bits for registers that has been written to
           }
        }
     }
     //as far as it is not written by an instr in the function, the caller will assume it does not need to be save.as for reading, we will still need to save as it could be alive in one basic block but not antother, so it will not be saved by the instrumentation at such instructions. so we need to make sure that we save nonetheless. in the worst case, we wll only be saving double.
     if(bitmask_reg){
         //save
         Instruction* entry_instr = &(func.entry->instrs[0]);
         insert_security_rule(entry_instr, SAVE_AT_ENTRY,bitmask_reg,0);
         //restore
         for (auto retID : func.terminations) {
            BasicBlock &bb = func.blocks[retID];
            Instruction *exit_instr = bb.lastInstr();
            if (exit_instr->opcode == Instruction::Return) { //TODO: look for lonjmp as well
                insert_security_rule(exit_instr, RESTORE_AT_EXIT, bitmask_reg, 0);
            }
         }
     }
  }
}
static void mark_main_entry(JanusContext *jc){
   for(auto &func: jc->functions){
      if(func.name.compare("main")==0){
         if (func.entry){
            RewriteRule rule;
            BasicBlock *bb = func.entry;
            if(bb == NULL) return;
            rule = RewriteRule((RuleOp)ENABLE_MONITORING, bb->instrs->pc, bb->instrs->pc, bb->instrs->id);
            rule.reg0 = 0;
            rule.reg1 = 0;
            insertRule(0, rule, bb);
         }
      }
   }
}
void
generateSBCETSRule(JanusContext *jc)
{
    cout<<"SBCETS RULES"<<endl; 
    //if plt section not identified in disassembler, use the hack to get basicblocks
    if(!jc->pltsection)
        printplt(jc);
    //for JASAN_NULL, mark all basic blocks with null(no-op) rules, to indicate no need to process it dynamically  
    if(jc->mode == JSBCETS_NULL){
        mark_null_rules(jc);
        //HACK: to deal with the basic blocks not recognised in the elf
        mark_noop_blocks(jc); //to solve the issue of DR starting bb from noop sometimes.
        return;
    }
    mark_main_entry(jc);
   
    if(detect_global_overflows){
        load_symbol_table(jc);
    }
    if(detect_heap_overflows){
        monitor_malloc_callsite(jc);
    }
    if(detect_stack_overflows){
       monitor_stack_access(jc);
    }
    //print_stack_details(jc);
    monitor_mem_access(jc);
    //use liveness for rsi, rdi and rax around function calls
    if(jc->mode == JSBCETS_LIVE){
        analyze_leaf_functions(jc);
    /*    analyze_call_sites(jc);
    */
    }
    
    if(temporal_safety){ 
        if(detect_heap_overflows)// free() call sites
            monitor_free_callsite(jc);
        /*if(detect_stack_overflows){ //use-after-stack-dealloc
        }*/
    }
        //attach null rules to remaining basic blocks, to indicate NOT to further processing dynamically
    if(null_rules){
      mark_null_rules(jc);
//      mark_noop_blocks(jc); //TODO:NEED to make sure we attach the correct rules here. not just noop 
    }
}
