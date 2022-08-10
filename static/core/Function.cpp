#include "Function.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <queue>
#include <sstream>
#include <string>

#include "AST.h"
#include "Analysis.h"
#include "Arch.h"
#include "ControlFlow.h"
#include "Executable.h"
#include "JanusContext.h"
#include "SSA.h"

using namespace std;
using namespace janus;

Function::Function(JanusContext *gc, FuncID fid, const Symbol &symbol,
                   uint32_t size)
    : context(gc), fid(fid), size(size)
{
    startAddress = symbol.startAddr;
    endAddress = startAddress + size;

    int offset = startAddress - (symbol.section->startAddr);

    contents = symbol.section->contents + offset;

    name = symbol.name;
    replace(name.begin(), name.end(), '.', '_'); // replace all '.' to '_'

    if (symbol.type == SYM_FUNC)
        isExecutable = true;
    else
        isExecutable = false;

    translated = false;
    hasIndirectStackAccesses = false;
    available = true;
    isExternal = false;
    implicitStack = false;

    liveRegIn = NULL;
    liveRegOut = NULL;
}

Function::~Function()
{
    /* Now we free all the instructions */
    for (auto &instr : minstrs) {
        if (instr.operands)
            free(instr.operands);
        if (instr.name)
            free(instr.name);
    }
}

/* retrieve further information from instructions */
void Function::translate()
{
    if (translated)
        return;

    // XXX: change this bit; this will always work because getCFG is called in
    // the condition
    if (!getCFG().entry) {
        available = false;
        return;
    }

    translated = true;

    GASSERT(entry, "CFG not found in " << name << endl);

    /* Scan the code to retrieve stack information
     * this has to be done at first */
    analyseStack(this);

    /* Scan for memory accesses */
    scanMemoryAccess(this);

    /* For other mode, memory accesses are enough,
     * Only paralleliser and analysis mode needs to go further */
    if (context->mode != JPARALLEL && context->mode != JPROF &&
        context->mode != JANALYSIS && context->mode != JVECTOR &&
        context->mode != JOPT && context->mode != JSECURE &&
        context->mode != JFETCH)
        return;

    /* Construct the abstract syntax tree of the function */
    buildASTGraph(this);

    /* Perform variable analaysis */
    variableAnalysis(this);

    /* Peform liveness analysis */
    livenessAnalysis(this);
}

static void assign_loop_blocks(Loop *loop)
{
    BasicBlock *entry = loop->parent->getCFG().entry;
    for (auto bid : loop->body) {
        entry[bid].parentLoop = loop;
    }
    // then call subloops
    for (auto subLoop : loop->subLoops)
        assign_loop_blocks(subLoop);
}

void Function::analyseLoopRelations()
{
    // get loop array
    Loop *loopArray = context->loops.data();
    // step 1 we simply use a O(n^2) comparison
    for (auto loopID : loops) {
        for (auto loopID2 : loops) {
            if (loopID != loopID2) {
                // if loop 2 start block is in the loop 1's body
                if (loopArray[loopID - 1].contains(
                        loopArray[loopID2 - 1].start->bid)) {
                    // then loop 2 is descendant of loop 1
                    loopArray[loopID - 1].descendants.insert(loopArray +
                                                             loopID2 - 1);
                    // loop 1 is ancestor of loop 2
                    loopArray[loopID2 - 1].ancestors.insert(loopArray + loopID -
                                                            1);
                }
            }
        }
    }
    // step 2 assign immediate parent and children
    map<LoopID, int> levels;
    int undecided = 0;
    for (auto loopID : loops) {
        Loop &loop = loopArray[loopID - 1];
        // starts with loops with 0 ancestors
        if (loop.ancestors.size() == 0) {
            loop.level = 0;
            levels[loopID] = 0;
        } else {
            levels[loopID] = -1;
            undecided++;
        }
    }
    // if all loops are level 0, simply return
    if (!undecided)
        goto assign_blocks;
    // if all are undecided, it is a recursive loop
    if (undecided == loops.size())
        return;

    // step 3 recursively calculates the levels for each loop
    bool converge;
    do {
        converge = true;
        for (auto loopID : loops) {
            Loop &loop = loopArray[loopID - 1];
            if (loop.ancestors.size() == 0)
                continue;
            bool allVisited = true;
            LoopID maxLoopID = 0;
            int maxLevel = -1;
            for (auto anc : loop.ancestors) {
                if (levels[anc->id] == -1) {
                    allVisited = false;
                    break;
                }
                if (maxLevel < levels[anc->id]) {
                    maxLevel = levels[anc->id];
                    maxLoopID = anc->id;
                }
            }
            // if all visited, this loop is the immediate children of the
            // deepest loop.
            if (allVisited) {
                loopArray[maxLoopID - 1].subLoops.insert(loopArray + loopID -
                                                         1);
                loopArray[loopID - 1].parentLoop = loopArray + maxLoopID - 1;
                levels[loopID] = levels[maxLoopID] + 1;
                loopArray[loopID - 1].level = levels[loopID];
            } else {
                converge = false;
            }
        }
    } while (!converge);

assign_blocks:
    // assign basic block to inner most loops
    for (auto loopID : loops) {
        assign_loop_blocks(loopArray + loopID - 1);
    }
}

