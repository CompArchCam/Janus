#include "asanRule.h"
#include "JanusContext.h"
#include "IO.h"
#include "Arch.h"
#include "Alias.h"
#include "offsetAnalysis.h"
#include <algorithm>
#include <assert.h>
#include <regex>
#include <fstream> //For file operation
#include <unordered_map>
#include <iomanip>
#include <stack>
using namespace janus;
using namespace std;

#define GCC_CODE 1

static int count_rule=0;
static bool has_debug_info = false;
static bool null_rules = true;
static int detect_stack_overflows = 1;
std::map<PCAddress, pair<Expr*, Expr*>> malloc_metadata;
static std::pair<Expr*, Expr*> malloc_bb;
std::set<PCAddress> safeAccess;

std::set<PCAddress> asanlib_missing_blocks{140672, 370592, 370320, 371600, 370985, 370348, 370452, 371648, 142464, 371661, 371808, 370478, 142328, 370609, 370327,371613, 370362, 370584, 372932, 372963, 372967};


/*-----------------------Function Prototypes ----------------------*/
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

/*----------------- Routine to add security related rules -----------------------*/
static void insert_asan_rule(Instruction *instr, RuleOp ruleID, int data1, int data2, int data3 = 0, int data4 = 0){
    RewriteRule rule;
    BasicBlock *bb = instr->block;
    if(bb == NULL) return;     
    rule = RewriteRule(ruleID, bb->instrs->pc, instr->pc, instr->id);
    rule.reg0 = data1;
    rule.reg1 = data2;
    rule.reg2 = data3;
    rule.reg3 = data4;
    insertRule(0, rule, bb);
    count_rule++;
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
std::string printLine(const std::smatch& m){
	std::string lineMatch;

	lineMatch += "\n";
	lineMatch += m.prefix().str();
	lineMatch += m.str();
	lineMatch += m.suffix().str();
	lineMatch += "\n";
	return (std::move(lineMatch));
}

string removeZero(string str)
{
    // Count leading zeros
    int i = 0;
    while (str[i] == '0')
        i++;
    // The erase function removes i characters from given index (0 here)
    str.erase(0, i);
    return str;
}
void search_plt(char * filename) {
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
void monitor_malloc(JanusContext *jc){
  for(auto &func: jc->functions){
     if ((!func.entry && !func.instrs.size()) || func.isExternal) continue;
     for(auto it: func.calls){
        Instruction *instr = &(func.instrs[it.first]);
        if(isAllocCall(instr)){ //do further analysis
            ExpandedExpr msize(ExpandedExpr::SUM);
            //ExpandedExpr mbase(ExpandedExpr::SUM);
            Expr mbase;
            //Step 1: calculate size of malloc allocation
            for(auto &vs : instr->inputs){
              if(vs->type == JVAR_REGISTER && vs->value == JREG_RDI){
                  if(!vs->expr->expandedFuncForm){
                      expandExpr(vs->expr, &func);
                  }
                  msize.merge(*vs->expr->expandedFuncForm);
              } 
            }
            //Step 2: calculate base of malloc allocation
            for(auto &vs : instr->outputs){
               
               /*if(!vs->expr->expandedFuncForm)
                   expandExpr(vs->expr, &func);
               mbase.merge(*vs->expr->expandedFuncForm);
                 */
                 mbase = *vs->expr;
                 break;
            }
            //Step 3: build expression for base and base+size, and save for later use 
            msize.addTerm(mbase);
            //msize.merge(mbase);
            malloc_metadata[instr->pc] = make_pair(new Expr(mbase), new Expr(msize));
        }
     }
  }
}
static void monitor_loop_access(JanusContext *jc){
    for(auto &loop:  jc->loops){
       Iterator *miter = loop.mainIterator;
       if(miter == NULL) continue;
       for(auto &mr : loop.memoryReads){
          VarState* mvs = mr->vs; 
          if(!traceToAllocPC(mvs)) continue;   //if it does not trace to a malloc, skip
          ExpandedExpr loopRange(ExpandedExpr::SUM);
          get_range_expr(mr, &loopRange, miter);
          //if this memory node is linked to a malloc site (heap access), and loop range is within malloc base+bound, safe access
          if(mvs->mallocsite && loopRange < *malloc_metadata[mvs->mallocsite].second->ee){
              for(auto acc : mr->readBy){
                  safeAccess.insert(acc->pc);
              }
          }
       }
       for(auto &mw : loop.memoryWrites){
          VarState* mvs = mw->vs; 
          if(!traceToAllocPC(mvs)) continue;   //if it does not trace to a malloc, skip
          //Iterator *miter = loop.mainIterator;
          ExpandedExpr loopRange(ExpandedExpr::SUM);
          get_range_expr(mw, &loopRange, miter);
          if(mvs->mallocsite && loopRange < *malloc_metadata[mvs->mallocsite].second->ee){
              for(auto acc : mw->writeFrom){
                  safeAccess.insert(acc->pc);
              }
          }
       }
    }
    
}
static void monitor_mem_access(JanusContext *jc){
    RewriteRule rule;
    for(auto &func: jc->functions){
        if ((!func.entry && !func.instrs.size() || (func.isExternal))) continue;
        if(gcc_clang_func.count(func.name)) continue;
       for(auto &bb : func.blocks){
            for(auto &meminstr : bb.minstrs){              //Memory Instruction
                Instruction* raw_instr = meminstr.instr;       //Raw Instrcution
                MachineInstruction* minstr = raw_instr->minstr; //Machine Instruction
                if(minstr == NULL || minstr->isLEA()) continue;
                

                //JASAN_OPT = JASAN_SCEV + JASAN_LIVE, optimised version includes both SCEV and liveness
                //if instruction does not trace back to malloc (intra-procedural) or is deemed safe, skip
                if(jc->mode == JASAN_OPT || jc->mode ==  JASAN_SCEV){
                    if(!traceToAlloc(meminstr.mem) || safeAccess.count(raw_instr->pc))continue;
                }
                uint32_t bitmask_flags = 0x1; //always live
                uint32_t bitmask_regs = 0x0;
                uint32_t size =meminstr.mem->size; //memory operand
               
                //add liveness information for advanced/improved analysis
                if(jc->mode == JASAN_LIVE || jc->mode == JASAN_OPT){
                    //if(func.name != "main" && func.name != "perl_parse")
                    bitmask_flags = func.liveFlagIn[raw_instr->id].bits;
                    bitmask_regs = func.liveRegIn[raw_instr->id].bits;
                }

                if (meminstr.type == MemoryInstruction::Read ||
                                        meminstr.type == MemoryInstruction::ReadAndRead) {
                    insert_asan_rule(raw_instr, (RuleOp)MEM_R_ACCESS,bitmask_flags,bitmask_regs);
                }
                else if (meminstr.type == MemoryInstruction::Write){
                    insert_asan_rule(raw_instr, (RuleOp)MEM_W_ACCESS,bitmask_flags,bitmask_regs);
                }
                else if (meminstr.type == MemoryInstruction::ReadAndWrite){
                    insert_asan_rule(raw_instr, (RuleOp)MEM_RW_ACCESS,bitmask_flags,bitmask_regs);
                }
            }
        }
    }
}
static void 
monitor_stack_access(JanusContext* jc){
  //Step 1: look for functions which have stack canaries
  int ccount = 0;
  for(auto &func: jc->functions){
     if(!func.hasCanary) continue;
     if ((!func.entry && !func.instrs.size()) || func.isExternal) continue;
     if(gcc_clang_func.count(func.name)) continue;
     if(func.name.compare("main") != 0) continue;
     PCAddress cr_instr = func.canaryReadIns;
     PCAddress cc_instr = func.canaryCheckIns;
#ifdef DEBUG_VERBOSE
     cout<<"canaryReadIns: "<<hex<<cr_instr<<" canarayCheckIns: "<<cc_instr<<endl;
#endif
     BasicBlock* entry = func.entry;
     if(!entry || !(entry->instrs)) continue;
     int size = entry->size;
     uint32_t bitmask_flags; //always live
     uint32_t bitmask_regs;
     for(int i=0; i<size; i++){           //checking only the entry block
         Instruction instr = entry->instrs[i]; 
        //look for instruction with canary. the next instruction moves canary to rbp(-0x8). instrumentation after that one.
         if(instr.pc == cr_instr){
            if(i+1 <entry->size){
                ccount++;
                //Store shadow stack location where canary value is stored
                //mov    %fs:0x28,%rax  -> canary load instruction
                //mov    %rax,+0x8(%rsp)   -> move canary to current stack frame. store -0x8(rbp) as the canary location 
                //store canary before the second instruction, poison it after the second instruction. 
                Instruction &instr_after = entry->instrs[i+1];
                if(instr_after.minstr->isMOV()){ //need to make sure it's the correct move

                    bitmask_flags = (jc->mode == JASAN_LIVE || jc->mode == JASAN_OPT) ? func.liveFlagIn[instr_after.id].bits : 0x1;
                    bitmask_regs =  (jc->mode == JASAN_LIVE || jc->mode == JASAN_OPT) ? func.liveRegIn[instr_after.id].bits : 0x0;

                    insert_asan_rule(&instr_after, (RuleOp)STORE_CANARY_SLOT, bitmask_flags,bitmask_regs, ccount, 0);
                }
            }
            if(i+2 <entry->size){
                Instruction &next = entry->instrs[i+2];
                bitmask_flags = (jc->mode == JASAN_LIVE || jc->mode == JASAN_OPT) ? func.liveFlagIn[next.id].bits : 0x1;
                bitmask_regs =  (jc->mode == JASAN_LIVE || jc->mode == JASAN_OPT) ? func.liveRegIn[next.id].bits : 0x0;

                insert_asan_rule(&next, (RuleOp)POISON_CANARY_SLOT, bitmask_flags,bitmask_regs, ccount,0);
            }
         } 
     }
#if GCC_CODE //for g++ code
     //2. Unpoison Stack , need to make sure that it's the same canary slot
     //mov    0x8(%rsp),%rdi            //unpoison before this instruction to allow canary check in subseq instr
     //xor    %fs:0x28,%rdi             //checking if canary value has been modified
     int ic=0;
     for(auto &instr : func.instrs){
         if(instr.minstr->checksStackCanary() && ic>0){
             Instruction prev = func.instrs[ic-1];
             if(prev.minstr && prev.minstr->isMOV() && prev.isMemoryAccess()){
                bitmask_flags = (jc->mode == JASAN_LIVE || jc->mode == JASAN_OPT) ? func.liveFlagIn[prev.id].bits : 0x1;
                bitmask_regs =  (jc->mode == JASAN_LIVE || jc->mode == JASAN_OPT) ? func.liveRegIn[prev.id].bits : 0x0;
                insert_asan_rule(&prev, (RuleOp)UNPOISON_CANARY_SLOT, bitmask_flags,bitmask_regs, ccount,0);
                break;
             }
         }
         ic++;
     }
     for(auto iid : func.longjmps){
        Instruction &longjmp = func.instrs[iid]; 
        bitmask_flags = (jc->mode == JASAN_LIVE || jc->mode == JASAN_OPT) ? func.liveFlagIn[iid].bits : 0x1;
        bitmask_regs =  (jc->mode == JASAN_LIVE || jc->mode == JASAN_OPT) ? func.liveRegIn[iid].bits : 0x0;
        insert_asan_rule(&longjmp, (RuleOp)UNPOISON_CANARY_LONGJMP, bitmask_flags,bitmask_regs, 0,0);
        
        RewriteRule rule2;
        rule2 = RewriteRule((RuleOp)PROF_START, longjmp.block, PRE_INSERT);
        insertRule(0, rule2, longjmp.block);
        
     }
     for(auto &it: func.calls){
         if(it.second->name == "__sigsetjmp@plt"){
         }
     }
#else
     //2. Unpoison Stack , need to make sure that it's the same canary slot
     //mov    %fs:0x28,%rax            
     //cmp    0x8(rsp),%rax             //unpoison before this, also LEA address match to counter value
     //for(auto &instr : func.instrs){
     for(auto &bb : func.blocks){
         for(int i=0; i<bb.size ; i++){
             auto &instr = bb.instrs[i];
             uint32_t bitmask_flags = 0x1; //always live
             uint32_t bitmask_regs = 0x0;
             if(instr.pc == cc_instr ){
                 if(jc->mode == JASAN_LIVE || jc->mode == JASAN_OPT){
                    bitmask_flags = func.liveFlagIn[instr.id].bits;
                    bitmask_regs = func.liveRegIn[instr.id].bits;
                 }
                 if(i+1< bb.size){
                     Instruction &next = bb.instrs[i+1];
                     if(next.minstr && next.minstr->isCMP() && next.isMemoryAccess()){
                        insert_asan_rule(&next, (RuleOp)UNPOISON_CANARY_SLOT, bitmask_flags,bitmask_regs);
                        break;
                     }
                 }
             }
         }
      }
#endif
   }

}
static void
printplt(JanusContext *jc){
     string filename=string(rs_dir + get_binfile_name(jc->name)+ ".s");
     insert_null_rule(jc->program.pltSectionStartAddr);
     search_plt(const_cast<char*>(filename.c_str()));

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
            count_rule++;
         }
      }
   }
}
void mark_null_rules_missing_blocks(){

   for(auto b : asanlib_missing_blocks){
        insert_null_rule(b);
   }

}
static bool inRegSet(uint64_t bits, uint32_t reg){
  if((bits >> (reg-1)) & 1)
      return true;
  return false;
}
int save_count = 0;
int restore_count = 0;
int callsiteCounter=0;

