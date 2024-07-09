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
#define BINARY_PIC 1
#define BINARY_NONPIC 2

static int count_rule=0;
static bool has_debug_info = false;
static bool null_rules = true;
static bool backward_cfi = true;
static bool forward_cfi = true;
PCAddress firstPC;
PCAddress lastPC;
PCAddress GOTAddress;
std::set<uintptr_t> ICF_targets;
std::set<uintptr_t> callback_targets;
std::set<uintptr_t> IJF_targets;//
std::set<uintptr_t> IJF_srcs;    //id of instruction
std::set<int> IJ_Func;     //id of function
#define PROLOGUE 0
#define INSTR_REG_SET 0xc6     //RCX, RDX, RSI, RDI
#define INSTR_REG_SET1 0xc0 //RDI, RSI
/*-----------------------Function Prototypes ----------------------*/
/* 32-bit code on 32 or 64 bit (eax = 32bit, rax = 64 bit)

  base registers: eax-edx, esp,ebp, esi, edi
  index registers: eax-edx, ebp, esi, edi
  64-bit code on 64-bit x86

  base: GPR  rax-rdx, rsp, rbp, edx, eecx, r8-r15
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
     if ((!func.entry && !func.instrs.size()) || func.isExternal || func.name == "_plt" || func.name == "_plt_got" || func.name == "_plt_sec") continue;
     if(lib_funcs.count(func.name)) continue;
//TODO: always save for indirect jumps in _plt_got and _plt. do this separately.
     bool ecx_written = false; 
     bool edx_written = false; 
     int save_ecx = 0; 
     int save_edx = 0; 
     //if function has subcalls, skip
     //if(func.subCalls.size()  || func.jumpCalls.size() ) continue;
     //cout<<"func: "<<func.name<<endl;
     if(!func.isLeaf())         continue;
     for(auto &instr: func.instrs){
        for(auto op : instr.outputs){
           if(op->type == JVAR_REGISTER){
                 if(op->value == JREG_ECX) //writes ECX, no need to save
                     ecx_written = true;
                 if(op->value == JREG_EDX) //writes EDX, no need to save
                     edx_written = true;
           }
        }
     }
     //as far as it is not written by an instr in the function, the caller will assume it does not need to be save.as for reading, we will still need to save as it could be alive in one basic block but not antother, so it will not be saved by the instrumentation at such instructions. so we need to make sure that we save nonetheless. in the worst case, we wll only be saving double.
     if(!edx_written) save_edx = 1;
     if(!ecx_written) save_ecx = 1;
     if(save_edx || save_ecx){
         //save
         Instruction* entry_instr = &(func.entry->instrs[0]);
         insert_security_rule(entry_instr, SAVE_AT_ENTRY,save_ecx, save_edx, 0,0);
         //restore
         for (auto retID : func.terminations) {
            BasicBlock &bb = func.blocks[retID];
            Instruction *exit_instr = bb.lastInstr();
            if (exit_instr->opcode == Instruction::Return) { //TODO: look for lonjmp as well
                insert_security_rule(exit_instr, RESTORE_AT_EXIT, save_ecx, save_edx, 0, 0);
            }
         }
     }
  }
}
static void analyze_call_sites(JanusContext *jc){
    int save_ecx, save_edx;
  for(auto &func: jc->functions){
     if ((!func.entry && !func.instrs.size()) ) continue;
     if(func.isLeaf()) continue; //already taken care of in the analyze_leaf_function
     //only save ecx and edx at the entry/exit of the function, if they have instructions instrumented AND they are not being written to. because, if they are being written to, the caller must have saved them.
     save_ecx = (func.writeSet.contains(JREG_ECX)) ? 0 : 1; 
     save_edx = (func.writeSet.contains(JREG_EDX)) ? 0 : 1;
     if(save_edx || save_ecx){
         //save
         Instruction* entry_instr = &(func.entry->instrs[0]);
         insert_security_rule(entry_instr, SAVE_AT_ENTRY,save_ecx, save_edx, 0,0);
         //restore
         for (auto retID : func.terminations) {
            BasicBlock &bb = func.blocks[retID];
            Instruction *exit_instr = bb.lastInstr();
            if (exit_instr->opcode == Instruction::Return) { //TODO: look for lonjmp as well
                insert_security_rule(exit_instr, RESTORE_AT_EXIT, save_ecx, save_edx, 0, 0);
            }
         }
     }
  }
}
static void readCodePointers(JanusContext *jc){
    // Open the file in binary mode
    uintptr_t ro_start =  jc->program.RODATA_offset;
    uintptr_t ro_end =  ro_start + jc->program.RODATA_size - 3;

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
         if (isValidInstrAddr(jc, address)){
             ICF_targets.insert(address);
             IJF_targets.insert(address);
             if(jc->functionMap.count(address) || isEndBranchAddr(jc, address))
                 callback_targets.insert(address);
         }
         else{
            int current_pos = file.tellg();
            //if it is rodata section
            if(current_pos >= ro_start && current_pos <= ro_end){
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
        //TODO: new change. we need to make sure these are also checked for external functions.
        //if ((!func.entry && !func.instrs.size())) continue;
        for(auto &bb: func.blocks){
            Instruction &instr = bb.instrs[bb.size-1];
            int id = instr.id;
            if(func.indirectCTIs.find(id) == func.indirectCTIs.end()) continue; //skip instructions which are not indirect calls
            bitmask_flags = (jc->mode == JCFI_LIVE) ? func.liveFlagIn[id].bits : 0x1;
            bitmask_reg =  (jc->mode == JCFI_LIVE) ? func.liveRegIn[id].bits : 0x0;
            RewriteRule rule;
            if(instr.opcode == Instruction::Call ){
                rule = RewriteRule(VERIFY_CALL_TARGET_INTRA, bb.instrs->pc, instr.pc, instr.id);
                rule.reg0 = bitmask_flags; //instrumentaiton routine modifies RDI, RCX, RDX, should always be in liveness as we dont know the target. 
                rule.reg1 = bitmask_reg;
                rule.reg2 = 0;
                rule.reg3 = 0;
                insertRule(0, rule, &bb);
            }
            else if(instr.opcode  == Instruction::DirectBranch || instr.opcode == Instruction::ConditionalBranch){
                IJ_Func.insert(func.fid);
                rule = RewriteRule(VERIFY_JMP_TARGET, bb.instrs->pc, instr.pc, instr.id);
                rule.reg0 = bitmask_flags; //instrumentaiton routine modifies RCX, RDX, should always be in liveness as we dont know the target.
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
     //if(jc->program.binaryType != BINARY_PIC) return;
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
    cout<<"FILENAME: "<<fname<<endl;
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
                    //if the call target is known, take a union of target function entry block liveness, else just save R1(DR_REG_RSI)&R2(DR_REG_RDI)
                    /*if()
                    else{*/
                        bitmask_reg |= INSTR_REG_SET1;
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
                        bitmask_reg |= INSTR_REG_SET1;
                        rule.reg0 = bitmask_flags;
                        rule.reg1 = bitmask_reg;
                        rule.reg2 = instr.minstr->size;
                        rule.reg3 = 0;
                        insertRule(0, rule, &bb);
                     }
                 }
                 if(func.name == "main") continue;

                 if(instr.opcode == Instruction::Return){
                   // if(instr.pc == 0x16aef) continue; //(ONLY for ld-so lazy symbol resolve)
                    bitmask_flags = (jc->mode == JCFI_LIVE) ? func.liveFlagIn[instr.id].bits : 0x1;
                    bitmask_reg =  (jc->mode == JCFI_LIVE) ? func.liveRegIn[instr.id].bits : 0x0;
                    if(jc->mode == JCFI_LIVE){
                         /*HACK: if a functions writes a reg but does not read it (after last write:?), add it to liveness set for return as we are modifying edi and esi    
                         1e887:       8b 3c 24                mov    (%esp),%edi
                         1e88a:       c3                      ret

                         */
                         /*uint32_t rdi_status = (func.writeSet.contains(JREG_EDI) && !func.readSet.contains(JREG_EDI)) << JREG_RDI;
                         uint32_t rsi_status = (func.writeSet.contains(JREG_ESI) && !func.readSet.contains(JREG_ESI)) << JREG_RSI;
                         bitmask_reg |= rdi_status;
                         bitmask_reg |= rsi_status;*/
                         /*auto callset = jc->callSet.find(func.startAddress);
                         if(callset != jc->callSet.end()){
                             if(instr.pc == (uintptr_t)0x118c){
                                 cout<<"bitmask_reg:"<<bitmask_reg<<endl;
                                 cout<<"func.startAddr: "<<hex<<func.startAddress<<endl;
                             }
                             for(auto bb_callee : callset->second){
                                BasicBlock* succ = bb_callee.succ1;
                                if(succ){ 
                                   Function* callee = succ->parentFunction; 
                                   bitmask_reg |= callee->liveRegIn[succ->startInstID].bits;
                                    //get parent function, then check success of all call blocks predecessors)
                                }
                             }
                         }
                         else{ //if callee is not found, we came to this function through indirect call. just save all registers (needed for low-level code)
                             bitmask_reg |= INSTR_REG_SET; 
                         }*/
                         if(lib_funcs.count(func.name)){
                             bitmask_reg |= INSTR_REG_SET;
                         }

                    }
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
         outfile<<hex<<sym.first<<endl;
     }
     for(auto &addr : callback_targets){
         outfile<<hex<<addr<<endl;
     } 
     outfile.close();
}
static void buildCallSet(JanusContext *jc){
    for(auto func : jc->functions){
        for(auto calls : func.calls){
            PCAddress targetAddr = calls.second->startAddress; //target function address
            Instruction instr = func.instrs[calls.first];
            BasicBlock bb = func.blocks[instr.block->bid];
            jc->callSet[targetAddr].emplace_back(bb);
        }
    }
}
void
generateCFIRule(JanusContext *jc)
{
    cout<<"GENERATING CFI RULES....."<<endl;
    //HACK: mark entry of main, to avoid accessing shadow memory set up before it has been set up
    mark_main_entry(jc);
    mark_main_exit(jc);
    
    if(jc->mode == JCFI_LIVE){
      // buildCallSet(jc);
    }
    if(backward_cfi){
        enforceBackwardCFI(jc);
    }
    if(forward_cfi){
       enforceForwardCFI(jc);
       dump_icf_targets(jc);
       dump_eSym(jc);
    }
    //use liveness for edx, ecx and rax around function calls
    if(jc->mode == JCFI_LIVE){
        cout<<"STEP 1"<<endl;
        analyze_leaf_functions(jc);
        cout<<"STEP 1 done"<<endl;
        //analyze_call_sites(jc);
    }
    //attach null rules to remaining basic blocks, to indicate NOT to further processing dynamically
    if(null_rules)
      mark_null_rules(jc);
}
