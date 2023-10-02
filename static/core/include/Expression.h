/* Janus Expression/ Abstract Syntax Tree */

#ifndef _JANUS_EXPR_
#define _JANUS_EXPR_

#include "janus.h"
#include <set>
#include <map>
#include <string>
#include <iostream>
#include <utility>

namespace janus {
class Instruction;
class VarState;
class Function;
class Loop;
class ExpandedExpr;
class Iterator;
class Variable;
class SCEV;

/** \brief An abstract instruction/statement that represents a data flow operation
 *
 */
class Expr {
public:
    enum ExprKind
    {
        NONE = 0,      // Invalid/uninitialized 
        INTEGER,       // Integer/Immediate values
        VAR,           // Variable:Register/Stack .
        MEM,           // Memory/Memory Reference
        UNARY,         // Unary operations
        BINARY,        // Binary operations
        PHI,           // Phi operations
        CALL,          // Call operations
        ADDREC,        // Add recurrence operations
        EXPANDED       // Expanded expression
    };

    enum OpType
    {
        UDF = 0,        //Undefined, used for unrecognized instructions
        NEG,            // Negate type
        MOV,            // Move unary type
        ADD,            // Add operations
        SUB,            // Subtract operations
        MUL,            // Multiply operations
        DIV,            // Divide operations
        CMP,            // Compare operations
        SHL,            // Left shift
        LSR,            // Logical right shift
        ASR,            // Arithmetic right shift
        BIT             // Bitwise operation
    };

    struct UnaryExpr {
        Expr                *e;
        OpType              op;
    };

    struct BinaryExpr {
        Expr                *e1;
        Expr                *e2;
        OpType              op;
    };

    struct PhiExpr {
        Expr                *e1;
        Expr                *e2;
    };

    struct AddRecExpr {
        Expr                *base;
        Expr                *stride;
        Iterator            *iter;
    };
    //TODO: call arguments

    /** \brief Type of the current expression */
    ExprKind                kind;
    /** \brief Link to the original variable state */
    VarState                *vs;
    /** \brief Link to the expanded form in cyclic scope */
    mutable
    ExpandedExpr            *expandedCyclicForm;
    /** \brief Link to the expanded form in loop scope */
    mutable
    ExpandedExpr            *expandedLoopForm;
    /** \brief Link to the expanded form in function scope */
    mutable
    ExpandedExpr            *expandedFuncForm;
    /** \brief The expression is an iterator of loop */
    mutable
    Iterator                *iteratorTo;
    union {
        /** \brief Integer expression */
        int64_t             i;
        /** \brief Variable (SSA) expression */
        VarState            *v;
        /** \brief Memory reference expression */
        Expr                *m;
        /** \brief Unary reference expression */
        UnaryExpr           u;
        /** \brief Binary reference expression */
        BinaryExpr          b;
        /** \brief Phi reference expression */
        PhiExpr             p;
        /** \brief Add recurrence expression */
        AddRecExpr          rec;
        /** \brief A pointer to expanded expression */
        ExpandedExpr        *ee;
    };

    Expr():kind(NONE),vs(NULL),expandedLoopForm(NULL),expandedFuncForm(NULL),expandedCyclicForm(NULL),iteratorTo(NULL){}
    Expr(int i):kind(INTEGER),i(i),vs(NULL),expandedLoopForm(NULL),expandedFuncForm(NULL),expandedCyclicForm(NULL),iteratorTo(NULL){}
    Expr(int64_t i):kind(INTEGER),i(i),vs(NULL),expandedLoopForm(NULL),expandedFuncForm(NULL),expandedCyclicForm(NULL),iteratorTo(NULL){}
    Expr(ExpandedExpr *ee):kind(EXPANDED),ee(ee),vs(NULL),expandedLoopForm(NULL),expandedFuncForm(NULL),expandedCyclicForm(NULL),iteratorTo(NULL){}
    /** \brief Construct an Expr for given variable state */
    Expr(VarState *vs);
    Expr(ExpandedExpr ee);
    ///Add the value of another expr to the current expr
    void                            add(Expr &expr);
    ///Add the expanded expression into the current expr
    void                            add(ExpandedExpr *ex);
    ///Add the value of another expr to the current expr
    void                            subtract(Expr &expr);
    ///Multiply the value of another expr to the current expr
    void                            multiply(Expr &expr);
    ///Add another option for the value of this expr
    void                            phi(Expr &expr);
    ///Negate the current value
    void                            negate();
    ///Return an ExpandedExpr of type MUL representing a left shift of the current expr by another expr
    ExpandedExpr                    shl(Expr &expr);
    ///return true if the expression is a leaf node
    bool                            isLeaf();
    ///return true if the expression is a register node
    bool                            isRegister();
    ///return true if the expression is evaluated as one
    bool                            isOne();
    ///return true if the loop has loop iterator
    bool                            hasIterator(Loop *loop);
    ///try to simplify the experssion
    void                            simplify();
    ///Expand the expression itself
    void                            expand();
};

//operators
bool operator==(const Expr& expr1, const Expr& expr2);
bool operator<(const Expr& expr1, const Expr& expr2);
std::ostream& operator<<(std::ostream& out, const Expr& expr);

/** \brief A collapsed representation of a sub-AST that generates the value of a given **Expr**.
 *
 * The **ExpandedExpr** class is used to represent the arithmetic relation from a set of leaf **Expr** in the AST.
 * The leaf **Expr** is defined with a scope, for example, a loop boundary or a function frame.
 * The expansion of the ExpandedExpr will stop if it hits the predefined boundary.
 *
 * For example, an expanded **ExpandedExpr::SUM** expression can be written as:
 * \code '+' term1*coeff1, term2*coeff2, term3*coeff3 \endcode
 * refers to a collapsed binary tree representing
 * (coeff1*term1 + coeff2*term2 + coeff3*term3), where the term must be different leaves **Expr**
 *
 * An expanded **ExpandedExpr::PHI** expression means the expression can only be one of the terms at a time:
 * \code 'phi' term1*(1), term2*(1), term3*(1) \endcode
 * It refers to an option from (term1, term2, term3), where term could be another **ExpandedExpr** (but none of them would be **ExpandedExpr::PHI**).
 *
 * The **ExpandedExpr** class always maintains its internal order so that there is no other isomorphic form.
 * When the **ExpandedExpr::PHI** and **ExpandedExpr::SUM** expressions are merged, the **ExpandedExpr::PHI** is always place on the top.
 * For example,
 * \code
 * A = (a,b)
 * B = (c,d)
 * A+B = (a+c,a+d,b+c,b+d) \endcode
 */
class ExpandedExpr {
public:
    enum ExpandedScope
    {
        LoopScope,       ///Expansion stops when reaching outside of the current loop
        FunctionScope,   ///Expansion stops when reaching outside of the current function
    };

