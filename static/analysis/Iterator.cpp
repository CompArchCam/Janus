#include "Iterator.h"
#include "BasicBlock.h"
#include "Dependence.h"
#include "IO.h"
#include "Loop.h"
#include "Variable.h"
#include <queue>

using namespace janus;
using namespace std;

static void getIteratorFinalValue(ExpandedExpr &boundExpr, Iterator &iterator);

Iterator::Iterator(VarState *vs, janus::Loop *loop)
    : vs(vs), loop(loop), main(false)
{
    kind = ITERATOR_NONE;
    initKind = NONE;
    finalKind = NONE;
    initExprs = NULL;
    finalExprs = NULL;

    if (!vs->expr) {
        return;
    }

    // step 1: get cyclic expression of the iterator
    ExpandedExpr *cyclicExpr = vs->expr->expandedCyclicForm;
    if (!cyclicExpr)
        return;
    if (cyclicExpr->exprs.size() == 1) {
        Expr::ExprKind ekind;
        int64_t value;
        tie(ekind, value) = cyclicExpr->evaluate(loop);
        if (ekind == Expr::INTEGER) {
            kind = INDUCTION_IMM;
            stride = value;
            strideKind = INTEGER;
        } else if (ekind == Expr::VAR) {
            kind = INDUCTION_VAR;
            strideVar = (VarState *)value;
            strideKind = SINGLE_VAR;
        } else {
            kind = ITER_GENERIC;
            strideExprs = new ExpandedExpr(ExpandedExpr::SUM);
            *strideExprs = *cyclicExpr;
        }
    } else {
        kind = ITER_GENERIC;
        strideExprs = new ExpandedExpr(ExpandedExpr::SUM);
        *strideExprs = *cyclicExpr;
    }

    // step 2: get the lower bound of the iterator
    // as already calculated in buildCyclicExpr (Dependence analysis)
    // the initial value is stored in expandedFuncForm
    ExpandedExpr *initExpExpr = vs->expr->expandedFuncForm;
    if (!initExpExpr)
        return;
    if (initExpExpr->exprs.size() == 1) {
        Expr::ExprKind ekind;
        int64_t value;
        tie(ekind, value) = initExpExpr->evaluate(loop);
        if (ekind == Expr::INTEGER) {
            initKind = INTEGER;
            initImm = value;
        } else if (ekind == Expr::VAR) {
            initKind = SINGLE_VAR;
            initVar = (VarState *)value;
        } else {
            auto term = initExpExpr->exprs.begin();
            initKind = EXPR;
            initExpr = (*term).first;
        }
    }
    // clear the rubbish
    delete initExpExpr;
    vs->expr->expandedFuncForm = NULL;
}

bool janus::operator<(const Iterator &iter1, const Iterator &iter2)
{
    return ((uint64_t)(iter1.vs) < (uint64_t)(iter2.vs));
}

