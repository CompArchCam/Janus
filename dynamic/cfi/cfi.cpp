/* JANUS Client for secure execution */

/* Header file to implement a JANUS client */
#include "janus_api.h"
#include "dr_api.h"
# include "drsyms.h"
#include <inttypes.h>
#include <iostream>
#include <cstring>
#include <fstream>
#include <sstream>
#include <fcntl.h>
#include <unistd.h>
#include<cstddef>
#include "elf.h"
#include "cfi.h"
#include "instrumentation.h"
#include "hashTable.h"
#include "symTable.h"
using namespace std;

#ifndef SELFMAG
#define SELFMAG 4
#endif
//execution mode
#define DYN_ONLY_MODE 0         
#define HYBRID_MODE 1           //default mode
#define STAT_ONLY_MODE 0
#define FORWARD_CFI 1           //enabled by default
#define BACKWARD_CFI 1          //enabled by default
#define CALCULATE_AIR 0
#define TAB_MAX_SIZE 1024
#define MAX_STR_LEN 256
#define MAX_MODULES_ALLOWED 16
# define MAX_SYM_RESULT 256
//#define KEYBASE 0x400000
//For 32-bit
#define KEYBASE 0x8048000
//#define ESYM_TABLE_SIZE 8192
#define ESYM_TABLE_SIZE 32768
//colour codes
#define BLUE "\e[34m"
#define RED "\e[31m"
#define BOLD "\e[1m"
#define GREEN "\e[32m"
#define RESET "\e[0m"

/*this version is implemented for 32-bit binaries. to work properly, we need t use 32-bit DR version, and compile Dynamic compoenent with -m32 flags. also, all the code bases will need to be adjusted to 32-bit base for non-PIC code*/ 
char name[MAX_SYM_RESULT];
char file[MAX_SYM_RESULT];
std::set<uintptr_t> exp_set[MAX_MODULES_ALLOWED];
std::map<uintptr_t, int> ret_targets;
bool ld_loaded = false;
int vdso_id=-1;
const char* main_module;
int main_id= -1;
uint64_t error_counter=0;
uintptr_t ld_addr;
app_pc orig_main;
stack_thread_t *local;
const char *app_name;

static dr_emit_flags_t event_basic_block(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating);
static void generate_security_events(JANUS_CONTEXT);
static void generate_events_by_rule(JANUS_CONTEXT, instr_t *instr);

stack_thread_t *shadowstack;
static void verify_return_target(JANUS_CONTEXT, instr_t* trigger, uint64_t bitmask_flags, uint64_t bitmask_reg);
static void store_return_target(JANUS_CONTEXT, instr_t* trigger, uint64_t bitmask_flags, uint64_t bitmask_reg);
static void verify_jmp_target(JANUS_CONTEXT, instr_t* trigger, uint64_t bitmask_flags, uint64_t bitmask_reg, opnd_t target_opnd, int id);
static void verify_call_target(JANUS_CONTEXT, instr_t* trigger, uint64_t bitmask_flags, uint64_t bitmask_reg, opnd_t target_opnd, int id);
static void unwind_longjmp(JANUS_CONTEXT, instr_t* trigger, uint64_t bitmask_flags, uint64_t bitmask_reg);

