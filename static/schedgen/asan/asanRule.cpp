#include "asanRule.h"
#include "JanusContext.h"
#include "IO.h"
#include "Arch.h"
#include "Alias.h"
//#include "offsetAnalysis.h"
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
static int detect_stack_overflows = 0;
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
static void insert_asan_rule(Instruction *instr, RuleOp ruleID, int data1, int data2){
    RewriteRule rule;
    BasicBlock *bb = instr->block;
    if(bb == NULL) return;     
    rule = RewriteRule(ruleID, bb->instrs->pc, instr->pc, instr->id);
    rule.reg0 = data1;
    rule.reg1 = data2;
    insertRule(0, rule, bb);
    count_rule++;
}
static void insert_null_rule(PCAddress bb){
    RewriteRule rule;
    rule = RewriteRule(NO_RULE, bb , bb , 0);
    rule.reg0 = 0;
    rule.reg1 = 0;
    insertRule_null(0, rule);
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

static void monitor_mem_access(JanusContext *jc){
    RewriteRule rule;
    for(auto &func: jc->functions){
        if ((!func.entry && !func.instrs.size() || (func.isExternal))) continue;
        if(gcc_clang_func.count(func.name)) continue;
#ifdef DEBUG_VERBOSE
        cout<<"===============Func: "<<func.name<<"================="<<endl;
#endif
       for(auto &bb : func.blocks){
            for(auto &meminstr : bb.minstrs){              //Memory Instruction
                Instruction* raw_instr = meminstr.instr;       //Raw Instrcution
                MachineInstruction* minstr = raw_instr->minstr; //Machine Instruction
                if(minstr == NULL || minstr->isLEA()) continue;
                uint32_t bitmask_flags = 0x1; //always live
                uint32_t bitmask_regs = 0x0;
                uint32_t size =meminstr.mem->size; //memory operand
                //add liveness information for advanced/improved analysis
                if(jc->mode == JASAN_LIVE || jc->mode == JASAN_OPT){
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
                //TODO: shall we check if the base is stack
            }
        }
    }
}
static void 
monitor_stack_access(JanusContext* jc){
  //Step 1: look for functions which have stack canaries
  for(auto &func: jc->functions){
     //cout<<"===============Func: "<<func.name<<"================="<<endl;
     if(!func.hasCanary) continue;
     if ((!func.entry && !func.instrs.size()) || func.isExternal) continue;
     if(gcc_clang_func.count(func.name) || !func.hasCanary) continue;
     PCAddress cr_instr = func.canaryReadIns;
     PCAddress cc_instr = func.canaryCheckIns;
#ifdef DEBUG_VERBOSE
     cout<<"canaryReadIns: "<<hex<<cr_instr<<" canarayCheckIns: "<<cc_instr<<endl;
#endif
     BasicBlock* entry = func.entry;
     if(!entry || !(entry->instrs)) continue;
     int size = entry->size;
     for(int i=0; i<size; i++){           //checking only the entry block
         uint32_t bitmask_flags = 0x1; //always live
         uint32_t bitmask_regs = 0x0;
         Instruction instr = entry->instrs[i]; 
        //look for instruction with canary. the next instruction moves canary to rbp(-0x8). instrumentation after that one.
         if(instr.pc == cr_instr){
            if(jc->mode == JASAN_LIVE || jc->mode == JASAN_OPT){
                bitmask_flags = func.liveFlagIn[instr.id].bits;
                bitmask_regs = func.liveRegIn[instr.id].bits;
            }
            insert_asan_rule(&instr, (RuleOp)STORE_CANARY_SLOT, bitmask_flags,bitmask_regs);
            if(i+1 <entry->size){
                Instruction &next = entry->instrs[i+1];
                if(next.minstr->isMOV()){ //need to make sure it's the correct move
                    //Store shadow stack location where canary value is stored
                    //mov    %fs:0x28,%rax  -> canary load instruction
                    //mov    %rax,+0x8(%rsp)   -> move canary to current stack frame. store -0x8(rbp) as the canary location 
                    bitmask_flags = 0x1; //always live
                    bitmask_regs = 0x0;
                    if(jc->mode == JASAN_LIVE || jc->mode == JASAN_OPT){
                        bitmask_flags = func.liveFlagIn[next.id].bits;
                        bitmask_regs = func.liveRegIn[next.id].bits;
                    }
                    insert_asan_rule(&next, (RuleOp)POISON_CANARY_SLOT, bitmask_flags,bitmask_regs);
                }
            }
         } 
     }
#if GCC_CODE //for g++ code
     //2. Unpoison Stack , need to make sure that it's the same canary slot
     //mov    0x8(%rsp),%rdi            //unpoison before this instruction to allow canary check in subseq instr
     //xor    %fs:0x28,%rdi             //checking if canary value has been modified
     int ic=0;
     for(auto &instr : func.instrs){
         if(instr.minstr->pc == cc_instr && ic>0){
             Instruction prev = func.instrs[ic-1];
             if(prev.minstr && prev.minstr->isMOV() && prev.isMemoryAccess()){
                insert_asan_rule(&prev, (RuleOp)UNPOISON_CANARY_SLOT, 1,0);
                //cout<<"rule added for unpoison canary"<<endl;
                break;
             }
         }
         ic++;
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
                        //cout<<"rule added for unpoison canary"<<endl;
                        break;
                     }
                 }
             }
         }
      }
#endif
   }

}
static void mark_null_rules(JanusContext *jc){
   for(auto &func: jc->functions){
      //cout<<"===============Func: "<<func.name<<"================="<<endl;
      //if ((!func.entry && !func.instrs.size())) continue;
      for(auto &bb : func.blocks){
         if(!rewriteRules[0].ruleMap.count(bb.instrs->pc)){ //if bb not found in map, insert empty rule
             insert_null_rule(bb.instrs->pc);
         } 
      }
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
      //cout<<"===============Func: "<<func.name<<"================="<<endl;
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
void mark_noop_blocks(JanusContext *jc){

   for(auto b : jc->nopInstrs){
        insert_null_rule(b);
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

    //analyse remaining memory accesses 
    monitor_mem_access(jc);

    //add rules for canary value shadowing and poisoning
    if(detect_stack_overflows){
       monitor_stack_access(jc);
    }
    //attach null rules to remaining basic blocks, to indicate NOT to further processing dynamically
    if(null_rules)
      mark_null_rules(jc);
}
