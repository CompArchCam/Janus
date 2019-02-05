/* Janus Abstract Variable Definition */

#ifndef _JANUS_VARIABLE_STATIC_
#define _JANUS_VARIABLE_STATIC_
#include "janus.h"

#include <string>
#include <vector>
#include <set>

#define FLAGS_REGISTER JREG_EFLAGS

namespace janus {

class Loop;
class VariableState;
class Instruction;
class Expr;
class BasicBlock;

/** \brief C++ wrapper of shared Janus Variable structure */
struct Variable : JVar
{
    Variable();
    Variable(const JVar &var);
    Variable(uint32_t reg);
    Variable(uint64_t value);
};

bool operator<(const Variable &a, const Variable &b);
bool operator==(const Variable &a, const Variable &b);
std::ostream& operator<<(std::ostream& out, const Variable& v);

/** \brief A single variable in SSA form, part of the SSA graph. */
struct VarState : Variable {
    /** \brief Each state has its distinct id. */
    int                     id;
    /** \brief Version number of the same storage. */
    int                     version;
    /** \brief Whether the state is a phi variable */
    bool                    isPHI;
    /** \brief Whether the state is not used by its dominating blocks */
    bool                    notUsed;
    /** \brief **Instruction** defining this variable, *NULL* otherwise.*/ 
    Instruction             *lastModified;
    /** \brief List of predecessors (for phi nodes and memory operands).
     *
     * Contains the list of all possible states that can be inherited in this variable (phi
     * nodes).
     * 
     * For memory operands of [base+offset] or [base\*scale+offset] form, phi contains a single
     * element pointing to the state of the base register when the reference is made.
     *
     * For any other type of variable, phi is empty.
     */
    std::set<VarState*>     pred;
    /** \brief Contains a list of instructions that process this variablestate as one of their 
     *         inputs */
    std::set<Instruction*>  dependants;
    /** \brief Pointer to the defining block.
     *
     * Pointer to the **BasicBlock** object this state corresponds to or is defined in:
     *
     * - To the corresponding function's entry block for initial values
     * - To the defining instruction's block for normal variables and memory operands
     * - To the block where the different definitions first merge for phi nodes.
     */
    BasicBlock              *block;
    /** \brief Represent the variable state in terms of an AST/Expression
     *
     * The pointer to expr can be used to evaluate the value range of this variable state.
     * Note that this pointer is only available after the AST has been constructed.
     */
    Expr                    *expr;
    /** \brief Pointer to the outermost loop (containing *block),
     *    in which this state is a constant for every iteration
     *
     * This field contains the **outermost** one of the loops, that contains *block* in its
     * body and the value of the variable is **known to remain constant** for each iteration of
     * the loop. Note, that it is guaranteed to also remain constant in any loops contained within
     * (nested in) *constLoop*, as otherwise the inner loop would contain a phi node first.
     *
     * This field is intended to be accessed through *Loop::isConstant(VarState\*)*, 
     * which determines whether this variable is constant in the given loop.
     *
     * Note: This field may contain false positives -- see *dependsOnMemory*.
     *
     * Note: Variable may still be constant in other loops.
     */
    Loop                    *constLoop;
    /** \brief Can the value loaded possibly depend on some memory value?
     *
     * This field is true if and only if there is at least one possible definition (path), such
     * that the current variable in this state depends on some value loaded from the main memory.
     * If this is set, it is not certain that this variable is a constant or induction variable,
     * despite other fields implying so. (Ie the information provided by other fields may rely on
     * the assumption that (some) memory values remain constant, and it would require complicated
     * or impossible alias analysis to avoid the false positives)
     */
    bool                    dependsOnMemory;
    /** \brief Constructs an empty variable */
    VarState();
    VarState(Variable var);
    VarState(Variable var, BasicBlock *block, Instruction* lastModified);
    VarState(Variable var, BasicBlock *block, bool isPHI);
};

std::ostream& operator<<(std::ostream& out, VarState* vs);
std::ostream& operator<<(std::ostream& out, VarState& vs);
std::ostream& operator<<(std::ostream& out, JVarProfile &vp);

} /* END Janus namespace */

#endif