static void loadSymbolsAndHashTables(const char * binName, int id, uintptr_t base);
static int  getmodulo2size(int size);
typedef struct{
    uint32_t key_count;
    uint32_t key_lookup;
}h_metadata;
#define ICF_MAX_ENTRIES 10000
std::set<uintptr_t> icalls[MAX_MODULES_ALLOWED];
std::set<uintptr_t> ijmps[MAX_MODULES_ALLOWED];
std::set<uintptr_t> rets[MAX_MODULES_ALLOWED];
int nr_icalls[MAX_MODULES_ALLOWED];
int nr_ijmps[MAX_MODULES_ALLOWED];
int nr_ret[MAX_MODULES_ALLOWED];
typedef struct {
    uintptr_t start;
    uintptr_t end;
} PLTsection;
PLTsection PLT[MAX_MODULES_ALLOWED];
HashTable hashTable_IC[MAX_MODULES_ALLOWED];            //indirect call
HashTable hashTable_esym;            //indirect call
HashTable hashTable_IJ[MAX_MODULES_ALLOWED];            //indirect jump targets which are func entries
h_metadata histo_esym[ESYM_TABLE_SIZE];
h_metadata* histo_IC[MAX_MODULES_ALLOWED];
h_metadata* histo_IJ[MAX_MODULES_ALLOWED];
ElfData elf_data[MAX_MODULES_ALLOWED]; 
uint64_t counter[MAX_MODULES_ALLOWED] = {0};
#define R1 DR_REG_XDI
#define R2 DR_REG_XSI
#define R3 DR_REG_XCX
#define R4 DR_REG_XDX
#define OFFSET_STACK_TOP offsetof(stack_thread_t, top)
/*------------------------------------------------------------------------------*/
/*------------------------------ Utility Routines ------------------------------*/
/*------------------------------------------------------------------------------*/
char* get_binfile_name(string filepath){
    // Make a copy of the string to avoid modifying const data
    char* filepathCopy = new char[filepath.length() + 1];
    strcpy(filepathCopy, filepath.c_str());

    // Returns first token
    char* token = strtok(filepathCopy, "/");

    char *filename = token;
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
unsigned long get_code_size(const char *file_path) {
    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    // Read ELF header
    Elf32_Ehdr ehdr;
    if (read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
        perror("Failed to read ELF header");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Check ELF magic number
    if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0) {
        fprintf(stderr, "Not an ELF file\n");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Seek to section header table
    if (lseek(fd, ehdr.e_shoff, SEEK_SET) < 0) {
        perror("Failed to seek to section header table");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Read section headers
    Elf32_Shdr *shdrs = (Elf32_Shdr *)malloc(ehdr.e_shnum * sizeof(Elf32_Shdr));
    if (read(fd, shdrs, ehdr.e_shnum * sizeof(Elf32_Shdr)) != ehdr.e_shnum * sizeof(Elf32_Shdr)) {
        perror("Failed to read section headers");
        free(shdrs);
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Seek to section header string table
    if (lseek(fd, shdrs[ehdr.e_shstrndx].sh_offset, SEEK_SET) < 0) {
        perror("Failed to seek to section header string table");
        free(shdrs);
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Read section header string table
    char *shstrtab = (char*)malloc(shdrs[ehdr.e_shstrndx].sh_size);
    if (read(fd, shstrtab, shdrs[ehdr.e_shstrndx].sh_size) != shdrs[ehdr.e_shstrndx].sh_size) {
        perror("Failed to read section header string table");
        free(shstrtab);
        free(shdrs);
        close(fd);
        exit(EXIT_FAILURE);
    }
    const char *target_sections[] = {".text", ".init", ".fini", ".plt"};
    int nr_sections = sizeof(target_sections)/sizeof(target_sections[0]);
    unsigned long combined_size = 0;
    // Iterate through section headers to find the target section
    for (int i = 0; i < ehdr.e_shnum; i++) {
        for(int j= 0; j< nr_sections;  j++){
            if (strcmp(&shstrtab[shdrs[i].sh_name], target_sections[j]) == 0) {
                combined_size += shdrs[i].sh_size;
                break;
            }
        }
    }
    free(shstrtab);
    free(shdrs);
    close(fd);
    return combined_size;
}

/*------------------------------------------------------------------------------*/
/*-----------------Instrumentation Call Back Routines---------------------------*/
/*------------------------------------------------------------------------------*/
static void verify_jmp_target(JANUS_CONTEXT, instr_t *instr,uint64_t bitmask_flag, uint64_t bitmask_reg, opnd_t target_opnd, int id){

    instr_t *meta_instr;
    instr_t *restore_label, *restore_lookup_label;
    restore_label = INSTR_CREATE_label(drcontext);
    //32-bit C calling convention is to save EAX, ECX, EDX
    dr_save_reg(drcontext, bb, instr,R3, SPILL_SLOT_2); //RCX, caller-saved
    dr_save_reg(drcontext, bb, instr,R4, SPILL_SLOT_3); //RDX, caller-saved
    //32-bit C calling convention is to push arguments on stack
    //need to push target operand before saving flags, as it may clobber eax register which is used for address calculation  
    meta_instr = INSTR_CREATE_push(drcontext,target_opnd);
    instrlist_meta_preinsert(bb, instr,meta_instr);
   if(bitmask_flag)
        dr_save_arith_flags(drcontext, bb, instr, SPILL_SLOT_5);
    
    
    meta_instr = INSTR_CREATE_push_imm(drcontext, OPND_CREATE_INT32(&hashTable_IJ[id]));
    instrlist_meta_preinsert(bb, instr,meta_instr);
    
    //save rax which stores arith flags, before call
    dr_save_reg(drcontext, bb, instr,DR_REG_XAX, SPILL_SLOT_6);
    
    meta_instr = INSTR_CREATE_call(drcontext,opnd_create_pc((byte *)&lookupAddr));
    instrlist_meta_preinsert(bb, instr, meta_instr);

    meta_instr = INSTR_CREATE_add(drcontext,opnd_create_reg(DR_REG_XSP), OPND_CREATE_INT32(8));
    instrlist_meta_preinsert(bb, instr, meta_instr);

    meta_instr = INSTR_CREATE_cmp(drcontext,opnd_create_reg(DR_REG_XAX),OPND_CREATE_INT32(0));
    instrlist_meta_preinsert(bb, instr,meta_instr); 

    
    meta_instr = INSTR_CREATE_jcc(drcontext,OP_jnz,opnd_create_instr(restore_label));
    instrlist_meta_preinsert(bb, instr, meta_instr);
    
    //dr_insert_clean_call(drcontext, bb, instr, (void *)print_jmp_error, false, 2, OPND_CREATE_INT32(instr_get_app_pc(instr)), target_opnd);
    
    
    meta_instr = INSTR_CREATE_inc(drcontext,OPND_CREATE_ABSMEM((byte *)&error_counter, OPSZ_4));
    instrlist_meta_preinsert(bb, instr, meta_instr);
    
    //restore registers and flags
    instrlist_meta_preinsert(bb, instr, restore_label);
    //restore rax which stores arith flags, after call
    dr_restore_reg(drcontext, bb, instr,DR_REG_XAX, SPILL_SLOT_6);
    if(bitmask_flag)
        dr_restore_arith_flags(drcontext, bb, instr, SPILL_SLOT_5);
    dr_restore_reg(drcontext, bb, instr,R4, SPILL_SLOT_3);
    dr_restore_reg(drcontext, bb, instr,R3, SPILL_SLOT_2);
}
static void verify_call_target(JANUS_CONTEXT, instr_t *instr,uint64_t bitmask_flag, uint64_t bitmask_reg, opnd_t target_opnd, int id){

    instr_t *meta_instr;
    instr_t *restore_label, *restore_lookup_label, *error_label;
    restore_label = INSTR_CREATE_label(drcontext);
    error_label = INSTR_CREATE_label(drcontext);

    dr_save_reg(drcontext, bb, instr,R1, SPILL_SLOT_2); //RDI, for temporary data hold
    dr_save_reg(drcontext, bb, instr,R3, SPILL_SLOT_3); //RCX, caller-saved
    dr_save_reg(drcontext, bb, instr,R4, SPILL_SLOT_4); //RDX, caller-saved
   //load target operand value before saving arith flags 
    meta_instr = INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(R1), target_opnd);
    instrlist_meta_preinsert(bb, instr,meta_instr);
    if(bitmask_flag)
        dr_save_arith_flags(drcontext, bb, instr, SPILL_SLOT_5);
    
    //first check in IC
    meta_instr = INSTR_CREATE_push(drcontext, opnd_create_reg(R1));
    instrlist_meta_preinsert(bb, instr,meta_instr);
    
    
    meta_instr = INSTR_CREATE_push_imm(drcontext, OPND_CREATE_INT32(&hashTable_IC[id]));
    instrlist_meta_preinsert(bb, instr,meta_instr);
    
    //save rax which stores arith flags, before call
    dr_save_reg(drcontext, bb, instr,DR_REG_XAX, SPILL_SLOT_6);
    
    meta_instr = INSTR_CREATE_call(drcontext,opnd_create_pc((byte *)&lookupAddr));
    instrlist_meta_preinsert(bb, instr, meta_instr);

    //restore stack
    meta_instr = INSTR_CREATE_add(drcontext,opnd_create_reg(DR_REG_XSP), OPND_CREATE_INT32(8));
    instrlist_meta_preinsert(bb, instr, meta_instr);

    meta_instr = INSTR_CREATE_cmp(drcontext,opnd_create_reg(DR_REG_XAX),OPND_CREATE_INT32(0));
    instrlist_meta_preinsert(bb, instr,meta_instr);

    meta_instr = INSTR_CREATE_jcc(drcontext,OP_jnz,opnd_create_instr(restore_label));
    instrlist_meta_preinsert(bb, instr, meta_instr);
    
    meta_instr = INSTR_CREATE_push(drcontext, opnd_create_reg(R1));
    instrlist_meta_preinsert(bb, instr,meta_instr);
    
    meta_instr = INSTR_CREATE_push_imm(drcontext, OPND_CREATE_INT32(&hashTable_esym));
    instrlist_meta_preinsert(bb, instr,meta_instr);
    
    meta_instr = INSTR_CREATE_call(drcontext,opnd_create_pc((byte *)&lookupAddr));
    instrlist_meta_preinsert(bb, instr, meta_instr);

    //restore stack after second call
    meta_instr = INSTR_CREATE_add(drcontext,opnd_create_reg(DR_REG_XSP), OPND_CREATE_INT32(8));
    instrlist_meta_preinsert(bb, instr, meta_instr);

    meta_instr = INSTR_CREATE_cmp(drcontext,opnd_create_reg(DR_REG_XAX),OPND_CREATE_INT32(0));
    instrlist_meta_preinsert(bb, instr,meta_instr);
    
    meta_instr = INSTR_CREATE_jcc(drcontext,OP_jnz,opnd_create_instr(restore_label));
    instrlist_meta_preinsert(bb, instr, meta_instr);

   // dr_insert_clean_call(drcontext, bb, instr, (void *)print_call_error, false, 2, OPND_CREATE_INT32(instr_get_app_pc(instr)), opnd_create_reg(R1));

    meta_instr = INSTR_CREATE_inc(drcontext,OPND_CREATE_ABSMEM((byte *)&error_counter, OPSZ_4));
    instrlist_meta_preinsert(bb, instr, meta_instr);
    
    //restore registers and flags
    instrlist_meta_preinsert(bb, instr, restore_label);
    //restore rax before restoring arith flags, as XAX would have been clobbered by calls.
    dr_restore_reg(drcontext, bb, instr,DR_REG_XAX, SPILL_SLOT_6);
    if(bitmask_flag)
        dr_restore_arith_flags(drcontext, bb, instr, SPILL_SLOT_5);
    dr_restore_reg(drcontext, bb, instr,R4, SPILL_SLOT_4);
    dr_restore_reg(drcontext, bb, instr,R3, SPILL_SLOT_3);
    dr_restore_reg(drcontext, bb, instr,R1, SPILL_SLOT_2);
}
static void verify_return_target(JANUS_CONTEXT, instr_t *instr,uint64_t bitmask_flag, uint64_t bitmask_reg){
    instr_t *meta_instr;
    instr_t *restore_label, *unwind_label, *error_label;
    restore_label = INSTR_CREATE_label(drcontext);
    unwind_label = INSTR_CREATE_label(drcontext);
    error_label = INSTR_CREATE_label(drcontext);
    /*** save context*****/
    dr_save_reg(drcontext, bb, instr,R1, SPILL_SLOT_2);
    dr_save_reg(drcontext, bb, instr, R2, SPILL_SLOT_3);
    dr_save_reg(drcontext, bb, instr, R3, SPILL_SLOT_4);
    dr_save_reg(drcontext, bb, instr, R4, SPILL_SLOT_6);
    if(bitmask_flag)
        dr_save_arith_flags(drcontext, bb, instr, SPILL_SLOT_5);

    /*** store value in stack ***/
    //1. load value at local in R1, this value would be the memory address of local->stack.
    PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_imm(drcontext,
                opnd_create_reg(R1),
                    OPND_CREATE_INT32(&(local->stack))
                       ));
    //2. load the value of top in R2
    PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_ld(drcontext,
                    opnd_create_reg(R2),
                         OPND_CREATE_MEM32(R1, OFFSET_STACK_TOP)
                        ));
    //laod return address at [rsp] at R3
    PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_ld(drcontext,
                    opnd_create_reg(R3),
                             OPND_CREATE_MEM32(DR_REG_XSP, 0)));
    //unwind:
    instrlist_meta_preinsert(bb, instr, unwind_label);
    //compare if top == 0 (stack empty)
    meta_instr = INSTR_CREATE_cmp(drcontext,opnd_create_reg(R2),OPND_CREATE_INT32(0));
    instrlist_meta_preinsert(bb, instr,meta_instr);


    meta_instr = INSTR_CREATE_jcc(drcontext,OP_jz,opnd_create_instr(error_label));
    instrlist_meta_preinsert(bb, instr, meta_instr);

    //top--,
    meta_instr = XINST_CREATE_sub_s(drcontext,opnd_create_reg(R2),OPND_CREATE_INT32(1));
    instrlist_meta_preinsert(bb, instr, meta_instr);


    meta_instr = INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(R4), opnd_create_base_disp( R1, R2, 4, 0,OPSZ_4));
    instrlist_meta_preinsert(bb, instr, meta_instr);

    
    //compare [rsp] with stack[top+4], if not equal, then jump to loop
    meta_instr = INSTR_CREATE_cmp(drcontext,opnd_create_reg(R3),opnd_create_reg(R4));
    instrlist_meta_preinsert(bb, instr,meta_instr);
    
    meta_instr = INSTR_CREATE_jcc(drcontext,OP_jne,opnd_create_instr(unwind_label));
    instrlist_meta_preinsert(bb, instr, meta_instr);
    
    meta_instr = INSTR_CREATE_jmp(drcontext, opnd_create_instr(restore_label));
    instrlist_meta_preinsert(bb, instr, meta_instr);


    instrlist_meta_preinsert(bb, instr, error_label);
   //  increment error counter
    meta_instr = INSTR_CREATE_inc(drcontext,OPND_CREATE_ABSMEM((byte *)&error_counter, OPSZ_4));
    instrlist_meta_preinsert(bb, instr, meta_instr);
    

    //insert label for flag/reg restore instructions
    instrlist_meta_preinsert(bb, instr, restore_label);
    
    //restore value of top
    PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_st(drcontext,
                               OPND_CREATE_MEM32(R1, OFFSET_STACK_TOP),
                                opnd_create_reg(R2)));


    if(bitmask_flag)
        dr_restore_arith_flags(drcontext, bb, instr, SPILL_SLOT_5);
    dr_restore_reg(drcontext, bb, instr, R4, SPILL_SLOT_6);
    dr_restore_reg(drcontext, bb, instr, R3, SPILL_SLOT_4);
    dr_restore_reg(drcontext, bb, instr, R2, SPILL_SLOT_3);
    dr_restore_reg(drcontext, bb, instr, R1, SPILL_SLOT_2);

}
static void store_return_target(JANUS_CONTEXT, instr_t *instr,uint64_t bitmask_flag, uint64_t bitmask_reg){
    instr_t *meta_instr;
    app_pc instr_pc = instr_get_app_pc(instr);
    app_pc ret_pc = instr_pc + instr_length(drcontext, instr);
    instr_t *restore_label, *error_label;
    /*--------- saving RSI/RDI registers -----------------*/
    dr_save_reg(drcontext, bb, instr, R1, SPILL_SLOT_2);
    dr_save_reg(drcontext, bb, instr, R2, SPILL_SLOT_3);
   
    restore_label = INSTR_CREATE_label(drcontext);
    error_label = INSTR_CREATE_label(drcontext);

    //1. load value at local in R1, this value would be the memory address of local->stack.
    PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_imm(drcontext,
                opnd_create_reg(R1),
                    OPND_CREATE_INT32((&local->stack)))
                       );
    //2. load the value of top in R2
    PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_ld(drcontext,
                    opnd_create_reg(R2),
                         OPND_CREATE_MEM32(R1, OFFSET_STACK_TOP)
                        ));
    //2. store the value of instr_pc+ size into local->stack[local->top]
    PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_st(drcontext,
                         opnd_create_base_disp(R1, R2, 4, 0, OPSZ_4),
                            OPND_CREATE_INT32(ret_pc)
                            ));
    //increment top (without affecting status flags)
    meta_instr = XINST_CREATE_add(drcontext,opnd_create_reg(R2), OPND_CREATE_INT32(1));
    instrlist_meta_preinsert(bb, instr, meta_instr);
    
    //2. store the incremented value of top
    PRE_INSERT(bb, instr,
        INSTR_CREATE_mov_st(drcontext,
                            OPND_CREATE_MEM32(R1, OFFSET_STACK_TOP),
                            opnd_create_reg(R2)
                        ));
    
        
    //insert label for flag/reg restore instructions
    instrlist_meta_preinsert(bb, instr, restore_label);
    dr_restore_reg(drcontext, bb, instr, R1, SPILL_SLOT_2);
    dr_restore_reg(drcontext, bb, instr, R2, SPILL_SLOT_3);
}
//TODO: for dynamic only version, we need to check the return address, and then check if the difference between it and the top of the stack address is <= 8
/*------------------------------------------------------------------------------*/
/*----------------------  Symbol and HashTable Routines-------------------------*/
/*------------------------------------------------------------------------------*/
static bool addr_in_code_section(uintptr_t address, ElfData& my_elf){
   if((address >= my_elf.text_start && address < my_elf.text_end) || (address >= my_elf.init_start && address < my_elf.init_end) && (address >= my_elf.fini_start && address < my_elf.fini_end) || (address >= my_elf.plt_start && address < my_elf.plt_end)){
        return true;
   } 
   return false;
}
static void loadSymbolsAndHashTables(const char * binName, int id, uintptr_t base){

    //STEP 1: read symbols from ELF

    std::ifstream binFile(binName, std::ios::binary | std::ios::ate);

    if (!binFile.is_open()) {
        std::cerr << "Error opening the file: " <<binName<< std::endl;
        return;
    }
    uint32_t fileSize = binFile.tellg();

    cerr<<"Reading file \""<<binName<<"\" size: "<<fileSize<<" bytes."<<endl;

    uint8_t *filebuffer = new uint8_t[fileSize + 2048];
    binFile.seekg (0, ios::beg);
    binFile.read((char *)filebuffer,fileSize);
    if(strncmp((char *)filebuffer,ELFMAG,4)==0) {
        switch(filebuffer[EI_CLASS]) {
            case ELFCLASS32:
                parseELF32(filebuffer, &elf_data[id]);
            break;
            case ELFCLASS64:
                parseELF64(filebuffer);
            break;
            default:
            break;
        }
    }
    ElfData &my_elf = elf_data[id];
    PLT[id].start = my_elf.plt_start;
    PLT[id].end = my_elf.plt_end;

    //STEP 2: read binary addresses and build hashtables

     binFile.seekg (0, ios::beg);
     //set the stream to read binary
     binFile >> std::noskipws;
    std::set<uintptr_t> ic_set, ij_set;
    uintptr_t address;
    while (binFile.read(reinterpret_cast<char*>(&address), sizeof(address))) {
        // Display the hex address
         if (my_elf.symbols.count(address)){
              ic_set.insert(base+address);
              ij_set.insert(base+address);
              insert(&hashTable_esym, base+address, 1);

         }
         else{
             if(addr_in_code_section(address, my_elf)){
                  ij_set.insert(base+address);
             }
             else{
                 int current_pos = binFile.tellg();
                 if(current_pos >= (my_elf.ro_start) && current_pos <= (my_elf.ro_end)){
                      uintptr_t offsetAddr= address + my_elf.GOTAddress;
                      if(addr_in_code_section(offsetAddr, my_elf)){
                          ij_set.insert(base+address);
                      }
                }
                else{
                      uintptr_t offsetAddr= address + my_elf.GOTAddress;
                      if(addr_in_code_section(offsetAddr, my_elf)){
                          ij_set.insert(base+address);
                      }

                }
            }

         }
        // Slide the cursor back by 3 bytes
        binFile.seekg(-3, std::ios::cur);
    }
    //populate hash tables
    uint32_t size;
    //IC table
    size = getmodulo2size(ic_set.size());
    initHashTable(&hashTable_IC[id], size);
    for(auto addr: ic_set){
        insert(&hashTable_IC[id], addr, 1);
    }
    //IJ table
    size = getmodulo2size(ij_set.size());
    initHashTable(&hashTable_IJ[id], size);
    for(auto addr: ij_set){
        insert(&hashTable_IJ[id], addr, 1);
    }
    //ESYM table
    if(ld_loaded){
         insert(&hashTable_IJ[id], ld_addr, 1);
     }
    // Close the file
    binFile.close();
}
static int  getmodulo2size(int size){
    if(size & size - 1){
        //size is not module 2, find the next closest module 2
        unsigned int position = 0;
        int x = size;
        x |= (x >> 1);
        x |= (x >> 2);
        x |= (x >> 4);
        x |= (x >> 8);
        x |= (x >> 16);
        position = x + 1;
        return position*2;
    } 
    return size*2;
}

