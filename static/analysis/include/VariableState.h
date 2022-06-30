#ifndef _VARIABLE_STATE
#define _VARIABLE_STATE

#include "Variable.h"

namespace janus
{
class BasicBlock;
class MachineInstruction;
class UpdateSequence;
class Expr;

/** \brief Types of Variables in SSA form. See VariableState for details. */
enum VariableStateType {
    VARSTATE_INITIAL,
    VARSTATE_NORMAL,
    VARSTATE_PHI,
    VARSTATE_MEMORY_OPND,
    VARSTATE_STK_VAR,
    VARSTATE_RIP_VAR
};

/** \brief A single variable in SSA form, part of the SSA graph.
 *
 * The **VariableState** class is used to represent a single variable in SSA
 * Form. We represent the SSA form as a graph for each function. We can have the
 * following types of nodes (variables):
 *
 * - Normal variables (*assignments*) are defined in the code as the output of
 * some operation. Assignment nodes have their *lastModified* field set to point
 * to the **MachineInstruction** object that defined them.
 * - *Phi nodes* represent junctions, they contain a list of possible
 * predecessors in the field *phi*. They are guaranteed to have more than one
 * entry in *phi*. All phi nodes are bound to the **start** of the basic block
 * where the different definitions first merge (not to the block where the new
 * definition is first used).
 * - *Memory operands* represent the input address to a load instruction. If the
 * address is given in [base+offset] form, then they are linked through *phi* to
 * the base register (1 element), otherwise *phi* is empty. The
 * *Variable::isLoad()* function of their variable (getVar()) returns *true*.
 * - Any other variable is an *initial value*. These are the inputs to the
 * function or uninitialized registers that are used as inputs to some
 * instruction.
 */
class VariableState
{
  public:
    // var
    /** \brief Returns the underlying (non-SSA) Variable object.
     */
    Variable getVar();
    /** \brief Sets the underlying (non-SSA) Variable object.
     */
    void setVar(Variable v);

    /** \brief Pointer to the defining block.
     *
     * Pointer to the **BasicBlock** object this state corresponds to or is
     * defined in:
     *
     * - To the corresponding function's entry block for initial values
     * - To the defining instruction's block for normal variables and memory
     * operands
     * - To the block where the different definitions first merge for phi nodes.
     */
    BasicBlock *block;

    /** \brief For easy look up. */
    int id;
    /** \brief Version number of the same storage. */
    int version;
    /** \brief True iff this VariableState holds an immediate value (constant).
     */
    bool isConstant;
    /** \brief Currently unused
     */
    bool isInduction;
    /** \brief Set to the loop in which this variable is a secondary induction
     * variable
     *
     *  A secondary induction variable is a temporary variable that is
     * calculated only from induction variables and constant variables.
     *
     *  Unlike for induction variables no additional fields (such as
     * repInductionVar, constDifference, update) are set at the moment.
     */
    Loop *secondaryInductionLoop = NULL;
    /** \brief Pointer to the Loop in which this variable is an induction
     * variable.
     *
     * inductionLoop!=NULL if and only if it is an induction variable increased
     * by upd in every iteration of the loop pointed to.
     *
     * Note, that this loop is well-defined, and the variable is necessarily
     * constant in any nested loops.
     */
    Loop *inductionLoop = NULL;
    /** \brief Representative element of a group of equivalent induction
     * variables.
     *
     * Two variables are in the same group iff their updates are confirmed to be
     * equivalent, hence making them equal in every iteration up to a constant
     * difference. Each variable within the group has this field point to the
     * same value.
     *
     * Eg in the loop for(int i=0, j=1; i<100; i++,j++) {}, i and j are
     * equivalent up to the constant difference of 1.
     *
     * Currently only induction variables discovered together are considered
     * equal, ie only those that are connected via assignments.
     */
    VariableState *repInductionVar = NULL;
    /** \brief The constant difference from repInductionVar.
     *
     * This SSA variable can be obtained from repInductionVar by applying
     * constDifference.
     */
    UpdateSequence *constDifference = NULL;

    /** \brief Value added in each iteration.
     *
     * If inductionLoop!=NULL, then upd contains the update sequence for this
     * variable. This non-zero amount is added to the variable in every
     * iteration.
     *
     * Otherwise it equals NULL.
     */
    UpdateSequence *update = NULL;