    enum ExpandedExprKind
    {
        EMPTY,///<Empty expressions
        SUM,  ///<sum of all the nodes in the expression
        PHI,  ///<phi selection from one of the nodes in the expression, it has higher priority than the SUM
        MUL,  ///<product of all the nodes in the expression
        SENSI ///<sensitivity list only, no actual calculation listed
    };

    ///specifies the operation of the expanded expression
    ExpandedExprKind                kind;
    ///specifies the current scope
    ExpandedScope                   scope;
    ///pointer to the the current scope
    union {
        Loop                        *loop;
        Function                    *function;
    };
    /** \brief The collection of expressions, 
     * 
     * the lvalue (const) represents a leaf expression node according to the boundary
     * the rvalue represents the coefficient after expansion
     */
    std::map<Expr, Expr>            exprs;

    ExpandedExpr():kind(EMPTY){}
    ExpandedExpr(ExpandedExprKind kind):kind(kind),scope(LoopScope){};

    ///Return the coeff of the expr (only for SUM expressions)
    Expr                            get(Expr node);
    Expr                            get(VarState *vs);
    Expr                            get(int i);
    ///Return true if the expression is empty
    bool                            isEmpty();
    ///Remove one of the term
    void                            remove(Expr node);
    ///Return whether the expression has the term
    bool                            hasTerm(VarState *vs);
    bool                            hasTerm(Expr node);
    ///Add one term to the expression
    void                            addTerm(int value);
    void                            addTerm(Expr node);
    void                            addTerm(Expr *node);
    ///Add one term with an integer multiplier to the expression
    void                            addTerm(Expr node,  int coeff);
    void                            addTerm(Expr *node, int coeff);
    /** \brief Main addTerm operation
     *
     * Add one term in the existing collection of expressions
     * Here are the rules of thumb:
     *
     * SUM:
     * \code
     * A = a*t1 + b*t2 + c*t3
     * if (n!=(t1,t2,t3))
     *   A.addterm(n, e) = a*t1 + b*t2 + c*t3 + e*n
     * if (n==t1)
     *   A.addterm(n, e) = (a+e)*t1 + b*t2 + c*t3
     * \endcode
     * PHI: (coefficient must be 1)
     * \code
     * A = a*t1 , b*t2 , c*t3
     * if (n!=(t1,t2,t3))
     *   A.addterm(n, e) = a*t1 , b*t2 , c*t3, e*n
     * if (n==t1 && a==e)
     *   A.addterm(n, e) = A
     * \endcode
     */
    void                            addTerm(Expr node,  Expr &coeff);
    void                            addTerm(Expr *node, Expr &coeff);
    ///Merge with another ExpandedExpr
    void                            merge(ExpandedExpr &expr);
    /** \brief Main merge operation
     *
     * Merge the given expression in itself
     */
    void                            merge(ExpandedExpr *expr);
    /** \brief Sensitivity merge
     *
     * Merge the sensitivity list from two ambiguous expressions into one
     */
    void                            sensiMerge(ExpandedExpr *expr);
    void                            sensiMerge(ExpandedExpr &expr);
    ///Expand all its terms into the **ExpandedExpr** type
    void                            expand();
    ///negate the value of the current ExpandedExpr
    void                            negate();
    void                            subtract(Expr expr);
    ///Subtract with another ExpandedExpr
    void                            subtract(ExpandedExpr &expr);
    ///Multiply with another Expr
    void                            multiply(Expr expr);
    ///Divide by another Expr
    void                            divide(Expr multi);
    ///shl
    ExpandedExpr                    shl(Expr &expr);
    int                             size() {return exprs.size();}
    ///Return true if the expression has no terms that are related to loop iterators
    bool                            isConstant();
    ///Return true if the expression has a term that is loop iterator
    Iterator                        *hasIterator(Loop *loop);
    ///Return true if the expression has no array base expressions
    bool                            isArrayBase(Loop *loop);
    ///Remove the current loop iterator
    ExpandedExpr                    removeIterator(Loop *loop);
    ///Infer the array base from the current expression, return NULL if no array base is found
    ExpandedExpr                    *getArrayBase(Loop *loop);
    ///Simplify the expressions, constant elimination
    void                            simplify();
    ///Cascade N degree f(i) -> f(i+N)
    bool                            cascade(Loop *loop, int N);
    //Evalute the value of the expression
    std::pair<Expr::ExprKind,int64_t> evaluate(Loop *loop);
    ///Return the value of loop's normalised boundary (0,1,2..N) if the expression is evaluated as zero. Return -1 if failed.
    long int                        solve(Loop *loop);
    ///Expand the leaf nodes of the current expression to loop boundary
    void                            extendToLoopScope(Loop *loop);
    ///Expand the leaf nodes of the current expression to function boundary
    void                            extendToFuncScope(Function *func, Loop *loop);
    ///Expand the leaf nodes of the current expression to memory boundary / register leaf should be replaced unless function arguments
    void                            extendToMemoryScope(Function *func, Loop *loop);
    ///Expand the the current expression to the terms that contains the variable, return false if the variable is not found
    bool                            extendToVariableScope(Function *func, Loop *loop, Variable *var);
    ///Remove the all immediate value in the expression
    void                            removeImmediate();
    ///Convert to sensitivity based expression
    void                            convertToSensibleExpr(){kind=SENSI;};
};

ExpandedExpr operator+(ExpandedExpr &expr1, ExpandedExpr &expr2);
ExpandedExpr operator-(const ExpandedExpr &expr1, const ExpandedExpr &expr2);
ExpandedExpr operator&(const ExpandedExpr &expr1, const ExpandedExpr &expr2);
std::ostream& operator<<(std::ostream& out, const ExpandedExpr& expr);
bool operator<(const ExpandedExpr& expr1, const ExpandedExpr& expr2);
bool operator==(const ExpandedExpr& expr1, const ExpandedExpr& expr2);
} //end namespace janus

