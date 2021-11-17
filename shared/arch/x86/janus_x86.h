#ifndef _JANUS_ARCH_X86_
#define _JANUS_ARCH_X86_

#include "../../static/capstone/include/capstone/x86.h"

/** \brief Register mappings used by static analyser/dynamic translator
 * this map also the same as DynamoRIO mappings */
enum {
    JREG_NULL=0, /**< Sentinel value indicating no register, for address modes. */
    /* 64-bit general purpose */
    JREG_RAX, /**< The "rax" register. */
    JREG_RCX, /**< The "rcx" register. */
    JREG_RDX, /**< The "rdx" register. */
    JREG_RBX, /**< The "rbx" register. */
    
    JREG_RSP, /**< The "rsp" register. */
    JREG_RBP, /**< The "rbp" register. */
    JREG_RSI, /**< The "rsi" register. */
    JREG_RDI, /**< The "rdi" register. */
    
    JREG_R8, /**< The "r8" register. */
    JREG_R9, /**< The "r9" register. */
    JREG_R10, /**< The "r10" register. */
    JREG_R11, /**< The "r11" register. */
    
    JREG_R12, /**< The "r12" register. */
    JREG_R13, /**< The "r13" register. */
    JREG_R14, /**< The "r14" register. */
    JREG_R15, /**< The "r15" register. */
    
    /* 32-bit general purpose */
    JREG_EAX, /**< The "eax" register. */
    JREG_ECX, /**< The "ecx" register. */
    JREG_EDX, /**< The "edx" register. */
    JREG_EBX, /**< The "ebx" register. */
    
    JREG_ESP, /**< The "esp" register. */
    JREG_EBP, /**< The "ebp" register. */
    JREG_ESI, /**< The "esi" register. */
    JREG_EDI, /**< The "edi" register. */
    
    JREG_R8D, /**< The "r8d" register. */
    JREG_R9D, /**< The "r9d" register. */
    JREG_R10D, /**< The "r10d" register. */
    JREG_R11D, /**< The "r11d" register. */
    
    JREG_R12D, /**< The "r12d" register. */
    JREG_R13D, /**< The "r13d" register. */
    JREG_R14D, /**< The "r14d" register. */
    JREG_R15D, /**< The "r15d" register. */
    
    /* 16-bit general purpose */
    JREG_AX, /**< The "ax" register. */
    JREG_CX, /**< The "cx" register. */
    JREG_DX, /**< The "dx" register. */
    JREG_BX, /**< The "bx" register. */
    
    JREG_SP, /**< The "sp" register. */
    JREG_BP, /**< The "bp" register. */
    JREG_SI, /**< The "si" register. */
    JREG_DI, /**< The "di" register. */
    
    JREG_R8W, /**< The "r8w" register. */
    JREG_R9W, /**< The "r9w" register. */
    JREG_R10W, /**< The "r10w" register. */
    JREG_R11W, /**< The "r11w" register. */
    
    JREG_R12W, /**< The "r12w" register. */
    JREG_R13W, /**< The "r13w" register. */
    JREG_R14W, /**< The "r14w" register. */
    JREG_R15W, /**< The "r15w" register. */
    
    /* 8-bit general purpose */
    JREG_AL, /**< The "al" register. */
    JREG_CL, /**< The "cl" register. */
    JREG_DL, /**< The "dl" register. */
    JREG_BL, /**< The "bl" register. */
    
    JREG_AH, /**< The "ah" register. */
    JREG_CH, /**< The "ch" register. */
    JREG_DH, /**< The "dh" register. */
    JREG_BH, /**< The "bh" register. */
    
    JREG_R8L, /**< The "r8l" register. */
    JREG_R9L, /**< The "r9l" register. */
    JREG_R10L, /**< The "r10l" register. */
    JREG_R11L, /**< The "r11l" register. */
    
    JREG_R12L, /**< The "r12l" register. */
    JREG_R13L, /**< The "r13l" register. */
    JREG_R14L, /**< The "r14l" register. */
    JREG_R15L, /**< The "r15l" register. */
    
    JREG_SPL, /**< The "spl" register. */
    JREG_BPL, /**< The "bpl" register. */
    JREG_SIL, /**< The "sil" register. */
    JREG_DIL, /**< The "dil" register. */
    
