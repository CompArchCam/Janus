#ifndef _UPDATE_SEQUENCE
#define _UPDATE_SEQUENCE

#include "Variable.h"
#include "VariableState.h"
#include "new_instr.h"
#include <map>
#include <vector>

using namespace std;

namespace janus{

/** \brief Class to represent the sequence of add and sub updates to a variable in a loop.
 */
class UpdateSequence {
    public:
    /** \brief Adds a new update.
     *
     * The multiplier should be +1 for the add operation and -1 for the sub operation, but other
     * values are also processed.
     */
    void addUpdate(VariableState *vs, long long multiplier);
    /** \brief Adds an update adding *value* to the variable.
     *
     * Same as calling addUpdate for a *vs* containing the constant and *multiplier==1*
     */
    void addConstUpdate(long long value);

    /** \brief Constructs an empty update sequence. */
    UpdateSequence();
    /** \brief Copy constructor */
    UpdateSequence(const UpdateSequence &c);

    /** \brief Equals operator. Returns true if the two update sequences are equivalent
     *
     * Compares the two objects, and returns true iff it is *able to* establish a bijection between
     * the update operations, hence making the update sequences mathematically equivalent.
     *
     * Note, that returning false only means that the analysis couldn't prove equivalence, not that
     * there is no equivalence.
     * 
     * Note, that there might be false positives as well -- see *VariableState::dependsOnMemory*.
     */
    bool operator==(const UpdateSequence &other) const;
    /** \brief Returns the compliment of this->operator==(other). */
    bool operator!=(const UpdateSequence &other) const;

    /** \brief Returns the difference of the two update sequences.
     *
     * Returns a valid UpdateSequence *ret*, such that applying *ret* and *oth* to some variable
     * is equivalent to applying *this* to it.
     */
    inline UpdateSequence& operator-(const UpdateSequence &oth) const {
        UpdateSequence *out = new UpdateSequence(*this);

        out->constupd -= oth.constupd;

        for (auto &p : oth.multiplier)
            out->addUpdate(p.first,-p.second);
        return *out;
    }

    /** \brief Generates instructions to apply this sequence of updates to the variable with ID 
     *         *in* at the specified position. Returns the ID of the result variable.
     *
     * Precondition: bb must be within a loop, and all updates must be constant with respect to
     * bb->parentLoop.
     */
    int compute(BasicBlock *bb, int index, int in, vector<new_instr*> &gen, int &varid);

    /** \brief Generates instructions to calculate this sequence of updates times N. Returns an
     *         operand for a new_instr.
     *
     * Return value: The returned operand is either a register operand or an immediate value
     *               operand. In the former case the second part of the pair contains the abstract
     *               variable which should replace the register, in the latter case the second part
     *               equals -1.
     */
    pair<Operand, int> computeNtimes(BasicBlock *bb, int index, int64_t N,
                                                            vector<new_instr*> &gen, int &varid);

    private:
    /* Maps a VariableState to the number of times it is added (negative for subtraction) */
    map<VariableState*,int> multiplier;
    /* (Numeric) constant added in every iteration */
    long long constupd = 0ll;
};


} //namespace janus

#endif
