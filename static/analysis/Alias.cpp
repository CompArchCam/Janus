#include "Alias.h"
#include "Affine.h"
#include "BasicBlock.h"
#include "Expression.h"
#include "Function.h"
#include "IO.h"
#include "JanusContext.h"
#include "MemoryLocation.h"
#include "SSA.h"
#include "Utility.h"
#include <cstring>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <stack>

using namespace janus;
using namespace std;

// lamport test for single variables
static AliasType lamportTest(ExpandedSCEV &diff);
static AliasType GCDTest(ExpandedSCEV &e1, ExpandedSCEV &e2, ExpandedSCEV &diff,
                         Loop &loop);
static AliasType banerjeeTest(ExpandedSCEV &diff);

void aliasAnalysis(janus::Loop *loop)
{
    Function *parent = loop->parent;
    BasicBlock *entry = parent->entry;

    /* Read set */
    auto &readSets = loop->memoryReads;
    /* Write set */
    auto &writeSets = loop->memoryWrites;

    LOOPLOG2("---------------------------------------------------------"
             << endl);
    LOOPLOG2("Loop " << dec << loop->id << " Alias Analysis" << endl);

    // step 0: affine analysis
    // scan for a specific affine conditions for the given loop
    affineAnalysis(loop);

    LOOPLOG2("\n\tStep 0: Extracting loop ranges in the loop nest:" << endl);
    LOOPLOG2("\t\tMain Loop " << dec << loop->id << " range "
                              << *loop->mainIterator << " trip count "
                              << loop->staticIterCount << endl);

    auto myNest = parent->context->loopNests[loop->nestID];

    for (auto l : myNest) {
        if (l != loop->id) {
            Loop *nl = parent->context->loops.data() + (l - 1);
            if (nl->mainIterator) {
                LOOPLOG2("\t\tLoop " << nl->id << " range " << *nl->mainIterator
                                     << " trip count " << nl->staticIterCount
                                     << endl);
            }
        }
    }

    // step 1: record all memory access into read and write sets
    // and build scalar evolution
    LOOPLOG2("\n\tStep 1: Analyse each memory access in the loop:" << endl);
    for (auto bid : loop->body) {
        BasicBlock &bb = entry[bid];
        for (auto &mi : bb.minstrs) {
            // skip for load effective address
            if (mi.mem->type != JVAR_MEMORY)
                continue;

            LOOPLOG2("\t\t" << mi << endl);
            // create a memory variable for each memory instruction
            MemoryLocation *memVar = new MemoryLocation(&mi, loop);
            LOOPLOG2("\t\t\tMemory expression: " << memVar->expr
                                                 << " constructed" << endl);
            // create a memory variable for each memory instruction
            memVar->getSCEV();
            LOOPLOG2("\t\t\tScalar evolution: " << *memVar->escev << endl);
            // insert the memory location into the loop array accesses
            MemoryLocation *location = getOrInsertMemLocations(loop, memVar);
            // add to read and write set
            if (mi.type == MemoryInstruction::Read ||
                mi.type == MemoryInstruction::ReadAndRead) {
                location->readBy.insert(mi.instr);
                readSets.insert(location);
            } else if (mi.type == MemoryInstruction::Write) {
                location->writeFrom.insert(mi.instr);
                writeSets.insert(location);
            } else if (mi.type == MemoryInstruction::ReadAndWrite) {
                location->readBy.insert(mi.instr);
                readSets.insert(location);
                location->writeFrom.insert(mi.instr);
                writeSets.insert(location);
            }
        }
    }

    LOOPLOG2("\n\t"
             << "Identified read set: " << endl);
    IF_LOOPLOG2(for (auto memLoc
                     : readSets) { loopLog2 << "\t\t" << *memLoc << endl; });
    LOOPLOG2("\t"
             << "Identified write set: " << endl);
    IF_LOOPLOG2(for (auto memLoc
                     : writeSets) { loopLog2 << "\t\t" << *memLoc << endl; });
    LOOPLOG2("\tRecognised array bases:" << endl);
    IF_LOOPLOG2(for (auto &memBase
                     : loop->arrayAccesses) {
        loopLog2 << "\t\t" << memBase.first << " ";
        // print the accessed instructions
        for (auto ml : memBase.second) {
            if (ml) {
                for (auto r : ml->readBy)
                    loopLog2 << "r" << dec << r->id << " ";
                for (auto w : ml->writeFrom)
                    loopLog2 << "w" << dec << w->id << " ";
            }
        }
        loopLog2 << endl;
    });

    // step 2: perform alias analysis on each memory write with respect to each
    // memory read with the same array base alias analysis is only performed
    // within the same memory base
    LOOPLOG2("\n\tStep 2: Performing SCEV-based alias analysis on loop memory "
             "accesses"
             << endl);
    for (auto memWrite : writeSets) {
        LOOPLOG2("\t\tCheck write: " << *memWrite << endl);
        // if the write expression is fixed for all iterations
        // then we have write after write memory dependencies
        if (!memWrite->containIterator(loop)) {
            LOOPLOG2("\t\tFound a write after write dependency. \n\t\tIt could "
                     "be resolved by privatizing memories :"
                     << *memWrite << endl);
            loop->memoryDependences[memWrite].insert(memWrite);
            continue;
        }

        // retrieve all the reads/write with the same array base
        auto &locations = loop->arrayAccesses[memWrite->escev->getArrayBase()];
        // check the alias relation against each read.
        for (auto mem : locations) {
            // we skip all invisible reads and writes
            if (mem == memWrite)
                continue;

            LOOPLOG2("\t\t\tvs: " << *mem << endl);
            // check the alias relation
            checkAliasRelation(*memWrite, *mem, loop);
        }
    }

    // step 3: encode array bases for further runtime checks
    for (auto &memBase : loop->arrayAccesses) {
        // get the iterators contributed to this base
        // print the accessed instructions
        auto &iterSet = loop->arrayIterators[memBase.first];
        for (auto ml : memBase.second) {
            if (ml->writeFrom.size())
                loop->arrayToCheck.insert(memBase.first);
            for (auto iter : ml->escev->strides) {
                iterSet.insert(iter.first);
            }
        }
    }

    for (auto &memBase : loop->arrayIterators) {
        JVarProfile profile;
        profile.type = ARRAY_PROFILE;
        // temp fix
        if (memBase.first.kind == Expr::EXPANDED)
            continue;
        if (memBase.first.vs == NULL)
            continue;
        profile.array.base = *(JVar *)memBase.first.vs;
        profile.version = memBase.first.vs->version;
        long max_range = 0;
        for (auto iter : memBase.second) {
            max_range += iter->stride * iter->loop->staticIterCount;
        }
        profile.array.max_range.type = JVAR_CONSTANT;
        profile.array.max_range.value = max_range;
        loop->encodedVariables.push_back(profile);
    }

    IF_LOOPLOG2(if (loop->memoryDependences.size() == 0 &&
                    loop->undecidedMemAccesses.size() == 0) {
        loopLog2 << "\nThis loop is a DOALL loop ";
        if (loop->arrayToCheck.size() * (loop->arrayAccesses.size() - 1))
            loopLog2 << "with runtime array base check of "
                     << loop->arrayToCheck.size() *
                            (loop->arrayAccesses.size() - 1)
                     << endl;
        else
            loopLog2 << "!" << endl;
    });
}