    /* 64-BIT MMX */
    JREG_MM0, /**< The "mm0" register. */
    JREG_MM1, /**< The "mm1" register. */
    JREG_MM2, /**< The "mm2" register. */
    JREG_MM3, /**< The "mm3" register. */
    
    JREG_MM4, /**< The "mm4" register. */
    JREG_MM5, /**< The "mm5" register. */
    JREG_MM6, /**< The "mm6" register. */
    JREG_MM7, /**< The "mm7" register. */
    
    /* 128-BIT XMM */
    JREG_XMM0, /**< The "xmm0" register. */
    JREG_XMM1, /**< The "xmm1" register. */
    JREG_XMM2, /**< The "xmm2" register. */
    JREG_XMM3, /**< The "xmm3" register. */
    
    JREG_XMM4, /**< The "xmm4" register. */
    JREG_XMM5, /**< The "xmm5" register. */
    JREG_XMM6, /**< The "xmm6" register. */
    JREG_XMM7, /**< The "xmm7" register. */
    
    JREG_XMM8, /**< The "xmm8" register. */
    JREG_XMM9, /**< The "xmm9" register. */
    JREG_XMM10, /**< The "xmm10" register. */
    JREG_XMM11, /**< The "xmm11" register. */
    
    JREG_XMM12, /**< The "xmm12" register. */
    JREG_XMM13, /**< The "xmm13" register. */
    JREG_XMM14, /**< The "xmm14" register. */
    JREG_XMM15, /**< The "xmm15" register. */
    
    /* floating point registers */
    JREG_ST0, /**< The "st0" register. */
    JREG_ST1, /**< The "st1" register. */
    JREG_ST2, /**< The "st2" register. */
    JREG_ST3, /**< The "st3" register. */
    
    JREG_ST4, /**< The "st4" register. */
    JREG_ST5, /**< The "st5" register. */
    JREG_ST6, /**< The "st6" register. */
    JREG_ST7, /**< The "st7" register. */
    
    /* segments (order from "Sreg" description in Intel manual) */
    GSEG_ES, /**< The "es" register. */
    GSEG_CS, /**< The "cs" register. */
    GSEG_SS, /**< The "ss" register. */
    GSEG_DS, /**< The "ds" register. */
    GSEG_FS, /**< The "fs" register. */
    GSEG_GS, /**< The "gs" register. */
    
    /* debug & control registers (privileged access only; 8-15 for future processors) */
    JREG_DR0, /**< The "dr0" register. */
    JREG_DR1, /**< The "dr1" register. */
    JREG_DR2, /**< The "dr2" register. */
    JREG_DR3, /**< The "dr3" register. */
    
    JREG_DR4, /**< The "dr4" register. */
    JREG_DR5, /**< The "dr5" register. */
    JREG_DR6, /**< The "dr6" register. */
    JREG_DR7, /**< The "dr7" register. */
    
    JREG_DR8, /**< The "dr8" register. */
    JREG_DR9, /**< The "dr9" register. */
    JREG_DR10, /**< The "dr10" register. */
    JREG_DR11, /**< The "dr11" register. */
    
    JREG_DR12, /**< The "dr12" register. */
    JREG_DR13, /**< The "dr13" register. */
    JREG_DR14, /**< The "dr14" register. */
    JREG_DR15, /**< The "dr15" register. */
    
    /* cr9-cr15 do not yet exist on current x64 hardware */
    JREG_CR0, /**< The "cr0" register. */
    JREG_CR1, /**< The "cr1" register. */
    JREG_CR2, /**< The "cr2" register. */
    JREG_CR3, /**< The "cr3" register. */
    
    JREG_CR4, /**< The "cr4" register. */
    JREG_CR5, /**< The "cr5" register. */
    JREG_CR6, /**< The "cr6" register. */
    JREG_CR7, /**< The "cr7" register. */
    
    JREG_CR8, /**< The "cr8" register. */
    JREG_CR9, /**< The "cr9" register. */
    JREG_CR10, /**< The "cr10" register. */
    JREG_CR11, /**< The "cr11" register. */
    
