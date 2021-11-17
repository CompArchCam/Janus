#ifndef __new_instr
#define __new_instr

#include "MachineInstruction.h"
#include <map>
#include <vector>
#include "capstone/x86.h"

#define Instruction x86_insn

/** \brief Types of new_instr.
 *
 * See *new_instr* for details
 */
enum new_instr_type {
    NEWINSTR_DEF,
    NEWINSTR_FIXED,
    NEWINSTR_OTHER
};

/** \brief New instructions to be inserted
 *
 * Newly created instructions waiting to be inserted. 
 *
 * These instructions are not yet translated to actual registers, and hence they operate on  
 * abstract variables, identified by a unique int ID. The computation has the following form:
 *
 * Each variable used from the original code is first *defined*, ie assigned a variable ID. This is
 * achieved by inserting a new_instr with *opcode==-1* and the *def* field containing the ID and
 * the variable to be used. This just assigns the variable ID, no implicit copying is done.
 *
 * Then each instruction in the following computation is described by a valid *opcode* (machine
 * instruction) and a list of an appropriate number of operands (*opnds*). Each operand is given
 * by a pair of an *Operand* and an ID. If the Operand contains no register access, the ID is -1.
 * Otherwise the pair is interpreted as exactly the given operand, but using whichever register is
 * assigned to the abstract variable at that time in execution instead of the register specified
 * within the Operand. (This includes simple register accesses, memory addresses in base+offset
 * form, etc -- Eg if at runtime variable #15 is stored in register R9, then the pair
 * ([2*RAX + 0xFF], 15) is interpreted as [2*R9 + 0xFF])
 *
 * The outputs of real (non-definition) instructions are also stored to ease future analysis.
 */
class new_instr {
  public:
    /** \brief Valid capstone machine instruction opcode (real instructions) or X86_INS_INVALID (definitions).
     */
    Instruction opcode;
    /** \brief Pointer to the original MachineInstruction object, if this is a duplication of 
     *         existing calculation.
     *
     * Otherwise NULL. (When this value is NULL, the opcode has to be X86_INS_MOV, X86_INS_ADD, 
     * X86_INS_SUB or X86_INS_IMUL).
     */
    janus::MachineInstruction *cloned = NULL;
    /** \brief Operands -- see class description for details (real instructions)
     *
     * These are the operands to the machine instruction in order.
     *
     * Only explicit operands are listed here. If there are implicit operands, *fixedRegister* is 
     * set, and *def* contains all operands.
     */
    std::vector<std::pair<janus::Operand,int> > opnds;
    /** \brief True if the instruction can not be relocated, since it uses specific registers.
     *
     * Some instructions use specific registers (which do not depend on the operands), some 
     * others have relocatable alternatives. This field is set to true if we can't or don't want 
     * to relocate the instruction.
     * 
     * If set, the instruction will be translated in such a way, that all implicit and explicit
     * operands occupy the same location as in the original code.
     */
    bool fixedRegisters = false;

    /** \brief Abstract variables read (real instructions) */
    std::vector<int> inputs;
    /** \brief Abstract variables modified (real instructions) */
    std::vector<int> outputs;
    /** \brief Normal variable -- variable ID assignments (definitions and fixed register
     *         instructions!)
     *
     * In the case of fixed-register instructions, this holds all pairings, for all input **and**
     * output variables.
     */
    std::map<int,janus::Variable> def;

    /** \brief Program counter */
    uint64_t pc;

    new_instr(Instruction opcode, uint64_t pc): opcode(opcode), pc(pc) {};
    new_instr(janus::MachineInstruction *mi, uint64_t pc): opcode((x86_insn)mi->opcode), pc(pc),
                                                                                    cloned(mi) {
        fixedRegisters = mi->hasImplicitOperands();
    };
    new_instr(uint64_t pc): opcode(X86_INS_INVALID), pc(pc) {};

    /** \brief Adds a new operand, which simply consists of the value of the given variable */
    void push_var_opnd(int id);
    /** \brief Pushes the given original operand with the register being replaced by the variable 
     *         ID given. */
    inline void push_opnd(int id, janus::Operand &original) { opnds.push_back({original,id}); }

    /** \brief Returns a human-readable representation of the instruction/definition */
    std::string print();

    /** \brief Returns the set of registers read by this instruction defined in the original code.
     *
     * For definitions, returns the set of registers assigned to a new variable.
     */
    uint32_t originalRegsRead();

    /** \brief Returns the type of this instruction (definition, fixed register instruction or other instruction). */
    new_instr_type type();

    /** \brief Returns true iff the instruction accesses memory.
     *
     * Only genuine accesses are counted, reference-only (eg LEA) instructions do not return true.
     */
    bool hasMemoryAccess();
};

#endif