static void loadHashTables(const char * binname, int id, uintptr_t base_offset){
    int size;
    std::ifstream infile;
    int hash = 0;
    char  base_name[MAX_STR_LEN];
    char  ic_target[MAX_STR_LEN];
    char  ij_target[MAX_STR_LEN];
    char  isym_file[MAX_STR_LEN];
    char  esym_file[MAX_STR_LEN];

    strcpy(base_name,rs_dir);
    strcat(base_name,binname);

    strcpy(ic_target,base_name);
    strcat(ic_target,"_ic.txt");

    strcpy(ij_target,base_name);
    strcat(ij_target,"_ij.txt");

    strcpy(esym_file,base_name);
    strcat(esym_file,"_esym.txt");
    std::string str;
    uintptr_t addr;
  /*indirect call target*/
    infile.open(ic_target, std::ios::in);
    std::getline(infile, str);
    if(!str.empty()){
        size = std::stoul(str);
        //make it a module 2 size
        size = getmodulo2size(size);
        initHashTable(&hashTable_IC[id], size);
        while (std::getline(infile, str)) {
          std::istringstream iss(str);
          iss >>std::hex>>addr;
          addr += base_offset;
          insert(&hashTable_IC[id], addr,1);
        }
    }
    infile.close();

    /*indirect jump target*/
    infile.open(ij_target, std::ios::in);
    std::getline(infile, str);
    if(!str.empty()){
        size = std::stoul(str);
        size = size + 1; //for adding loader symbol
        size = getmodulo2size(size);
        initHashTable(&hashTable_IJ[id], size);
        while (std::getline(infile, str)) {
          std::istringstream iss(str);
          iss >>std::hex>>addr;
          addr += base_offset;
          insert(&hashTable_IJ[id], addr, 1);
        }
        insert(&hashTable_IJ[id], addr, 1);
        if(ld_loaded){
             insert(&hashTable_IJ[id], ld_addr, 1);
        }
    }
    infile.close();
  //TODO: for dynamically loaded libraries.  
  uint32_t exp_count= 0; 
    infile.open(esym_file, std::ios::in);
    while (std::getline(infile, str)) {
        std::istringstream iss(str);
        iss >> std::hex >> addr;
        addr +=  base_offset;
       insert(&hashTable_esym, addr,1);
    }
    infile.close();
}
/*------------------------------------------------------------------------------*/
/*----------------------  Translation Analysis Routines-------------------------*/
/*------------------------------------------------------------------------------*/
static void
generate_dynamic_events(JANUS_CONTEXT){


    instr_t     *instr, *last;
    app_pc      current_pc, first_pc, call_pc;

    opnd_t src, dest;
    int num_srcs, num_dsts, i;
    last = instrlist_last_app(bb);
    int matched = -1, id;
    opnd_t target_opnd;
    char * module_name;
    uintptr_t addr;
#if FORWARD_CFI
    if(instr_is_call_indirect(last)){
        target_opnd = instr_get_target(last);
        for (id = 0; id < nmodules; ++id) {
            if (dr_module_contains_addr(loaded_modules[id],instr_get_app_pc(last))) {
                matched = id;
                break;
            }
        }
        if(matched == -1){
            cout<<"ERROR: instr  not in loaded modules"<<endl;
            return;
        }
#if CALCULATE_AIR
        addr = (uintptr_t)instr_get_app_pc(last);
        if(!icalls[matched].count(addr)){
               icalls[matched].insert(addr);
               nr_icalls[matched]++;
        }
#endif
        verify_call_target(janus_context, last,  1/*bitmask_flags*/, 0/*bitmask_reg*/, target_opnd, matched);
    }
    int opcode = instr_get_opcode(last);
    if(opcode == OP_jmp_ind || opcode == OP_jmp_far_ind)
    {
        target_opnd = instr_get_target(last);
        app_pc icf_pc = instr_get_app_pc(last);
        for (id = 0; id < nmodules; ++id) {
            if (dr_module_contains_addr(loaded_modules[id],instr_get_app_pc(last))) {
                matched = id;
                break;
            }
        }
        if(matched == -1){
            cout<<"ERROR: instr  not in loaded modules"<<endl;
            return;
        }
/*#if CALCULATE_AIR
       addr = (uintptr_t)instr_get_app_pc(last);
        if(!ijmps[matched].count(addr)){
               ijmps[matched].insert(addr);
               nr_ijmps[matched]++;
        }
#endif*/
        //special case for plt jmps - target set is cross-module callbacks or exported symbol addresses
        if((uintptr_t)icf_pc >= PLT[matched].start && (uintptr_t)icf_pc < PLT[matched].end){
        //    verify_call_target(janus_context, last,  1/*bitmask_flags*/, 0/*bitmask_reg*/, target_opnd, matched);
           return;
        }
        else{
#if CALCULATE_AIR
            if(!ijmps[matched].count((uintptr_t)icf_pc)){
                   ijmps[matched].insert((uintptr_t)icf_pc);
                   nr_ijmps[matched]++;
            }
#endif
            verify_jmp_target(janus_context, last,  1/*bitmask_flags*/, 0/*bitmask_reg*/, target_opnd, matched);
            return;
        }
   }

#endif //end FORWARD_CFI 
    #if BACKWARD_CFI
    if(instr_is_call(last)){
        store_return_target(janus_context, last,  1/*bitmask_flags*/, 0/*bitmask_reg*/);

    }
    else if(instr_is_return(last)){
#if CALCULATE_AIR
        for (id = 0; id < nmodules; ++id) {
            if (dr_module_contains_addr(loaded_modules[id],instr_get_app_pc(last))) {
                matched = id;
                break;
            }
        }
        if(matched == -1){
            cout<<"ERROR: instr  not in loaded modules"<<endl;
            return;
        }
       addr = (uintptr_t)instr_get_app_pc(last);
        if(!rets[matched].count(addr)){
               rets[matched].insert(addr);
               nr_ret[matched]++;
        }
#endif
         verify_return_target(janus_context, last,  1/*bitmask_flags*/, 0/*bitmask_reg*/);
    }
#endif
    return;
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
    opnd_t src_opnd, target_opnd;
    int id = -1;
    int matched = -1;
    uintptr_t addr;
    switch (rule_opcode) {
#if BACKWARD_CFI
	case SAVE_RETURN_TARGET:
                store_return_target(janus_context, trigger,  1/*bitmask_flags*/,  0/*bitmask_reg*/);
	break;
	case VERIFY_RETURN_TARGET:
#if CALCULATE_AIR
            for (id = 0; id < nmodules; ++id) {
                if (dr_module_contains_addr(loaded_modules[id],instr_get_app_pc(instr))) {
                    matched = id;
                    break;
                }
            }
            if(matched != -1){
                addr = (uintptr_t)instr_get_app_pc(instr);
                if(!rets[matched].count(addr)){
                       rets[matched].insert(addr);
                       nr_ret[matched]++;
                }
            }
#endif
            verify_return_target(janus_context, trigger,  1/*bitmask_flags*/, 0/*bitmask_reg*/);
	break;
#endif
#if 0
	case HANDLE_CXX_EX:
            unwind_stack_cxx(janus_context, trigger, flag_live_on ? rule->reg0 : 1/*bitmask_flags*/, reg_live_on ? rule->reg1 : 0/*bitmask_reg*/);
	break;
#endif
#if FORWARD_CFI
       case VERIFY_CALL_TARGET_INTRA:
            target_opnd = instr_get_target(instr);
            for (id = 0; id < nmodules; ++id) {
                if (dr_module_contains_addr(loaded_modules[id],instr_get_app_pc(instr))) {
                    matched = id;
                    break;
                }
            }
            if(matched == -1) return;
#if CALCULATE_AIR
            addr = (uintptr_t)instr_get_app_pc(instr);
            if(!icalls[matched].count(addr)){
                   icalls[matched].insert(addr);
                   nr_icalls[matched]++;
            }
#endif
            verify_call_target(janus_context, trigger,  1/*bitmask_flags*/, 0/*bitmask_reg*/, target_opnd, matched);

        break;
        case VERIFY_JMP_TARGET:
            target_opnd = instr_get_target(instr);
            for (id = 0; id < nmodules; ++id) {
                if (dr_module_contains_addr(loaded_modules[id],instr_get_app_pc(instr))) {
                    matched = id;
                    break;
                }
            }
            if(matched == -1) return;
#if CALCULATE_AIR
            addr = (uintptr_t)instr_get_app_pc(instr);
            if(!ijmps[matched].count(addr)){
                   ijmps[matched].insert(addr);
                   nr_ijmps[matched]++;
            }
#endif
            verify_jmp_target(janus_context, trigger,  1/*bitmask_flags*/, 0/*bitmask_reg*/, target_opnd, matched);
	break;
#endif //FORWARD_CFI

        default:
                //fprintf(stderr,"In basic block 0x%lx static rule not recognised %d\n",bbAddr,rule_opcode);
            break;
        }
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
/*------------------------------------------------------------------------------*/
/*----------------------  DR Event Call Back Routines---------------------------*/
/*------------------------------------------------------------------------------*/
static void
exit_summary() {
#if CALCULATE_AIR
      cout<<"main_module:"<<main_module<<endl;
      char  binfilepath[MAX_STR_LEN];
      strcpy(binfilepath,rs_dir);
      strcat(binfilepath,app_name);
      strcat(binfilepath,"_AIR.txt");
      printf("binfilepath: %s\n", binfilepath);
      std::ofstream airfile(binfilepath);
      double dair = 0; /* dynamic average indirect target reduction */
      unsigned long n = 0; /* counter for indirect target instructions */
      unsigned long s = 0; /* total nr of valid indirect target destinations */

      double dair_icall = 0;
      double dair_ijmp = 0;
      double dair_ret = 0;
      unsigned long n_icall = 0;
      unsigned long n_ijmp = 0;
      unsigned long n_ret = 0;

      unsigned long nr_valid_icall_targets = 0;
      unsigned long nr_valid_ijmp_targets = 0;
      unsigned long nr_valid_ret_targets = 0;
    for(int m =0; m < nmodules; m++){
        if(strcmp(dr_module_preferred_name(loaded_modules[m]), "libdynamorio.so") == 0 || strcmp(dr_module_preferred_name(loaded_modules[m]), "libjcfi.so") == 0) continue;
        unsigned long i = 0;
        double dair_dso = 0; /* dynamic average indirect target reduction for the current dso */
        unsigned long n_dso = 0; /* counter for indirect target instructions for the current dso */

        double dair_dso_icall = 0;
        double dair_dso_ijmp = 0;
        double dair_dso_ret = 0;

        unsigned long n_dso_icall = 0;
        unsigned long n_dso_ijmp = 0;
        unsigned long n_dso_ret = 0;

        unsigned long nr_dso_valid_icall_targets = 0;
        unsigned long nr_dso_valid_ijmp_targets = 0;
        unsigned long nr_dso_valid_ret_targets = 0;
        s+= get_code_size(loaded_modules[m]->full_path);
           cout<<"module:"<<loaded_modules[m]->full_path<<" intra targets: "<<get_size(&hashTable_IC[m])<<" inter targets: "<<get_size(&hashTable_esym)<<endl;
        for(i=0; i< nr_icalls[m]; i++){
           unsigned long nr_valid_targets = get_size(&hashTable_IC[m]) + get_size(&hashTable_esym);
           nr_dso_valid_icall_targets += nr_valid_targets;
           dair_dso += ((double)1 - (((double)nr_valid_targets)/(double)s));
           n_dso++;
           dair_dso_icall += ((double)1 - (((double)nr_valid_targets)/(double)s));
           n_dso_icall++;
        }
        for(i = 0; i < nr_ijmps[m]; i++) {
           unsigned long nr_valid_targets = get_size(&hashTable_IJ[m]);
           nr_dso_valid_ijmp_targets += nr_valid_targets;
           dair_dso += ((double)1 - (((double)nr_valid_targets)/(double)s));
           n_dso++;
           dair_dso_ijmp += ((double)1 - (((double)nr_valid_targets)/(double)s));
               n_dso_ijmp++;
         }

         for(i = 0; i < nr_ret[m]; i++) {
               unsigned long nr_valid_targets = 1;
               nr_dso_valid_ret_targets += nr_valid_targets;
               dair_dso += ((double)1 - (((double)nr_valid_targets)/(double)s));
               n_dso++;
               dair_dso_ret += ((double)1 - (((double)nr_valid_targets)/(double)s));
               n_dso_ret++;
         }
          /* add the values for this dso to the overall value */
         dair += dair_dso;
         n += n_dso;

        dair_icall += dair_dso_icall;
        dair_ijmp += dair_dso_ijmp;
        dair_ret += dair_dso_ret;

        n_icall += n_dso_icall;
        n_ijmp += n_dso_ijmp;
        n_ret += n_dso_ret;

        nr_valid_icall_targets += nr_dso_valid_icall_targets;
        nr_valid_ijmp_targets += nr_dso_valid_ijmp_targets;
        nr_valid_ret_targets += nr_dso_valid_ret_targets;
        //printf( " * DSO %s\n", dr_module_preferred_name(loaded_modules[m]));
        airfile<< " * DSO "<< dr_module_preferred_name(loaded_modules[m])<<endl;
        airfile<<"  * icalls "<<nr_icalls[m]<<" (valid targets: "<<nr_dso_valid_icall_targets<<" )"<<endl;
        airfile<<"  * ijmps "<<nr_ijmps[m]<<" (valid targets: "<< nr_dso_valid_ijmp_targets<<" )"<<endl;
        airfile<<"  * retinstrs "<<nr_ret[m]<<" (valid targets: "<<nr_dso_valid_ret_targets<<" )"<<endl;
        airfile<<"  ************ DAIR ************"<<endl;
        airfile<<"  * DAIR "<< (unsigned long)((dair_dso/(double)n_dso)*(double)10000)<<endl;
        airfile<<"  ******************************"<<endl;
        airfile<<"  * icall DAIR "<< (unsigned long)((dair_dso_icall/(double)n_dso_icall)*(double)10000)<<endl;
        airfile<<"  ******************************"<<endl;
        airfile<<"  * ijmp DAIR "<< (unsigned long)((dair_dso_ijmp/(double)n_dso_ijmp)*(double)10000)<<endl;
        airfile<<"  ******************************"<<endl;
        airfile<<"  * ret DAIR "<< (unsigned long)((dair_dso_ret/(double)n_dso_ret)*(double)10000)<<endl;
        airfile<<"  ******************************"<<endl<<endl;
     }
      airfile<<" ************ OVERALL ************"<<endl;
      airfile<<" * Nr of total targets: "<<s<<endl;
      airfile<<" * Nr of ICF instructions: "<<n<<endl;
      airfile<<" * Nr of valid targets in average: "<< (unsigned long)((nr_valid_icall_targets+nr_valid_ijmp_targets+nr_valid_ret_targets)/n)<<endl;

      airfile<<" * Total DAIR "<< (unsigned long)((dair/(double)n)*(double)10000)<<endl;
      airfile<<" * Total icall DAIR "<< (unsigned long)((dair_icall/(double)n_icall)*(double)10000)<<endl;
      airfile<<" * Total ijmp DAIR "<< (unsigned long)((dair_ijmp/(double)n_ijmp)*(double)10000)<<endl;
      airfile<<" * Total ret DAIR "<< (unsigned long)((dair_ret/(double)n_ret)*(double)10000)<<endl;
      airfile<<" *********************************"<<endl;     

#endif
    cerr<<"app: "<<app_name<<"   Total overflow error: "<<dec<<error_counter<<endl;
}
void on_thread(void *drcontext)
{
    stack_thread_t *local_stack = (stack_thread_t *)malloc(sizeof(stack_thread_t));
    memset(local_stack, 0, sizeof(stack_thread_t));
    assert(local_stack != NULL);
    local_stack->top = 0;
    dr_set_tls_field(drcontext, local_stack);
    local = (stack_thread_t*)dr_get_tls_field(dr_get_current_drcontext());
}

void on_thread_exit(void *drcontext)
{
    free((stack_thread_t*)dr_get_tls_field(drcontext));
    if(error_counter)
       cout<<"\033[31m" <<"Total overflow error: "<< "\033[0m" <<dec<<error_counter<<endl;
   else
       cout<<"\033[32m"<<"Total overflow error: "<<"\033[0m" <<dec<<error_counter<<endl;
    std::ofstream outputFile("/local/scratch/ma843/rwdir-cfi/outerr.txt", std::ios_base::app);

    if (outputFile.is_open()) {
        // Print output directly to the file
        outputFile<<app_name<<" error: " <<error_counter<<endl;
        outputFile.flush();
        // Close the file
        outputFile.close();
    } else {
        std::cerr << "Error opening file!\n";
    }
}
static void
event_module_unload(void *drcontext, const module_data_t *info){
 //TODO: unload exported symbols of this module
#if DEBUG_VERBOSE
    dr_fprintf(STDOUT, " module unloaded -  %s \n", info->full_path);
#endif
    //find my module and free its hashtables
    //hashTable_ic[id].free();
}
static void
event_module_load(void *drcontext, const module_data_t *info, bool loaded){
    char  filepath[MAX_STR_LEN];
    char  binfilepath[MAX_STR_LEN];
    bool load_schedule = true;
    bool rules_found = false;
    uintptr_t base;
#if (DEBUG_VERBOSE || DEBUG_CFI )
    dr_fprintf(STDOUT, " full_name %s \n", info->full_path);
    dr_fprintf(STDOUT, " module_name %s \n", dr_module_preferred_name(info));
    dr_fprintf(STDOUT, " start" PFX "\n", info->start);
    dr_fprintf(STDOUT, " end" PFX "\n", info->end);
#endif
    loaded_modules[nmodules] = dr_copy_module_data(info);
    if(strcmp(dr_module_preferred_name(info), "linux-gate.so.1") == 0) {
#if !TEST_OVERHEAD
#if FORWARD_CFI
        uintptr_t vdso_addr = (uintptr_t)info->start + 0xce0; 
        insert(&hashTable_esym, vdso_addr, 1);
          int hash = hashFunction(&hashTable_esym,vdso_addr);
          histo_esym[hash].key_count++;
#endif
#endif
        vdso_id=nmodules-1 ; 
        return;
    }
#if !TEST_OVERHEAD
#if FORWARD_CFI
    if(strcmp(dr_module_preferred_name(info), "libdynamorio.so") == 0) {
        uintptr_t DR_addr = (uintptr_t)info->start + 0xf9c90; 
        insert(&hashTable_esym, DR_addr, 1);
          int hash = hashFunction(&hashTable_esym,DR_addr);
          histo_esym[hash].key_count++;
    }
    if(strcmp(dr_module_preferred_name(info), main_module) == 0){
        main_id = nmodules;
    }
    if(strcmp(dr_module_preferred_name(info), "ld-linux.so.2") == 0) {
        ld_loaded = true;
        ld_addr = (uintptr_t)info->start + 0x16ad0; //jump into routine for resolving symbols from plt table. add to _ij target set 
        for(int i=0; i<nmodules-1; i++){
             insert(&hashTable_IJ[i], ld_addr, 1);
        }
    }
#endif
#endif
#if !DYN_ONLY_MODE
    if(load_schedule && strcmp(dr_module_preferred_name(info), "linux-gate.so.1") != 0){
        char * binfile = get_binfile_name(info->full_path);
        strcpy(filepath, info->full_path);
        strcpy(binfilepath,rs_dir);
        strcat(binfilepath,binfile);
        strcat(binfilepath,".jrs");
        FILE *file = fopen(binfilepath, "r");
        if(file != NULL) {
            rules_found=true;
        }
    //fclose(file);
        if(rules_found){
#if FORWARD_CFI
            if((uintptr_t)info->start == (uintptr_t)KEYBASE)
                base = 0;
            else
                base = (uintptr_t)info->start;
#endif
            nmodules++;
            load_static_rules_security(binfilepath, info);
        }
        else{ /* rules not found, analyze dynamically */
#if FORWARD_CFI
            loadSymbolsAndHashTables(binfile, nmodules, base);
#endif
            nmodules++;
        }
    }
#endif


#if DYN_ONLY_MODE
    if(strcmp(dr_module_preferred_name(info), "linux-gate.so.1") != 0){
        char * binfile = get_binfile_name(info->full_path);
        strcpy(filepath, info->full_path);
        if((uintptr_t)info->start == (uintptr_t)KEYBASE)
            base = 0;
        else
            base = (uintptr_t)info->start;
#if FORWARD_CFI
        loadSymbolsAndHashTables(filepath, nmodules, base);
#endif
        nmodules++;
    }
#endif
}

static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating)
{
    uint64_t num_instructions = 0;
    //get current basic block starting address
    PCAddress bbAddr = (PCAddress)dr_fragment_app_pc(tag);
    //lookup in the hashtable to check if there is any rule attached to the block
    RRule *rule;
#if STAT_ONLY_MODE || HYBRID_MODE
    rule = get_static_rule_security(bbAddr);
    if (rule){
        if(rule->opcode != NO_RULE){
            generate_security_events(janus_context);
        }
    }
#if HYBRID_MODE && !STAT_ONLY_MODE
    //dynamically generated code, or not seen statically (e.g. vdso)
        generate_dynamic_events(janus_context);
#endif
#endif
#if DYN_ONLY_MODE && !STAT_ONLY_MODE && !HYBRID_MODE
        generate_dynamic_events(janus_context);
#endif
    return DR_EMIT_DEFAULT;
}



