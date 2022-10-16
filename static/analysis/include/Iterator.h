/*! \file Iterator.h
 *  \brief Loop Iterator Analysis
 *
 * It is used for identifying iterators in a loop
 * And also used for loop dependence analysis
 */
#ifndef _JANUS_ITERATOR_
#define _JANUS_ITERATOR_

#include "Expression.h"
#include "janus.h"
#include <iostream>

namespace janus
{
class VarState;
class Loop;
class Expr;
class ExpandedExpr;
class SCEV;

/** \brief An iterator represents a special class of Variable that controls the
 * loop iterator count
 *
 *  The most typical iterator is the induction variable
 *  Others can be a linked list node etc
 *  The loop iterator starts with a initial value and final value */
class Iterator
{
  public:
    enum IteratorKind {
        ITERATOR_NONE,
        INDUCTION_IMM,    // induction whose stride is immediate value
        INDUCTION_VAR,    // induction stride is a constant variable (stack,
                          // absolute)
        ITER_LINKED_LIST, // the iterator is a linked list
        ITER_GENERIC,     // the iterator update can have a fixed expression
        REDUCTION_PLUS    // a reduction variable with plus operation
    };
    enum ValueKind { NONE, INTEGER, SINGLE_VAR, EXPR, EXPANDED_EXPR };

    enum StepKind {
        UNDECIDED,
        CONSTANT_IMM,
        CONSTANT_VAR,
        CONSTANT_EXPR,
    };

    /** \brief Value of this variable */
    VarState *vs;
    /** \brief Type of the current expression */
    IteratorKind kind;
    /** \brief Its parent loop */
    Loop *loop;
    /** \brief Scalar evolution */
    SCEV *scev;
    /** \brief Main iterator of the loop */
    bool main;
    ValueKind strideKind;
    union {
        int64_t stride;
        VarState *strideVar;
        Expr strideExpr;
        ExpandedExpr *strideExprs;
    };

    ValueKind initKind;
    /// The initial value of the iterator, is typically the other path of the
    /// loop start phi
    union {
        int64_t initImm;
        VarState *initVar;
        Expr initExpr;
    };
    ExpandedExpr *initExprs;

    ValueKind finalKind;
    /// The final value of the iterator, is typically the value when comparison
    /// results lead outside of the loop
    union {
        int64_t finalImm;
        VarState *finalVar;
        Expr finalExpr;
    };
    ExpandedExpr *finalExprs;

    StepKind stepKind;
    /// The final value of the iterator, is typically the value when comparison
    /// results lead outside of the loop
    union {
        int64_t step;
        VarState *stepVar;
        Expr stepExpr;
    };
    ExpandedExpr *stepExprs;

    /// The variable state that stores the dynamic loop boundary
    VarState *checkState;

    Iterator() : kind(ITERATOR_NONE){};
    Iterator(VarState *vs, janus::Loop *loop);
    /// Return -1 if the iteration count is not a immediate value (statically
    /// undecided)
    int64_t getLoopIterationCount();

    Expr getInitExpr();
    Expr getStrideExpr();

    Instruction *getUpdateInstr();
};

bool operator<(const Iterator &iter1, const Iterator &iter2);
std::ostream &operator<<(std::ostream &out, const Iterator &iter);

} // end namespace janus

/** \brief Analyse all iterator variables in the loop */
bool iteratorAnalysis(janus::Loop *loop);
/** \brief Analyse all the leftover iterator variables in the loop (second pass)
 */
bool postIteratorAnalysis(janus::Loop *loop);
/** \brief Encode the loop iterator into encoded form */
void encodeIterators(janus::Loop *loop);
#endif
