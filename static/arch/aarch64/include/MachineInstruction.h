/* Janus Machine Instruction Definition */

#ifndef _Janus_MINSTR_
#define _Janus_MINSTR_

#include "Operand.h"
#include "janus.h"

namespace janus
{
typedef uint32_t InstID;
typedef uint32_t InstOp;

/* Maximum 8 coarse types
 * an instruction may have many coarse types */
enum MachineInstCoarseType {
    INSN_NORMAL = 0,
    INSN_CONTROL = 1,  // 0. instructions that modify PC
    INSN_MEMORY = 2,   // 1. explicit memory instructions
    INSN_FP = 4,       // 2. instructions that contain floating point registers
    INSN_STACK = 8,    // 3. now we want to separate stacks
    INSN_COMPLEX = 16, // 4. other complex instructions
    INSN_PREINDEX =
        32, // 5. instructions using the preindex addressing mode (AArch64)
    INSN_POSTINDEX =
        64, // 6. instructions using the postindex addressing mode (AArch64)
    /* NOTE: no instruction can use post and pre indexing (currently) so if this
       was redesigned for AArch64 they should be mutually exclusive */
};

/* Maximum 8 coarse types
 * an instruction can only have one fine type */
enum MachineInstFineType {
    INSN_NORMAL2 = 0,
    INSN_CMP, // explicit modify RFLAGs
    INSN_CJUMP,
    INSN_JUMP,
    INSN_CALL,
    INSN_RET,
    INSN_INT,
    INSN_IRET,
    INSN_NOP,
    INSN_OTHER
};

class Operand;
class BasicBlock;

/* Janus Machine Instruction Class
 * Minimise the space as small as possible */

class MachineInstruction
{
  public:
    InstID id; // identifier for lookup
    uint32_t coarseType : 8;
    MachineInstFineType fineType : 8;
    uint32_t size : 8;
    uint32_t aux : 8;
    InstOp opcode : 16;
    uint32_t opndCount : 16;
    Operand *operands; // array of operands
    uint64_t pc;
    GBitMask regReads;  // 64-bit bit mask
    GBitMask regWrites; // including GPR and SIMD

    /* XXX: having double free problem if using std::string on newer version of
     * c++ */
    char *name;
    // we make the cast internally
    MachineInstruction(InstID id, void *cs_handle, void *cs_instr);

    /* internal query */
    bool isType(MachineInstCoarseType type);
    bool useReg(uint32_t reg);
    bool readReg(uint32_t reg);
    bool writeReg(uint32_t reg);
    PCAddress getTargetAddress();
    bool hasMemoryAccess();
    bool hasMemoryReference();
    /* instruction query functions - used by analyseStack */
    bool isAllocatingStackFrame();
    int getStackFrameAllocSize();
    bool isSubSP();
    int getSPSubSize();
    bool isMoveSPToX29();
    bool isDeallocatingStackFrame();
    int getStackFrameDeallocSize();
    bool isX29MemAccess();

    void setUsesPropagatedStackFrame(bool value);
    bool usesPropagatedStackFrame();
    /*
    int                             isMoveStackPointer();
    bool                            isIndirectCall();
    bool                            isMakingStackBase();
    bool                            isRecoveringStackBase();
    bool                            isConditionalMove();
    bool                            isCall();
    bool                            isReturn();
    */

    /** \brief returns true if the instruction modifies EFLAG registers */
    bool writeControlFlag();
    /** \brief returns true if the instruction modifies EFLAG registers */
    bool readControlFlag();
    /** \brief returns whether this is a vector related instruction */
    /*bool                            isVectorInstruction();*/

    /* bool                            isXMMInstruction(); - Not relevant for
       ARM. Could instead check for ARM NEON SIMD instructions if needed. */

    /** \brief returns whether this instruction reads/writes any register
     * (excluding EFLAGS) which is not given as an operand.
     */
    /*bool                            hasImplicitOperands();*/
    /** \brief returns true if this instruction is not supported by Janus
     * runtime */
    /*bool                            isNotSupported();*/
};

std::ostream &operator<<(std::ostream &out, const MachineInstruction &instr);
} // end namespace janus

#endif
