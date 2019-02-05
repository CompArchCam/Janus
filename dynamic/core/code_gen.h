/*! \file code_gen.h
 *  \brief Janus Code Generation Routine
 *  
 *  This header defines the API to convert Janus rewrite schedule to DynamoRIO IR
 */

#ifndef _JANUS_DYNAMIC_CODE_GEN_
#define _JANUS_DYNAMIC_CODE_GEN_

#ifdef __cplusplus
   extern "C" {
#endif

#include <stdint.h>
#include "janus_api.h"


/** \brief Creates a DR operand (opnd_t) from the given JVar object).
 *
 * The scale variable is used for [offset + index * scale + displacement] form.
 */
opnd_t create_opnd(const JVar v);

/** \brief Creates a DR operand (opnd_t) from the given JVar stack object).
 *
 * It creates a shared stack reference. The shared stack pointer has to be specified.
 */
opnd_t create_shared_stack_opnd(const JVar v, reg_id_t shared_stk_ptr);

/** \brief Encode the list of DR instructions into machine code.
 *
 * It returns the start_pc of the encoded code snippet.
 * After calling, the instruction list is deallocated internally.
 */
app_pc
generate_runtime_code(void *drcontext, uint32_t max_size, instrlist_t *bb);

/** \brief Return the register id that has the same length with specified operand
 */
reg_id_t
reg_with_same_width(reg_id_t reg, opnd_t opnd);

/** \brief Create a JVar from reg_id_t
 */
JVar
reg_to_JVar(reg_id_t reg);

/** \brief Creates a register or immediate DR operand (opnd_t).
 *
 * If type is set, it returns an immediate operand with the given value,
 * otherwise it returns a register operand containing the given register.
 */
//opnd_t opnd_regOrImm(bool type, int64_t value);

/** \brief Clones the instruction located at the given location, changing the registers in its input and output operands to the registers given in *in* and *out*.
 *
 * The resulting instruction is marked as a meta-instruction.
 */
//instr_t *clone_with_regs(void *drcontext, instr_t *instr, const uint8_t *regs);

/** \brief Inserts a general MOV instruction where both operands are registers or spill slots.
 *
 * Creates and inserts a MOV, save or restore instruction depending on the sign of its operands.
 * At least one of the operands has to be a register.
 */
//void createMoveInstruction(void *drcontext, instrlist_t *ilist, instr_t *where,
//                                                                    int32_t dst, int32_t src);

//void front_end_exit();

#ifdef __cplusplus
    }
#endif

#endif