/** \brief Check alias relation for two memory locations
 */
void checkAliasRelation(MemoryLocation &m1, MemoryLocation &m2, Loop *loop)
{
    AliasType result = UnknownAlias;

    if (m1.escev && m2.escev) {
        // expand the scalar evolution expressions for both locations
        ExpandedSCEV &e1 = *m1.escev;
        ExpandedSCEV &e2 = *m2.escev;

        if (e1.kind == ExpandedSCEV::Ambiguous) {
            LOOPLOG2("\t\t\tscev ambiguous" << endl);
            loop->undecidedMemAccesses.insert(&m1);
            return;
        }

        if (e2.kind == ExpandedSCEV::Ambiguous) {
            LOOPLOG2("\t\t\tscev ambiguous" << endl);
            loop->undecidedMemAccesses.insert(&m2);
            return;
        }

        // get the range difference of the two expressions
        ExpandedSCEV rdiff = getRangeDiff(e1, e2, loop);
        LOOPLOG2("\t\t\trdiff: " << rdiff << endl);
        // if both are single index variable with the same scale, we use lamport
        // test
        if (e1.strides.size() == 1 && e2.strides.size() == 1 &&
            rdiff.strides.size() == 1 &&
            (*e1.strides.begin()).second.i == (*e2.strides.begin()).second.i) {
            result = lamportTest(rdiff);
            if (result == MustAlias) {
                loop->memoryDependences[&m1].insert(&m2);
                return;
            } else if (result == MustNotAlias)
                return;
        }

        // perform the GCD dependence test
        result = GCDTest(e1, e2, rdiff, *loop);
        if (result == MustAlias) {
            loop->memoryDependences[&m1].insert(&m2);
            return;
        } else if (result == MustNotAlias)
            return;

        // perform the banerjee dependence test
        result = banerjeeTest(rdiff);
        if (result == MustAlias) {
            loop->memoryDependences[&m1].insert(&m2);
            return;
        } else if (result == MustNotAlias)
            return;

        // TODO: use integer set solver for polyhedral dependence analysis
        // at last if all tests failed, put it in undecided pool
        loop->undecidedMemAccesses.insert(&m1);
        LOOPLOG2("\t\t\tCurrently janus is not able to find the dependencies "
                 "relation for this pair"
                 << endl);
    } else {
        // use normal expression analysis
        // perform difference of two expressions
        ExpandedExpr diff = m1.expr - m2.expr;
        LOOPLOG2("\t\t\tDiff: " << diff << endl);
        auto eval = diff.evaluate(loop);
        if (eval.first == Expr::INTEGER) {
            int64_t diffImm = eval.second;
            if (diffImm != 0) {
                if (loop->mainIterator &&
                    loop->mainIterator->stepKind == Iterator::CONSTANT_IMM &&
                    loop->mainIterator->kind == Iterator::INDUCTION_IMM) {
                    int64_t max_bound =
                        loop->mainIterator->stride * loop->mainIterator->step;
                    if (abs(diffImm) >= abs(max_bound)) {
                        LOOPLOG2("\t\t\tVector difference is beyond the loop "
                                 "boundary "
                                 << loop->mainIterator->stride *
                                        loop->mainIterator->step
                                 << " for this write-read pair, therefore no "
                                    "cross-iteration dependence"
                                 << endl);
                        return;
                    }
                }
                LOOPLOG2("\t\t\tFound a cross iteration dependence for this "
                         "write-read pair"
                         << endl);
                loop->memoryDependences[&m1].insert(&m2);
            } else {
                LOOPLOG2("\t\t\tInternal uses for this write-read pair"
                         << endl);
            }
        } else {
            if (diff.exprs.size()) {
                loop->undecidedMemAccesses.insert(&m1);
                LOOPLOG2("\t\t\tCurrently janus is not able to find the "
                         "dependencies relation for this pair"
                         << endl);
            } else {
                LOOPLOG2("\t\t\tFound an exact self-dependence for this "
                         "write-read pair, it is safe for parallelisation"
                         << endl);
            }
        }
    }
}