static bool finalValueAnalysis(Loop *loop)
{
    auto &iterators = loop->iterators;
    auto &miter = loop->mainIterator;

    for (auto checkID : loop->check) {
        BasicBlock &checkBlock = loop->parent->entry[checkID];
        // get the last conditional jump, a check block is always conditional
        // jump
        Instruction *cjump = checkBlock.lastInstr();
        Instruction *cmpInstr = NULL;
        // check the input of the jump instruction
        for (auto vi : cjump->inputs) {
            if (vi->type == JVAR_CONTROLFLAG) {
                cmpInstr = vi->lastModified;
            }
        }

        if (!cmpInstr) {
            LOOPLOG("\t\tCould not locate an instruction that modifies control "
                    "flag! "
                    << endl);
            return false;
        }

        if (cmpInstr->opcode != Instruction::Compare) {
            // if it is not a compare, then it could be a subtraction
            if (cmpInstr->opcode == Instruction::Sub) {
                LOOPLOG(
                    "\t\tFound a subtraction instruction that modifies eflags! "
                    << *cmpInstr << endl);
            } else {
                LOOPLOG("\t\tFound irregular instruction that modifies eflags! "
                        << *cmpInstr << endl);
                return false;
            }
        }

        if (cmpInstr->inputs.size() != 2) {
            LOOPLOG("\t\tFound irregular compare instruction with less or more "
                    "than two operands! "
                    << *cmpInstr << endl);
            return false;
        }

        // obtain the expressions for both comparison operands
        auto cmpOP1 = expandExpr(cmpInstr->inputs[0]->expr, loop);
        auto cmpOP2 = expandExpr(cmpInstr->inputs[1]->expr, loop);
        if (!(cmpOP1 && cmpOP2)) {
            LOOPLOG("\t\tFailed to retrieve expression for comparison. "
                    "\n\tDisable further analysis for this loop"
                    << endl);
            loop->unsafe = true;
            return false;
        } else if (cmpOP1->kind == ExpandedExpr::SENSI ||
                   cmpOP2->kind == ExpandedExpr::SENSI) {
            LOOPLOG("\t\tAmbiguous expression or not yet implemented "
                    "operations.\n\tDisable further analysis for this loop"
                    << endl);
            loop->unsafe = true;
            return false;
        } else if (cmpOP1->kind == ExpandedExpr::PHI ||
                   cmpOP2->kind == ExpandedExpr::PHI) {
            LOOPLOG("\t\tPHI expressions are currently disabled for further "
                    "analysis for this loop"
                    << endl);
            loop->unsafe = true;
            return false;
        }
        ExpandedExpr boundExpr = *cmpOP1 - *cmpOP2;
        LOOPLOG("\t\tBound expr " << boundExpr << endl);

        // check the both expressions, and link the condition with only one loop
        // iterator
        for (auto &it : iterators) {
            if (boundExpr.hasTerm(it.first)) {
                if (miter) {
                    LOOPLOG("\t\tLoop contains multiple iterators! " << endl);
                }
                miter = &(it.second);
                miter->main = true;
                miter->checkState = NULL;
                if (!cmpOP1->hasTerm(it.first))
                    miter->checkState =
                        loop->getAbsoluteStorage(cmpInstr->inputs[0]);
                if (!cmpOP2->hasTerm(it.first)) {
                    if (miter->checkState) {
                        LOOPLOG("\t\tLoop contains irregular comparisons! "
                                << endl);
                        miter->checkState = NULL;
                    } else {
                        miter->checkState =
                            loop->getAbsoluteStorage(cmpInstr->inputs[1]);
                    }
                }
            }
        }

        if (!miter) {
            LOOPLOG("\t\tCould not find the main iterator! - Try expanding the "
                    "expression to variable scope "
                    << endl);
            // check the expressions again
            for (auto &it : iterators) {
                Variable itVar = *(Variable *)it.first;
                ExpandedExpr boundTest = boundExpr;
                bool found =
                    boundTest.extendToVariableScope(loop->parent, loop, &itVar);
                LOOPLOG("\t\tBound expr expanded to: " << boundTest << endl);
                if (found) {
                    if (miter) {
                        LOOPLOG("\t\tLoop contains multiple iterators! "
                                << endl);
                    }
                    miter = &(it.second);
                    miter->main = true;
                    miter->checkState = NULL;
                    cmpOP1->extendToVariableScope(loop->parent, loop, &itVar);
                    cmpOP2->extendToVariableScope(loop->parent, loop, &itVar);
                    if (!cmpOP1->hasTerm(it.first))
                        miter->checkState = cmpInstr->inputs[0];
                    if (!cmpOP2->hasTerm(it.first)) {
                        if (miter->checkState) {
                            LOOPLOG("\t\tLoop contains irregular comparisons! "
                                    << endl);
                            miter->checkState = NULL;
                        } else
                            miter->checkState = cmpInstr->inputs[1];
                    }
                    // overwrite bound expression
                    boundExpr = boundTest;
                    break;
                }
            }
            LOOPLOG("\t\tFound the main iterator!" << endl);
        }

        // get the last value of this iterator based on the compare and jump
        // instruction for the moment, we list those cases one by one Note: this
        // is not architectural independent
        // XXX: improve this in the future
        // cjump->print(&cout);
        // check which cjump target or fall through target is outside of the
        // loop
        int targetID = cjump->getTargetInstID();
        if (targetID < 0) {
            LOOPLOG("\t\tCould not find the conditional jump target! " << endl);
            return false;
        }
#ifdef ARCH_SPEC_FIX
        BlockID bid = loop->parent->minstrs[targetID].block->bid;
        // cout <<loop->parent->entry[bid].instrs->id<<endl;

        bool stayInLoop = loop->contains(bid);
        switch (cjump->opcode) {
        case X86_INS_JNE:
            if (stayInLoop) {
                getIteratorFinalValue(boundExpr, *miter);
            } else {
                LOOPLOG("\t\tJNE jumps outside of the loop!" << endl);
                return false;
            }
            break;
        case X86_INS_JE:
            if (stayInLoop) {
                LOOPLOG("\t\tJE jumps inside of the loop!" << endl);
                return false;
            } else {
                getIteratorFinalValue(boundExpr, *miter);
            }
            break;
        case X86_INS_JA:
            // op1 is above op2
            if (!boundExpr.cascade(loop, -1)) {
                LOOPLOG("\t\tFailing to cascade" << endl);
            }
            getIteratorFinalValue(boundExpr, *miter);
            break;
        case X86_INS_JB:
            // op1 is below op2
            if (!boundExpr.cascade(loop, -1)) {
                LOOPLOG("\t\tFailing to cascade" << endl);
            }
            getIteratorFinalValue(boundExpr, *miter);
            break;
        default:
            LOOPLOG("\t\tUnrecognised conditional jump! " << endl);
            IF_LOOPLOG(cjump->print(&loopLog));
            break;
        }
#endif
        if (miter)
            getIteratorFinalValue(boundExpr, *miter);
    }
    return true;
}

