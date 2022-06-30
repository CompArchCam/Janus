#ifndef _Janus_OPERAND_
#define _Janus_OPERAND_

#include "Variable.h"
#include "janus.h"
#include <iostream>
#include <string>

namespace janus
{
// ----------------------------------------------------------
//      Definition of a Janus basic operand
// ----------------------------------------------------------
enum OperandType {
    OPND_INVALID = 0, // uninitialized/invalid operand.
    OPND_REG,         // Register operand.
    OPND_IMM,         // Immediate operand.
    OPND_MEM,         // Memory operand.
    OPND_FP           // Floating-Point operand.
};

enum OperandAccess { OPND_READ = 1, OPND_WRITE = 2, OPND_READ_WRITE = 3 };

enum OperandStruct {
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
 * We won't use the structure from capstone since we need to buffer all operands
 * in memory It is better to minimise the size of this structure */
class Operand
{
  public:
    OperandType type : 8;        // OperandType
    OperandStruct structure : 8; // Further diversion if needed
    size_t size : 8;             // data width
    uint8_t access;              // READ, WRITE or READ_WRITE w.r.t GMInst

    /* Different types of memory addressing mode
     * TODO: move this to x86 specific file */
    struct FullMem {
        uint8_t base;   // base register
        uint8_t index;  // index register
        uint16_t scale; // scale for the index register
        int32_t disp;   // 16-bit displacement
    };

    struct DispMem {
        uint32_t base; // base register
        int32_t disp;  // 32-bit displacement
    };

    struct SegMem {
        uint8_t segment; // segment register
        uint8_t base;    // index register
        uint8_t index;   // index register
        int32_t disp;    // 32-bit displacement
    };

    // For other addressing mode, you can fill in the generic value structure
    union {
        uint64_t reg;    // pure register operand
        FullMem fullMem; // full memory addressing mode
        DispMem dispMem; // long displacement addressing mode
        SegMem segMem;   // segment addressing mode
        uint64_t value; // based on the structure, we have different decoding on
                        // this value
        int64_t intImm; // pure integer immediate operand
        double fpImm;   // pure floating point immediate operand
    };

    Operand();
    Operand(void *capstone_opnd); // capstone operand structure

    bool isVectorRegister();
    bool isXMMRegister();
    bool isMemoryAccess();
    /* We use a 64-bit integer to represent the variable names
     * used in the loop. A variable includes a register, stack location
     * and PC-relative heap memory accesses */
    Variable lift(PCAddress pc);
};

std::ostream &operator<<(std::ostream &out, const Operand &v);
} // end namespace janus
#endif