static AliasType lamportTest(ExpandedSCEV &diff)
{
    if (diff.start.kind == Expr::INTEGER) {
        // try to solve the diff equation
        int64_t diffImm = diff.start.i;
        // if diffImm is zero, it means local reads and writes
        if (diffImm == 0) {
            LOOPLOG2("\t\t\tLocal exact self-dependence pairs" << endl);
            return MustNotAlias;
        }
        // get the difference of iterators
        Iterator *iter = (*diff.strides.begin()).first;
        Expr scale = (*diff.strides.begin()).second;
        if (scale.kind != Expr::INTEGER)
            return UnknownAlias;
        int64_t strideImm = scale.i;
        if (iter == NULL)
            return UnknownAlias;
        /* lamport test:
         * Dependency for [a*i+c1] vs [a*i+c2] occurs when
         * -(c1-c2)/a has integer solution in the iterator range
         */
        if (diffImm % strideImm == 0) {
            // it must falls in range of the iterator
            if (iter && iter->initKind == Iterator::INTEGER &&
                iter->finalKind == Iterator::INTEGER) {
                if (!((diffImm >= iter->initImm) &&
                      (diffImm <= iter->finalImm))) {
                    LOOPLOG2("\t\t\tDistance "
                             << diffImm
                             << " beyond loop range, therefore no dependence"
                             << endl);
                    return MustNotAlias;
                } else if (diffImm >= abs(iter->initImm - iter->finalImm)) {
                    LOOPLOG2("\t\t\tDistance "
                             << diffImm << " beyond loop range"
                             << abs(iter->initImm - iter->finalImm)
                             << ", therefore no dependence" << endl);
                    return MustNotAlias;
                }
            }
            LOOPLOG2("\t\t\tFound a cross iteration dependence of distance "
                     << diffImm << " within range "
                     << abs(iter->initImm - iter->finalImm)
                     << " for this write-read pair" << endl);
            return MustAlias;
        }
    }

    return UnknownAlias;
}