bool iteratorAnalysis(Loop *loop)
{
    auto &iterators = loop->iterators;
    auto &miter = loop->mainIterator;
    LOOPLOG("\tPerforming iterator analysis on loop variables" << endl);

    if (loop->check.size() > 1) {
        LOOPLOG("\t\tLoop contains multiple exits and checking conditions"
                << endl);
        loop->unsafe = true;
        return false;
    } else if (loop->check.size() == 0) {
        LOOPLOG("\t\tLoop contains no checking conditions, hence no iterator "
                "is found"
                << endl);
        loop->unsafe = true;
        return false;
    }

    // step 1: identify all possible loop iterators from phi variables
    for (auto depVar : loop->phiVariables) {
        LOOPLOG("\t\tAnalysing candidate variable " << depVar << endl);
        Iterator it(depVar, loop);
        iterators[depVar] = it;
    }

    // register iterators in the parent function
    for (auto &iter : loop->iterators) {
        loop->parent->iterators[iter.first] = &(iter.second);
        iter.first->expr->iteratorTo = &(iter.second);
    }

    // conclude the register set for iterators
    loop->iteratorRegs = 0;
    for (auto &iter : loop->iterators) {
        if (iter.first->type == JVAR_REGISTER) {
            loop->iteratorRegs.insert(iter.first->value);
        }
    }
    // step 2: identify loop boundary operations
    if (!finalValueAnalysis(loop))
        return false;

    // till now, the iterator has been identified
    if (miter) {
        LOOPLOG("\t\tIterator identified: " << (Variable)*miter->vs << " "
                                            << *miter << endl);
        miter->main = true;
    }

    LOOPLOG("\tProceed to the second pass of analysis" << endl);
    return true;
}

/* Checks whether a given undecidedPhiVariable is never used within the loop
 * itself*/
