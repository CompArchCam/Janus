#include "Loop.h"
#include "Alias.h"
#include "Analysis.h"
#include "BasicBlock.h"
#include "Dependence.h"
#include "Function.h"
#include "IO.h"
#include "Iterator.h"
#include "JanusContext.h"
#include "Utility.h"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <queue>
#include <set>
#include <sstream>
#include <stack>
using namespace janus;
using namespace std;

/* A loop is identified when:
 * For every back edge from a node Y to a node X, X dominates Y
 * then node X is called the loop start node
 * Y is called the loop end node
 * Each loop has only one start node (X) but may have multiple end nodes (Ys) */
void searchLoop(JanusContext *gc, Function *function)
{
    if (!gc || !function || !function->isExecutable)
        return;

    auto &loopArray = gc->loops;

    BasicBlock *entry;
    uint32_t size;

    if (!function->isExecutable) {
        entry = nullptr;
        size = 0;
    } else {
        entry = function->getCFG().entry;
        size = function->getCFG().blocks.size();
    }

    if (size == 0)
        return;

    /* A set to find if a loop has been created to avoid duplication */
    set<uint32_t> loopPool;

    /* Search for the back edges in the CFG */
    for (int i = 0; i < size; i++) {
        BasicBlock &bb = entry[i];
        BasicBlock *succ1 = bb.succ1;
        BasicBlock *succ2 = bb.succ2;

        /* A to B is a backedge if
         * A.firstVisitStep > B.firstVisitStep &&
         * A.secondVisitStep < B.secondVisitStep
         */
        if (succ1 && bb.firstVisitStep > succ1->firstVisitStep &&
            bb.secondVisitStep < succ1->secondVisitStep &&
            succ1->dominates(bb) &&
            loopPool.find(succ1->bid) == loopPool.end()) {
            loopPool.insert(succ1->bid);
        }
        if (succ2 && bb.firstVisitStep > succ2->firstVisitStep &&
            bb.secondVisitStep < succ2->secondVisitStep &&
            succ2->dominates(bb) &&
            loopPool.find(succ2->bid) == loopPool.end()) {
            loopPool.insert(succ2->bid);
        }

        /* Special case for single basic block loop */
        if ((succ1 && (bb.bid == succ1->bid)) ||
            (succ2 && (bb.bid == succ2->bid))) {
            loopPool.insert(bb.bid);
        }
    }

    loopArray.reserve(loopPool.size() + 1);
    /* Now all loop stack blocks have been recognised
     * contruct loops */
    for (auto block_id : loopPool) {
        cout << block_id << ": Loop " << loopArray.size() << endl;
        loopArray.emplace_back(entry + block_id, function);
    }
}

/* We use a global variable to increment loop id
 * and this loop id starts from 1 */
static LoopID globalLoopID = 1;