static void analyze_leaf_functions(JanusContext *jc){

  for(auto &func: jc->functions){
     if ((!func.entry && !func.instrs.size()) || func.isExternal) continue;
     if(gcc_clang_func.count(func.name) || func.name == "_plt") continue;

     bool rdi_written = false; 
     bool rsi_written = false; 
     int save_rdi = 0; 
     int save_rsi = 0; 
     //if function has subcalls, skip
     if(func.subCalls.size()  || func.jumpCalls.size() ) continue;
     //if function has no memory instructions, skip
     bool readWriteMem = false;
     for(auto &bb : func.blocks){
       if(bb.minstrs.size()){ //even if one memory instruction found, we proceed
           readWriteMem = true;
           break;
       }
     }
     if(!readWriteMem) continue;    //if not memory read/write instruction, no need to save/restore
     for(auto &instr: func.instrs){
        for(auto op : instr.outputs){
           if(op->type == JVAR_REGISTER){
                 if(op->value == JREG_RDI) //writes RDI, no need to save
                     rdi_written = true;
                 if(op->value == JREG_RSI) //writes RSI, no need to save
                     rsi_written = true;
           }
        }
     }
     //as far as it is not written by an instr in the function, the caller will assume it does not need to be save.as for reading, we will still need to save as it could be alive in one basic block but not antother, so it will not be saved by the instrumentation at such instructions. so we need to make sure that we save nonetheless. in the worst case, we wll only be saving double.
     if(!rsi_written) save_rsi = 1;
     if(!rdi_written) save_rdi = 1;
     if(save_rsi || save_rdi){
         //save
         Instruction* entry_instr = &(func.entry->instrs[0]);
        save_count++;
         insert_asan_rule(entry_instr, SAVE_AT_ENTRY,save_rdi, save_rsi, save_count,0);
         //restore
        restore_count++;
         for (auto retID : func.terminations) {
            BasicBlock &bb = func.blocks[retID];
            Instruction *exit_instr = bb.lastInstr();
            if (exit_instr->opcode == Instruction::Return) { //TODO: look for lonjmp as well
                insert_asan_rule(exit_instr, RESTORE_AT_EXIT, save_rdi, save_rsi, restore_count, 0);
            }
         }
     }
  }
}
void
generateASANRule(JanusContext *jc)
{
    //if plt section not identified in disassembler, use the hack to get basicblocks
    if(!jc->pltsection) 
        printplt(jc);
    //for JASAN_NULL, mark all basic blocks with nulli(no-op) rules, to indicate no need to process it dynamically  
    if(jc->mode == JASAN_NULL){
        mark_null_rules(jc);
        //HACK: to deal with the basic blocks not recognised in the elf
        if(get_binfile_name(jc->name) == "libclang_rt.asan-x86_64.so"){
             mark_null_rules_missing_blocks();
        }
        mark_noop_blocks(jc); //to solve the issue of DR starting bb from noop sometimes.
        return;
    }
    //HACK: mark entry of main, to avoid accessing shadow memory set up before it has been set up
    mark_main_entry(jc);

    if(jc->mode == JASAN_OPT || jc->mode == JASAN_SCEV){
        monitor_malloc(jc);   
        monitor_loop_access(jc);
    }
    //analyse remaining memory accesses 
    monitor_mem_access(jc);
    //use liveness for rsi, rdi and rax around function calls
    if(jc->mode == JASAN_LIVE || jc->mode == JASAN_OPT){
        analyze_leaf_functions(jc);
    }

    //add rules for canary value shadowing and poisoning
    if(detect_stack_overflows){
       monitor_stack_access(jc);
    }
    //attach null rules to remaining basic blocks, to indicate NOT to further processing dynamically
    if(null_rules)
      mark_null_rules(jc);
}