static int get_gcd(int a, int b) { return b == 0 ? a : get_gcd(b, a % b); }

static AliasType GCDTest(ExpandedSCEV &e1, ExpandedSCEV &e2, ExpandedSCEV &diff,
                         Loop &loop)
{
    // get the strides for all iterators in both e1 and e2
    int gcd = -1;
    for (auto term : e1.strides) {
        if (term.second.kind == Expr::INTEGER) {
            if (gcd == -1)
                gcd = term.second.i;
            else
                gcd = get_gcd(gcd, term.second.i);
        } else
            return UnknownAlias;
    }
    for (auto term : e2.strides) {
        if (term.second.kind == Expr::INTEGER) {
            if (gcd == -1)
                gcd = term.second.i;
            else
                gcd = get_gcd(gcd, term.second.i);
        } else
            return UnknownAlias;
    }

    // get the start difference
    if (diff.start.kind == Expr::INTEGER) {
        // try to solve the diff equation
        int64_t diffImm = abs(diff.start.i);

        if (diffImm % gcd == 0) {
            if (loop.staticIterCount > 0 &&
                diffImm / gcd > loop.staticIterCount) {
                LOOPLOG2("\t\t\tFound stride GCD of "
                         << gcd << " for distance " << diffImm
                         << ", which is beyond loop boundary" << endl);
                return MustNotAlias;
            } else {
                LOOPLOG2("\t\t\tFound stride GCD of "
                         << gcd << " for distance " << diffImm
                         << ",therefore a dependency exists" << endl);
                return MustAlias;
            }
        } else {
            LOOPLOG2("\t\t\tFound stride GCD of "
                     << gcd << " could not divide distance " << diffImm
                     << ", therefore no dependency" << endl);
            return MustNotAlias;
        }
    }

    return UnknownAlias;
}

static AliasType banerjeeTest(ExpandedSCEV &diff) { return UnknownAlias; }

ExpandedSCEV getRangeDiff(ExpandedSCEV &e1, ExpandedSCEV &e2, janus::Loop *loop)
{
    LOOPLOG2("\t\t\tSCEV: " << e1 << " - " << e2 << endl);
    ExpandedSCEV diff;

    Expr start(0);
    start.add(e1.start);
    start.subtract(e2.start);
    diff.start = start;
    diff.start.simplify();

    // copy e1 to diff first
    for (auto i1 : e1.strides) {
        diff.strides[i1.first] = i1.second;
    }

    for (auto i2 : e2.strides) {
        Iterator *iter = i2.first;

        if (diff.strides.find(iter) != diff.strides.end()) {
            // if the iterator is parent loops, perform subtraction, else keep
            // the iterator
            if (iter->loop != loop && iter->loop->isAncestorOf(loop)) {
                diff.strides[iter].subtract(i2.second);
            }
        } else {
            Expr neg = i2.second;
            neg.negate();
            diff.strides[iter] = neg;
        }
    }

    // remove zero terms
    for (auto e = diff.strides.begin(); e != diff.strides.end();) {
        Expr coeff = (*e).second;
        // remove the term if the coeff is zero
        if (coeff.i == 0)
            diff.strides.erase(e++);
        else
            ++e;
    }
    return diff;
}

