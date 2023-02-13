#include "janus.h"
#include "MemoryLocation.h"
#include "Instruction.h"
#include "BasicBlock.h"
#include "Loop.h"
#include "Function.h"
#include "Expression.h"
#include "IO.h"

#include <iostream>
#include <sstream>
#include <stack>

using namespace janus;
using namespace std;

MemoryLocation::MemoryLocation(MemoryInstruction *mi, Loop *loop)
:expr(ExpandedExpr::SUM), escev(NULL), loop(loop)
{
    type = Complex;
    if (loop == NULL || mi == NULL) return;

    vs = mi->mem;
    VarState *baseState = NULL;
    VarState *indexState = NULL;
    for (auto vi: vs->pred) {
        if ((Variable)*vi == Variable((uint32_t)vs->base)) {
            baseState = vi;
        }
        if ((Variable)*vi == Variable((uint32_t)vs->index)) {
            indexState = vi;
        }
    }

    if (baseState) {
        ExpandedExpr *baseExpr = expandExpr(baseState->expr, loop);
        if (!baseExpr) {
            type = Unknown;
            return;
        }
        expr.sensiMerge(baseExpr);
    }
    if (indexState) {
        ExpandedExpr *indexExpr = expandExpr(indexState->expr, loop);
        if (!indexExpr) {
            type = Unknown;
            return;
        }
        if (vs->scale > 1) {
            ExpandedExpr copy = *indexExpr;
            copy.multiply(Expr((int)vs->scale));
            expr.sensiMerge(copy);
        } else
            expr.sensiMerge(indexExpr);
    }
    int disp = (int)vs->value;
    if (disp)
        expr.addTerm(Expr(disp));

    //simplify the expression
    expr.simplify();

    //examine the memory address expressions
    //if the memory address conforms to polyhedral expression, such as
    //addr = A*i + B*j + C*k + D, where i, j, k are inner loop iterators,
    //and A, B, C, D are either immediate values or loop constants
    //we call this location as "Affine"
    bool hasLoopIterator = false;
    bool hasSubLoopIterator = false;
    if (expr.hasIterator(loop) != NULL)
        hasLoopIterator = true;

    for (auto subLoop: loop->descendants) {
        expr.hasIterator(subLoop);
        hasSubLoopIterator = true;
    }

    if (hasLoopIterator && !hasSubLoopIterator) type = Affine1D;
    if (hasSubLoopIterator) type = AffinemD;
    if (!hasLoopIterator && !hasSubLoopIterator) type = Constant;

	std::cout << "MemoryLocation::MemoryLocation expression type "<< expr.kind << "." << std::endl;
}

SCEV::SCEV(Expr start, Expr stride, Iterator *iter)
:start(start),stride(stride),iter(iter)
{

}

void SCEV::addScalarTerm(Expr expr)
{
    start.add(expr);
    if (start.kind == Expr::EXPANDED) start.ee->simplify();
}

void SCEV::addScalarTerm(Expr expr, Expr coeff)
{
    ExpandedExpr ee(ExpandedExpr::SUM);
    ee.addTerm(expr, coeff);
    start.add(&ee);
    if (start.kind == Expr::EXPANDED) start.ee->simplify();
}

void SCEV::addStrideTerm(Expr expr)
{
    stride.add(expr);
    if (stride.kind == Expr::EXPANDED) stride.ee->simplify();
}

void SCEV::multiply(Expr expr)
{
    stride.multiply(expr);
    start.multiply(expr);
}

ExpandedSCEV::ExpandedSCEV(SCEV &scev)
:start(0)
{
    if (scev.iter == NULL) return;
    Loop *loop = scev.iter->loop;
    //force start to be expanded
    start.kind = Expr::EXPANDED;
    start.ee = new ExpandedExpr(ExpandedExpr::SUM);

    strides[scev.iter] = scev.stride;
    //scan start expression to check if there is additional iterators
    stack<Expr> checkList;

    checkList.push(scev.start);

    while (!checkList.empty()) {
        Expr test = checkList.top();
        checkList.pop();

        //extend the current expression to function scope
        if (test.kind != Expr::INTEGER) {
            test.expand();
            test.ee->extendToFuncScope(loop->parent, loop);
        }

        if (test.kind != Expr::EXPANDED)
            start.add(test);
        else {
            for (auto e: test.ee->exprs) {
                Expr term = e.first;
                if (term.kind == Expr::ADDREC) {
                    strides[term.rec.iter] = *term.rec.stride;
                    checkList.push(*term.rec.base);
                } else
                    start.ee->addTerm(e.first, e.second);
            }
        }
    }
    start.simplify();
}

Expr ExpandedSCEV::getArrayBase()
{
    if (start.kind != Expr::EXPANDED) {
        if (start.kind == Expr::INTEGER) {
            Expr copy = start;
            copy.i = 0;
            return copy;
        } else return start;
    }
    else {
        Expr result;
        for (auto e: start.ee->exprs) {
            Expr term = e.first;
            //skip all integer part of the expression
            if (term.kind == Expr::INTEGER && e.second.kind == Expr::INTEGER) {
                continue;
            } else if (result.kind == Expr::NONE) {
                result = e.first;
            } else {
                return start;
            }
        }
        return result;
    }
}

bool janus::operator<(const MemoryLocation &a, const MemoryLocation &b)
{

    Variable avar = *(Variable *)a.vs;
    Variable bvar = *(Variable *)b.vs;
    if (avar < bvar) return true;
    else if (avar == bvar) {
        return (a.expr < b.expr);
    }
    return false;
}

ostream& janus::operator<<(std::ostream& out, const SCEV& scev)
{
    if (scev.iter)
        out<<"{"<<scev.start<<",+,"<<scev.stride<<"}@L"<<dec<<scev.iter->loop->id;
    else
        out<<"{"<<scev.start<<",+,"<<scev.stride<<"}@L?";
}

ostream& janus::operator<<(std::ostream& out, const ExpandedSCEV& escev)
{
    out<<"{"<<escev.start;
    for (auto s: escev.strides) {
        out <<",+,"<<s.second<<"*l"<<dec<<s.first->loop->id;
    }
    out<<"}";
}

//A memory variable is only equal when its base and index are equal
bool janus::operator==(const MemoryLocation &a, const MemoryLocation &b)
{
    return (a.expr == b.expr);
}

ostream& janus::operator<<(ostream& out, const MemoryLocation& mloc)
{
    //out <<mloc.vs<<": "<<mloc.expr;
    out<<mloc.vs;

    if (mloc.readBy.size()) {
        out <<" by ";
        for (auto read: mloc.readBy) {
            out<<dec<<read->id<<" ";
        }
    }

    if (mloc.writeFrom.size()) {
        out <<" from ";
        for (auto write: mloc.writeFrom) {
            out<<dec<<write->id<<" ";
        }
    }
/*
    if (mloc.type == MemoryLocation::Affine1D) {
        out<<" Affine1D ";
    }
    else if (mloc.type == MemoryLocation::AffinemD) {
        out<<" AffinemD ";
    }
*/
    if (mloc.escev) {
        out<<" Base ("<<mloc.escev->getArrayBase()<<")";
    }
    return out;
}

bool
MemoryLocation::containIterator(Loop *loop)
{
    if (expr.hasIterator(loop)) return true;
    if (!escev) return false;
    for (auto stride: escev->strides) {
        if (stride.first->loop == loop)
            return true;
    }
    return false;
}

VarState *
MemoryLocation::getIndirectMemory()
{
    for (auto term: expr.exprs) {
        if (term.first.kind == Expr::MEM)
            return term.first.vs;
    }
    return NULL;
}
