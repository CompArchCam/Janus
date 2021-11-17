#ifndef _JANUS_STATE_
#define _JANUS_STATE_

/* A private copy of the machine state */
typedef struct _pstate_t {
    uint64_t rax;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rbx;
    uint64_t rsp;
    uint64_t rbp;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rflags;
    uint64_t pc;
    uint64_t xmm0;
    uint64_t xmm0u;
    uint64_t xmm1;
    uint64_t xmm1u;
    uint64_t xmm2;
    uint64_t xmm2u;
    uint64_t xmm3;
    uint64_t xmm3u;
    uint64_t xmm4;
    uint64_t xmm4u;
    uint64_t xmm5;
    uint64_t xmm5u;
    uint64_t xmm6;
    uint64_t xmm6u;
    uint64_t xmm7;
    uint64_t xmm7u;
    uint64_t xmm8;
    uint64_t xmm8u;
    uint64_t xmm9;
    uint64_t xmm9u;
    uint64_t xmm10;
    uint64_t xmm10u;
    uint64_t xmm11;
    uint64_t xmm11u;
    uint64_t xmm12;
    uint64_t xmm12u;
    uint64_t xmm13;
    uint64_t xmm13u;
    uint64_t xmm14;
    uint64_t xmm14u;
    uint64_t xmm15;
    uint64_t xmm15u;
} pstate_t;

/* For easy access, it is either a state or an array */
typedef union _upstate_t {
	pstate_t state;
	uint64_t regs[18+16*2];
} priv_state_t;

#define PSTATE_PC_OFFSET (8*17)
#define PSTATE_FLAG_OFFSET (8*16)
#define PSTATE_SIMD_OFFSET (8*18)
#define CACHE_LINE_WIDTH 64

/* A global copy of the machine state 
 * The state is only modified after a thread has been commited 
/* To avoid false sharing, each register takes up to a cache line */
/* The mask bit needs to be aligned with the following structure */
typedef struct _sstate_t {
    uint64_t rax;   uint64_t _rax[7];
    uint64_t rcx;   uint64_t _rcx[7];
    uint64_t rdx;   uint64_t _rdx[7];
    uint64_t rbx;   uint64_t _rbx[7];
    uint64_t rsp;   uint64_t _rsp[7];
    uint64_t rbp;   uint64_t _rbp[7];
    uint64_t rsi;   uint64_t _rsi[7];
    uint64_t rdi;   uint64_t _rdi[7];
    uint64_t r8;    uint64_t _r8[7];
    uint64_t r9;    uint64_t _r9[7];
    uint64_t r10;   uint64_t _r10[7];
    uint64_t r11;   uint64_t _r11[7];
    uint64_t r12;   uint64_t _r12[7];
    uint64_t r13;   uint64_t _r13[7];
    uint64_t r14;   uint64_t _r14[7];
    uint64_t r15;   uint64_t _r15[7];
    uint64_t xmm0;  uint64_t xmm0u; uint64_t _xmm0[6];
    uint64_t xmm1;  uint64_t xmm1u; uint64_t _xmm1[6];
    uint64_t xmm2;  uint64_t xmm2u; uint64_t _xmm2[6];
    uint64_t xmm3;  uint64_t xmm3u; uint64_t _xmm3[6];
    uint64_t xmm4;  uint64_t xmm4u; uint64_t _xmm4[6];
    uint64_t xmm5;  uint64_t xmm5u; uint64_t _xmm5[6];
    uint64_t xmm6;  uint64_t xmm6u; uint64_t _xmm6[6];
    uint64_t xmm7;  uint64_t xmm7u; uint64_t _xmm7[6];
    uint64_t xmm8;  uint64_t xmm8u; uint64_t _xmm8[6];
    uint64_t xmm9;  uint64_t xmm9u; uint64_t _xmm9[6];
    uint64_t xmm10;  uint64_t xmm10u; uint64_t _xmm10[6];
    uint64_t xmm11;  uint64_t xmm11u; uint64_t _xmm11[6];
    uint64_t xmm12;  uint64_t xmm12u; uint64_t _xmm12[6];
    uint64_t xmm13;  uint64_t xmm13u; uint64_t _xmm13[6];
    uint64_t xmm14;  uint64_t xmm14u; uint64_t _xmm14[6];
    uint64_t xmm15;  uint64_t xmm15u; uint64_t _xmm15[6];
    uint64_t rflags; uint64_t _rflags[7];
} shared_state_t;
#endif