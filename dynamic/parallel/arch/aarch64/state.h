#ifndef _JANUS_STATE_
#define _JANUS_STATE_

/* A private copy of the machine state */
typedef struct _pstate_t {
    uint64_t x0;
    uint64_t x1;
    uint64_t x2;
    uint64_t x3;
    uint64_t x4;
    uint64_t x5;
    uint64_t x6;
    uint64_t x7;
    uint64_t x8;
    uint64_t x9;
    uint64_t x10;
    uint64_t x11;
    uint64_t x12;
    uint64_t x13;
    uint64_t x14;
    uint64_t x15;
    uint64_t x16;
    uint64_t x17;
    uint64_t x18;
    uint64_t x19;
    uint64_t x20;
    uint64_t x21;
    uint64_t x22;
    uint64_t x23;
    uint64_t x24;
    uint64_t x25;
    uint64_t x26;
    uint64_t x27;
    uint64_t x28;
    uint64_t x29;
    uint64_t x30;
    uint64_t sp;

    uint64_t q0;
    uint64_t q0u;
    uint64_t q1;
    uint64_t q1u;
    uint64_t q2;
    uint64_t q2u;
    uint64_t q3;
    uint64_t q3u;
    uint64_t q4;
    uint64_t q4u;
    uint64_t q5;
    uint64_t q5u;
    uint64_t q6;
    uint64_t q6u;
    uint64_t q7;
    uint64_t q7u;
    uint64_t q8;
    uint64_t q8u;
    uint64_t q9;
    uint64_t q9u;
    uint64_t q10;
    uint64_t q10u;
    uint64_t q11;
    uint64_t q11u;
    uint64_t q12;
    uint64_t q12u;
    uint64_t q13;
    uint64_t q13u;
    uint64_t q14;
    uint64_t q14u;
    uint64_t q15;
    uint64_t q15u;
    uint64_t q16;
    uint64_t q16u;
    uint64_t q17;
    uint64_t q17u;
    uint64_t q18;
    uint64_t q18u;
    uint64_t q19;
    uint64_t q19u;
    uint64_t q20;
    uint64_t q20u;
    uint64_t q21;
    uint64_t q21u;
    uint64_t q22;
    uint64_t q22u;
    uint64_t q23;
    uint64_t q23u;
    uint64_t q24;
    uint64_t q24u;
    uint64_t q25;
    uint64_t q25u;
    uint64_t q26;
    uint64_t q26u;
    uint64_t q27;
    uint64_t q27u;
    uint64_t q28;
    uint64_t q28u;
    uint64_t q29;
    uint64_t q29u;
    uint64_t q30;
    uint64_t q30u;
    uint64_t q31;
    uint64_t q31u;

    uint64_t nzcv;
    uint64_t pc;
} pstate_t;

/* For easy access, it is either a state or an array */
typedef union _upstate_t {
	pstate_t state;
	uint64_t regs[32+32*2+2];
} priv_state_t;

#define PSTATE_PC_OFFSET (sizeof(uint64_t)*32 + 2*sizeof(uint64_t)*32 + sizeof(uint64_t))
#define PSTATE_FLAG_OFFSET (sizeof(uint64_t)*32 + 2*sizeof(uint64_t)*32)
#define PSTATE_SIMD_OFFSET (sizeof(uint64_t)*32)
#define CACHE_LINE_WIDTH 64

/* A global copy of the machine state 
 * The state is only modified after a thread has been commited */ 
/* To avoid false sharing, each register takes up to a cache line */
/* The mask bit needs to be aligned with the following structure */
typedef struct _sstate_t {

    uint64_t x0; uint64_t _x0[7];
    uint64_t x1; uint64_t _x1[7];
    uint64_t x2; uint64_t _x2[7];
    uint64_t x3; uint64_t _x3[7];
    uint64_t x4; uint64_t _x4[7];
    uint64_t x5; uint64_t _x5[7];
    uint64_t x6; uint64_t _x6[7];
    uint64_t x7; uint64_t _x7[7];
    uint64_t x8; uint64_t _x8[7];
    uint64_t x9; uint64_t _x9[7];
    uint64_t x10; uint64_t _x10[7];
    uint64_t x11; uint64_t _x11[7];
    uint64_t x12; uint64_t _x12[7];
    uint64_t x13; uint64_t _x13[7];
    uint64_t x14; uint64_t _x14[7];
    uint64_t x15; uint64_t _x15[7];
    uint64_t x16; uint64_t _x16[7];
    uint64_t x17; uint64_t _x17[7];
    uint64_t x18; uint64_t _x18[7];
    uint64_t x19; uint64_t _x19[7];
    uint64_t x20; uint64_t _x20[7];
    uint64_t x21; uint64_t _x21[7];
    uint64_t x22; uint64_t _x22[7];
    uint64_t x23; uint64_t _x23[7];
    uint64_t x24; uint64_t _x24[7];
    uint64_t x25; uint64_t _x25[7];
    uint64_t x26; uint64_t _x26[7];
    uint64_t x27; uint64_t _x27[7];
    uint64_t x28; uint64_t _x28[7];
    uint64_t x29; uint64_t _x29[7];
    uint64_t x30; uint64_t _x30[7];
    uint64_t sp; uint64_t _sp[7];

    uint64_t q0; uint64_t _q0[7];
    uint64_t q1; uint64_t _q1[7];
    uint64_t q2; uint64_t _q2[7];
    uint64_t q3; uint64_t _q3[7];
    uint64_t q4; uint64_t _q4[7];
    uint64_t q5; uint64_t _q5[7];
    uint64_t q6; uint64_t _q6[7];
    uint64_t q7; uint64_t _q7[7];
    uint64_t q8; uint64_t _q8[7];
    uint64_t q9; uint64_t _q9[7];
    uint64_t q10; uint64_t _q10[7];
    uint64_t q11; uint64_t _q11[7];
    uint64_t q12; uint64_t _q12[7];
    uint64_t q13; uint64_t _q13[7];
    uint64_t q14; uint64_t _q14[7];
    uint64_t q15; uint64_t _q15[7];
    uint64_t q16; uint64_t _q16[7];
    uint64_t q17; uint64_t _q17[7];
    uint64_t q18; uint64_t _q18[7];
    uint64_t q19; uint64_t _q19[7];
    uint64_t q20; uint64_t _q20[7];
    uint64_t q21; uint64_t _q21[7];
    uint64_t q22; uint64_t _q22[7];
    uint64_t q23; uint64_t _q23[7];
    uint64_t q24; uint64_t _q24[7];
    uint64_t q25; uint64_t _q25[7];
    uint64_t q26; uint64_t _q26[7];
    uint64_t q27; uint64_t _q27[7];
    uint64_t q28; uint64_t _q28[7];
    uint64_t q29; uint64_t _q29[7];
    uint64_t q30; uint64_t _q30[7];
    uint64_t q31; uint64_t _q31[7];

    // Maybe need to add NZCV flags here?

} shared_state_t;
#endif

