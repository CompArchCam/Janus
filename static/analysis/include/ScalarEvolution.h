/*! \file ScalarEvolution.h
 *  \brief Scalar Evolution Analysis
 *
 * It is used for induction variable analysis and alias analysis
 */
#ifndef _Janus_SCEV_
#define _Janus_SCEV_

#include "janus.h"
#include "Variable.h"
#include <map>
#include <string>

namespace janus{
class VariableState;
class Loop;
class MemoryInstruction;
class SCExpr;

/**  \brief A SCScale class represents either an integer scale or a variable scale.
 *
 *   A **SCScale** class should be considered constant for the loop */
class SCScale {
public:
    long int       intScale;
    SCExpr        *scaleExpr;

    SCScale():intScale(0),scaleExpr(NULL){}
    SCScale(int scale):intScale(scale),scaleExpr(NULL){}
    SCScale(long int scale):intScale(scale),scaleExpr(NULL){}
    SCScale(VariableState *vs);
    SCScale(SCExpr *expr);
    ~SCScale(){};
    void            negate();
    void            absolute();
    ///Add the current scale with new scale
    void            add(SCScale scale2);
    ///Subtract the input scale from current scale
    void            subtract(SCScale scale2);
    ///Multiply the current scale with new scale
    void            multiply(SCScale scale2);
    ///Divide the current scale
    void            divide(int dividend);
};

bool operator==(SCScale &scale1, SCScale &expr2);
bool operator!=(SCScale &scale1, SCScale &expr2);

/**  \brief A SCExpr class represents a commutative collection of expressions.
 *
 *   Currently **SCExpr** is a summation of **Variable** times **SCScale** together.
 *   expr = v1 * s1 + v2 * s2 + v3 * s3 ... + C where v are **VariableState** and s are **SCScale**
 */
class SCExpr {
public:
    /** \brief 
     *  If first item == 1, that means a constant */
    std::map<VariableState *, SCScale> exprs;
    void                negate();
    ///Make all its scales to be positive
    void                absolute();
    ///Add one term to the expression vi * si
    void                addTerm(VariableState *vs, SCScale scale);
    ///Merge with another expression
    void                merge(SCExpr &expr);
    void                merge(SCExpr *expr);
    ///Subtract with expression
    void                subtract(SCExpr &expr);
    void                subtract(SCExpr *expr);
    ///Muliply with another expression
    void                multiply(SCExpr &expr);
    void                multiply(SCExpr *expr);
    void                multiply(SCScale scale);
    void                divide(int dividend);
    ///simplify the expressions, constant elimination
    void                simplify();
    ///Visualise the expression
    std::string         print();
    ///Return the value of loop's induction variables if the expression is evaluated as zero. Return -1 if failed.
    long int            solve(Loop *loop);
    ///Return whether the expression has the term
    bool                hasTerm(VariableState *state);
    ///Return whether the expression is constant
    bool                isConstant();
    ///Evaluate the maximum value of this expression, return -1 if not able to evaluate
    long int            evaluateMax();
    ///Return the size
    int                 size() {return exprs.size();}
    /** \brief Return the induction variable for this expression.
     *
     *  If there are multiple induction variables, it returns NULL */
    VariableState       *getInductionVar();
};

bool operator==(SCExpr &expr1, SCExpr &expr2);
bool operator!=(SCExpr &expr1, SCExpr &expr2);
SCExpr operator+(SCExpr &expr1, SCExpr &expr2);
SCExpr operator-(SCExpr &expr1, SCExpr &expr2);
SCExpr operator&(SCExpr &expr1, SCExpr &expr2);

enum SCEV_Type {
    ///Scalar evolution can't be constructed due to irregular memory access
    SCEV_FAIL = 0,
    ///The memory access is a regular array access.
    SCEV_AFFINE_ARRAY
};

/**  \brief A ScalarEvolution class represents a 3-item tuple: {base, op, stride} \@loop  
 *
 *  The **ScalarEvolution** class is used to represent the range of an array access in terms of
 *  {base, +, stride} \@loop.
 *
 *  The value of *v* can be evaluated as base + indvar * stride and indvar is the induction variable of the ScalarEvolution::loop . */
class ScalarEvolution {
private:
    ///The array access used for scalar evolution.
    Variable            myvar;

public:
    ///the type of the scalar evolution.
    SCEV_Type           type;
    ///The total scalar expression for this variable
    SCExpr              expression;
    ///The base expression for this variable
    SCExpr              baseExpr;
    ///The variable expression for this variable (MIV only)
    SCExpr              varExpr;
    ///The stride expression for this variable (the scale of the induction variable)
    SCExpr              strideExpr;
    ///quick reference to integer stride
    long int            intStride;
    VariableState       *inductVar;
    ///The loop to be evaluated (starting from its main induction variable)
    Loop                *loop;
    ///SSA graph has to be available to construct the ScalarEvolution
    ScalarEvolution():type(SCEV_FAIL){}
    ScalarEvolution(Variable var, MemoryInstruction *mi, Loop *loop);
    ///The variation range does not exceed one stride length
    bool                isPrivateToIteration();
    std::string         print();
};

} //namespace janus

bool
buildInductionStrideExpression(janus::VariableState *vs, janus::VariableState *induct, janus::SCScale &stride);

///construct a sclar expression in terms of the loop induction variable
bool
buildSCExpr(janus::VariableState *vs, janus::SCExpr &expr, janus::Loop *loop);

#endif