int ExpandedSCEV::evaluate1StepDistance(Loop *loop)
{
    int result = 0;

    if (start.kind == Expr::INTEGER)
        result += start.i;
    else if (start.kind == Expr::EXPANDED) {
        auto eval = start.ee->evaluate(loop);
        if (eval.first == Expr::INTEGER)
            result += eval.second;
    }

    for (auto [_, expr] : strides) {
        // evaluate the difference when the iterator is 1 step away
        if (expr.kind == Expr::INTEGER)
            result += expr.i;
    }

    // get absolute distance
    if (result < 0)
        result = -result;
    return result;
}

/** \brief return the iterator of the current loop */
Iterator *ExpandedSCEV::getCurrentIterator(Loop *loop)
{
    for (auto &[iterator, expr] : strides) {
        if (iterator->loop == loop)
            return iterator;
    }
    // TODO: Handle case where we cannot find any iterator to loop
}

static void buildSCEV(SCEV *scev, ExpandedExpr &expr, Loop *loop)
{
    for (auto e : expr.exprs) {
        Expr term = e.first;
        if (term.iteratorTo) {
            if (term.iteratorTo->kind == Iterator::ITER_GENERIC) {
                scev->kind = SCEV::Ambiguous;
                return;
            }
            // if the term belongs to the current loop
            if (term.iteratorTo->loop == loop) {
                SCEV cur;
                cur.addScalarTerm(term.iteratorTo->getInitExpr());
                cur.addStrideTerm(term.iteratorTo->getStrideExpr());
                cur.multiply(e.second);
                scev->addScalarTerm(cur.start);
                scev->addStrideTerm(cur.stride);
                scev->iter = term.iteratorTo;
            } else {
                // expand the add recurrence of the loop iterator
                Expr init = term.iteratorTo->getInitExpr();

                if (init.kind == Expr::EXPANDED)
                    init.ee->extendToFuncScope(loop->parent, loop);
                else if (init.kind == Expr::VAR) {
                    ExpandedExpr *ee = expandExpr(init.v->expr, loop);
                    init.ee = ee;
                    init.kind = Expr::EXPANDED;
                } else {
                    ExpandedExpr *ee = expandExpr(&init, loop);
                    init.ee = ee;
                    init.kind = Expr::EXPANDED;
                }

                // alter the expression from iterators to recurrences
                ExpandedExpr temp(ExpandedExpr::SUM);
                // remove the iterator first
                for (auto e = init.ee->exprs.begin();
                     e != init.ee->exprs.end();) {
                    Expr node = (*e).first;
                    Expr coeff = (*e).second;
                    if (node.iteratorTo && node.kind != Expr::ADDREC) {
                        init.ee->exprs.erase(e++);
                        temp.addTerm(node, coeff);
                    } else
                        ++e;
                }

                SCEV *new_scev = new SCEV();
                if (temp.size() > 1) {
                    LOOPLOG2("\t\t\tMultiple iterator in SCEV "
                             << temp << " not yet implemented" << endl);
                    return;
                } else if (temp.size() == 1) {
                    buildSCEV(new_scev, temp, loop);
                    // create new node
                    Expr node;
                    node.kind = Expr::ADDREC;
                    node.rec.base = &new_scev->start;
                    node.rec.stride = &new_scev->stride;
                    node.rec.iter = ((*temp.exprs.begin()).first).iteratorTo;
                    init.ee->addTerm(node);
                }

                SCEV cur;
                cur.addScalarTerm(init);
                cur.addStrideTerm(term.iteratorTo->getStrideExpr());
                cur.multiply(e.second);
                scev->iter = term.iteratorTo;
                scev->addScalarTerm(cur.start);
                scev->addStrideTerm(cur.stride);
            }
        } else
            scev->addScalarTerm(e.first, e.second);
    }
}

