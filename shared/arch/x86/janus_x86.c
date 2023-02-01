/* x86 specific analysis in C */
#include "janus_arch.h"

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

/* Bitmap representation of all x86 registers
 * The bit 0-15 are GPR integer registers and 16-31 are SIMD registers
 * bit 32-62 are free to use (TODO: extend YMM16-31)
 * bit 63 are eflag registers  */
static const uint64_t regBitMaps[] =
{
    0, /**< Sentinel value indicating no register", for address modes. */
    /* 64-bit general purpose */
    1, /**< The "rax" register. */
    1<<1, /**< The "rcx" register. */
    1<<2, /**< The "rdx" register. */
    1<<3, /**< The "rbx" register. */
    1<<4, /**< The "rsp" register. */
    1<<5, /**< The "rbp" register. */
    1<<6, /**< The "rsi" register. */
    1<<7, /**< The "rdi" register. */

    1<<8, /**< The "r8" register. */
    1<<9, /**< The "r9" register. */
    1<<10, /**< The "r10" register. */
    1<<11, /**< The "r11" register. */

    1<<12, /**< The "r12" register. */
    1<<13, /**< The "r13" register. */
    1<<14, /**< The "r14" register. */
    1<<15, /**< The "r15" register. */

    /* 32-bit general purpose */
    1<<0, /**< The "eax" register. */
    1<<1, /**< The "ecx" register. */
    1<<2, /**< The "edx" register. */
    1<<3, /**< The "ebx" register. */

    1<<4, /**< The "esp" register. */
    1<<5, /**< The "ebp" register. */
    1<<6, /**< The "esi" register. */
    1<<7, /**< The "edi" register. */

    1<<8, /**< The "r8d" register. */
    1<<9, /**< The "r9d" register. */
    1<<10, /**< The "r10d" register. */
    1<<11, /**< The "r11d" register. */

    1<<12, /**< The "r12d" register. */
    1<<13, /**< The "r13d" register. */
    1<<14, /**< The "r14d" register. */
    1<<15, /**< The "r15d" register. */

    /* 16-bit general purpose */
    1, /**< The "ax" register. */
    1<<1, /**< The "cx" register. */
    1<<2, /**< The "dx" register. */
    1<<3, /**< The "bx" register. */

    1<<4, /**< The "sp" register. */
    1<<5, /**< The "bp" register. */
    1<<6, /**< The "si" register. */
    1<<7, /**< The "di" register. */

    1<<8, /**< The "r8w" register. */
    1<<9, /**< The "r9w" register. */
    1<<10, /**< The "r10w" register. */
    1<<11, /**< The "r11w" register. */

    1<<12, /**< The "r12w" register. */
    1<<13, /**< The "r13w" register. */
    1<<14, /**< The "r14w" register. */
    1<<15, /**< The "r15w" register. */

    /* 8-bit general purpose */
    1, /**< The "al" register. */
    1<<1, /**< The "cl" register. */
    1<<2, /**< The "dl" register. */
    1<<3, /**< The "bl" register. */

    1, /**< The "ah" register. */
    1<<1, /**< The "ch" register. */
    1<<2, /**< The "dh" register. */
    1<<3, /**< The "bh" register. */

    1<<8, /**< The "r8l" register. */
    1<<9, /**< The "r9l" register. */
    1<<10, /**< The "r10l" register. */
    1<<11, /**< The "r11l" register. */

    1<<12, /**< The "r12l" register. */
    1<<13, /**< The "r13l" register. */
    1<<14, /**< The "r14l" register. */
    1<<15, /**< The "r15l" register. */

    1<<4, /**< The "spl" register. */
    1<<5, /**< The "bpl" register. */
    1<<6, /**< The "sil" register. */
    1<<7, /**< The "dil" register. */

    /* 64-BIT MMX */
    1<<16, /**< The "mm0" register. */
    1<<17, /**< The "mm1" register. */
    1<<18, /**< The "mm2" register. */
    1<<19, /**< The "mm3" register. */

    1<<20, /**< The "mm4" register. */
    1<<21, /**< The "mm5" register. */
    1<<22, /**< The "mm6" register. */
    1<<23, /**< The "mm7" register. */

    /* 128-BIT XMM */
    1<<16, /**< The "xmm0" register. */
    1<<17, /**< The "xmm1" register. */
    1<<18, /**< The "xmm2" register. */
    1<<19, /**< The "xmm3" register. */

    1<<20, /**< The "xmm4" register. */
    1<<21, /**< The "xmm5" register. */
    1<<22, /**< The "xmm6" register. */
    1<<23, /**< The "xmm7" register. */

    1<<24, /**< The "xmm8" register. */
    1<<25, /**< The "xmm9" register. */
    1<<26, /**< The "xmm10" register. */
    1<<27, /**< The "xmm11" register. */

    1<<28, /**< The "xmm12" register. */
    1<<29, /**< The "xmm13" register. */
    1<<30, /**< The "xmm14" register. */
    1<<31, /**< The "xmm15" register. */

    /* floating point registers */
    1<<16, /**< The "st0" register. */
    1<<17, /**< The "st1" register. */
    1<<18, /**< The "st2" register. */
    1<<19, /**< The "st3" register. */

    1<<20, /**< The "st4" register. */
    1<<21, /**< The "st5" register. */
    1<<22, /**< The "st6" register. */
    1<<23, /**< The "st7" register. */

    /* segments (order from "Sreg" description in Intel manual) */
    0, /**< The "es" register. */
    0, /**< The "cs" register. */
    0, /**< The "ss" register. */
    0, /**< The "ds" register. */
    0, /**< The "fs" register. */
    0, /**< The "gs" register. */

    /* debug & control registers (privileged access only; 8-15 for future processors) */
    0, /**< The "dr0" register. */
    0, /**< The "dr1" register. */
    0, /**< The "dr2" register. */
    0, /**< The "dr3" register. */

    0, /**< The "dr4" register. */
    0, /**< The "dr5" register. */
    0, /**< The "dr6" register. */
    0, /**< The "dr7" register. */

    0, /**< The "dr8" register. */
    0, /**< The "dr9" register. */
    0, /**< The "dr10" register. */
    0, /**< The "dr11" register. */

    0, /**< The "dr12" register. */
    0, /**< The "dr13" register. */
    0, /**< The "dr14" register. */
    0, /**< The "dr15" register. */

    /* cr9-cr15 do not yet exist on current x64 hardware */
    0, /**< The "cr0" register. */
    0, /**< The "cr1" register. */
    0, /**< The "cr2" register. */
    0, /**< The "cr3" register. */

    0, /**< The "cr4" register. */
    0, /**< The "cr5" register. */
    0, /**< The "cr6" register. */
    0, /**< The "cr7" register. */

    0, /**< The "cr8" register. */
    0, /**< The "cr9" register. */
    0, /**< The "cr10" register. */
    0, /**< The "cr11" register. */

    0, /**< The "cr12" register. */
    0, /**< The "cr13" register. */
    0, /**< The "cr14" register. */
    0, /**< The "cr15" register. */

    0, /**< Sentinel value indicating an invalid register. */

    /* 256-BIT YMM */
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    /* 256-BIT ZMM */
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    1<<8, /**< The "r8b" register. */
    1<<9, /**< The "r9b" register. */
    1<<10, /**< The "r10b" register. */
    1<<11, /**< The "r11b" register. */

    1<<12, /**< The "r12b" register. */
    1<<13, /**< The "r13b" register. */
    1<<14, /**< The "r14b" register. */
    1<<15, /**< The "r15b" register. */
    0,
    0,
    1ll<<63, /* EFLAGS */
    0,
    0,
    0,
    0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0
};

