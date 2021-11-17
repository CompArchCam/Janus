#ifndef __OPT_rule_structs
#define __OPT_rule_structs

#include "rule_isa.h"
#include "rule_var.h"
#include <cstdlib>

#ifdef __cplusplus
extern "C" {
#endif


#ifdef _STATIC_ANALYSIS_COMPILED
    //only compile these parts if we are compiling the static analysis tool
    #include "SchedGen.h"
#endif
namespace janus {
    class Variable;
    class BasicBlock;
}

/** \brief Abstract class defining the basic methods and fields of all OPT_ rules.
 *
 * Subclasses implement internally the encoding and decoding to/from the RewriteRule/RRule format, 
 * using the two registers optimally for the specific rule type.
 *
 * \note In the "Layout" sections [0-31] refers to uregX.down and [32-63] refers to uregX.up
 */
struct OPT_HINT {
    /** \brief Denotes the operation to be performed. */
    GHINT_OP opcode;
    /** \brief Address of the BasicBlock containing this rule. */
    PCAddress blockAddr;
    union {
        /** \brief Program counter pointing to the instruction succeeding the prefetch sequence
         *         (ie the one containing the load operand). */
        PCAddress pc;
        /** \brief Generic pointer that can be used after the address has been resolved */
        void *ptr;
    };
    /** \brief Unique id, determining the order for rules pointing to the same location. */
    uint16_t ID;

    /** \brief For storing items as a linked list. */
    OPT_HINT *next = NULL;

    OPT_HINT(PCAddress blockAddr, PCAddress pc, uint16_t ID): blockAddr(blockAddr), pc(pc), ID(ID)
                                                                                               {
        next = NULL;
    };
    OPT_HINT(janus::BasicBlock *bb, PCAddress pc, uint16_t ID);
    OPT_HINT() {
        next = NULL;
    };

    #ifdef _STATIC_ANALYSIS_COMPILED
    /** \brief Converts this rule into a RewriteRule, storing all the basic information, and
     *         fills the two registers with data from which the rule can be decoded again.
     */
    virtual janus::RewriteRule *encode() = 0;
    #endif
};

//structs storing and coding-decoding specific rules:
/** \brief Struct storing, encoding and decoding OPT_PREFETCH rules.
 *
 * Layout:\n
 *     + reg0: [0-15] ID, [32-35] scale (for [base + index*scale + offset] memory operands)\n
 *     + reg1: [0-63] opnd
 */
struct OPT_PREFETCH_rule : public OPT_HINT {
    /** \brief Scale used for type VAR_MEM_BASE */
    uint8_t scale : 4;
    /** \brief Memory operand to prefetch */
    union {
        GVar opnd;
        uint64_t reg1;
    };

    OPT_PREFETCH_rule(janus::BasicBlock *bb, PCAddress pc, uint16_t ID, janus::Variable var);
    OPT_PREFETCH_rule(RRule &rule);
    #ifdef _STATIC_ANALYSIS_COMPILED
    virtual janus::RewriteRule *encode();
    #endif
};

/** \brief Struct storing, encoding and decoding OPT_INSERT_MOV rules.
 *
 * Rule: insert MOV instruction for the specified registers or spill slots.
 *
 * Layout:\n
 *      + reg0: [0-15] ID\n
 *      + reg1: [0-31] destination operand, [32-63] source operand
 *
 * Operands: 32-bit signed values -- Positive numbers encode registers, negative numbers encode 
 *           spill slots.
 */
struct OPT_INSERT_MOV_rule : public OPT_HINT {
    union {
        struct {
            /** \brief Destination operand. */
            int32_t dst;
            /** \brief Source operand. */
            int32_t src;
        };
        uint64_t reg1;
    };

    OPT_INSERT_MOV_rule(janus::BasicBlock *bb, PCAddress pc, uint16_t ID, int32_t dst, int32_t src);
    OPT_INSERT_MOV_rule(RRule &rule);
    #ifdef _STATIC_ANALYSIS_COMPILED
    virtual janus::RewriteRule *encode();
    #endif
};

/** \brief Struct storing, encoding and decoding OPT_INSERT_MEM_MOVE rules.
 *
 * Rule: insert MOV instruction with the specified operands (involving no spill slots).
 *
 * Layout:\n
 *      + reg0: [0-15] ID, [16-19] scale (for src=[base + index*scale + offset])
 *              [32-63] Destination register
 *      + reg1: [0-63] Source operand (memory operand)
 *
 * Operands: destination is a register, source is a memory address or immediate value or register operand!
 */
struct OPT_INSERT_MEM_MOV_rule : public OPT_HINT {
    /** \brief Scale used for type VAR_MEM_BASE */
    uint8_t scale : 4;
    /** \brief Destination register */
    uint32_t dst;
    /** \brief union used for easier conversion to RewriteRule and from RRule */
    union {
        /** \brief Source memory address. */
        GVar src;
        uint64_t reg1;
    };

    OPT_INSERT_MEM_MOV_rule(janus::BasicBlock *bb, PCAddress pc, uint16_t ID, uint32_t dst,
                                                                               janus::Variable var);
    OPT_INSERT_MEM_MOV_rule(RRule &rule);
    #ifdef _STATIC_ANALYSIS_COMPILED
    virtual janus::RewriteRule *encode();
    #endif
};

/** \brief Struct storing, encoding and decoding OPT_INSERT_ADD rules.
 *
 * Rule: insert ADD instruction - add register1/immediate1 to register0.
 *
 * Layout:\n
 *      + reg0: [0-15] ID, [16] source_opnd_type, [32-63] destination operand
 *      + reg1: [0-63] source operand
 *
 * Operands: destination is a register, source is a register or 64-bit immediate value.
 */
struct OPT_INSERT_ADD_rule : public OPT_HINT {
    /** \brief Flag. True: source operand is register, False: source operand is immediate value */
    bool source_opnd_type;
    /** \brief Destination operand (register) */
    uint32_t dst;
    /** \brief Source operand (register or immediate value -- depending on source_opnd_type) */
    uint64_t src;

    OPT_INSERT_ADD_rule(janus::BasicBlock *bb, PCAddress pc, uint16_t ID, bool source_opnd_type,
                                                                      uint32_t dst, uint64_t src);
    OPT_INSERT_ADD_rule(RRule &rule);
    #ifdef _STATIC_ANALYSIS_COMPILED
    virtual janus::RewriteRule *encode();
    #endif
};

/** \brief Struct storing, encoding and decoding OPT_INSERT_SUB rules.
 *
 * Rule: insert SUB instruction - subtract register1/immediate1 from register0.
 *
 * Layout:\n
 *      + reg0: [0-15] ID, [16] source_opnd_type, [32-63] destination operand
 *      + reg1: [0-63] source operand
 *
 * Operands: destination is a register, source is a register or 64-bit immediate value.
 */
struct OPT_INSERT_SUB_rule : public OPT_HINT {
    /** \brief Flag. True: source operand is register, False: source operand is immediate value */
    bool source_opnd_type;
    /** \brief Destination operand (register) */
    uint32_t dst;
    /** \brief Source operand (register or immediate value -- depending on source_opnd_type) */
    uint64_t src;

    OPT_INSERT_SUB_rule(janus::BasicBlock *bb, PCAddress pc, uint16_t ID, bool source_opnd_type,
                                                                      uint32_t dst, uint64_t src);
    OPT_INSERT_SUB_rule(RRule &rule);
    #ifdef _STATIC_ANALYSIS_COMPILED
    virtual janus::RewriteRule *encode();
    #endif
};

/** \brief Struct storing, encoding and decoding OPT_INSERT_IMUL rules.
 *
 * Rule: insert IMUL instruction - multiply register0 by immediate1.
 *
 * Layout:\n
 *      + reg0: [0-15] ID, [32-63] destination operand
 *      + reg1: [0-63] source operand
 *
 * Operands: destination is a register, source is a register or 64-bit immediate value.
 */
struct OPT_INSERT_IMUL_rule : public OPT_HINT {
    /** \brief Destination operand (register) */
    uint32_t dst;
    /** \brief Source operand (register or immediate value -- depending on source_opnd_type)
     *
     * Union is defined for easier conversiont to RewriteRule and from RRule.
     */
    union {
        int64_t src;
        uint64_t reg1;
    };

    OPT_INSERT_IMUL_rule(janus::BasicBlock *bb, PCAddress pc, uint16_t ID, uint32_t dst,
                                                                                     int64_t src);
    OPT_INSERT_IMUL_rule(RRule &rule);
    #ifdef _STATIC_ANALYSIS_COMPILED
    virtual janus::RewriteRule *encode();
    #endif
};

/** \brief Struct storing, encoding and decoding OPT_RESTORE_REG rules.
 *
 * Rule: The given register (reg) has to be saved before the given prefetch sequence (and 
 *       restored afterwards). The register/spill slot specified by store can be used for this.
 *
 * Layout:\n
 *      + reg0: [0-15] ID, [32-63] register to be stored
 *      + reg1: [0-63] store
 *
 * Operands: reg is a register, store is a register (positive) or spill slot (negative).
 */
struct OPT_RESTORE_REG_rule : public OPT_HINT {
    /** \brief Destination operand (register) */
    uint32_t reg;
    /** \brief Source operand (register or immediate value -- depending on source_opnd_type)
     *
     * Union is defined for easier conversiont to RewriteRule and from RRule.
     */
    union {
        int64_t store;
        uint64_t reg1;
    };

    OPT_RESTORE_REG_rule(janus::BasicBlock *bb, PCAddress pc, uint16_t ID, uint32_t reg,
                                                                                   int64_t store);
    OPT_RESTORE_REG_rule(RRule &rule);
    #ifdef _STATIC_ANALYSIS_COMPILED
    virtual janus::RewriteRule *encode();
    #endif
};

/** \brief Struct storing, encoding and decoding OPT_RESTORE_FLAGS rules.
 *
 * Rule: The contents of the EFLAGS register has to be saved before the given prefetch serquence
 *       (and restored afterwards). Since this can not be directly copied, the stack is used.
 *
 * Layout:\n
 *      + reg0: [0-15] ID
 *
 * Operands: none
 */
struct OPT_RESTORE_FLAGS_rule : public OPT_HINT {
    OPT_RESTORE_FLAGS_rule(janus::BasicBlock *bb, PCAddress pc, uint16_t ID);
    OPT_RESTORE_FLAGS_rule(RRule &rule);
    #ifdef _STATIC_ANALYSIS_COMPILED
    virtual janus::RewriteRule *encode();
    #endif
};

/** \brief Struct storing, encoding and decoding OPT_INSERT_IMUL rules.
 *
 * Rule: insert a clone of the specified instruction, change the registers used to the ones 
 * specified in *opnd[]*.
 *
 * Layout:\n
 *      + reg0: [0-15] ID, [16] fixedRegisters, [32-63] operand registers
 *      + reg1: [0-63] cloneAddr
 *
 * Operands: 4 registers, which should replace the registers in the original instruction, and an 
 * address to insert the cloned instruction to.
 *
 * \note NOTE THAT THE STORED PC REFERS TO THE INSTRUCTION TO BE CLONED AND CLONEADDR REFERS TO 
 * THE PLACE TO INSERT TO! Despite this, the ID refers to its position in the sequence inserted 
 * before colneAddr.
 */
struct OPT_CLONE_INSTR_rule : public OPT_HINT {
    /** \brief True iff the registers in the operands should not be changed. */
    bool fixedRegisters;

    /** \brief The new registers to be used within the operands. Up to 4 operands are supported.
     *
     * Union is defined for easier conversiont to RewriteRule and from RRule.
     */
    union {
        /** \brief Registers. Order: sources first, followed by destinations. */
        uint8_t opnd[4];
        uint32_t reg0_up;
    };
    /** \brief Address (pc) of the place to insert the cloned instruction to.
     *
     * Union is defined for easier conversiont to RewriteRule and from RRule.
     */
    union {
        uint64_t cloneAddr;
        uint64_t reg1;
    };

    OPT_CLONE_INSTR_rule(janus::BasicBlock *bb, PCAddress pc, uint16_t ID, uint8_t operands[4],
                                                       bool fixedRegisters, uint64_t clonedAddr);
    OPT_CLONE_INSTR_rule(RRule &rule);
    #ifdef _STATIC_ANALYSIS_COMPILED
    virtual janus::RewriteRule *encode();
    #endif
};

/** \brief Struct storing, encoding and decoding OPT_SAVE_REG_TO_STACK rules.
 *
 * Rule: insert an instruction at the specified location to save the given register to the stack,
 * to comply with the calling convention (save&restore registers)
 *
 * Layout:\n
 *      + reg0: [0-15] ID
 *      + reg1: [0-63] register to save
 *
 * Operand: the register which is to be saved to the stack
 */
struct OPT_SAVE_REG_TO_STACK_rule : public OPT_HINT {
    /** \brief The registers to be saved.
     *
     * Union is defined for easier conversiont to RewriteRule and from RRule.
     */
    union {
        /** \brief The register to be saved. */
        uint64_t reg;
        uint64_t reg1;
    };

    OPT_SAVE_REG_TO_STACK_rule(janus::BasicBlock *bb, PCAddress pc, uint16_t ID, uint64_t reg);
    OPT_SAVE_REG_TO_STACK_rule(RRule &rule);
    #ifdef _STATIC_ANALYSIS_COMPILED
    virtual janus::RewriteRule *encode();
    #endif
};

/** \brief Struct storing, encoding and decoding OPT_RESTORE_REG_FROM_STACK rules.
 *
 * Rule: insert an instruction at the specified location to restore the given register from the
 * stack, to comply with the calling convention (save&restore registers)
 *
 * Layout:\n
 *      + reg0: [0-15] ID
 *      + reg1: [0-63] register to save
 *
 * Operand: the register which is to be saved to the stack
 */
struct OPT_RESTORE_REG_FROM_STACK_rule : public OPT_HINT {
    /** \brief The registers to be saved.
     *
     * Union is defined for easier conversiont to RewriteRule and from RRule.
     */
    union {
        /** \brief The register to be saved. */
        uint64_t reg;
        uint64_t reg1;
    };

    OPT_RESTORE_REG_FROM_STACK_rule(janus::BasicBlock *bb, PCAddress pc, uint16_t ID, uint64_t reg);
    OPT_RESTORE_REG_FROM_STACK_rule(RRule &rule);
    #ifdef _STATIC_ANALYSIS_COMPILED
    virtual janus::RewriteRule *encode();
    #endif
};

#ifdef __cplusplus
}
#endif

#endif