/* And thus is only a WAW dependency (with a possibly irregular modification
 * pattern), which can be resolved */
bool writeAfterWriteAnalysis(janus::Loop *loop, janus::VarState *phiVar)
{
    // We do DFS on the data flow edges (VarState::succ),
    // starting from phiVar, to find all Varstates that might use phiVar's data
    std::set<VarState *>
        markedStates; // All states that ever get visited in the BFS
    std::queue<VarState *> BFSQueue;
    BFSQueue.push(phiVar);
    markedStates.insert(phiVar);
    while (!BFSQueue.empty()) {
        VarState *vs = BFSQueue.front();
        BFSQueue.pop();
        for (VarState *vsSucc : vs->succ) {
            if (markedStates.find(vsSucc) == markedStates.end()) {
                BFSQueue.push(vsSucc);
                markedStates.insert(vsSucc);
            }
        }
    }

    // Now check whether any instruction within the loop uses any of the marked
    // states Since that would mean our undecided variable is read before being
    // overwritten, so is unsafe!
    for (VarState *vs : markedStates) {
        for (Instruction *instr : vs->dependants) {
            if (loop->contains(instr->block->bid)) {
                return false;
            }
        }
    }

    return true;
}

bool postIteratorAnalysis(janus::Loop *loop)
{
    if (loop->unsafe)
        return false;
    auto &miter = loop->mainIterator;

    // step 1: if the loop contains undecided depending variables
    // traverse once again since more information is available now
    if (loop->undecidedPhiVariables.size()) {
        LOOPLOG("\tPerforming dependence analysis (Second pass)" << endl);
        bool foundNewIterator = false;
        for (auto iter = loop->undecidedPhiVariables.begin();
             iter != loop->undecidedPhiVariables.end();) {
            VarState *unPhi = *iter;
            // firstly try to construct cyclic relations
            if (buildCyclicExpr(unPhi->expr, loop)) {
                if (unPhi->expr->kind == Expr::NONE) {
                    loop->unsafe = true;
                    return false;
                }
                LOOPLOG("\t\t" << unPhi << " Cylic "
                               << *unPhi->expr->expandedCyclicForm << endl);
                loop->phiVariables.insert(unPhi);
            } else if (writeAfterWriteAnalysis(loop, unPhi)) {
                // In this case the phi variable is not an iterator,
                // but a private variable with an irregular modification pattern
                // (hence the phi node) And conditional merging will be used
                // after the loop for it
                if (unPhi->type == JVAR_REGISTER) {
                    loop->registerToConditionalMerge.insert(unPhi->value);
                } else if (unPhi->type == JVAR_STACK ||
                           unPhi->type == JVAR_STACKFRAME) {
                    loop->stackToConditionalMerge.insert(*(Variable *)unPhi);
                } else {
                    LOOPLOG("\t\t"
                            << unPhi
                            << "is private phi variable with conditional "
                               "merging, but not stack or register!"
                            << endl);
                }
                LOOPLOG("\t\t"
                        << unPhi
                        << " is only a WAW dependence, and will be resolved "
                           "with conditional merging after the loop finishes!"
                        << endl);
                iter = loop->undecidedPhiVariables.erase(iter);
                continue; // Don't do the rest of iterator checks (because this
                          // is not an iterator!)
            } else {
                loop->unsafe = true;
                LOOPLOG("\tCould not find cyclic expressions for phi node "
                        << unPhi << endl);
                return false;
            }
            Iterator it(unPhi, loop);
            if (it.kind != Iterator::ITERATOR_NONE) {
                loop->iterators[unPhi] = it;
                foundNewIterator = true;
            } else {
                // iterator could not be found, set this loop unsafe
                loop->unsafe = true;
                return false;
            }
            iter++;
        }

        if (foundNewIterator) {
            // Only redo the iterator analysis if we've found a new iterator in
            // the undecided variables
            finalValueAnalysis(loop);

            // assign loop iterators again
            for (auto &iter : loop->iterators) {
                loop->parent->iterators[iter.first] = &(iter.second);
                iter.first->expr->iteratorTo = &(iter.second);
            }
        }
    }

    if (!miter) {
        loop->unsafe = true;
        LOOPLOG("\tFurther analysis disabled due to failed iterator analysis"
                << endl);
        return false;
    }

    LOOPLOG("\tLoop " << dec << loop->id << " post iterator " << *miter
                      << endl);
    LOOPLOG("\tPerforming iteration count analysis" << endl);
    miter->main = true;

    // step 2: now the init, final, stride values are found for the iterator
    // work out the number of steps for the iterator
    // simplest case, all integers
    if (miter->kind == Iterator::INDUCTION_IMM &&
        miter->initKind == Iterator::INTEGER &&
        miter->finalKind == Iterator::INTEGER) {
        miter->strideKind = Iterator::INTEGER;
        miter->stepKind = Iterator::CONSTANT_IMM;
        miter->step = (miter->finalImm - miter->initImm) / miter->stride + 1;
        loop->staticIterCount = miter->step;
        LOOPLOG("\t\tTotal iteration count: " << miter->step << endl);
    } else if (miter->kind == Iterator::INDUCTION_IMM) {
        miter->strideKind = Iterator::INTEGER;
        // second simplest case, strides are integers
        // extend init expression to function scope
        ExpandedExpr *initExprs = NULL;
        ExpandedExpr *finalExprs = NULL;
        if (miter->initKind == Iterator::EXPR) {
            miter->initKind = Iterator::EXPANDED_EXPR;
            initExprs = expandExpr(&miter->initExpr, loop->parent, loop);
            if (initExprs && initExprs->kind != ExpandedExpr::EMPTY) {
                LOOPLOG("\t\tExpand init expr to: " << *initExprs << endl);
            } else
                initExprs = NULL;
            miter->initExprs = initExprs;
        } else if (miter->initKind == Iterator::EXPANDED_EXPR) {
            initExprs = miter->initExprs;
            initExprs->extendToFuncScope(loop->parent, loop);
            LOOPLOG("\t\tExpand init expr to: " << *initExprs << endl);
        } else if (miter->initKind == Iterator::INTEGER) {
            initExprs = new ExpandedExpr(ExpandedExpr::SUM);
            initExprs->addTerm(Expr(miter->initImm));
        } else if (miter->initKind == Iterator::SINGLE_VAR) {
            initExprs = expandExpr(miter->initVar->expr, loop->parent, loop);
            if (initExprs && initExprs->kind != ExpandedExpr::EMPTY) {
                LOOPLOG("\t\tExpand init expr to: " << *initExprs << endl);
            } else
                initExprs = NULL;
            miter->initExprs = initExprs;
        }

        // extend final expression to function scope
        if (miter->finalKind == Iterator::EXPR) {
            miter->finalKind = Iterator::EXPANDED_EXPR;
            finalExprs = expandExpr(&miter->finalExpr, loop->parent, loop);

            if (finalExprs && finalExprs->kind != ExpandedExpr::EMPTY) {
                LOOPLOG("\t\tExpand final expr to: " << *finalExprs << endl);
            } else
                finalExprs = NULL;
            miter->finalExprs = finalExprs;
        } else if (miter->finalKind == Iterator::EXPANDED_EXPR) {
            finalExprs = miter->finalExprs;
            finalExprs->extendToFuncScope(loop->parent, loop);
            LOOPLOG("\t\tExpand final expr to: " << *finalExprs << endl);
        } else if (miter->finalKind == Iterator::INTEGER) {
            finalExprs = new ExpandedExpr(ExpandedExpr::SUM);
            finalExprs->addTerm(Expr(miter->finalImm));
        } else if (miter->finalKind == Iterator::SINGLE_VAR) {
            finalExprs = expandExpr(miter->finalVar->expr, loop->parent, loop);
            if (finalExprs && finalExprs->kind != ExpandedExpr::EMPTY) {
                LOOPLOG("\t\tExpand final expr to: " << *finalExprs << endl);
            } else
                finalExprs = NULL;
            miter->finalExprs = finalExprs;
        }

        if (initExprs && finalExprs && initExprs->kind != ExpandedExpr::SENSI &&
            finalExprs->kind != ExpandedExpr::SENSI) {
            ExpandedExpr spectrum = *finalExprs - *initExprs;
            // spectrum.extendToFuncScope(loop->parent);
            miter->stepKind = Iterator::CONSTANT_EXPR;
            miter->stepExprs = new ExpandedExpr(ExpandedExpr::EMPTY);
            *miter->stepExprs = spectrum;
            LOOPLOG("\t\tSpectrum expression: " << spectrum << endl);
            auto pair = spectrum.evaluate(loop);
            if (pair.first == Expr::INTEGER) {
                miter->stepKind = Iterator::CONSTANT_IMM;
                miter->step = pair.second / miter->stride + 1;
                loop->staticIterCount = miter->step;
                LOOPLOG("\t\tTotal iteration count: " << dec << miter->step
                                                      << endl);
            } else {
                LOOPLOG("\t\tTotal iteration count: ("
                        << spectrum << ")/" << miter->stride << endl);
                miter->stepKind = Iterator::CONSTANT_EXPR;
                *miter->stepExprs = spectrum;
            }

            // update init and final immediate expressions
            if (miter->initExprs) {
                auto pairi = miter->initExprs->evaluate(loop);
                if (pairi.first == Expr::INTEGER) {
                    miter->initKind = Iterator::INTEGER;
                    miter->initImm = pairi.second;
                }
            }

            if (miter->finalExprs) {
                auto pairf = miter->finalExprs->evaluate(loop);
                if (pairf.first == Expr::INTEGER) {
                    miter->finalKind = Iterator::INTEGER;
                    miter->finalImm = pairf.second;
                }
            }

        } else {
            LOOPLOG("\t\tError in obtaining the spectrum expression! " << endl);
        }
    }
    LOOPLOG("\t\tLoop " << dec << loop->id << " improved iterator " << *miter
                        << endl);

    // step 3: conclude scalar evolution for each iterator
    for (auto iterP : loop->iterators) {
        Iterator &iter = iterP.second;
        Expr start;
        Expr stride;
        if (iter.initKind == Iterator::INTEGER)
            start = Expr(iter.initImm);
        else if (iter.initKind == Iterator::SINGLE_VAR)
            start = Expr(iter.initVar);
        else if (iter.initKind == Iterator::EXPR)
            start = iter.initExpr;
        // else start = Expr(iter.initExprs);

        if (iter.kind == Iterator::INDUCTION_IMM)
            stride = Expr(iter.stride);
        else if (iter.kind == Iterator::INDUCTION_VAR)
            stride = Expr(iter.strideVar);
        else if (iter.kind == Iterator::ITER_GENERIC)
            stride = Expr(iter.strideExprs);

        iter.scev = new SCEV(start, stride, &iter);
    }

    // Step 4: try to recognise reduction operations
    for (auto &iter : loop->iterators) {
        if (iter.second.kind == Iterator::INDUCTION_VAR) {
            VarState *varStride = iter.second.strideVar;
            iter.second.strideKind = Iterator::SINGLE_VAR;
            if (!loop->isConstant(varStride) &&
                loop->constPhiVars.find(varStride) ==
                    loop->constPhiVars.end()) {
                LOOPLOG("\t\tLoop " << dec << loop->id << " stride "
                                    << iter.second << " is a reduction variable"
                                    << endl);
                iter.second.kind = Iterator::REDUCTION_PLUS;
                LOOPLOG("\t\tReduction variables not supported!" << endl);
                loop->unsafe = true;
                return false; // Reduction variables not supported!
            }
        } else if (iter.second.kind == Iterator::ITER_GENERIC) {
            // for strides that are not immediate, check if the stride
            // expression is a constant expand the stride expression
            ExpandedExpr *strideExpr = iter.second.strideExprs;
            strideExpr->extendToLoopScope(loop);
            if (!strideExpr->isConstant()) {
                LOOPLOG("\t\tLoop " << dec << loop->id << " stride "
                                    << iter.second << " is a reduction variable"
                                    << endl);
                iter.second.kind = Iterator::REDUCTION_PLUS;
                iter.second.strideKind = Iterator::EXPANDED_EXPR;
                LOOPLOG("\t\tReduction variables not supported!" << endl);
                loop->unsafe = true;
                return false; // Reduction variables not supported!
            }
        } else
            iter.second.strideKind = Iterator::INTEGER;
    }

    return true;
}

