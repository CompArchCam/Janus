#include "cfiRule.h"
#include "JanusContext.h"
#include "IO.h"
#include "Arch.h"
#include "Alias.h"
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
#define BASE_32BIT 0x8048000
static int count_rule=0;
static bool has_debug_info = false;
static bool null_rules = true;
static bool backward_cfi = true;
static bool forward_cfi = true;
PCAddress firstPC;
PCAddress lastPC;
PCAddress GOTAddress;
uint32_t safeChecks=0;
std::set<uintptr_t> ICF_targets;
std::set<uintptr_t> callback_targets;
std::set<uintptr_t> IJF_targets;//
std::set<uintptr_t> IJF_srcs;    //id of instruction
std::set<int> IJ_Func;     //id of function
std::set<uintptr_t> safeICT;
uintptr_t ro_start;
uintptr_t ro_end;
PCAddress ro_base; 
#define PROLOGUE 0

/*----------------- Routine to add security related rules -----------------------*/
static bool isEndBranchAddr(JanusContext *jc,PCAddress addr);
static bool isValidInstrAddr(JanusContext *jc,PCAddress addr);
static void insert_security_rule(Instruction *instr, RuleOp ruleID, int data1, int data2, int data3 = 0, int data4 = 0){
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
bool isLongjmpFunc(string funcName){
   return funcName.find("longjmp_chk") != std::string::npos || funcName.find("__longjmp_chk") != std::string::npos;
}
static string get_binfile_name(string filepath){
    // Make a copy of the string to avoid modifying const data
    char* filepathCopy = new char[filepath.length() + 1];
    strcpy(filepathCopy, filepath.c_str());

    // Returns first token
    char* token = strtok(filepathCopy, "/");
    char *filename = token;
    // Keep printing tokens while one of the delimiters present in str.
    while (token != NULL)
    {
        token = strtok(NULL, "/");
        if(token == NULL){
            break;
        }
        else{
            filename = token;
        }
    }
    string finalname = (filename != NULL) ? std::string(filename) : "";
    delete[] filepathCopy;
    return finalname;
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
static void mark_main_exit(JanusContext *jc){
   for(auto &func: jc->functions){
      if(func.name.compare("main")==0){
         for(auto &exit_id: func.terminations){
            RewriteRule rule;
            BasicBlock bb = func.blocks[exit_id];
            int size = bb.size;
            rule = RewriteRule((RuleOp)DISABLE_MONITORING, bb.instrs->pc, bb.instrs[size-1].pc, bb.instrs[size-1].id);
            rule.reg0 = 0;
            rule.reg1 = 0;
            insertRule(0, rule, &bb);
            count_rule++;
         }
      }
   }
}


static void analyze_leaf_functions(JanusContext *jc){

  for(auto &func: jc->functions){
     if ((!func.entry && !func.instrs.size()) || func.isExternal) continue;
     if(lib_funcs.count(func.name)) continue;

     bool rdi_written = false; 
     bool rsi_written = false; 
     int save_rdi = 0; 
     int save_rsi = 0; 
     //if function has subcalls, skip
     //if(func.subCalls.size()  || func.jumpCalls.size() ) continue;
     if(!func.isLeaf())         continue;
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
         insert_security_rule(entry_instr, SAVE_AT_ENTRY,save_rdi, save_rsi, 0,0);
         //restore
         for (auto retID : func.terminations) {
            BasicBlock &bb = func.blocks[retID];
            Instruction *exit_instr = bb.lastInstr();
            if (exit_instr->opcode == Instruction::Return) { //TODO: look for lonjmp as well
                insert_security_rule(exit_instr, RESTORE_AT_EXIT, save_rdi, save_rsi, 0, 0);
            }
         }
     }
  }
}
static void analyze_call_sites(JanusContext *jc){
    int save_rdi, save_rsi;
  for(auto &func: jc->functions){
     if ((!func.entry && !func.instrs.size()) || func.isExternal) continue;
     if(lib_funcs.count(func.name) || func.name == "_plt" || func.name == "main") continue;
     if(func.isLeaf()) continue; //already taken care of in the analyze_leaf_function
     //only save rdi and rsi at the entry/exit of the function, if they have memory instructions AND they are not being written to. because, if they are being written to, the caller must have saved them.
     bool readWriteMem = false;
     for(auto &bb : func.blocks){
       if(bb.minstrs.size()){ //even if one memory instruction found, we proceed
           readWriteMem = true;
           break;
       }
     }
     if(!readWriteMem) continue;    //if not memory read/write instruction, no need to save/restore
     save_rdi = (func.writeSet.contains(JREG_RDI)) ? 0 : 1; 
     save_rsi = (func.writeSet.contains(JREG_RSI)) ? 0 : 1;
     if(save_rsi || save_rdi){
         //save
         Instruction* entry_instr = &(func.entry->instrs[0]);
         insert_security_rule(entry_instr, SAVE_AT_ENTRY,save_rdi, save_rsi, 0,0);
         //restore
         for (auto retID : func.terminations) {
            BasicBlock &bb = func.blocks[retID];
            Instruction *exit_instr = bb.lastInstr();
            if (exit_instr->opcode == Instruction::Return) { //TODO: look for lonjmp as well
                insert_security_rule(exit_instr, RESTORE_AT_EXIT, save_rdi, save_rsi, 0, 0);
            }
         }
     }
  }
}
static void readCodePointers(JanusContext *jc){
    // Open the file in binary mode
    /*uintptr_t ro_start =  jc->program.RODATA_offset;
    uintptr_t ro_end =  ro_start + jc->program.RODATA_size - 3;
    */
    std::ifstream file(jc->name, std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "Error opening the file" << std::endl;
        return;
    }
     //set the stream to read binary
     file >> std::noskipws;

    uintptr_t address;
    while (file.read(reinterpret_cast<char*>(&address), sizeof(address))) {
        // Display the hex address
        //TODO: add function boundary as additional ICF target condition
         if (isValidInstrAddr(jc, address)){
             ICF_targets.insert(address);
             IJF_targets.insert(address);
             if(jc->functionMap.count(address) || isEndBranchAddr(jc, address))
                 callback_targets.insert(address);
         }
         else{
            int current_pos = file.tellg();
            //TODO: check if we need to update it to ro_start-base, as curr_pos may not be the same.
            //if it is rodata section
            //TODO: for non-PIC, we subtract BASE OFFSET, for PIC, we dont
            //if(current_pos >= (ro_start-BASE_32BIT) && current_pos <= (ro_end-BASE_32BIT)){
            if(current_pos >= (ro_start) && current_pos <= (ro_end)){
                PCAddress offsetAddr= address + GOTAddress;
                if(isValidInstrAddr(jc, offsetAddr)){
                    IJF_targets.insert(offsetAddr);
                }
            }
         }
        // Slide the cursor back by 3 bytes
        file.seekg(-3, std::ios::cur);
    }
    file.close();
}
static bool isValidInstrAddr(JanusContext *jc,PCAddress addr){
    //1. if it is less than start address of any section or greater than return exit
    //2. check for each function
    if(addr < firstPC || addr > lastPC) return false;
    //TODO: check if it is within range of exe sections ).plt, .init, .fini etc)
    if(jc->instructionSet.find(addr) != jc->instructionSet.end()){
           return true; 
    }
    return false;
}

static bool isEndBranchAddr(JanusContext *jc,PCAddress addr){
    //1. if it is less than start address of any section or greater than return exit
    //2. check for each function
    if(addr < firstPC || addr > lastPC) return false;
    
    if(jc->instructionSet.find(addr) != jc->instructionSet.end()){
        if(jc->instructionSet[addr].minstr->isEndBranch()){
            return true;
        } 
    }
    return false;
}
static void
enforceForwardCFI(JanusContext *jc){
    uint32_t bitmask_flags; 
    uint32_t bitmask_reg;
     //STEP 1: read constant code pointers from the bianry
     auto firstFunction = jc->functionMap.begin();
     auto lastFunction =  std::prev(jc->functionMap.end());
     GOTAddress = jc->program.GOTAddress;
     firstPC = firstFunction->second->startAddress;
     lastPC  = lastFunction->second->endAddress-1;
     readCodePointers(jc);
     
     //STEP 2:. Go through all indirect calls and jmps
     for(auto &func: jc->functions){
        if ((!func.entry && !func.instrs.size()) || func.isExternal) continue;
        for(auto &bb: func.blocks){
            Instruction &instr = bb.instrs[bb.size-1];
            int id = instr.id;
            if(func.indirectCTIs.find(id) == func.indirectCTIs.end()) continue; //skip instructions which are not indirect calls
            bitmask_flags = (jc->mode == JCFI_LIVE) ? func.liveFlagIn[id].bits : 0x1;
            bitmask_reg =  (jc->mode == JCFI_LIVE) ? func.liveRegIn[id].bits : 0x0;
            RewriteRule rule;
            if(instr.opcode == Instruction::Call ){
                rule = RewriteRule(VERIFY_CALL_TARGET_INTRA, bb.instrs->pc, instr.pc, instr.id);
                rule.reg0 = bitmask_flags;
                rule.reg1 = bitmask_reg;
                rule.reg2 = 0;
                rule.reg3 = 0;
                insertRule(0, rule, &bb);
            }
            else if(instr.opcode  == Instruction::DirectBranch || instr.opcode == Instruction::ConditionalBranch){
                if(safeICT.count(instr.pc)) continue;  //skip as it is bound-checked jmp table ICT 
                //if(instr.pc == 0x11471b) continue; //TODO: replace it with isLongjmpFunc(func.name), special case, applicable to libc
                IJ_Func.insert(func.fid);
                rule = RewriteRule(VERIFY_JMP_TARGET, bb.instrs->pc, instr.pc, instr.id);
                rule.reg0 = bitmask_flags;
                rule.reg1 = bitmask_reg;
                rule.reg2 = func.startAddress;
                rule.reg3 = 0;
                insertRule(0, rule, &bb);
                //mark this instruction as source instr
                IJF_srcs.insert(instr.pc);
            }
        }
     }
     //STEP 3: if it is a pic binary go through global offset tables
     for(auto &func: jc->functions){
         for(auto vs: func.allStates){
              if(vs->type == JVAR_CONSTANT || vs->type == JVAR_ABSOLUTE){
                 //offset will have negative value, so we make it GOTAddress - (negative val)
                 PCAddress addr = GOTAddress + (PCAddress)vs->value;
                 if(isValidInstrAddr(jc,addr)){
                     ICF_targets.insert(addr);
                     IJF_targets.insert(addr);
                     if(jc->functionMap.count(addr) || isEndBranchAddr(jc,addr))
                         callback_targets.insert(addr);
                 }
             }
          } 
     }


}
void dump_icf_targets(JanusContext *jc){
    fstream outfile; 
     //indirect calls (same for ij)
    string fname=string(rs_dir + get_binfile_name(jc->name)+ "_ic.txt");
    outfile.open(fname,std::ios::out | std::ios::trunc);
    outfile<<dec<<ICF_targets.size()<<endl;
    for(auto tgt : ICF_targets){
        outfile<<hex<<tgt<<endl;
    }
    outfile.close();
     
    //indirect jmps
    fname=string(rs_dir + get_binfile_name(jc->name)+ "_ij.txt");
    outfile.open(fname,std::ios::out | std::ios::trunc);
    outfile<<dec<<IJF_targets.size()<<endl;
    for(auto tgt : IJF_targets){
        outfile<<hex<<tgt<<endl;
    }
    outfile.close();
}

static void
enforceBackwardCFI(JanusContext *jc){
    uint32_t bitmask_flags; 
    uint32_t bitmask_reg;
               
    for(auto &func: jc->functions){
     //1. Save fall-through address
        if(func.name == "_start"){
            for(auto &bb: func.blocks){
                Instruction &instr = bb.instrs[bb.size-1];
                if(instr.opcode == Instruction::Call && func.callSites[instr.id]->name == "__libc_start_main@plt"){
                    bitmask_flags = (jc->mode == JCFI_LIVE) ? func.liveFlagIn[instr.id].bits : 0x1;
                    bitmask_reg =  (jc->mode == JCFI_LIVE) ? func.liveRegIn[instr.id].bits : 0x0;
                    Instruction &push_instr = bb.instrs[bb.size-2];
                    if(!push_instr.minstr->isPUSH()) continue; 
                    uintptr_t address;
                    for(auto ip: push_instr.inputs){
                        if(ip->type == JVAR_CONSTANT || ip->type == JVAR_ABSOLUTE){
                              address = ip->value;
                        }
                       
                    }
                    RewriteRule rule;
                    rule = RewriteRule(SAVE_MAIN_ENTRY, bb.instrs->pc, instr.pc, instr.id);
                    rule.reg0 = bitmask_flags;
                    rule.reg1 = bitmask_reg;
                    rule.reg2 = address;
                    rule.reg3 = 0;
                    insertRule(0, rule, &bb);
                 }
            }

        }
        else{
            if(func.isExternal) continue;
            for(auto &bb: func.blocks){
                Instruction &instr = bb.instrs[bb.size-1];
                if(instr.opcode == Instruction::Call){
                    bitmask_flags = (jc->mode == JCFI_LIVE) ? func.liveFlagIn[instr.id].bits : 0x1;
                    bitmask_reg =  (jc->mode == JCFI_LIVE) ? func.liveRegIn[instr.id].bits : 0x0;
                    RewriteRule rule;
                    rule = RewriteRule(SAVE_RETURN_TARGET, bb.instrs->pc, instr.pc, instr.id);
                    rule.reg0 = bitmask_flags;
                    rule.reg1 = bitmask_reg;
                    rule.reg2 = instr.minstr->size;
                    rule.reg3 = 0;
                    insertRule(0, rule, &bb);
                 }
                 if(instr.opcode == Instruction::DirectBranch || instr.opcode == Instruction::ConditionalBranch){
                     if(func.jumpToFunc.find(instr.id) != func.jumpToFunc.end()){
                        bitmask_flags = (jc->mode == JCFI_LIVE) ? func.liveFlagIn[instr.id].bits : 0x1;
                        bitmask_reg =  (jc->mode == JCFI_LIVE) ? func.liveRegIn[instr.id].bits : 0x0;
                        RewriteRule rule;
                        rule = RewriteRule(SAVE_RETURN_TARGET, bb.instrs->pc, instr.pc, instr.id);
                        rule.reg0 = bitmask_flags;
                        rule.reg1 = bitmask_reg;
                        rule.reg2 = instr.minstr->size;
                        rule.reg3 = 0;
                        insertRule(0, rule, &bb);
                     }
                 }
                 if(func.name == "main") continue;

                 if(instr.opcode == Instruction::Return){
                    //if(instr.pc == 0x16aef) continue; //(ONLY for ld-so lazy symbol resolve)
                    bitmask_flags = (jc->mode == JCFI_LIVE) ? func.liveFlagIn[instr.id].bits : 0x1;
                    bitmask_reg =  (jc->mode == JCFI_LIVE) ? func.liveRegIn[instr.id].bits : 0x0;
                    RewriteRule rule;
                    rule = RewriteRule(VERIFY_RETURN_TARGET, bb.instrs->pc, instr.pc, instr.id);
                    rule.reg0 = bitmask_flags;
                    rule.reg1 = bitmask_reg;
                    rule.reg2 = 0;
                    rule.reg3 = 0;
                    insertRule(0, rule, &bb);
                 }
            }
        }
    }
}
static void dump_iSym(JanusContext *jc){
    fstream outfile;
     string fn = string(rs_dir + get_binfile_name(jc->name)+ "_isym.txt");
     outfile.open(fn,std::ios::out | std::ios::trunc);
     outfile<<jc->program.importedSymbols.size()<<endl;
     for(auto &sym: jc->program.importedSymbols){
         outfile<<sym<<endl;
     }
     outfile.close();
}
static void dump_eSym(JanusContext *jc){
    fstream outfile;
     string fn = string(rs_dir + get_binfile_name(jc->name)+ "_esym.txt");
     outfile.open(fn,std::ios::out| std::ios::trunc);
     for(auto &sym: jc->program.exportedSymbols){
         //only dump address, skip symbols
         if(!callback_targets.count(sym.first)){ //avoid duplication
             outfile<<hex<<sym.first<<endl;
         }
     }
     for(auto &addr : callback_targets){
         outfile<<hex<<addr<<endl;
     } 
     outfile.close();
}
static void analyse_jmptable_targets(JanusContext *jc){
//STEP 1: check for all the functions that have an indirect jmp
    //cout<<"*******************Going to check for jmptables*************************"<<endl;
    for(auto &func: jc->functions){
        bool found = false; 
        if ((!func.entry && !func.instrs.size()) || func.isExternal) continue;
        for(auto &instr: func.instrs){
            int id = instr.id;
            if(func.indirectCTIs.find(id) != func.indirectCTIs.end()){ //skip instructions which are not indirect calls
            //jmp dword ptr [eax*4 + 0x812b600], which is for jmp *0x812b600(, %eax, 4)
                std::regex pattern(R"(jmp dword ptr \[[a-zA-Z0-9]+[*][0-9]+ \+ 0x[0-9a-fA-F]+\])");
                string input(instr.minstr->name);
                std::smatch match;
                bool matching_reg = false;
                int upper_limit = 0;
                uintptr_t jmp_offset;
                if (std::regex_search(input, match, pattern)) {
                    //found an indirect jmp that matches the pattern for jmp table . TODO: //what about instructions which are like (1) mov *0x812b600(, %eax, 4), ecx (2) jmp *(%ecx),
                   if(id-2 >= 0){
                       auto &ja_instr = func.instrs[id-1];
                       auto &cmp_instr = func.instrs[id-2];
                       if(ja_instr.minstr->opcode != X86_INS_JA || cmp_instr.minstr->opcode != X86_INS_CMP) continue;
                       for(auto &vs : instr.inputs){
                          if(vs->type == JVAR_MEMORY && (vs->value >= ro_start && vs->value < ro_end) && vs->index){ // *0x812b600(, %eax, 4)
                              jmp_offset = vs->value;
                              for(auto &vm: cmp_instr.inputs){    // cmp eax, 0x9
                                  if(vm->type == JVAR_REGISTER){
                                      if(vm->value == vs->index){ //reg in comparison is same as the index reg of jmp 
                                          matching_reg = true;
                                      }
                                  }
                                  else if(vm->type == JVAR_CONSTANT){
                                      upper_limit = vm->value;
                                  }
                              }
                              if(matching_reg && upper_limit) { 
                                  safeICT.insert(instr.pc);
                                  safeChecks++;
                                  break; 
                              } //found the combination, exit the loop
                          }
                      }//end: going through instruction inputs to find 
                   }//end: id-2>=0
                }//end: matched pattern

            }//end: indirect CTI
        }//end: loop over instrs

    }//end: loop over functions
    //STEP 2: for those functions, check for offsets or base addresses that might be pointing to jump table and add in a set 
    //STEP 3: scan .rodata and check for all 4 byte combinattions and see if any of them match it
}
void
generateCFIRule(JanusContext *jc)
{
    cout<<"GENERATING CFI RULES....."<<endl;
    ro_start =  jc->program.RODATA_offset;
    ro_end =  ro_start + jc->program.RODATA_size - 3;
    if(backward_cfi){
        enforceBackwardCFI(jc);
    }
    if(forward_cfi){
       analyse_jmptable_targets(jc);
       enforceForwardCFI(jc);
       dump_icf_targets(jc);
       dump_eSym(jc);
       //cout<<"Number of Safe Checks: "<<dec<<safeChecks<<endl;
    }
    //use liveness for rsi, rdi and rax around function calls
    if(jc->mode == JCFI_LIVE){
        analyze_leaf_functions(jc);
        analyze_call_sites(jc);
    }
    //attach null rules to remaining basic blocks, to indicate NOT to further processing dynamically
    if(null_rules)
      mark_null_rules(jc);
}