    /** \brief Can the value loaded possibly depend on some memory value?
     *
     * This field is true if and only if there is at least one possible
     * definition (path), such that the current variable in this state depends
     * on some value loaded from the main memory. If this is set, it is not
     * certain that this variable is a constant or induction variable, despite
     * other fields implying so. (Ie the information provided by other fields
     * may rely on the assumption that (some) memory values remain constant, and
     * it would require complicated or impossible alias analysis to avoid the
     * false positives)
     */
    bool dependsOnMemory;

    /** \brief The variable state is not used as inputs for its subsequent
     * instructions.
     *
     * This field is true if the variable state or its subsequent states are not
     * used by downstream instructions. It means the information stored in this
     * state can be used as scratch space.
     */
    bool notUsed;

    /** \brief Pointer to the outermost loop (containing *block),
     *    in which this state is a constant for every iteration
     *
     * This field contains the **outermost** one of the loops, that contains
     * *block* in its body and the value of the variable is **known to remain
     * constant** for each iteration of the loop. Note, that it is guaranteed to
     * also remain constant in any loops contained within (nested in)
     * *constLoop*, as otherwise the inner loop would contain a phi node first.
     *
     * This field is intended to be accessed through
     * *Loop::isConstant(VariableState\*)*, which determines whether this
     * variable is constant in the given loop.
     *
     * Note: This field may contain false positives -- see *dependsOnMemory*.
     *
     * Note: Variable may still be constant in other loops.
     */
    Loop *constLoop = NULL;

    /** \brief This points to a state equal to this, or itself if it is not
     * constant in any loops.
     *
     * The value of this and equalsTo are the same (in every iteration).
     *
     * Note: false positives are possible, see *dependsOnMemory*.
     */
    VariableState *equalsTo;

    /** \brief Represent the variablestate in terms of an AST/Expression
     *
     * The pointer to expr can be used to evaluate the value range of this
     * variable state. Note that this pointer is only available after the AST
     * has been constructed.
     */
    Expr *expr;

    /** \brief Returns the representative element of the equal group.
     *
     * Finds the element r = equalsTo->equalsTo->... such that r->equalsTo==r. r
     * is called the representative element of the group.
     *
     * Sideeffect: compresses the path (links equalsTo of all states along the
     * path to r)
     *
     * Complexity: O(n). Amortized O(1) per query for a large number of queries.
     */
    VariableState *rep();

    /** \brief Returns whether two states (constant in the same loop) are
     * considered equal. */
    bool equals(VariableState *oth);

    /** \brief Returns the type of variable that this state is. */
    VariableStateType type();

    /** \brief Marks this *VariableState* object as a recognized stack-based
     * variable.
     *
     * Changes the type to VARSTATE_STK_VAR.
     */
    void markAsStackVariable();

    /*** Graph structure ***/
    /** \brief Instruction defining this variable (normal), *NULL* otherwise.
     *
     * If this is a *normal variable*, then lastModified contains the
     * instruction defining it. For any of the other types it is set to *NULL*;
     */
    MachineInstruction *lastModified;
    /** \brief List of predecessors (for phi nodes and memory operands).
     *
     * Contains the list of all possible states that can be inherited in this
     * variable (phi nodes).
     *
     * For memory operands of [base+offset] or [base\*scale+offset] form, phi
     * contains a single element pointing to the state of the base register when
     * the reference is made.
     *
     * For any other type of variable, phi is empty.
     */
    std::vector<VariableState *> phi;

    /** \brief Contains a list of instructions that process this variablestate
     * as one of their inputs.
     */
    std::vector<MachineInstruction *> dependants;
    /** \brief Phi nodes or memory operands *p* for which p->phi contains this
     *
     * Pointers to all states that can directly inherit the value of this state.
     * States in dependants are necessarily phi nodes or memory operands.
     */
    std::vector<VariableState *> linksToPhi;

    /** \brief Constructs a normal variable state */
    VariableState(Variable var, BasicBlock *block,
                  MachineInstruction *lastModified);
    /** \brief Constructs a phi node or memory operand with phi=*vec
     *
     * No copying is done, so vec must not go out of scope, be deleted manually
     * or reused (modified).
     */
    VariableState(Variable var, BasicBlock *block,
                  std::vector<VariableState *> *vec); // phi node
    /** \brief Constructs a variable state holding an initial value.
     *
     * This is also the constructor to be used to initialize an "empty" object,
     * which can be filled later.
     */
    VariableState(Variable var, BasicBlock *block); // initial value

  private:
    Variable var;
    bool is_STK_VAR;
};

std::ostream &operator<<(std::ostream &out, VariableState *vs);
std::ostream &operator<<(std::ostream &out, const VariableState &vs);
} // namespace janus

#endif
