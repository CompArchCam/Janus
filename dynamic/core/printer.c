#include "gprinter.h"
#include <stdio.h>

const char *print_janus_mode(JMode mode) {
    switch(mode) {
        case GPARALLEL: return "Automatic Parallelisation";
        case GPARAFUNC: return "Automatic Function Parallelisation";
        case GEXEMODEL: return "Parallelism Explorer";
        case GTIME: return "Dynamic Loop Timer";
        case GLTIME: return "Accurate Loop Timer";
        case GPROFILE: return "Runtime Profiler";
        case JCUSTOM: return "Custom tool with Cinnamon DSL";
        case GCTRACE: return "Dynamic Procedure Tracer";
        default: return "Free Mode";
    }
}

const char *print_rule_opcode(GHINT_OP op)
{
    switch (op) {
        case GNORMAL: return "GNORMAL";
        case PRF_LOOP_START: return "PRF_LOOP_START";
        case PRF_LOOP_END: return "PRF_LOOP_END";
        case PRF_LOOP_ITER: return "PRF_LOOP_ITER";
        case PRF_CALL_START: return "PRF_CALL_START";
        case PRF_CALL_END: return "PRF_CALL_END";
            
        case PRF_CAM_START: return "PRF_CAM_START";
        case PRF_CAM_END: return "PRF_CAM_END";
            
        case PARA_PRODUCE: return "PARA_PRODUCE";
        case PARA_CONSUME: return "PARA_CONSUME";

        case PARA_CALL_START: return "PARA_CALL_START";
        case PARA_CALL_END: return "PARA_CALL_END";
        
        case TIMER_INIT: return "TIMER_INIT";
        case TIMER_OUTPUT: return "TIMER_OUTPUT";
        case TIMER_RESUME: return "TIMER_RESUME";
        case TIMER_PAUSE: return "TIMER_PAUSE";
            
        case THREAD_CREATE: return "THREAD_CREATE";
        case PARA_ITER_START: return "PARA_ITER_START";
        case PARA_LOOP_INIT: return "PARA_LOOP_INIT";
        case PARA_LOOP_INIT_SIMPLE: return "PARA_LOOP_INIT_SIMPLE";
        case PARA_LOOP_ITER: return "PARA_LOOP_ITER";        
        case PARA_LOOP_END: return "PARA_LOOP_END";
        case PARA_LOOP_END_SIMPLE: return "PARA_LOOP_END_SIMPLE";
        case PARA_ALLOCA_STACK: return "PARA_ALLOCA_STACK";
        case PARA_DALLOCA_STACK: return "PARA_DALLOCA_STACK";
        case PARA_FUNC_START: return "PARA_FUNC_START";
        case THREAD_EXIT: return "THREAD_EXIT";
            
        case TRANS_START: return "TRANS_START";
        
        case TRANS_COMMIT: return "TRANS_COMMIT";
        case TRANS_COMPARE: return "TRANS_COMPARE";
        case TRANS_PROCEED: return "TRANS_PROCEED";
        case TRANS_WAIT: return "TRANS_WAIT";
        case TRANS_LOAD: return "STM_LOAD";
        case TRANS_STORE: return "STM_STORE";
        case TRANS_MEM: return "TRANS_MEM";
        case TRANS_MEM_FAST: return "TRANS_MEM_FAST";
        case TRANS_STACK: return "TRANS_STACK";
        case TRANS_STACK_IND: return "TRANS_STACK_INDIRECT";
        case PARA_UPDATE_ITER: return "PARA_UPDATE_ITER";
        case PARA_UPDATE_CHECK: return "PARA_UPDATE_CHECK";
        case PARA_SPILL_REG: return "PARA_SPILL_REG";
        case PARA_RECOVER_REG: return "PARA_RECOVER_REG";
            
        case EM_START: return "EM_START";
        case EM_END: return "EM_END";
        case EM_LOOP_START: return "EM_LOOP_START";
        case EM_LOOP_END: return "EM_LOOP_END";
        case EM_LOOP_ITER: return "EM_LOOP_ITER";
        //case EM_START_TX: return "EM_START_TX";
        //case EM_END_TX: return "EM_END_TX";
        case EM_SYNC_START: return "EM_SYNC_START";
        case EM_SYNC_END: return "EM_SYNC_END";
        case EM_CALL_START: return "EM_CALL_START";
        case EM_CALL_END: return "EM_CALL_END";
        case EM_MEM: return "EM_MEM";
        case EM_REG: return "EM_REG";
        case EM_PRED_CHECK: return "EM_PRED_CHECK";
        case EM_REG_INDUCTION: return "EM_REG_INDUCTION";
        case EM_STACK: return "EM_STACK";
        case EM_SEEN_BB: return "EM_SEEN_BB";
        case EM_STATUS: return "EM_STATUS";
        case JANUS_DEBUG: return "JANUS_DEBUG";
        case REG_RECOVER: return "REG_RECOVER";
        case CTIMER_LOOP_START: return "CTIMER_LOOP_START";
        case CTIMER_LOOP_END: return "CTIMER_LOOP_END";
        case CTRACE_CALL_START: return "CTRACE_CALL_START";
        case CTRACE_CALL_END: return "CTRACE_CALL_END";
        case CTRACE_CALL: return "CTRACE_CALL";
        case CTRACE_CALL_RET: return "CTRACE_CALL_RET";
        default: return "Null";
    }
}

