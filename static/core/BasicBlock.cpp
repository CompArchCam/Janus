#include "BasicBlock.h"
#include "Function.h"
#include "IO.h"
#include <sstream>

using namespace std;
using namespace janus;

BasicBlock::BasicBlock(Function *func, BlockID bid, InstID startInstID,
                       InstID endInstID, bool isFake)
    : bid(bid), startInstID(startInstID), endInstID(endInstID),
      parentFunction(func), parentLoop(NULL)
{
    fake = isFake;
    /* For blocks that end without control flow instructions,
     * the natural end should be the previous instruction */
    if (fake)
        endInstID--;

    if (endInstID < startInstID) {
        cerr << "Error in creating basic blocks! in function " << func->name
             << endl;
        exit(-1);
    }

    size = endInstID - startInstID + 1;

    instrs = (parentFunction->instrs.data()) + startInstID;

    // idom = NULL;
    // ipdom = NULL;
    succ1 = NULL;
    succ2 = NULL;
    firstVisitStep = 0;
    secondVisitStep = 0;
    terminate = false;

    /*
    //XXX this does not result in a complete result and should be okay to simply
    omit completely
    //save last states; outputs of later instructions overwrite the outputs of
    previous ones for (int i=0; i<size; i++) { MachineInstruction &mi =
    instrs[i]; mi.block = this;

        for (Variable &v : mi.varInputs) {
            if (v.isLoad()) continue;
            if (lastStates.find(v) != lastStates.end()) {
                mi.inputs.push_back(lastStates[v]);
            } else inputs.insert(v);
        }

        for (VarState *vs : mi.outputs) {
            lastStates[vs->getVar()] = vs;
        }
    }*/
}

Instruction *BasicBlock::lastInstr() { return &(instrs[size - 1]); }

bool BasicBlock::dominates(const BasicBlock &block) const
{
    const auto &domTree = *(parentFunction->getSSA().domTree);
    if (block.bid < domTree.size()) {
        return domTree.at(block.bid).contains(bid);
    } else {
        return false;
    }
    // return (*parentFunction->getSSA().domTree)[block.bid].contains(bid);
}

bool BasicBlock::dominates(const BasicBlock *block) const
{
    const auto &domTree = *(parentFunction->getSSA().domTree);
    if (block->bid < domTree.size()) {
        return domTree.at(block->bid).contains(bid);
    } else {
        return false;
    }
    // return (*parentFunction->getSSA().domTree)[block->bid].contains(bid);
}

// DFS through ancestors to find the closest definition
// Finding one definition is sufficient, since due to the properties of the SSA
// form, there can be
//       no other definitions (or otherwise there would be a phi node in
//       between)
static VarState *findAlive(Variable var, BasicBlock *bb,
                           set<BasicBlock *> &visited)
{
    if (visited.find(bb) != visited.end())
        return NULL;
    visited.insert(bb);

    if (bb->lastStates.find(var) != bb->lastStates.end())
        return bb->lastStates[var]; // defined somewhere in this block

    // Not defined here, recurse
    for (auto bp : bb->pred) {
        VarState *vs = findAlive(var, bp, visited);
        if (vs)
            return vs; // definition found
    }

    // no definition found
    return NULL;
}

VarState *BasicBlock::alive(Variable var)
{
    set<BasicBlock *> visited;

    for (auto pn : phiNodes)
        if ((Variable)*pn == var)
            return pn; // defined as a phi node in this block

    for (auto bp : pred) {
        visited.clear();
        VarState *vs = findAlive(var, bp, visited);
        if (vs)
            return vs; // defined in an ancestor reached through bp
    }

    // return initial value
    // if (parentFunction->inputs.find(var) != parentFunction->inputs.end())
    // return parentFunction->inputs[var];

    return NULL; // undefined
}

VarState *BasicBlock::alive(Variable var, int index)
{
    // search for definition in this block
    for (int i = index - 1; i >= 0; i--) {
        for (auto vs : instrs[i].outputs)
            if ((Variable)*vs == var)
                return vs;
    }

    // search for definition in predecessor blocks
    return alive(var);
    return NULL;
}

void BasicBlock::printDot(void *outputStream)
{
    ostream &os = *(ostream *)outputStream;
    if (size < 1) {
        os << "BB" << bid << endl;
    }

    for (int i = 0; i < size; i++) {
        Instruction &instr = instrs[i];
        instr.printDot(outputStream);
        os << "\\l";
    }
}
