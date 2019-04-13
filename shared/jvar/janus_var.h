/*! \file rule_isa.h
 *  \brief Janus variable specification.
 *  
 *  This header defines the basic structure of a Janus variable.
 *  A Janus variable JVar is a intermediate representation that represents a single storage including registers, stacks and memory operations.
 */
#ifndef _JANUS_VARIABLE_
#define _JANUS_VARIABLE_

#include <stdint.h>

/** \brief Type of a rule variable. */
typedef enum _jvar_type
{
    ///Register variables
    JVAR_REGISTER = 0,
    ///Stack variables (only in form stack with displacement)
    JVAR_STACK,
    ///Generic memory variables (in form: [base+index*scale+disp])
    JVAR_MEMORY,
    ///Absolute memory addresses (PC-relative addresses)
    JVAR_ABSOLUTE,
    ///Immediate value
    JVAR_CONSTANT,
    ///Flag variables
    JVAR_CONTROLFLAG,
    ///Polynomial variable type (reserved for x86)
    JVAR_POLYNOMIAL,
    ///Stack frame operand (reserved for AArch64)
    JVAR_STACKFRAME,
    ///Shifted immediate operand
    JVAR_SHIFTEDCONST,
    ///Shifted register operand
    JVAR_SHIFTEDREG,
    ///Unknown variable type
    JVAR_UNKOWN,
} JVarType;

/** \brief Janus Variable Definition
 *
 * The **JVar** variable is an intermediate representation shared across the static binary analyser and dynamic translator.
 * And this variable represent the most basic storage unit for different machines but maintains architecture independent.
 * A **JVar** is limited to 128-bit width and it can be used for the most cases.
 * Extend Var if your architecture is beyond 128-bit width.

 * In the static binary analyser, each instruction operand will be lifted to **JVar**. The SSA graph will be built on the versioned **JVar**
 * In the rewrite schedule, JVar is stored in binary form and it will be loaded by DynamoRIO
 * In the DynamoRIO, Jvar will be converted to opnd_t in DynamoRIO directly. It can be encoded dynamically.
 */
typedef struct _jvar
{
    uint64_t        value;
    uint8_t         base;
    uint8_t         index;
    uint8_t         scale;
    ///shift or segment information
    uint8_t         shift_type;
    uint8_t         shift_value;
    uint8_t         size;
    JVarType        type:16;
} JVar;

/** \brief A package of JVar, can be casted from JVar */
typedef struct _jvar_compact
{
    union {
        JVar        var;
        struct
        {
            uint64_t data1;
            uint64_t data2;
        };
    };
} JVarPack;

/** \brief Janus Variable Profile
 *
 * The **JVarProfile** variable is additional information encapsulated to convey information between the static and dynamic tool.
 */
/**  \brief obsolete types, remove in the future */
typedef enum _var_update_type
{
    UPDATE_UNKNOWN = 0,
    UPDATE_ADD = 1,
    UPDATE_SUB
} JVarUpdate;

typedef struct _induct {
    JVarUpdate          op;
    JVar                init;
    JVar                stride;
    JVar                check;
} Induction;

typedef struct _array {
    /** \brief static storage for this array base */
    JVar                base;
    /** \brief runtime value for this array base, filled by MEM_RECORD_BOUNDS */
    uint64_t            runtime_base_value;
    /** \brief static storage for this array range */
    JVar                max_range;
    /** \brief runtime value for this array base, filled by MEM_RECORD_RANGE */
    uint64_t            runtime_range_value;
} Array;

/** \brief Types of variable profile loaded by GVM */
typedef enum _var_profile_type
{
    /** \brief Profile for an induction variable */
    INDUCTION_PROFILE=0,
    /** \brief Profile for a reduction variable */
    REDUCTION_PROFILE,
    /** \brief Profile for an array base and its recurrence form */
    ARRAY_PROFILE,
    ABSOLUTE_PROFILE,
    DEPEND_PROFILE,
    MEM_BLOCK_PROFILE,
    CODE_PROFILE,
    /** \brief Profile for a variable that must be copied because of a ldp */
    COPY_PROFILE,
} JVarProfileType;

typedef struct _jvarprofile {
    JVar                    var;
    JVarProfileType         type;
    union {
        Induction   induction;
        Array       array;
    };
    int                     version;
} JVarProfile;

/** \brief A set of scratch registers, the registers are ranked based on their frequency, r0 are the least used. */
typedef struct _scratch {
    uint8_t         reg0;
    uint8_t         reg1;
    uint8_t         reg2;
    uint8_t         reg3;
} ScratchRegSet;

void print_var(JVar var);
void print_profile(JVarProfile *profile);

typedef struct rule_instr RRule;

/** \brief Encode the current JVar to rewrite rule */
void encode_jvar(JVar var, RRule *rule);
/** \brief Decode the the rewrite rule and get JVar */
JVar decode_jvar(RRule *rule);
#endif