static void getIteratorFinalValue(ExpandedExpr &boundExpr, Iterator &iterator)
{
    // simplify the expression first
    boundExpr.simplify();

    // if the boundexpr doesn't contain the iterator
    if (!boundExpr.hasTerm(iterator.vs))
        return;
    Expr A = boundExpr.get(iterator.vs);

    // cout <<"solve "<<boundExpr<<endl;
    if (boundExpr.size() == 2) {
        // case 1: A * iter + B == 0
        // final = -B/A
        if (boundExpr.hasTerm(Expr(1))) {
            Expr B = boundExpr.get(1);
            if (B.kind == Expr::INTEGER && A.kind == Expr::INTEGER) {
                if (A.i) {
                    int64_t finalValue = -B.i / A.i;
                    iterator.finalImm = finalValue;
                    iterator.finalKind = Iterator::INTEGER;
                }
            }
        } else if (A.i == -1) {
            for (auto e : boundExpr.exprs) {
                if (e.second.i == 1) {
                    iterator.finalKind = Iterator::SINGLE_VAR;
                    iterator.finalVar = e.first.vs;
                }
            }
        } else
            LOOPLOG("\t\tNot able to solve expression:" << boundExpr << endl);
    } else if (A.i == 1) {
        // case 2: 1 * iter + A + B + C ... = 0
        // final = -(A+B+C)
        iterator.finalKind = Iterator::EXPANDED_EXPR;
        // copy
        iterator.finalExprs = new ExpandedExpr(ExpandedExpr::SUM);
        *iterator.finalExprs = boundExpr;
        iterator.finalExprs->remove(*iterator.vs->expr);
        iterator.finalExprs->negate();
    } else if (A.i == -1) {
        // case 2: -1 * iter + A + B + C ... = 0
        // final = (A+B+C)
        iterator.finalKind = Iterator::EXPANDED_EXPR;
        // copy
        iterator.finalExprs = new ExpandedExpr(ExpandedExpr::SUM);
        *iterator.finalExprs = boundExpr;
        iterator.finalExprs->remove(*iterator.vs->expr);
    } else
        LOOPLOG("\t\tNot able to solve expression:" << boundExpr << endl);
}

