/*! \file loop_format.h
 *  \brief Defines the architecture specific API across both static and dynamic components
 */
#ifndef _JANUS_ARCH_X_
#define _JANUS_ARCH_X_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#ifdef JANUS_X86
#include "x86/janus_x86.h"
#endif

#ifdef JANUS_AARCH64
#include "aarch64/janus_aarch64.h"
#endif
///Return the string of the register name
const char *get_reg_name(int reg);
///Return string of the shift type
const char *get_shift_name(uint8_t shift);
///Return the size of given register
int get_reg_size(int reg);
///Return the bit representation of a given register
uint64_t get_reg_bit_array(int reg);
///Return the register id of the full size of the given register
uint32_t get_full_reg_id(uint32_t id);
///Return true if the register is a general purpose integer register
int jreg_is_gpr(int reg);
///Return true if the register is a SIMD register
int jreg_is_simd(int reg);

// Shift types (mainly used on AArch64)
//KEVIN: put this in janus_aarch64.h because we don't use them for x86
enum {
    JSHFT_INVALID = 0,
    JSHFT_LSL = 1,
    JSHFT_MSL = 2,
    JSHFT_LSR = 3,
    JSHFT_ASR = 4,
    JSHFT_ROR = 5,
    JSHFT_END = 6,
};

#ifdef __cplusplus
}
#endif

#endif
