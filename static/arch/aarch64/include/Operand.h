#ifndef _Janus_OPERAND_
#define _Janus_OPERAND_

#include "janus.h"
#include "Variable.h"
#include <string>
#include <iostream>

namespace janus {
// ----------------------------------------------------------
//      Definition of a Janus basic operand
// ----------------------------------------------------------
enum OperandType
{
    OPND_INVALID = 0,  // uninitialized/invalid operand.
    OPND_REG,          // Register operand.
    OPND_IMM,          // Immediate operand.
    OPND_MEM,          // Memory operand.
    OPND_FP,           // Floating-Point operand.
    OPND_CIMM = 64,    // C-Immediate
    OPND_REG_MRS,      // MRS register operand.
    OPND_REG_MSR,      // MSR register operand.
    OPND_PSTATE,       // PState operand.
    OPND_SYS,          // SYS operand for IC/DC/AT/TLBI instructions.
    OPND_PREFETCH,     // Prefetch operand (PRFM).
    OPND_BARRIER,      // Memory barrier operand (ISB/DMB/DSB instructions).
};

enum OperandAccess
{
    OPND_READ = 1,
    OPND_WRITE = 2,
    OPND_READ_WRITE = 3
};

enum OperandStruct
{
    OPND_NONE = 0,
    OPND_SEGMEM = 1,
    OPND_DISPMEM = 2,
    OPND_FULLMEM = 3,
    STACK_PTR_IMM = 4,
    STACK_BASE_IMM = 5,
    STACK_PTR_IND = 6,
    STACK_BASE_IND = 7
};

/* Each operand is 128-bit/16 bytes long
 * We won't use the structure from capstone since we need to buffer all operands in memory
 * It is better to minimise the size of this structure */
class Operand
{
public:
    OperandType         type:8;     //OperandType
    OperandStruct       structure:8;//Further diversion if needed
    size_t              size:8;     //data width
    uint8_t             access;     //READ, WRITE or READ_WRITE w.r.t GMInst

    void setRegister(uint64_t reg);

    struct Mem {
        uint32_t            base;       //base register
        uint32_t            index;      //index register
        int32_t             disp;       //displacement
    };

    struct Shift{
        uint8_t type;
        uint8_t value;
    };

    //For other addressing mode, you can fill in the generic value structure
    union {
        uint64_t            reg;        //pure register operand
        Mem                 mem;        //base/index/disp for a memory access
        uint64_t            value;      //based on the structure, we have different decoding on this value
        int64_t             intImm;     //pure integer immediate operand
        double              fpImm;      //pure floating point immediate operand
        uint64_t            other;      /* blanket uint64_t for:
                                              PState for MSR instruction
                                            | IC/DC/AT/TLBI operation
                                            | PRFM (prefetch) operation
                                            | Memory barrier operation (ISB/DMB/DSB instr) */
    };

    Shift shift;

    Operand();
    Operand(void *capstone_opnd);       //capstone operand structure

    bool                    isVectorRegister(); 
    bool                    isXMMRegister();
    bool                    isMemoryAccess();
    /* We use a 64-bit integer to represent the variable names
     * used in the loop. A variable includes a register, stack location
     * and PC-relative heap memory accesses */
    Variable                lift(PCAddress pc);
};

std::ostream& operator<<(std::ostream& out, const Operand& v);
} //end namespace janus
#endif
