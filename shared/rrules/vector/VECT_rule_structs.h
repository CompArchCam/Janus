#ifndef __VECT_rule_structs
#define __VECT_rule_structs

#include "rule_isa.h"
#include "janus_var.h"
#include <cstdlib>

#ifdef _STATIC_ANALYSIS_COMPILED
    //only compile these parts if we are compiling the static analysis tool
    #include "SchedGen.h"
#endif

namespace janus {
    class Variable;
    class BasicBlock;
}

#ifdef __cplusplus
extern "C" {
#endif


//#ifdef _STATIC_ANALYSIS_COMPILED
    //only compile these parts if we are compiling the static analysis tool
//    #include "SchedGen.h"
//#endif
//namespace janus {
//    class Variable;
//    class BasicBlock;
//}

/** \brief Abstract class defining the basic methods and fields of all VECT_ rules.
 *
 * Subclasses implement internally the encoding and decoding to/from the RewriteRule/RRule format, 
 * using the two registers optimally for the specific rule type.
 *
 * \note In the "Layout" sections [0-31] refers to uregX.down and [32-63] refers to uregX.up
 */
struct VECT_RULE {
    /** \brief Denotes the operation to be performed. */
    RuleOp opcode;
    /** \brief BasicBlock containing this rule. */
    janus::BasicBlock *bb;
    /** \brief Program counter pointing to the instruction tagged by this rule. */
    PCAddress pc;
    /** \brief Unique id, determining the order for rules pointing to the same location. */
    uint32_t ID;

    VECT_RULE(janus::BasicBlock *bb, PCAddress pc, uint32_t ID, RuleOp opcode);
    VECT_RULE();

    #ifdef _STATIC_ANALYSIS_COMPILED
    /** \brief Converts this rule into a RewriteRule, storing all the basic information, and
     *         fills the two registers with data from which the rule can be decoded again.
     */
    virtual janus::RewriteRule *encode();
    /** \brief Updates vectorWordSize if rule contains it.
     */
    virtual void updateVectorWordSize(uint32_t vectorWordSize) {};
    #endif
};

//structs storing and coding-decoding specific rules:
/** \brief Struct storing, encoding and decoding VECT rules.
 *
 * Layout:\n
 *     + reg0: [0-31] freeReg, [31-47] reg, [48-63] init\n
 *     + reg1: [0-31] vectorWordSize, [32-63] freeVectReg
 */
struct VECT_REDUCE_INIT_rule : public VECT_RULE {
    /** \brief Vector word size in bytes of loop. */
    uint32_t vectorWordSize;
    /** \brief Register to be initialised. */
    uint16_t reg;
    /** \brief Value to be used for initialisation. */
    uint16_t init;
    /** \brief Free 8-byte register. */
    uint32_t freeReg;
    /** \brief Free vector register. */
    uint32_t freeVectReg;

    VECT_REDUCE_INIT_rule(janus::BasicBlock *bb, PCAddress pc, uint32_t ID, uint16_t reg, uint16_t init, 
        uint32_t freeReg, uint32_t freeVectReg);
    VECT_REDUCE_INIT_rule(RRule &rule);
    #ifdef _STATIC_ANALYSIS_COMPILED
    virtual janus::RewriteRule *encode();
    virtual void updateVectorWordSize(uint32_t vectorWordSize);
    #endif
};

/** \brief Struct storing, encoding and decoding VECT rules.
 *
 * Layout:\n
 *     + reg0: [0-31] startOffset, [32-63] iterCount\n
 *     + reg1: [0-15] freeReg, [16-31] unusedReg, [32-63] vectorWordSize
 */
struct VECT_LOOP_PEEL_rule : public VECT_RULE {
    /** \brief Vector word size in bytes of loop. */
    uint32_t vectorWordSize;
    /** \brief Start of loop's pc's offset from trigger pc. */
    uint32_t startOffset;
    /** \brief 8-byte register that is unused within loop. */
    uint16_t unusedReg;
    /** \brief Free 8-byte register. */
    uint16_t freeReg;
    /** \brief Loop's iteration count. */
    uint32_t iterCount;

    VECT_LOOP_PEEL_rule(janus::BasicBlock *bb, PCAddress pc, uint32_t ID, uint32_t startOffset, 
        uint16_t unusedReg, uint16_t freeReg, uint32_t iterCount);
    VECT_LOOP_PEEL_rule(RRule &rule);
    #ifdef _STATIC_ANALYSIS_COMPILED
    virtual janus::RewriteRule *encode();
    virtual void updateVectorWordSize(uint32_t vectorWordSize);
    #endif
};

/** \brief Struct storing, encoding and decoding VECT rules.
 *
 * Layout:\n
 *     + reg0: [0-63] startOffset\n
 *     + reg1: [0-31] reg, [32-63] value
 */
struct VECT_REG_CHECK_rule : public VECT_RULE {
    /** \brief Start of loop's pc's offset from trigger pc. */
    uint64_t startOffset;
    /** \brief Register to check. */
    uint32_t reg;
    /** \brief Value to check register against. */
    uint32_t value;

    VECT_REG_CHECK_rule(janus::BasicBlock *bb, PCAddress pc, uint32_t ID, uint64_t startOffset, 
        uint32_t reg, uint32_t value);
    VECT_REG_CHECK_rule(RRule &rule);
    #ifdef _STATIC_ANALYSIS_COMPILED
    virtual janus::RewriteRule *encode();
    #endif
};

enum Aligned {
    ALIGNED,
    UNALIGNED,
    CHECK_REQUIRED
};

/** \brief Struct storing, encoding and decoding VECT rules.
 *
 * Layout:\n
 *     + reg0: [0-31] disp, [32-47] stride, [48-63] afterModification\n
 *     + reg1: [0-15] freeVectReg, [16-31] aligned, [32-47] memOpnd, [48-63] iterCount
 */
struct VECT_CONVERT_rule : public VECT_RULE {
    /** \brief Initial displacement of memory access. */
    uint32_t disp;
    /** \brief Is the instruction after the modification of the induction variable. */
    bool afterModification;
    /** \brief Stride in bytes of induction variable. */
    uint16_t stride;
    /** \brief Free vector register. */
    uint16_t freeVectReg;
    /** \brief Is the memory access aligned. */
    Aligned aligned;
    /** \brief Position of memory access. */
    uint16_t memOpnd;
    /** \brief Loop iteration count. */
    uint16_t iterCount;

    VECT_CONVERT_rule(janus::BasicBlock *bb, PCAddress pc, uint32_t ID, uint32_t disp, bool afterModication,
        uint16_t stride, uint16_t freeVectReg, Aligned aligned, uint16_t memOpnd, uint16_t iterCount);
    VECT_CONVERT_rule(RRule &rule);
    #ifdef _STATIC_ANALYSIS_COMPILED
    virtual janus::RewriteRule *encode();
    #endif
};

/** \brief Struct storing, encoding and decoding VECT rules.
 *
 * Layout:\n
 *     + reg0: [0-63] vectorWordSize\n
 *     + reg1: [0-63] reg
 */
struct VECT_INDUCTION_STRIDE_UPDATE_rule : public VECT_RULE {
    /** \brief Vector word size in bytes of loop. */
    uint64_t vectorWordSize;
    /** \brief Register to be unrolled. */
    uint64_t reg;

    VECT_INDUCTION_STRIDE_UPDATE_rule(janus::BasicBlock *bb, PCAddress pc, uint32_t ID, uint64_t reg);
    VECT_INDUCTION_STRIDE_UPDATE_rule(RRule &rule);
    #ifdef _STATIC_ANALYSIS_COMPILED
    virtual janus::RewriteRule *encode();
    virtual void updateVectorWordSize(uint32_t vectorWordSize);
    #endif
};

/** \brief Struct storing, encoding and decoding VECT rules.
 *
 * Layout:\n
 *     + reg0: [0-31] freeVectReg, [32-63] reg\n
 *     + reg1: [0-31] vectorWordSize, [32-63] isMultiply
 */
struct VECT_REDUCE_AFTER_rule : public VECT_RULE {
    /** \brief Vector word size in bytes of loop. */
    uint32_t vectorWordSize;
    /** \brief Reduction register. */
    uint32_t reg;
    /** \brief Free vector register. */
    uint32_t freeVectReg;
    /** \brief Does the reduction use multiplication or division. */
    bool isMultiply;

    VECT_REDUCE_AFTER_rule(janus::BasicBlock *bb, PCAddress pc, uint32_t ID, uint32_t reg, uint32_t freeVectReg, 
        bool isMultiply);
    VECT_REDUCE_AFTER_rule(RRule &rule);
    #ifdef _STATIC_ANALYSIS_COMPILED
    virtual janus::RewriteRule *encode();
    virtual void updateVectorWordSize(uint32_t vectorWordSize);
    #endif
};

/** \brief Struct storing, encoding and decoding VECT rules.
 *
 * Layout:\n
 *     + reg0: [0-63] reg\n
 */
struct VECT_REVERT_rule : public VECT_RULE {
    /** \brief Reduction register. */
    uint64_t reg;

    VECT_REVERT_rule(janus::BasicBlock *bb, PCAddress pc, uint32_t ID, uint64_t reg);
    VECT_REVERT_rule(RRule &rule);
    #ifdef _STATIC_ANALYSIS_COMPILED
    virtual janus::RewriteRule *encode();
    #endif
};

#ifdef __cplusplus
}
#endif

#endif