void MemoryLocation::getSCEV()
{
    if (!vs || vs->type != JVAR_MEMORY)
        return;
    if (escev)
        return;

    LOOPLOG2("\t\t\tExpr:" << expr << endl);

    escev = new ExpandedSCEV();

    // a stack for the expression to be expanded
    stack<pair<Iterator *, Expr>> checkList;

    // step 1: force expanding start expersion, so that it can be easily merged
    escev->start.kind = Expr::EXPANDED;
    escev->start.ee = new ExpandedExpr(ExpandedExpr::SUM);

    for (auto e : expr.exprs) {
        Expr term = e.first;
        if (term.iteratorTo) {
            // add to the stack
            checkList.push(make_pair(term.iteratorTo, e.second));
        } else {
            escev->start.ee->addTerm(e.first, e.second);
        }
    }

    while (!checkList.empty()) {
        auto check = checkList.top();
        checkList.pop();

        Iterator *iter = check.first;

        if (iter->kind == Iterator::ITER_GENERIC) {
            LOOPLOG2("\t\tScalar evolution contains complex iterators!"
                     << endl);
            escev->kind = ExpandedSCEV::Ambiguous;
            return;
        }

        Expr scale = check.second;

        Expr init = iter->getInitExpr();
        // check if the init has other iterators
        if (init.kind == Expr::INTEGER) {
            init.multiply(scale);
            escev->start.ee->addTerm(init);
        } else {
            // expand init
            if (init.kind == Expr::VAR) {
                ExpandedExpr *ee = expandExpr(init.v->expr, loop);
                init.ee = ee;
                init.kind = Expr::EXPANDED;
            } else if (init.kind != Expr::EXPANDED) {
                LOOPLOG2("\t\tScalar evolution case not yet handled" << endl);
                escev->kind = ExpandedSCEV::Ambiguous;
                return;
            }
            init.ee->extendToFuncScope(loop->parent, loop);

            if (init.ee->kind == ExpandedExpr::SENSI) {
                LOOPLOG2("\t\tScalar evolution init case not yet handled"
                         << endl);
                escev->kind = ExpandedSCEV::Ambiguous;
                return;
            }

            // check if the expanded init
            // whether it has more loop iterators
            for (auto term : init.ee->exprs) {
                Expr node = term.first;
                Expr coeff = term.second;
                coeff.multiply(scale);
                if (node.iteratorTo)
                    checkList.push(make_pair(node.iteratorTo, coeff));
                else
                    escev->start.ee->addTerm(node, coeff);
            }
        }

        // next consider strides
        Expr stride = iter->getStrideExpr();
        if (stride.kind == Expr::EXPANDED &&
            stride.ee->kind == ExpandedExpr::SENSI) {
            LOOPLOG2("\t\tScalar evolution building failed due to complex "
                     "stride expression"
                     << endl);
            escev->kind = ExpandedSCEV::Ambiguous;
            return;
        }
        stride.multiply(scale);
        // add to the final
        escev->strides[iter] = stride;
        escev->kind = ExpandedSCEV::Normal;
    }
    // simplify the base expression
    escev->start.simplify();
}

MemoryLocation *getOrInsertMemLocations(Loop *loop, MemoryLocation *loc)
{
    if (!loc || !loc->escev)
        return NULL;
    // obtain array base for this location
    Expr arrayBase = loc->escev->getArrayBase();

    // retrieve the array accesses
    auto query = loop->arrayAccesses.find(arrayBase);

    // retrieve the point to the actual storage of the memory location
    MemoryLocation *location = NULL;

    if (query == loop->arrayAccesses.end()) {
        loop->arrayAccesses[arrayBase].push_back(loc);
        location = loc;
        LOOPLOG2("\t\t\tInsert new array base ("
                 << arrayBase << ") with location " << *location << endl);
    } else {
        auto &locations = (*query).second;
        // For existing memory locations, try to find the same location
        // inefficient, switch to binary tree
        for (auto &l : locations) {
            if (l && (*l == *loc)) {
                location = l;
                LOOPLOG2("\t\t\tFound existing array base ("
                         << arrayBase << ") with existing location "
                         << *location << endl);
                delete loc;
                return location;
            }
        }
        if (!location) {
            locations.push_back(loc);
            location = loc;
            LOOPLOG2("\t\t\tFound existing array base ("
                     << arrayBase << ") with new location " << *location
                     << endl);
        }
    }
    return location;
}