Loop::Loop(BasicBlock *start, Function *parent)
    : start(start), parent(parent), parentLoop(NULL), analysed(false),
      level(-1), unsafe(false), dependenceAvailable(false), mainIterator()
{
    pass = false;
    staticIterCount = 0;
    id = globalLoopID++;
    needRuntimeCheck = false;
    mainIterator = NULL;
    affine = false;
    vectorWordSize = 0;

    /* Record this id in parent function */
    parent->loops.insert(id);

    BlockID startBID = start->bid;

    /* Step 0: check whether this loop contains only one block */
    if ((start->succ1 && start->succ1->bid == startBID) ||
        (start->succ2 && start->succ2->bid == startBID)) {
        for (auto pred : (start->pred)) {
            if (pred->bid != startBID)
                init.insert(pred->bid);
        }

        end.insert(startBID);
        body.insert(startBID);
        check.insert(startBID);

        if (start->succ1 && start->succ1->bid == startBID) {
            if (start->succ2)
                exit.insert(start->succ2->bid);
        } else if (start->succ1)
            exit.insert(start->succ1->bid);
        return;
    }

    /* Step 1: We interpret the end node of a loop to be
     * front-end of an back edge: end->start. Therefore the end node
     * could be the predecessor of start node.
     * A loop may have may back edges, such as B1->Start, B2->Start.
     * An end node needs to meet two conditions.
     * 1: the node must be dominated by the start node.
     * 2: the node must be one of the predecessor of the start node.
     */
    for (auto pred : (start->pred)) {
        if (start->dominates(pred))
            end.insert(pred->bid);
        else
            /* otherwise it is an init node */
            init.insert(pred->bid);
    }

    /* Step 2: We then starts from each end node of the loop,
     * traverse in the opposite direction of dataflow until
     * we meet the start node. For each node we visited,
     * if the start node dominate this node, then this node
     * is the body node of the loop.
     */
    uint32_t blockCount = parent->getCFG().blocks.size();
    bool *visited = (bool *)malloc(blockCount * sizeof(bool));
    memset(visited, 0, blockCount * sizeof(bool));
    BasicBlock *entry = parent->getCFG().entry;

    stack<BlockID> bbStack;

    for (auto endNodeID : end) {
        bbStack.push(endNodeID);

        while (!bbStack.empty()) {
            /* Retrieve the basic block from the stack */
            BlockID bid = bbStack.top();
            bbStack.pop();
            /* Insert in body set */
            body.insert(bid);
            visited[bid] = true;

            /* Stop until we hit the start node */
            if (bid == startBID)
                continue;

            BasicBlock &bb = entry[bid];
            /* Push the non-visited dominating predecessor in the stack */
            for (auto pred : bb.pred) {
                if (!visited[pred->bid] && start->dominates(pred)) {
                    bbStack.push(pred->bid);
                }
            }
        }
    }
    free(visited);

    /* Step 3: Go through all loop body nodes and find all exit node
     * and check note. An exit node is identified if the successor of
     * a body node is not identified as body node. If a body node
     * leads to an exit node, it should be identified as a check node */
    for (auto nodeID : body) {
        BasicBlock &bb = entry[nodeID];

        if (bb.succ1 && body.find(bb.succ1->bid) == body.end()) {
            check.insert(nodeID);
            exit.insert(bb.succ1->bid);
        }

        if (bb.succ2 && body.find(bb.succ2->bid) == body.end()) {
            check.insert(nodeID);
            exit.insert(bb.succ2->bid);
        }
    }
}

void Loop::analyse(JanusContext *gc)
{
    if (analysed)
        return;

    if (gc->manualLoopSelection && !pass) {
        // even if the loop is not selected, if it belongs to the same loop nest
        // with selected loop then we should analyse the loop
        for (auto l : ancestors)
            if (l->pass)
                goto proceed;
        for (auto l : descendants)
            if (l->pass)
                goto proceed;
        return;
    }

proceed:
    analysed = true;
    BasicBlock *entry = parent->getCFG().entry;

    /* translate parent function before analysis */
    if (parent)
        parent->translate();

    // Analyse sub-loop first
    // So that information in the inner loop should be available for analysis of
    // the outer loop
    for (auto *l : subLoops)
        l->analyse(gc);

    if (gc->mode == JPROF && removed) {
        LOOPLOG(
            "-----------------------------------------------------------------"
            << endl);
        if (invocation_count)
            LOOPLOG("Loop "
                    << dec << id << " is removed due to low profiled coverage: "
                    << coverage << " or low iteration: "
                    << (total_iteration_count / invocation_count) << endl);
        else
            LOOPLOG("Loop " << dec << id
                            << " is removed due to low profiled coverage: "
                            << coverage << endl);
        // we still analyse removed loops since it might contribute to alias
        // analysis of parent/children loops
        // TODO: reduce the number of loops with the help of alias analysis
        return;
    }

    LOOPLOG(
        "=========================================================" << endl);
    LOOPLOG("Analysing Loop " << dec << id << " in " << parent->name << endl);

    /* Step 1: examine all basic blocks, check the sub-calls
     * and unsafe exits */
    for (auto bid : body) {
        // check whether it is terminated by indirect branches
        // auto check = parent->unRecognised.find(bid);
        if (parent->getCFG().unRecognised.contains(bid)) {
            sync.insert(bid);
            continue;
        }
        // examine sub calls
        if (entry[bid].lastInstr()->opcode == Instruction::Call) {
            Function *func = parent->calls[entry[bid].lastInstr()->id];
            if (func) {
                calls[bid] = func->fid;
                subCalls.insert(func->fid);
                // merge further subcalls
                for (auto sub : func->subCalls)
                    subCalls.insert(sub);
            }
        }
    }

    /* Step 2: translate all subcalls */
    Function *functions = gc->functions.data();
    for (auto call : subCalls) {
        LOOPLOG("\tAnalysing sub function calls " << functions[call].name
                                                  << endl);
        functions[call].translate();
    }

    /* Step 3: variable analysis (find constant variables) */
    variableAnalysis(this);

    /* Step 4: dependence analysis for variables (only) */
    dependenceAnalysis(this);

    /* Step 5: iterator variable analysis */
    iteratorAnalysis(this);

    LOOPLOG("========================================================="
            << endl
            << endl);
}