uint32_t get_full_reg_id(uint32_t id)
{
    switch(id) {

        /* 8-bit general purpose */
        case JREG_AH: return JREG_RAX; /**< The "ax" register. */
        case JREG_CH: return JREG_RCX; /**< The "cx" register. */
        case JREG_DH: return JREG_RDX; /**< The "dx" register. */
        case JREG_BH: return JREG_RBX; /**< The "bx" register. */

        /* 8-bit general purpose */
        case JREG_AL: return JREG_RAX; /**< The "ax" register. */
        case JREG_CL: return JREG_RCX; /**< The "cx" register. */
        case JREG_DL: return JREG_RDX; /**< The "dx" register. */
        case JREG_BL: return JREG_RBX; /**< The "bx" register. */

        /* 16-bit general purpose */
        case JREG_AX: return JREG_RAX; /**< The "ax" register. */
        case JREG_CX: return JREG_RCX; /**< The "cx" register. */
        case JREG_DX: return JREG_RDX; /**< The "dx" register. */
        case JREG_BX: return JREG_RBX; /**< The "bx" register. */

        /* 32-bit general purpose */
        case JREG_EAX: return JREG_RAX; /**< The "eax" register. */
        case JREG_ECX: return JREG_RCX; /**< The "ecx" register. */
        case JREG_EDX: return JREG_RDX; /**< The "edx" register. */
        case JREG_EBX: return JREG_RBX; /**< The "ebx" register. */
        
        case JREG_ESP: return JREG_RSP; /**< The "esp" register. */
        case JREG_EBP: return JREG_RBP; /**< The "ebp" register. */
        case JREG_ESI: return JREG_RSI; /**< The "esi" register. */
        case JREG_EDI: return JREG_RDI; /**< The "edi" register. */
        
        case JREG_R8D: return JREG_R8; /**< The "r8d" register. */
        case JREG_R9D: return JREG_R9; /**< The "r9d" register. */
        case JREG_R10D: return JREG_R10; /**< The "r10d" register. */
        case JREG_R11D: return JREG_R11; /**< The "r11d" register. */
        
        case JREG_R12D: return JREG_R12; /**< The "r12d" register. */
        case JREG_R13D: return JREG_R13; /**< The "r13d" register. */
        case JREG_R14D: return JREG_R14; /**< The "r14d" register. */
        case JREG_R15D: return JREG_R15; /**< The "r15d" register. */
        

        
        case JREG_SP: return JREG_RSP; /**< The "sp" register. */
        case JREG_BP: return JREG_RBP; /**< The "bp" register. */
        case JREG_SI: return JREG_RSI; /**< The "si" register. */
        case JREG_DI: return JREG_RDI; /**< The "di" register. */
        
        case JREG_R8W: return JREG_R8; /**< The "r8w" register. */
        case JREG_R9W: return JREG_R9; /**< The "r9w" register. */
        case JREG_R10W: return JREG_R10; /**< The "r10w" register. */
        case JREG_R11W: return JREG_R11; /**< The "r11w" register. */
        
        case JREG_R12W: return JREG_R12; /**< The "r12w" register. */
        case JREG_R13W: return JREG_R13; /**< The "r13w" register. */
        case JREG_R14W: return JREG_R14; /**< The "r14w" register. */
        case JREG_R15W: return JREG_R15; /**< The "r15w" register. */

        case JREG_R8B: return JREG_R8; /**< The "r8B" register. */
        case JREG_R9B: return JREG_R9; /**< The "r9B" register. */
        case JREG_R10B: return JREG_R10; /**< The "r10B" register. */
        case JREG_R11B: return JREG_R11; /**< The "r11B" register. */
        
        case JREG_R12B: return JREG_R12; /**< The "r12B" register. */
        case JREG_R13B: return JREG_R13; /**< The "r13B" register. */
        case JREG_R14B: return JREG_R14; /**< The "r14B" register. */
        case JREG_R15B: return JREG_R15; /**< The "r15B" register. */


        case JREG_R8L: return JREG_R8; /**< The "r8l" register. */
        case JREG_R9L: return JREG_R9; /**< The "r9l" register. */
        case JREG_R10L: return JREG_R10; /**< The "r10l" register. */
        case JREG_R11L: return JREG_R11; /**< The "r11l" register. */
        
        case JREG_R12L: return JREG_R12; /**< The "r12l" register. */
        case JREG_R13L: return JREG_R13; /**< The "r13l" register. */
        case JREG_R14L: return JREG_R14; /**< The "r14l" register. */
        case JREG_R15L: return JREG_R15; /**< The "r15l" register. */
        
        case JREG_SPL: return JREG_RSP; /**< The "spl" register. */
        case JREG_BPL: return JREG_RBP; /**< The "bpl" register. */
        case JREG_SIL: return JREG_RSI; /**< The "sil" register. */
        case JREG_DIL: return JREG_RDI; /**< The "dil" register. */
    default: return id;
    }
}

