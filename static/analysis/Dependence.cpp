#include "Dependence.h"
#include "BasicBlock.h"
#include "Expression.h"
#include "IO.h"
#include "Loop.h"
#include <set>
#include <stack>
#include <vector>

using namespace std;
using namespace janus;

enum CyclicStatus {
    FoundCyclic,
    FoundConstPhi,
    FoundConst,
    FoundUndecidedPhi,
    Error
};

/* Internal AST traverser to find cross-iteration dependencies */
static CyclicStatus buildCyclicExpr(Expr *startExpr, Expr *currentExpr,
                                    ExpandedExpr *expr, Loop *loop,
                                    set<Expr *> &visitedPhi);

/* Traverse the SSA graph for each phi node that the start node of the loop
 * Construct a cyclic expression for each phi node */
void dependenceAnalysis(janus::Loop *loop)
{
    if (!loop)
        return;
    if (loop->dependenceAvailable)
        return;
    // skip loops with two init blocks
    if (loop->init.size() > 1)
        return;

    LOOPLOG("\tPerforming dependence analysis on loop variables" << endl);

    set<Expr *> visitedPhi;
    // step 1 : analyse each phi node at loop start
    for (auto phi : loop->start->phiNodes) {
        if (phi->notUsed)
            continue;
        LOOPLOG("\t\tAnalysing phi variable " << phi << endl);
        if (!phi->expr) {
            LOOPLOG("\t\tExpression " << phi << " not yet created" << endl);
            continue;
        }

        visitedPhi.clear();
        // conclude an expanded expression for the phi node
        CyclicStatus status =
            buildCyclicExpr(phi->expr, nullptr, nullptr, loop, visitedPhi);

        if (status == FoundCyclic) {
            // found cyclic expressions
            loop->phiVariables.insert(phi);
            loop->phiVars.insert(*(Variable *)phi);
        } else if (status == FoundConstPhi) {
            LOOPLOG(
                "\t\t\t"
                << phi
                << " is fake dependence since its cyclic expression is empty!"
                << endl);
            loop->constPhiVars.insert(phi);
        } else {
            LOOPLOG("\t\tDepending phi node " << phi << " cycle not found"
                                              << endl);
            loop->undecidedPhiVariables.insert(phi);
        }
    }

    loop->dependenceAvailable = true;
}

bool buildCyclicExpr(janus::Expr *expr, janus::Loop *loop)
{
    set<Expr *> visitedPhi;
    CyclicStatus status =
        buildCyclicExpr(expr, nullptr, nullptr, loop, visitedPhi);

    return (status == FoundCyclic);
}

/* Start with the predecessor of the phi node
 * Stops when the vs reaches the phi node
 * Return true when the cycle is detected
 * Return false when the loop boundary is reached while no cycle is detected */