/*------------------------------------------------------------------------------*/
/*--------------------------------- DR MAIN ----- ------------------------------*/
/*------------------------------------------------------------------------------*/
DR_EXPORT void dr_init(client_id_t id)
{
#ifdef JANUS_VERBOSE
    dr_fprintf(STDOUT,"\n---------------------------------------------------------------\n");
    dr_fprintf(STDOUT,"               Janus Secure Execution --- Control Flow Integrity\n");
    dr_fprintf(STDOUT,"---------------------------------------------------------------\n\n");
#endif
    set_client_mode((JMode)JCFI);
    module_data_t *main = dr_get_main_module();
    main_module = dr_module_preferred_name(main);
    app_name = dr_get_application_name();
    dr_free_module_data(main);
#if DYN_ONLY_MODE
    /*Initialise symbol library*/
    /*if (drsym_init(0) != DRSYM_SUCCESS) {
            printf("WARNING: unable to initialize symbol translation\n");
    }*/
#endif
#if !DYN_ONLY_MODE
    janus_init_asan(id);

    cout<<"\033[32m"<<"ENTERNED JANUS: mode is set to "<<print_janus_mode((JMode)get_client_mode())<<endl; 
#endif
   

#ifdef DEBUG_VERBOSE   
#if STAT_ONLY_MODE
    cout<<"MODE is STATIC ONLY with flag_liveness="<<flag_live_on<<" reg_liveness="<<reg_live_on<<"\033[0m"<<endl;
#endif
#if HYBRID_MODE
    cout<<"MODE is HYBRID with flag_liveness="<<flag_live_on<<" reg_liveness="<<reg_live_on<<"\033[0m"<<endl;
#endif
#if DYN_ONLY_MODE
    cout<<"MODE is DYN_ONLY"<<"\033[0m"<<endl;
#endif
#endif

    /* Register event callbacks. */
       // Initialize each hash table in the array

#if FORWARD_CFI
    initHashTable(&hashTable_esym, ESYM_TABLE_SIZE);
    for(int i=0; i<ESYM_TABLE_SIZE; i++){
           histo_esym[i].key_count = 0;
           histo_esym[i].key_lookup = 0;
    }
#endif

    dr_register_thread_init_event(on_thread);
    dr_register_bb_event(event_basic_block); 
    dr_register_thread_exit_event(on_thread_exit);
    dr_register_exit_event(exit_summary);
    dr_register_module_load_event(event_module_load);
    dr_register_module_unload_event(event_module_unload);
}

