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
std::set<uintptr_t> ICF_targets;
std::set<uintptr_t> callback_targets;
std::set<uintptr_t> IJF_targets;//
std::set<uintptr_t> IJF_srcs;    //id of instruction
std::set<int> IJ_Func;     //id of function

#define PROLOGUE 0

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

    std::ifstream file(jc->name, std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "Error opening the file" << std::endl;
        return;
    }
     //set the stream to read binary
     file >> std::noskipws;

    int address;
    while (file.read(reinterpret_cast<char*>(&address), sizeof(address))) {
        // Display the hex address
         if (jc->functionMap.count(address) || isValidInstrAddr(jc, address)){
             ICF_targets.insert(address);
             IJF_targets.insert(address);
             if(isEndBranchAddr(jc, address))
                 callback_targets.insert(address);
         }
         else{//treat as an offset
             PCAddress offsetAddr= jc->program.codeStartAddr+address;
              if(jc->functionMap.count(offsetAddr) || isValidInstrAddr(jc, address)){
             //    ICF_targets.insert(offsetAddr);
                 IJF_targets.insert(offsetAddr);

              }
         }
        // Slide the cursor back by 3 bytes
        file.seekg(-3, std::ios::cur);
    }

    // Close the file
    file.close();

}
/*static bool isValidInstrAddr(JanusContext *jc,PCAddress addr){
    //1. if it is less than start address of any section or greater than return exit
    //2. check for each function
    if(addr < firstPC || addr > lastPC) return false;
    for(auto &func : jc->functions){
        PCAddress startAddr = func.startAddress;
        PCAddress endAddr = func.endAddress;
        if(addr >= startAddr && addr < endAddr){
            for(auto instr: func.instrs){
                 if(instr.pc == addr){ 
                     return true;
                 }
            }
        }

    }
    return false;
}*/
static bool isValidInstrAddr(JanusContext *jc,PCAddress addr){
    //1. if it is less than start address of any section or greater than return exit
    //2. check for each function
    if(addr < firstPC || addr > lastPC) return false;
    if(jc->instructionSet.find(addr) != jc->instructionSet.end()){
           return true; 
    }
    return false;
}