//1+2+4+64+128+256+512+1024+2048+32768
//rax,rcx,rdx,rsi,rdi,r8,r9,r10,r11,r15
const uint64_t callerSavedRegs = 36807; //16+32+65536
const uint64_t restrictedRegs = 65584;
//1+2+4+64+128+256+512+1024+2048
//rax,rcx,rdx,rsi,rdi,r8,r9,r10,r11
const uint64_t argumentRegs = 4039;
//65536+131072+262144+524288+1048576+2097152+4194304+8388608+16777216+
//33554432+67108864+134217728+268435456+536870912+1073741824+-2147483648
//xmm0+xmm1+xmm2+xmm3+xmm4+xmm5+xmm6+xmm7+xmm8+xmm9+xmm10+xmm11+xmm12+xmm13+xmm14+xmm15
const uint64_t xmmRegs = 18446744073709486080U;

const char *get_reg_name(int reg)
{
    if (reg > 300) return "NULL";

    return regNameMaps[reg];
}

int get_reg_size(int reg)
{
    return 8;
}

///Return the bit representation of a given register
uint64_t get_reg_bit_array(int reg)
{
    if (reg >= JREG_INVALID2) return JREG_INVALID2;
    return regBitMaps[reg];
}

int jreg_is_gpr(int reg)
{
    if (reg == JREG_RSP) return 0;
    if (reg <= JREG_R15 && reg >= JREG_RAX) return 1;
    else return 0;
}

int jreg_is_simd(int reg)
{
    if ((reg >= JREG_MM0 && reg <= JREG_MM7) ||
        (reg >= JREG_XMM0 && reg <= JREG_XMM15) ||
        (reg >= JREG_YMM0 && reg <= JREG_YMM31) ||
        (reg >= JREG_ZMM0 && reg <= JREG_ZMM31)) return 1;
    else return 0;
}