void Function::print(void *outputStream)
{
    ostream &os = *(ostream *)outputStream;
    os << fid << " " << name << ": 0x" << hex << startAddress << "-0x"
       << endAddress << " size " << dec << size << endl;
}

void Function::visualize(void *outputStream)
{
    ostream &os = *(ostream *)outputStream;

    // if block size less than 4, there is no need to draw it since it is too
    // simple
    if (size <= 2)
        return;
    os << dec;
    os << "digraph " << name << "CFG {" << endl;
    os << "label=\"" << name << "_" << dec << fid + 1 << "_CFG\";" << endl;

    for (auto &bb : getCFG().blocks) {
        // name of the basic block
        os << "\tBB" << dec << bb.bid << " ";
        // append attributes
        os << "[";
        // os <<"label=<"<< bb.dot_print()<<">";
        os << "label=\"BB" << dec << bb.bid << " 0x" << hex << bb.instrs->pc
           << "\\l";
        bb.printDot(outputStream);
        os << "\"";
        os << ",shape=box"
           << "];";

        os << endl;
    }

    // print all the edges
    os << dec;
    for (auto &bb : getCFG().blocks) {
        if (bb.succ1)
            os << "\tBB" << bb.bid << " -> BB" << bb.succ1->bid << ";" << endl;
        if (bb.succ2)
            os << "\tBB" << bb.bid << " -> BB" << bb.succ2->bid << ";" << endl;
    }

    os << "} " << endl;

    // cfg tree
    os << "digraph " << name << "CFG_TREE {" << endl;
    os << "label=\"" << name << "_" << dec << fid + 1 << "_CFG_Simplify\";"
       << endl;

    for (auto &bb : getCFG().blocks) {
        // name of the basic block
        os << "\tBB" << dec << bb.bid << " ";
        // append attributes
        os << "[";
        // os <<"label=<"<< bb.dot_print()<<">";
        os << "label=\"BB" << dec << bb.bid << "\"";
        os << ",shape=box"
           << "];";
        os << endl;
    }

    // print the dom edge
    os << dec;
    for (auto &bb : getCFG().blocks) {
        if (bb.succ1)
            os << "\tBB" << bb.bid << " -> BB" << bb.succ1->bid << ";" << endl;
        if (bb.succ2)
            os << "\tBB" << bb.bid << " -> BB" << bb.succ2->bid << ";" << endl;
    }
    os << "} " << endl;

    // dominator tree
    os << "digraph " << name << "DOM_TREE {" << endl;
    os << "label=\"" << name << "_" << dec << fid + 1 << "_Dom_Tree\";" << endl;

    for (auto &bb : getCFG().blocks) {
        // name of the basic block
        os << "\tBB" << dec << bb.bid << " ";
        // append attributes
        os << "[";
        // os <<"label=<"<< bb.dot_print()<<">";
        os << "label=\"BB" << dec << bb.bid << "\"";
        os << ",shape=box"
           << "];";
        os << endl;
    }

    // print the dom edge
    // TODO: fix this somehow afterwards
    os << dec;
    // for (auto &bb : getCFG().blocks) {
    // if (bb.idom && bb.idom->bid != bb.bid)
    // os << "\tBB" << bb.idom->bid << " -> BB" << bb.bid << ";" << endl;
    //}

    os << "} " << endl;

    // post dominator tree
    os << "digraph " << name << "POST_DOM_TREE {" << endl;
    os << "label=\"" << name << "_" << dec << fid + 1 << "_Post_Dom_Tree\";"
       << endl;

    for (auto &bb : getCFG().blocks) {
        // name of the basic block
        os << "\tBB" << dec << bb.bid << " ";
        // append attributes
        os << "[";
        // os <<"label=<"<< bb.dot_print()<<">";
        os << "label=\"BB" << dec << bb.bid << "\"";
        os << ",shape=box"
           << "];";
        os << endl;
    }

    // print the dom edge
    os << dec;
    // for (auto &bb : getCFG().blocks) {
    // if (bb.ipdom)
    // os << "\tBB" << bb.ipdom->bid << " -> BB" << bb.bid << ";" << endl;
    //}

    os << "} " << endl;
}

bool Function::needSync()
{
    /* For the moment we only check the name
     * Check the relocation pattern in the future */
    if (name.find("@plt") != string::npos) {
        if (name.find("sqrt") != string::npos)
            return false;
        if (name.find("exp") != string::npos)
            return false;
        if (name.find("sincos") != string::npos)
            return false;
        if (name.find("atan") != string::npos)
            return false;
        return true;
    }
    return false;
}

ControlFlowGraph &Function::getCFG()
{
    if (!cfg) {
        cfg = make_unique<ControlFlowGraph>(this, context->functionMap);
    }
    return *cfg;
}