Expr Iterator::getInitExpr()
{
    if (initKind == INTEGER)
        return Expr(initImm);
    else if (initKind == SINGLE_VAR)
        return Expr(initVar);
    else if (initKind == EXPR)
        return initExpr;
    else
        return Expr(initExprs);
}

Expr Iterator::getStrideExpr()
{
    if (kind == INDUCTION_IMM)
        return Expr(stride);
    else if (kind == INDUCTION_VAR)
        return Expr(strideVar);
    else {
        return Expr(strideExprs);
    }
}

std::ostream &janus::operator<<(std::ostream &out, const Iterator &iter)
{
    out << "[";
    if (iter.initKind == Iterator::INTEGER)
        out << dec << iter.initImm;
    else if (iter.initKind == Iterator::SINGLE_VAR)
        out << iter.initVar;
    else if (iter.initKind == Iterator::EXPR)
        out << iter.initExpr;
    else if (iter.initKind == Iterator::EXPANDED_EXPR) {
        if (iter.initExprs)
            out << "(" << *iter.initExprs << ")";
        else
            out << "NULL";
    }
    out << ":";

    /* print stride */
    if (iter.kind == Iterator::INDUCTION_IMM)
        out << dec << iter.stride;
    else if (iter.kind == Iterator::INDUCTION_VAR)
        out << iter.strideVar;
    else
        out << *iter.strideExprs;

    out << ":";
    if (iter.finalKind == Iterator::INTEGER)
        out << dec << iter.finalImm;
    else if (iter.finalKind == Iterator::SINGLE_VAR)
        out << iter.finalVar;
    else if (iter.finalKind == Iterator::EXPR)
        out << iter.finalExpr;
    else if (iter.finalKind == Iterator::EXPANDED_EXPR) {
        if (iter.finalExprs)
            out << "(" << *iter.finalExprs << ")";
        else
            out << "NULL";
    }
    out << "]";
    if (iter.loop)
        out << "@L" << dec << iter.loop->id;
    return out;
}