void Loop::analyse2(JanusContext *gc)
{
    if (unsafe)
        return;
    if (gc->manualLoopSelection && !pass)
        return;

    LOOPLOG(
        "=========================================================" << endl);
    LOOPLOG("Analysing Loop " << dec << id << " Second Pass" << endl);

    /* Step 6: iterator analysis again */
    if (!postIteratorAnalysis(this)) {
        LOOPLOG("\tPost iterator analysis failed" << endl);
    }

    LOOPLOG("========================================================="
            << endl
            << endl);
}

void Loop::analyse3(JanusContext *gc)
{
    if (unsafe)
        return;
    if (gc->manualLoopSelection && !pass)
        return;

    LOOPLOG(
        "=========================================================" << endl);
    LOOPLOG("Analysing Loop " << dec << id << " Third Pass" << endl);

    /* Step 7: alias analysis */
    aliasAnalysis(this);

    /* Step 8: scratch register analysis */
    scratchAnalysis(this);

    /* Step 9: encode loop iterators */
    encodeIterators(this);
    LOOPLOG("========================================================="
            << endl
            << endl);
}

void Loop::name(void *outputStream)
{
    ostream &os = *(ostream *)outputStream;
    os << parent->name << ":" << start->startInstID;
}

void Loop::print(void *outputStream)
{
    ostream &os = *(ostream *)outputStream;
    os << id << " ";
    name(&os);
    os << endl;
}

void Loop::fullPrint(void *outputStream) { print(outputStream); }

bool Loop::contains(BlockID bid)
{
    if (body.find(bid) != body.end())
        return true;
    else
        return false;
}

bool Loop::contains(Instruction &instr) { return contains(instr.block->bid); }

bool Loop::isAncestorOf(Loop *l)
{
    if (l == this)
        return true;
    return descendants.find(l) != descendants.end();
}

bool Loop::isDescendantOf(Loop *l)
{
    if (l == this)
        return true;
    if (!l)
        return true;
    return ancestors.find(l) != ancestors.end();
}

Loop *Loop::lowestCommonAncestor(Loop *l)
{
    if (l == NULL)
        return NULL;
    if (isDescendantOf(l))
        return l;
    if (isAncestorOf(l))
        return this;

    set<Loop *> commonAncestors;

    // perform a union of the two loop's ancestors
    for (auto an : ancestors) {
        if (l->ancestors.find(an) != l->ancestors.end())
            commonAncestors.insert(an);
    }
    if (!commonAncestors.size())
        return NULL;
    // find lowest from the common ancestors
    Loop *lowest = NULL;

    // for each element, simply check which one is lowest
    for (auto can : commonAncestors) {
        if (!lowest) {
            lowest = can;
            continue;
        }
        if (can->isDescendantOf(lowest))
            lowest = can;
    }

    return lowest;
}

bool Loop::isConstant(VarState *vs)
{
    // Memory variable is constant only if its address is constant
    if (vs->type == JVAR_MEMORY) {
        bool cons = true;
        for (auto pred : vs->pred) {
            cons &= isConstant(pred);
        }
        return cons;
    }
    // VarState defined outside of the loop is constant (otherwise would go
    // through phi node)
    if (body.find(vs->block->bid) == body.end())
        return true;
    if (!vs->constLoop)
        return false;
    else if (vs->constLoop == this)
        return true;
    else
        // If vs is constant in an ancestor, then it must be constant here as
        // well
        return isDescendantOf(vs->constLoop);
    return false;
}

bool Loop::isInitial(VarState *vs)
{
    // skip immediate value
    if (vs->notUsed)
        return false;
    if (vs->type == JVAR_CONSTANT)
        return true;
    // VarState defined outside of the loop is constant (otherwise would go
    // through phi node)
    if (body.find(vs->block->bid) == body.end())
        return true;
    return false;
}

VarState *Loop::getAbsoluteConstant(VarState *vs)
{
    if (!isConstant(vs))
        return NULL;
    if (body.find(vs->block->bid) == body.end())
        return vs;
    if (vs->expr->u.op == Expr::MOV) {
        if (vs->expr->u.e->vs)
            return getAbsoluteConstant(vs->expr->u.e->vs);
        else
            return NULL;
    }
    return vs;
}