/*!
 * Expand the current expression to canonical form
 * \param[in] expr The expression class to be expanded
 * \param[in] loop The loop that the expansion would stop if traversal goes beyond the loop
 * \return The pointer to the expanded expression, (also buffered in expr->expandedLoopForm).
 */
janus::ExpandedExpr *expandExpr(/*IN*/janus::Expr *expr, janus::Loop *loop);
/*!
 * Expand the current expression to canonical form
 * \param[in] expr The expression class to be expanded
 * \param[in] func The function that the expansion would stop if traversal goes beyond the function
 * \param[in] loop The loop that the expression belongs to. The expanded expansion may be different for the same expression but from different loop (inner or outer).
 * \return The pointer to the expanded expression, (also buffered in expr->expandedFuncForm).
 * 
 * It is suggested that the expansion happens after the first pass of loop iterator analysis
 * So that the expansion can recognise loop iterator related expressions
 */
janus::ExpandedExpr *expandExpr(/*IN*/janus::Expr *expr, janus::Function *func, janus::Loop *loop);

janus::ExpandedExpr *expandExpr(/*IN*/janus::Expr *expr, janus::Function *func);

janus::Expr *newExpr(std::set<janus::Expr*> &exprs);
janus::Expr *newExpr(int64_t value, std::set<janus::Expr*> &exprs);
janus::Expr *newExpr(janus::VarState *vs, std::set<janus::Expr*> &exprs);
void buildExpr(janus::Expr &expr, std::set<janus::Expr*> &exprs, janus::Instruction *instr);

bool buildLoopExpr(janus::Expr *expr, janus::ExpandedExpr *expanded, janus::Loop *loop);
/* Graph traversal function in the AST tree */
bool buildFuncExpr(janus::Expr *expr, janus::ExpandedExpr *expanded,
                   janus::Function *func, janus::Loop *loop,
                   bool stopAtMemory, janus::Variable *stopAtVariable);
/*Graph travel without the loop information */
bool buildFuncExpr(janus::Expr *expr, janus::ExpandedExpr *expanded,
                   janus::Function *func,
                   bool stopAtMemory, janus::Variable *stopAtVariable);
#endif