/*static bool isEndBranchAddr(JanusContext *jc,PCAddress addr){
    //1. if it is less than start address of any section or greater than return exit
    //2. check for each function
    if(addr < firstPC || addr > lastPC) return false;
    for(auto &func : jc->functions){
        PCAddress startAddr = func.startAddress;
        PCAddress endAddr = func.endAddress;
        if(addr >= startAddr && addr < endAddr){
            for(auto &instr: func.instrs){
                 if(instr.pc == addr){ 
                    if(instr.minstr->isEndBranch()){
                        return true;
                    } 
                    return false;
                 }
            }
        }

    }
    return false;
}*/
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
     //if(jc->program.binaryType != BINARY_PIC) return;
     PCAddress GOTAddress = jc->program.GOTAddress;
     for(auto &func: jc->functions){
         for(auto vs: func.allStates){
              if(vs->type == JVAR_CONSTANT || vs->type == JVAR_ABSOLUTE){
                 //offset will have negative value, so we make it GOTAddress - (negative val)
                 PCAddress addr = GOTAddress + (PCAddress)vs->value;
                 if(jc->functionMap.count(addr) || isEndBranchAddr(jc,addr)){
                     ICF_targets.insert(addr);
                     IJF_targets.insert(addr);
                     callback_targets.insert(addr);
                 }
                 else{ //if it is a valid instruction address, put it as indirect jmp tgt
                     if(isValidInstrAddr(jc, addr)){
                         //TODO: refine;
                         IJF_targets.insert(addr);
                     //    ICF_targets.insert(addr);
                     }
                 }
              } 
         }
     }
     //STEP 4: adresses being moved around in instructions using push etc too. (for codes 464 and 435) 

     //STEP 5: inspect .rodata for jmp table offsets.


}
void dump_icf_targets(JanusContext *jc){
    fstream outfile; 
     //indirect calls (same for ij)
    string fname=string(rs_dir + get_binfile_name(jc->name)+ "_ic.txt");
    cout<<"FILENAME: "<<fname<<endl;
    outfile.open(fname,std::ios::out | std::ios::trunc);
    outfile<<ICF_targets.size()<<endl;
    for(auto tgt : ICF_targets){
        outfile<<hex<<tgt<<endl;
    }
    outfile.close();
     
    //indirect jmps
    fname=string(rs_dir + get_binfile_name(jc->name)+ "_ij.txt");
    outfile.open(fname,std::ios::out | std::ios::trunc);
    /*for(auto &func : jc->functions){
        if(IJ_Func.find(func.fid) != IJ_Func.end()){
           for(auto instr: func.instrs){
             if(IJF_srcs.find(instr.pc) != IJF_srcs.end()){
                 outfile<<hex<<instr.pc<<" ";
             }
           }
           outfile<<endl;
           for( auto instr: func.instrs){
              outfile<<hex<<instr.pc<<endl;
           }
       outfile<<"\t"<<endl;
        }
    }*/
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
                    //insert_security_rule(&instr, SAVE_RETURN_TARGET, bitmask_flags, bitmask_reg,instr.minstr->size ,0);
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
                    //insert_security_rule(&instr, SAVE_RETURN_TARGET, bitmask_flags, bitmask_reg,instr.minstr->size ,0);
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
                        //insert_security_rule(&instr, SAVE_RETURN_TARGET, bitmask_flags, bitmask_reg,instr.minstr->size ,0);
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
                   // if(instr.pc == 0x16aef) continue; //(ONLY for ld-so lazy symbol resolve)
                    bitmask_flags = (jc->mode == JCFI_LIVE) ? func.liveFlagIn[instr.id].bits : 0x1;
                    bitmask_reg =  (jc->mode == JCFI_LIVE) ? func.liveRegIn[instr.id].bits : 0x0;
                    //insert_security_rule(&instr, SAVE_RETURN_TARGET, bitmask_flags, bitmask_reg,instr.minstr->size ,0);
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
        //3. check longjmps and exceptions
    }
}
static void check_address_taken(JanusContext *jc){
    //1. check for address ranges in the executable. (need to check if it is PIC or non-PIC)
    //need to check for Executable and the code sections. the address needs to be in the .text section and data segment, and startAddress and endAddress is known
    //2. check for address taken functions. first check if it is within code addresses, then check if is on instruction boundary.
    for(auto &func: jc->functions){
        auto &instrTable = func.minstrTable;
        /*  if(instrTable.find(target) != instrTable.end()){
              //instruction boundary
          }*/
    }
    //for GOT table entries, check  where the immediate value is a negative offset (m/-0x([0-9a-fA-F]+, captures the hexadecimal value of the negative offset. Calculating Absolute Address: It calculates the absolute address ($_addr) by subtracting the offset from the base address represented by $got_addr.
    //code pointer arithmetic for jump tables.
    //Jump table targets are intra-procedural: the ICF transfer instruction and ICF target are in the same function.
//• The target address is computed using simple arithmetic operations such as additions and multiplication.
// Other than one quantity that serves as an index, all other quantities involved in the computation are constants in the code or data segment.
//All of the computation takes place within a fixed size window of instructions, currently set to 50 instructions in our implementation
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
     //outfile<<jc->program.exportedSymbols.size()<<endl;
     for(auto &sym: jc->program.exportedSymbols){
         //outfile<<hex<<sym.first<<"\t"<<sym.second<<endl;
         //only dump address, skip symbols
         outfile<<hex<<sym.first<<endl;
     }
     for(auto &addr : callback_targets){
         outfile<<hex<<addr<<endl;
     } 
     outfile.close();
}
void
generateCFIRule(JanusContext *jc)
{
    cout<<"GENERATING CFI RULES....."<<endl;
    //for JCFI_NULL, mark all basic blocks with nulli(no-op) rules, to indicate no need to process it dynamically  
#if 0
    if(jc->mode == JCFI_NULL){
        mark_null_rules(jc);
        //HACK: to deal with the basic blocks not recognised in the elf
        mark_noop_blocks(jc); //to solve the issue of DR starting bb from noop sometimes.
        return;
    }
#endif
    //HACK: mark entry of main, to avoid accessing shadow memory set up before it has been set up
    mark_main_entry(jc);
    mark_main_exit(jc);
    if(backward_cfi){
        enforceBackwardCFI(jc);
    }
    if(forward_cfi){
       enforceForwardCFI(jc);
       dump_icf_targets(jc);
       //skip imported symbols, we only go by if the target is in exported symbol table
       //dump_iSym(jc);
       dump_eSym(jc);
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
