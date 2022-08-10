/* Janus Abstract Instruction Definition */

#ifndef _JANUS_INSTR_
#define _JANUS_INSTR_

#include "Utility.h"
#include "Variable.h"
#include <iostream>
#include <vector>

namespace janus
{

class MachineInstruction;
struct VarState;
class BasicBlock;
class Function;

/** \brief An abstract definition of a machine instruction.
 *
 * - The **Instruction** class should be machine independent. For machine
 * dependent instruction, see the **MachineInstruction** class.
 * - It contains a list of variable state.
 */
class Instruction
{
  public:
    /* The opcode is modified from the Instruction.def from LLVM IR */
    enum Opcode {
        /// If the opcode is undefined, it means Janus is not interested in its
        /// analysis, you can find the detailed opcode from
        /// **MachineInstruction**
        Undefined,
        /* Terminator Instructions - These instructions are used to terminate a
           basic block of the program. */
        DirectBranch,
        ConditionalBranch,
        Call,
        Return,
        Interupt,

        /* Standard BinOP operators. */
        Add,
        Sub,
        Mul,
        Div,
        Rem,
        Shl,
        LShr,
        AShr,
        And,
        Or,
        Xor,

        /* Standard UnOP operators. */
        Mov,
        Neg,
        Not,
        Load,
        Store,
        Fence,
        GetPointer,

        /* Special operators. */
        Compare,
        Phi,
        Nop
    };

    /// Generic type of the instruction
    Opcode opcode;
    /// Instruction identifier
    InstID id;
    /// Raw instruction that contains machine specific details
    MachineInstruction *minstr;
    /// Program counter
    PCAddress pc;

    /// Set of input variables in SSA form.
    std::vector<VarState *> inputs;
    /// Set of output variables in SSA form.
    std::vector<VarState *> outputs;
    /// Immediate control dependent instruction
    // std::vector<Instruction *> controlDep;
    /// Pointer to the belonging basic block
    BasicBlock *block;
    /// Register bitset of all the register it reads (implicitly and explicitly)
    RegSet regReads;
    /// Register bitset of all the register it writes (implicitly and
    /// explicitly)
    RegSet regWrites;

    Instruction();
    Instruction(MachineInstruction *minstr);

    /* The following API is machine independent */
    /// Return a vector of variables from the inputs
    void getInputs(std::vector<Variable> &v);
    /// Return true if the instruction is a control flow instruction including
    /// jumps, returns, interupts etc
    bool isControlFlow();
    /// Return true if the instruction is a conditional jump
    bool isConditionalJump();
    /// Return true if the instruction contains memory accesses (including
    /// stack)
    bool isMemoryAccess();
    /// Return true if the instruction contains SIMD/floating point instructions
    bool isVectorInstruction();
    /// Return the target instruction ID from the current instruction, this must
    /// be a branch within the same function, return -1 if not found
    auto getTargetInstID() -> int;
    /// Return the pointer to the function based on the jump/call target of this
    /// instruction
    auto getTargetFunction() -> Function *;
    /// Print the instruction into dot format
    void printDot(void *outputStream);
};

std::ostream &operator<<(std::ostream &out, const Instruction &instr);

/** \brief A wrapper for the **Instruction** that contains memory accesses
 *
 * - The **Instruction** class should be machine independent. For machine
 * dependent instruction, see the **MachineInstruction** class.
 * - It contains a list of variable state.
 */
class MemoryInstruction
{
  public:
    enum MemoryType { Unknown, Read, Write, ReadAndWrite, ReadAndRead };
    /// Type
    MemoryType type;
    /// The memory operand
    VarState *mem;
    /// The non-memory operand
    VarState *nonMem;
    /// Pointer back to raw instruction
    Instruction *instr;

    MemoryInstruction(Instruction *instr);
};

std::ostream &operator<<(std::ostream &out, const MemoryInstruction &minstr);

} // namespace janus
#endif