VarState *Loop::getAbsoluteStorage(VarState *vs)
{
    if (vs->type == JVAR_CONSTANT)
        return NULL;
    if (!isConstant(vs))
        return NULL;
    if (body.find(vs->block->bid) == body.end())
        // If the varstate was defined outside the loop body, then the storage
        // will be constant
        return vs;
    if (vs->expr->u.op == Expr::MOV) {
        // If the varstate was defined by a MOV expression, we examine the
        // source operand
        VarState *vs2 = vs->expr->u.e->vs;

        if (vs2) {
            if (vs2->type == JVAR_CONSTANT) {
                // If the source is a constant, this works so we return it
                return vs2;
            } else {
                // Otherwise, we recursively examine the source operand,
                // in hopes of finding the origin operand with constant storage
                return getAbsoluteStorage(vs->expr->u.e->vs);
            }
        } else
            return NULL;
    }
    if (vs->expr->kind == Expr::INTEGER) {
        // JAN-59
        // This covers the case of when vs->expr comes from a lea with an rip
        // relative address (The RIP relative address gets changed to a constant
        // in the IR) Since then expr->kind isn't Expr::Unary (with u.op ==
        // Expr::MOV) But instead an Expr::INTEGER, bundled with a scalar value
        // So now we need to wrap that scalar value in a VarState
        return new VarState(Variable((uint64_t)vs->expr->i));
    }

    return vs;
}

void Loop::printDot(void *outputStream)
{
    ostream &os = *(ostream *)outputStream;
    GASSERT((parent != NULL && parent->entry != NULL),
            "Parent function CFG not found");
    BasicBlock *entry = parent->getCFG().entry;
    bool hasTemperature = (temperature.size() != 0);

    os << "digraph loop_" << id << "_CFG {" << endl;
    os << "label=\"loop_" << id << " in " << parent->name << " \\l";
    os << "\";" << endl;

    /* Print all basic blocks */
    for (auto bid : init) {
        BasicBlock &bb = entry[bid];
        os << "\tBB" << dec << bid << " ";
        os << "[label=\"BB " << dec << bb.bid << " 0x" << hex << bb.instrs->pc
           << "\\l";
        bb.printDot(outputStream);
        os << "\",style=\"filled\",shape=box,fillcolor=\"#33FF33\"];" << endl;
    }
    for (auto bid : body) {
        BasicBlock &bb = entry[bid];
        os << "\tBB" << dec << bid << " ";
        os << "[label=\"BB " << dec << bb.bid << " 0x" << hex << bb.instrs->pc
           << "\\l";
        bb.printDot(outputStream);
        os << "\"";
        if (bb.bid == start->bid)
            os << ",style=\"filled\",shape=box,color=black,fillcolor=gold];"
               << endl;
        else {
            int r = 0, g = 0, b = 0;
            toRGB(temperature[bb.instrs->pc], &r, &g, &b);
            os << ",style=\"filled\",shape=box,color=black,fillcolor=\"#" << hex
               << setfill('0') << setw(2) << r << setfill('0') << setw(2) << g
               << setfill('0') << setw(2) << b << "\"];" << endl;
        }
    }
    for (auto bid : exit) {
        BasicBlock &bb = entry[bid];
        os << "\tBB" << dec << bid << " ";
        os << "[label=\"BB " << dec << bb.bid << " 0x" << hex << bb.instrs->pc
           << "\\l";
        bb.printDot(outputStream);
        os << "\",style=\"filled\",shape=box,fillcolor=\"#CC99FF\"];" << endl;
    }

    /* Print all edges */
    for (auto bid : init) {
        BasicBlock &bb = entry[bid];
        if (bb.succ1) {
            os << "\tBB" << dec << bid << " -> BB" << bb.succ1->bid;
            if (bb.succ1->bid == start->bid)
                os << "[color=\"blue\"];" << endl;
            else
                os << ";" << endl;
        }
        if (bb.succ2) {
            os << "\tBB" << dec << bid << " -> BB" << bb.succ2->bid;
            if (bb.succ2->bid == start->bid)
                os << "[color=\"blue\"];" << endl;
            else
                os << ";" << endl;
        }
    }
    for (auto bid : body) {
        BasicBlock &bb = entry[bid];
        if (bb.succ1) {
            os << "\tBB" << dec << bid << " -> BB" << bb.succ1->bid;
            if (bb.succ1->bid == start->bid)
                os << "[color=\"red\"];" << endl;
            else
                os << ";" << endl;
        }
        if (bb.succ2) {
            os << "\tBB" << dec << bid << " -> BB" << bb.succ2->bid;
            if (bb.succ2->bid == start->bid)
                os << "[color=\"red\"];" << endl;
            else
                os << ";" << endl;
        }
    }
    os << "} " << endl;
}