void encodeIterators(Loop *loop)
{
    LOOPLOG("\tEncoding recognised variables into portable variable profiles"
            << endl);
    JVarProfile profile;

    for (auto &iters : loop->iterators) {
        Iterator &iter = iters.second;

        if (iter.kind == Iterator::INDUCTION_IMM ||
            iter.kind == Iterator::INDUCTION_VAR) {
            profile.var = (Variable)*iter.vs;
            profile.type = INDUCTION_PROFILE;
            profile.induction.op = UPDATE_ADD;

            if (iter.kind == Iterator::INDUCTION_IMM) {
                Variable stride;
                stride.type = JVAR_CONSTANT;
                stride.value = iter.stride;
                profile.induction.stride = stride;
            } else if (iter.kind == Iterator::INDUCTION_VAR) {
                profile.induction.stride = *iter.strideVar;
            }

            if (iter.main) {
                // only main iterator has the checks
                if (iter.checkState) {
                    profile.induction.check = *iter.checkState;
                    loop->checkVars.insert(*iter.checkState);
                } else if (iter.finalKind == Iterator::INTEGER) {
                    Variable check;
                    check.type = JVAR_CONSTANT;
                    check.value = iter.finalImm;
                    profile.induction.check = check;
                } else if (iter.finalKind == Iterator::SINGLE_VAR) {
                    profile.induction.check = *iter.finalVar;
                } else if (iter.finalKind == Iterator::EXPR) {
                    if (!iter.finalExpr.vs)
                        continue;
                    profile.induction.check = *iter.finalVar;
                } else {
                    if (iter.finalExprs) {
                        LOOPLOG("\t\t\tIrregular Check " << *iter.finalExprs
                                                         << endl);
                    }
                    profile.induction.check.type = JVAR_UNKOWN;
                }
            } else {
                profile.induction.check.type = JVAR_UNKOWN;
            }

            // if both the check and stride variables are constant, encode the
            // init value too
            if (iter.kind == Iterator::INDUCTION_IMM &&
                iter.finalKind == Iterator::INTEGER &&
                iter.initKind == Iterator::INTEGER) {
                Variable init;
                init.type = JVAR_CONSTANT;
                init.value = iter.initImm;
                profile.induction.init = init;
            } else
                profile.induction.init.type = JVAR_UNKOWN;
            loop->encodedVariables.push_back(profile);
            LOOPLOG("\t\t" << profile << endl);
        }
    }
}

Instruction *Iterator::getUpdateInstr()
{
    if (!vs || !vs->isPHI)
        return NULL;
    for (auto pred : vs->pred)
        if (pred && pred->lastModified &&
            pred->lastModified->opcode == Instruction::Add)
            return pred->lastModified;
    return NULL;
}
