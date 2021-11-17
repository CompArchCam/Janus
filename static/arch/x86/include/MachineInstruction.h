/* Janus Machine Instruction Definition */

#ifndef _Janus_MINSTR_
#define _Janus_MINSTR_

#include "janus.h"
#include "Operand.h"

namespace janus {
    typedef uint32_t InstID;
    typedef uint32_t InstOp;

    /* Maximum 8 coarse types
     * an instruction may have many coarse types */
    enum MachineInstCoarseType
    {
        INSN_NORMAL  = 0,
        INSN_CONTROL = 1,   //0. instructions that modify PC
        INSN_MEMORY  = 2,   //1. explicit memory instructions
        INSN_FP      = 4,   //2. instructions that contain floating point registers
        INSN_STACK   = 8,   //3. now we want to separate stacks
        INSN_COMPLEX = 16   //4. other complex instructions
    };

    /* Maximum 8 coarse types
     * an instruction can only have one fine type */
    enum MachineInstFineType
    {
        INSN_NORMAL2 = 0,
        INSN_CMP,      //explicit modify RFLAGs
        INSN_CJUMP,
        INSN_JUMP,
        INSN_CALL,
        INSN_RET,
        INSN_INT,
        INSN_IRET,
        INSN_NOP,
        INSN_OTHER        
    };

    /* Janus Machine Instruction Class
     * Minimise the space as small as possible */
    class Operand;
    class BasicBlock;

    class MachineInstruction
    {
    public: 
        InstID                          id;             //identifier for lookup
        uint32_t                        coarseType:8;
        MachineInstFineType             fineType:8;
        uint32_t                        size:8;
        uint32_t                        aux:8;
        InstOp                          opcode:16;
        uint32_t                        opndCount:16;
        Operand                         *operands;      //array of operands
        uint64_t                        pc;
        GBitMask                        regReads;       //64-bit bit mask
        GBitMask                        regWrites;      //including GPR and SIMD

        /* XXX: having double free problem if using std::string on newer version of c++ */
        char                            *name;
        //we make the cast internally
        MachineInstruction(InstID id, void *cs_handle, void *cs_instr);

        /* internal query */
        bool                            isType(MachineInstCoarseType type);
        bool                            useReg(uint32_t reg);
        bool                            readReg(uint32_t reg);
        bool                            writeReg(uint32_t reg);
        PCAddress                       getTargetAddress();
        bool                            hasMemoryAccess();
        bool                            hasMemoryReference();
        /* instruction query functions */
        int                             isMoveStackPointer();
        bool                            isIndirectCall();
        bool                            isMakingStackBase();
        bool                            isRecoveringStackBase();
        bool                            isConditionalMove();
        bool                            isCall();
        bool                            isReturn();

        /** \brief returns true if the instruction modifies EFLAG registers */
        bool                            writeControlFlag();
        /** \brief returns true if the instruction modifies EFLAG registers */
        bool                            readControlFlag();
        /** \brief returns whether this is a MOV instruction */
        bool                            isMOV();
        /** \brief returns whether this is a MOV immediate instruction */
        bool                            isMOV_imm(int *imm);
        /** \brief returns whether this is a partial MOV instruction: X[32:127] = X[32:127]; X[0:31] = Y */
        bool                            isMOV_partial();
        /** \brief returns whether this is an increment instruction */
        bool                            isINC();
        /** \brief returns whether this is a decrement instruction */
        bool                            isDEC();
        /** \brief returns whether this is an add instruction */
        bool                            isADD();
        /** \brief returns whether this is an sub instruction */
        bool                            isSUB();
        /** \brief returns whether this is a negate instruction */
        bool                            isNEG();
        /** \brief returns whether this is a push instruction decreasing the stack pointer */
        bool                            isPUSH();
        /** \brief returns whether this is a pop instruction increasing the stack pointer */
        bool                            isPOP();
        /** \brief returns whether this is an example of using LEA for addition/subtraction
         *         to the given variable.
         *
         *  Returns true iff this is a LEA instruction in which the given variable appears
         *  without scaling, ie the result is the given variable with a factor of 1 plus/minus
         *  some other variable and/or numeric constant.
         */
        bool                            isLEA_add_to(Variable var);
        /** \brief returns whether this is a load effective address */
        bool                            isLEA();
        /** \brief returns whether this is an xor instruction for setting zeros*/
        bool                            isXORself();
        /** \brief returns whether this is a type change instruction such as cdqe */
        bool                            isTypeChange();
        /** \brief returns positive if the instruction is a shift right instruction, the shift right immediate value */
        int                             isShiftRight();
        int                             isShiftLeft();
        /** \brief returns whether this is a compare instruction */
        bool                            isCMP();
        /** \brief returns whether this is a multiply instruction */
        bool                            isMUL();
        /** \brief returns whether this is a divide instruction */
        bool                            isDIV();
        /** \brief returns whether this is a vector related instruction */
        bool                            isVectorInstruction();
        /** \brief returns whether this instruction uses xmm, ymm, or zmm registers */
        bool                            isXMMInstruction();
        /** \brief returns whether this instructions uses the FPU register stack */
        bool                            isFPU();
        /** \brief returns whether this instruction reads/writes any register (excluding EFLAGS) 
         *         which is not given as an operand.
         */
        bool                            hasImplicitOperands();
        /** \brief returns true if this instruction is not supported by Janus runtime */
        bool                            isNotSupported();
    };

    std::ostream& operator<<(std::ostream& out, const MachineInstruction& instr);
} //end namespace janus

#endif
