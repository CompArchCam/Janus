/** \file MemoryLocation.h
 *  \brief Specification and analysis related to memory location */
#ifndef _JANUS_MEMORY_LOCATION_
#define _JANUS_MEMORY_LOCATION_

#include "Expression.h"
#include "Variable.h"
#include <map>
#include <set>
#include <string>

namespace janus
{
class Instruction;
class MemoryInstruction;
class Iterator;
class SCEV;
class ExpandedSCEV;

/** \brief Represents a memory location/ a close form group of locations in SSA
 * form in a loop */
class MemoryLocation
{
  public:
    /** \brief The type of a loop memory location */
    enum AddressType {
        /// The memory address is unknown (no expression built)
        Unknown,
        /// The memory address is a linear function of loop iterators addr = A*i
        /// + B (A is non-zero)
        Affine1D,
        /// The memory address is a linear function of multiple loop iterators
        /// addr = A*i + B*j + C*k + D (A could be zero)
        AffinemD,
        /// The memory address is a constant (that does not vary between loop
        /// iterations)
        Constant,
        /// The memory address is complicated (non-linear space)
        Complex
    };
    /** \brief The belonging loop */
    Loop *loop;
    /** \brief The type of the memory address */
    AddressType type;
    /** \brief Expanded expression for the memory address */
    ExpandedExpr expr;
    /** \brief Scalar evolution of this memory address, call **getSCEV** to set
     * this value */
    SCEV *scev;
    /** \brief Expanded scalar evolution of this address */
    ExpandedSCEV *escev;
    /** \brief Pointer to the memory variable in the SSA graph */
    VarState *vs;
    /** \brief Set of instructions that reads this memory location */
    std::set<Instruction *> readBy;
    /** \brief Set of instructions that updates this memory location */
    std::set<Instruction *> writeFrom;
    MemoryLocation()
        : type(Complex), expr(ExpandedExpr::SUM), vs(NULL), loop(NULL){};
    MemoryLocation(MemoryInstruction *mi, Loop *loop);
    /** \brief Get scalar evolution (symbolic range) of this memory address */
    void getSCEV();
    /** \brief Returns true if the memory location is a function of an iterator
     * of the specified loop */
    bool containIterator(Loop *loop);
    /** \brief Returns if the memory address is based on an another memory load,
     * otherwise return NULL */
    VarState *getIndirectMemory();

    /** \brief Tells whether two memory locations are alias */
    // MemLocationAliasType             alias(MemoryLocation &location);
    /** \brief The address of this location is not determined by the induction
     * variable of the loop */
    // bool                             isIndependentOfInduction(Loop *loop);
};

/** \brief The less than operand is only used for sorting memory locations in a
 * set */
bool operator<(const MemoryLocation &a, const MemoryLocation &b);
/** \brief Two memory locations are only equal when its scalar evolution are
 * exactly the same */
bool operator==(const MemoryLocation &a, const MemoryLocation &b);

std::ostream &operator<<(std::ostream &out, const MemoryLocation &mloc);

/** \brief Represents an add recurrences which can be represented as [start,
 * stride]@iter */
class SCEV
{
  public:
    enum SCEVType {
        /// The scalar evolution expression could not be built
        Ambiguous,
        /// Normal
        Normal
    };

    Expr start;
    Expr stride;
    Iterator *iter;
    SCEVType kind;

    SCEV() : start(0), stride(0), iter(NULL){};
    SCEV(Expr start, Expr stride, Iterator *iter);
    SCEV(MemoryLocation *mloc, Loop *loop);

    void addScalarTerm(Expr expr);
    void addScalarTerm(Expr expr, Expr coeff);
    void addStrideTerm(Expr expr);
    void multiply(Expr expr);
};

std::ostream &operator<<(std::ostream &out, const SCEV &scev);

/** \brief Represents an expanded add recurrences: {start + A*i + B*j + C*k} */
class ExpandedSCEV
{
  public:
    enum ESCEVType {
        /// The scalar evolution expression could not be built
        Ambiguous,
        /// Normal
        Normal
    };
    ESCEVType kind;
    /** \brief variable components */
    std::map<Iterator *, Expr> strides;
    /** \brief scalar base of the scev (no iterators) */
    Expr start;
    ExpandedSCEV() : start(0){};
    ExpandedSCEV(SCEV &scev);

    /** \brief return the iterator of the current loop */
    Iterator *getCurrentIterator(Loop *loop);
    /** \brief return the symbolic part of the scev start (for runtime check) */
    Expr getArrayBase();

    /** \brief evaluate the distance if the iterator is incremented by 1 */
    int evaluate1StepDistance(Loop *loop);
};

std::ostream &operator<<(std::ostream &out, const ExpandedSCEV &escev);

} // namespace janus

/** \brief Get range difference w.r.t the given loop */
janus::ExpandedSCEV getRangeDiff(janus::ExpandedSCEV &e1,
                                 janus::ExpandedSCEV &e2, janus::Loop *loop);
#endif