    JREG_CR12, /**< The "cr12" register. */
    JREG_CR13, /**< The "cr13" register. */
    JREG_CR14, /**< The "cr14" register. */
    JREG_CR15, /**< The "cr15" register. */
    
    JREG_INVALID, /**< Sentinel value indicating an invalid register. */

    /* 256-BIT YMM */
    JREG_YMM0, /**< The "ymm0" register. */
    JREG_YMM1, /**< The "ymm1" register. */
    JREG_YMM2, /**< The "ymm2" register. */
    JREG_YMM3, /**< The "ymm3" register. */
    
    JREG_YMM4, /**< The "ymm4" register. */
    JREG_YMM5, /**< The "ymm5" register. */
    JREG_YMM6, /**< The "ymm6" register. */
    JREG_YMM7, /**< The "ymm7" register. */
    
    JREG_YMM8, /**< The "ymm8" register. */
    JREG_YMM9, /**< The "ymm9" register. */
    JREG_YMM10, /**< The "ymm10" register. */
    JREG_YMM11, /**< The "ymm11" register. */
    
    JREG_YMM12, /**< The "ymm12" register. */
    JREG_YMM13, /**< The "ymm13" register. */
    JREG_YMM14, /**< The "ymm14" register. */
    JREG_YMM15, /**< The "ymm15" register. */
    JREG_YMM16, 
    JREG_YMM17,
    JREG_YMM18, 
    JREG_YMM19, 
    JREG_YMM20, 
    JREG_YMM21, 
    JREG_YMM22,
    JREG_YMM23, 
    JREG_YMM24, 
    JREG_YMM25, 
    JREG_YMM26, 
    JREG_YMM27,
    JREG_YMM28, 
    JREG_YMM29, 
    JREG_YMM30, 
    JREG_YMM31, 
    JREG_ZMM0,
    JREG_ZMM1, 
    JREG_ZMM2, 
    JREG_ZMM3, 
    JREG_ZMM4, 
    JREG_ZMM5,
    JREG_ZMM6, 
    JREG_ZMM7, 
    JREG_ZMM8, 
    JREG_ZMM9, 
    JREG_ZMM10,
    JREG_ZMM11, 
    JREG_ZMM12, 
    JREG_ZMM13, 
    JREG_ZMM14, 
    JREG_ZMM15,
    JREG_ZMM16, 
    JREG_ZMM17, 
    JREG_ZMM18, 
    JREG_ZMM19, 
    JREG_ZMM20,
    JREG_ZMM21, 
    JREG_ZMM22, 
    JREG_ZMM23, 
    JREG_ZMM24, 
    JREG_ZMM25,
    JREG_ZMM26, 
    JREG_ZMM27, 
    JREG_ZMM28, 
    JREG_ZMM29, 
    JREG_ZMM30,
    JREG_ZMM31,
    JREG_R8B, 
    JREG_R9B, 
    JREG_R10B, 
    JREG_R11B,
    JREG_R12B, 
    JREG_R13B, 
    JREG_R14B, 
    JREG_R15B,
    JREG_CS,
    JREG_DS,
    JREG_EFLAGS,
    JREG_IP,
    JREG_EIP,
    JREG_RIP,
    JREG_EIZ,
    JREG_ES,
    JREG_FPSW,
    JREG_FS,
    JREG_GS,
    JREG_RIZ,
    JREG_SS,
    JREG_FP0,
    JREG_FP1, 
    JREG_FP2, 
    JREG_FP3,
    JREG_FP4, 
    JREG_FP5, 
    JREG_FP6, 
    JREG_FP7,
    JREG_K0, 
    JREG_K1, 
    JREG_K2, 
    JREG_K3, 
    JREG_K4,
    JREG_K5, 
    JREG_K6, 
    JREG_K7,
    JREG_XMM16, 
    JREG_XMM17, 
    JREG_XMM18, 
    JREG_XMM19,
    JREG_XMM20, 
    JREG_XMM21, 
    JREG_XMM22, 
    JREG_XMM23, 
    JREG_XMM24,
    JREG_XMM25, 
    JREG_XMM26, 
    JREG_XMM27, 
    JREG_XMM28, 
    JREG_XMM29,
    JREG_XMM30, 
    JREG_XMM31,
    JREG_INVALID2
};

#endif