static const char *regNameMaps[] =
{
    "NULL", /**< Sentinel value indicating no register", for address modes. */
    /* 64-bit general purpose */
    "RAX", /**< The "rax" register. */
    "RCX", /**< The "rcx" register. */
    "RDX", /**< The "rdx" register. */
    "RBX", /**< The "rbx" register. */
    
    "RSP", /**< The "rsp" register. */
    "RBP", /**< The "rbp" register. */
    "RSI", /**< The "rsi" register. */
    "RDI", /**< The "rdi" register. */
    
    "R8", /**< The "r8" register. */
    "R9", /**< The "r9" register. */
    "R10", /**< The "r10" register. */
    "R11", /**< The "r11" register. */
    
    "R12", /**< The "r12" register. */
    "R13", /**< The "r13" register. */
    "R14", /**< The "r14" register. */
    "R15", /**< The "r15" register. */
    
    /* 32-bit general purpose */
    "EAX", /**< The "eax" register. */
    "ECX", /**< The "ecx" register. */
    "EDX", /**< The "edx" register. */
    "EBX", /**< The "ebx" register. */
    
    "ESP", /**< The "esp" register. */
    "EBP", /**< The "ebp" register. */
    "ESI", /**< The "esi" register. */
    "EDI", /**< The "edi" register. */
    
    "R8D", /**< The "r8d" register. */
    "R9D", /**< The "r9d" register. */
    "R10D", /**< The "r10d" register. */
    "R11D", /**< The "r11d" register. */
    
    "R12D", /**< The "r12d" register. */
    "R13D", /**< The "r13d" register. */
    "R14D", /**< The "r14d" register. */
    "R15D", /**< The "r15d" register. */
    
    /* 16-bit general purpose */
    "AX", /**< The "ax" register. */
    "CX", /**< The "cx" register. */
    "DX", /**< The "dx" register. */
    "BX", /**< The "bx" register. */
    
    "SP", /**< The "sp" register. */
    "BP", /**< The "bp" register. */
    "SI", /**< The "si" register. */
    "DI", /**< The "di" register. */
    
    "R8W", /**< The "r8w" register. */
    "R9W", /**< The "r9w" register. */
    "R10W", /**< The "r10w" register. */
    "R11W", /**< The "r11w" register. */
    
    "R12W", /**< The "r12w" register. */
    "R13W", /**< The "r13w" register. */
    "R14W", /**< The "r14w" register. */
    "R15W", /**< The "r15w" register. */
    
    /* 8-bit general purpose */
    "AL", /**< The "al" register. */
    "CL", /**< The "cl" register. */
    "DL", /**< The "dl" register. */
    "BL", /**< The "bl" register. */
    
    "AH", /**< The "ah" register. */
    "CH", /**< The "ch" register. */
    "DH", /**< The "dh" register. */
    "BH", /**< The "bh" register. */
    
    "R8L", /**< The "r8l" register. */
    "R9L", /**< The "r9l" register. */
    "R10L", /**< The "r10l" register. */
    "R11L", /**< The "r11l" register. */
    
    "R12L", /**< The "r12l" register. */
    "R13L", /**< The "r13l" register. */
    "R14L", /**< The "r14l" register. */
    "R15L", /**< The "r15l" register. */
    
    "SPL", /**< The "spl" register. */
    "BPL", /**< The "bpl" register. */
    "SIL", /**< The "sil" register. */
    "DIL", /**< The "dil" register. */
    
    /* 64-BIT MMX */
    "MM0", /**< The "mm0" register. */
    "MM1", /**< The "mm1" register. */
    "MM2", /**< The "mm2" register. */
    "MM3", /**< The "mm3" register. */
    
    "MM4", /**< The "mm4" register. */
    "MM5", /**< The "mm5" register. */
    "MM6", /**< The "mm6" register. */
    "MM7", /**< The "mm7" register. */
    
    /* 128-BIT XMM */
    "XMM0", /**< The "xmm0" register. */
    "XMM1", /**< The "xmm1" register. */
    "XMM2", /**< The "xmm2" register. */
    "XMM3", /**< The "xmm3" register. */
    
    "XMM4", /**< The "xmm4" register. */
    "XMM5", /**< The "xmm5" register. */
    "XMM6", /**< The "xmm6" register. */
    "XMM7", /**< The "xmm7" register. */
    
    "XMM8", /**< The "xmm8" register. */
    "XMM9", /**< The "xmm9" register. */
    "XMM10", /**< The "xmm10" register. */
    "XMM11", /**< The "xmm11" register. */
    
    "XMM12", /**< The "xmm12" register. */
    "XMM13", /**< The "xmm13" register. */
    "XMM14", /**< The "xmm14" register. */
    "XMM15", /**< The "xmm15" register. */
    
    /* floating point registers */
    "ST0", /**< The "st0" register. */
    "ST1", /**< The "st1" register. */
    "ST2", /**< The "st2" register. */
    "ST3", /**< The "st3" register. */
    
    "ST4", /**< The "st4" register. */
    "ST5", /**< The "st5" register. */
    "ST6", /**< The "st6" register. */
    "ST7", /**< The "st7" register. */
    
    /* segments (order from "Sreg" description in Intel manual) */
    "GSEG_ES", /**< The "es" register. */
    "GSEG_CS", /**< The "cs" register. */
    "GSEG_SS", /**< The "ss" register. */
    "GSEG_DS", /**< The "ds" register. */
    "GSEG_FS", /**< The "fs" register. */
    "GSEG_GS", /**< The "gs" register. */
    
    /* debug & control registers (privileged access only; 8-15 for future processors) */
    "DR0", /**< The "dr0" register. */
    "DR1", /**< The "dr1" register. */
    "DR2", /**< The "dr2" register. */
    "DR3", /**< The "dr3" register. */
    
    "DR4", /**< The "dr4" register. */
    "DR5", /**< The "dr5" register. */
    "DR6", /**< The "dr6" register. */
    "DR7", /**< The "dr7" register. */
    
    "DR8", /**< The "dr8" register. */
    "DR9", /**< The "dr9" register. */
    "DR10", /**< The "dr10" register. */
    "DR11", /**< The "dr11" register. */
    
    "DR12", /**< The "dr12" register. */
    "DR13", /**< The "dr13" register. */
    "DR14", /**< The "dr14" register. */
    "DR15", /**< The "dr15" register. */
    
    /* cr9-cr15 do not yet exist on current x64 hardware */
    "CR0", /**< The "cr0" register. */
    "CR1", /**< The "cr1" register. */
    "CR2", /**< The "cr2" register. */
    "CR3", /**< The "cr3" register. */
    
    "CR4", /**< The "cr4" register. */
    "CR5", /**< The "cr5" register. */
    "CR6", /**< The "cr6" register. */
    "CR7", /**< The "cr7" register. */
    
    "CR8", /**< The "cr8" register. */
    "CR9", /**< The "cr9" register. */
    "CR10", /**< The "cr10" register. */
    "CR11", /**< The "cr11" register. */
    
    "CR12", /**< The "cr12" register. */
    "CR13", /**< The "cr13" register. */
    "CR14", /**< The "cr14" register. */
    "CR15", /**< The "cr15" register. */
    
    "INVALID", /**< Sentinel value indicating an invalid register. */

    /* 256-BIT YMM */
    "YMM0", /**< The "ymm0" register. */
    "YMM1", /**< The "ymm1" register. */
    "YMM2", /**< The "ymm2" register. */
    "YMM3", /**< The "ymm3" register. */
    
    "YMM4", /**< The "ymm4" register. */
    "YMM5", /**< The "ymm5" register. */
    "YMM6", /**< The "ymm6" register. */
    "YMM7", /**< The "ymm7" register. */
    
    "YMM8", /**< The "ymm8" register. */
    "YMM9", /**< The "ymm9" register. */
    "YMM10", /**< The "ymm10" register. */
    "YMM11", /**< The "ymm11" register. */
    
    "YMM12", /**< The "ymm12" register. */
    "YMM13", /**< The "ymm13" register. */
    "YMM14", /**< The "ymm14" register. */
    "YMM15", /**< The "ymm15" register. */
    "YMM16", 
    "YMM17",
    "YMM18", 
    "YMM19", 
    "YMM20", 
    "YMM21", 
    "YMM22",
    "YMM23", 
    "YMM24", 
    "YMM25", 
    "YMM26", 
    "YMM27",
    "YMM28", 
    "YMM29", 
    "YMM30", 
    "YMM31", 
    "ZMM0",
    "ZMM1", 
    "ZMM2", 
    "ZMM3", 
    "ZMM4", 
    "ZMM5",
    "ZMM6", 
    "ZMM7", 
    "ZMM8", 
    "ZMM9", 
    "ZMM10",
    "ZMM11", 
    "ZMM12", 
    "ZMM13", 
    "ZMM14", 
    "ZMM15",
    "ZMM16", 
    "ZMM17", 
    "ZMM18", 
    "ZMM19", 
    "ZMM20",
    "ZMM21", 
    "ZMM22", 
    "ZMM23", 
    "ZMM24", 
    "ZMM25",
    "ZMM26", 
    "ZMM27", 
    "ZMM28", 
    "ZMM29", 
    "ZMM30",
    "ZMM31",
    "R8B", 
    "R9B", 
    "R10B", 
    "R11B",
    "R12B", 
    "R13B", 
    "R14B", 
    "R15B",
    "CS",
    "DS",
    "EFLAGS",
    "IP",
    "EIP",
    "RIP",
    "EIZ",
    "ES",
    "FPSW",
    "FS",
    "GS",
    "RIZ",
    "SS",
    "FP0",
    "FP1", 
    "FP2", 
    "FP3",
    "FP4", 
    "FP5", 
    "FP6", 
    "FP7",
    "K0", 
    "K1", 
    "K2", 
    "K3", 
    "K4",
    "K5", 
    "K6", 
    "K7",
    "XMM16", 
    "XMM17", 
    "XMM18", 
    "XMM19",
    "XMM20", 
    "XMM21", 
    "XMM22", 
    "XMM23", 
    "XMM24",
    "XMM25", 
    "XMM26", 
    "XMM27", 
    "XMM28", 
    "XMM29",
    "XMM30", 
    "XMM31",
};

const char *print_register_name(uint32_t reg)
{
	return regNameMaps[reg];
}

void print_rule(int tid, RRule *rule)
{
    printf("Thread %d %s 0x%lx:(%d)0x%lx reg0:0x%lx, reg1:0x%lx\n",tid, print_rule_opcode(rule->opcode),rule->block_address,rule->id,rule->pc,rule->reg0,rule->reg1);
}