static CyclicStatus buildCyclicExpr(Expr *startExpr,    // IN
                                    Expr *currentExpr,  // IN
                                    ExpandedExpr *expr, // OUT
                                    Loop *loop, set<Expr *> &visitedPhi)
{
    if (!currentExpr) {
        // initial stage
        // the startExpr must be a PHI expression
        if (!startExpr)
            return Error;

        if (startExpr->kind != Expr::PHI) {
            LOOPLOG("\t\t\tThe start expression for buildCyclicExpr must be a "
                    "PHI expression !"
                    << endl);
            return Error;
        }

        startExpr->expandedCyclicForm = nullptr;
        // the phi expression should have two upstream paths
        // one path must be a cycle and the other one must be initial values
        // from the function entry
        ExpandedExpr *e1 = new ExpandedExpr(ExpandedExpr::SUM);
        ExpandedExpr *e2 = new ExpandedExpr(ExpandedExpr::SUM);

        CyclicStatus r1 =
            buildCyclicExpr(startExpr, startExpr->p.e1, e1, loop, visitedPhi);
        CyclicStatus r2 =
            buildCyclicExpr(startExpr, startExpr->p.e2, e2, loop, visitedPhi);

        if (r1 == Error || r2 == Error) {
            LOOPLOG("\t\t\tNo cycle is found for phi node: "
                    << startExpr->vs << " (traversal error)!" << endl);
            return Error;
        }

        if (r1 == FoundCyclic) {
            if (e1->isEmpty())
                return FoundConstPhi;
            // if true, then it is a cyclic expression
            startExpr->expandedCyclicForm = e1;
        } else {
            e1->exprs.clear();
            e1->addTerm(startExpr->p.e1);
            startExpr->expandedFuncForm = e1;
        }

        if (r2 == FoundCyclic) {
            if (e2->isEmpty())
                return FoundConstPhi;
            startExpr->expandedCyclicForm = e2;
        } else {
            e2->exprs.clear();
            e2->addTerm(startExpr->p.e2);
            startExpr->expandedFuncForm = e2;
        }

        if (r1 == FoundUndecidedPhi || r2 == FoundUndecidedPhi) {
            LOOPLOG("\t\t\tNo cycle is found for phi node: " << startExpr->vs
                                                             << "!" << endl);
            delete e1;
            delete e2;
            return FoundUndecidedPhi;
        }

        if (r1 == FoundCyclic || r2 == FoundCyclic)
            return FoundCyclic;
        else if (r1 == FoundConstPhi || r2 == FoundConstPhi) {
            return FoundConstPhi;
        } else {
            return FoundConst;
        }

    } else {
        // recursive stage
        // if current == start, a cycle in SSA graph is detected
        if (startExpr == currentExpr || startExpr->vs == currentExpr->vs)
            return FoundCyclic;

        // if it is a constant in the loop, simply add the term
        if (currentExpr->isLeaf()) {
            expr->addTerm(currentExpr);
            return FoundConst;
        } else if (currentExpr->vs && loop->isConstant(currentExpr->vs)) {
            expr->addTerm(currentExpr);
            return FoundConst;
        }

        // then follow the expression upwards
        switch (currentExpr->kind) {
        case Expr::INTEGER:
        case Expr::VAR:
        case Expr::MEM:
            expr->addTerm(currentExpr);
            return FoundConst;
            break;
        case Expr::UNARY:
            if (currentExpr->u.op == Expr::MOV) {
                return buildCyclicExpr(startExpr, currentExpr->u.e, expr, loop,
                                       visitedPhi);
            } else if (currentExpr->u.op == Expr::NEG) {
                ExpandedExpr negExpr(ExpandedExpr::SUM);
                CyclicStatus result = buildCyclicExpr(
                    startExpr, currentExpr->u.e, &negExpr, loop, visitedPhi);
                negExpr.negate();
                expr->merge(negExpr);
                return result;
            } else {
                // LOOPLOG("\t\t\tUnrecognised unary expressions" <<endl);
                return Error;
            }
            break;
        case Expr::BINARY: {
            CyclicStatus r1;
            CyclicStatus r2;

            if (currentExpr->b.op == Expr::ADD ||
                currentExpr->b.op == Expr::MUL) {
                r1 = buildCyclicExpr(startExpr, currentExpr->b.e1, expr, loop,
                                     visitedPhi);
                r2 = buildCyclicExpr(startExpr, currentExpr->b.e2, expr, loop,
                                     visitedPhi);
            } else if (currentExpr->b.op == Expr::SUB) {
                r1 = buildCyclicExpr(startExpr, currentExpr->b.e1, expr, loop,
                                     visitedPhi);
                ExpandedExpr negExpr(ExpandedExpr::SUM);
                r2 = buildCyclicExpr(startExpr, currentExpr->b.e2, &negExpr,
                                     loop, visitedPhi);
                negExpr.negate();
                expr->merge(negExpr);
            } else {
                // LOOPLOG("\t\t\tUnrecognised binary expressions" <<endl);
                return Error;
            }

            // return status
            if (r1 == Error || r2 == Error)
                return Error;
            if (r1 == FoundUndecidedPhi || r2 == FoundUndecidedPhi)
                return FoundUndecidedPhi;
            if (r1 == FoundConstPhi) {
                expr->addTerm(currentExpr->b.e1);
            }
            if (r2 == FoundConstPhi) {
                expr->addTerm(currentExpr->b.e2);
            }
            if (r1 == FoundCyclic || r2 == FoundCyclic)
                return FoundCyclic;
            else
                return FoundConstPhi;
        } break;
        case Expr::PHI: {
            // check if it is already visited
            if (visitedPhi.find(currentExpr) != visitedPhi.end()) {
                return FoundConst;
            }
            // if not found, mark the phi node visited
            visitedPhi.insert(currentExpr);

            Function *func = loop->parent;
            // for this phi node, we first need to check if this phi belongs to
            // an iterator of any other loop
            if (currentExpr->vs && func->iterators.find(currentExpr->vs) !=
                                       func->iterators.end()) {
                Iterator *sit = func->iterators[currentExpr->vs];
                Loop *suspect = sit->loop;

                if (!sit->main) {
                    LOOPLOG(
                        "\t\t\tFound phi loop iterator but not a main iterator "
                        << dec << suspect->id << endl);
                    return FoundUndecidedPhi;
                }
                // if the loop is the descendants, use the final value of the
                // iterator
                if (loop->descendants.find(suspect) !=
                    loop->descendants.end()) {
                    LOOPLOG("\t\t\tFound phi iterator in its sub loop "
                            << dec << suspect->id << endl);
                    // add its final values to the cyclic expression
                    if (sit->finalKind == Iterator::INTEGER) {
                        expr->addTerm(Expr(sit->finalImm));
                        return FoundConst;
                    } else if (sit->finalKind == Iterator::EXPANDED_EXPR) {
                        CyclicStatus status = FoundConst;
                        for (auto term : sit->finalExprs->exprs) {
                            Expr t = term.first;
                            if (t.kind == Expr::INTEGER) {
                                expr->addTerm(t, term.second);
                            } else {
                                // try to find cycles from the final expressions
                                CyclicStatus status2 = buildCyclicExpr(
                                    startExpr, &t, expr, loop, visitedPhi);
                                if (status2 == FoundCyclic)
                                    status = FoundCyclic;
                                else if (status2 == FoundConst ||
                                         status2 == FoundConstPhi)
                                    continue;
                                else
                                    return status2;
                            }
                        }
                        return status;
                    } else {
                        LOOPLOG("\t\t\tFinal value for the iterator not found "
                                << dec << suspect->id << endl);
                        return FoundUndecidedPhi;
                    }
                }

                if (loop->ancestors.find(suspect) != loop->ancestors.end()) {
                    // treat it as a constant
                    expr->addTerm(currentExpr);
                    return FoundConst;
                }
            } else {
                /* If we encounter a phi node when building cyclic expression
                 * for a depending variable it means that the variable is
                 * updated with some conditions In a loop environment, there
                 * could be internal cycles in the SSA where phi nodes are
                 * referencing each other
                 * we should avoid checking those cycles but associate the
                 * expression with a condition */
                ExpandedExpr *ep1 = new ExpandedExpr(ExpandedExpr::SUM);
                CyclicStatus r1 = buildCyclicExpr(startExpr, currentExpr->p.e1,
                                                  ep1, loop, visitedPhi);
                ExpandedExpr *ep2 = new ExpandedExpr(ExpandedExpr::SUM);
                CyclicStatus r2 = buildCyclicExpr(startExpr, currentExpr->p.e1,
                                                  ep2, loop, visitedPhi);

                if (r1 == Error || r2 == Error)
                    return Error;
                else if (r1 == FoundCyclic && r2 == FoundCyclic) {
                    if (*ep1 == *ep2) {
                        expr->merge(ep1);
                        LOOPLOG("\t\t\tFound an iterator with phi expressions "
                                "in stride, but expression is the same: "
                                << currentExpr->vs << endl);
                        return r1;
                    } else {
                        LOOPLOG("\t\t\tFound an iterator with phi expressions "
                                "in stride, currently not handled "
                                << currentExpr->vs << endl);
                        return FoundUndecidedPhi;
                    }
                } else if (ep1->isEmpty() && ep2->isEmpty()) {
                    // if both expressions are empty, then it is a fake phi node
                    // created by compiler code generation it means no changes
                    // made per iteration
                    LOOPLOG("\t\t\tFound empty cyclic expressions in stride: "
                            << currentExpr->vs << " no actual dependency"
                            << endl);
                    loop->constPhiVars.insert(currentExpr->vs);
                    return FoundConstPhi;
                } else {
                    LOOPLOG("\t\t\tFound phi node in the iterator stride, "
                            "currently not handled "
                            << currentExpr->vs << endl);
                    return FoundUndecidedPhi;
                }
            }
        } break;
        default:
            LOOPLOG("\t\t\tUnrecognised expressions" << endl);
            return Error;
            break;
        }
    }
    return Error;
}
