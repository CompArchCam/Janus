#include "ControlFlow.h"
#include "BasicBlock.h"
#include "Function.h"
#include "JanusContext.h"
#include "Utility.h"
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <map>
#include <queue>

using namespace std;
using namespace janus;

#define BB_LEADER 2
#define BB_TERMINATOR 1

static void
buildBasicBlocks(Function &function);

static void
buildDominanceTree(Function &function);

static void
buildPostDominanceTree(Function &function);

//Construct control flow graph for each function
void 
buildCFG(Function &function)
{
    /* step 1: construct a vector of basic blocks
     * and link them together */
    buildBasicBlocks(function);
    /* set function entry */
    function.entry = function.blocks.data();
    function.numBlocks = function.blocks.size();

    if (function.numBlocks <= 1 || function.isExternal) return;

    /* step 2: analyse the CFG and build dominance tree */
    buildDominanceTree(function);

    /* step 3: analyse the CFG and build post-dominance tree */
    buildPostDominanceTree(function);

    /* step 4: analyse the CFG and build dominance frontiers */
    buildDominanceFrontiers(function);

    /* step 5: analyse the CFG and build post dominance frontiers */
    buildPostDominanceFrontiers(function);

    /* step 6: traverse the CFG */
    traverseCFG(function);
}

//Construct control dependence graph for each function
void 
buildCDG(Function &function)
{
    /* All of the instructions within a basic block are
     * immediately control dependent on the branches 
     * in the post dominance frontier of the block. */
    for (auto &bb: function.blocks) {
        for (int i=0; i<bb.size; i++) {
            Instruction &instr = bb.instrs[i];
            for (auto cb: bb.postDominanceFrontier) {
                Instruction *ci = cb->lastInstr();
                instr.controlDep.push_back(ci);
            }
        }
    }
}

static void
buildBasicBlocks(Function &function)
{
    int cornerCase = false;
    auto &instrs = function.instrs;
    auto &instrTable = function.minstrTable;
    auto &functionMap = function.context->functionMap;
    auto &blocks = function.blocks;

    /* Each function has a vector of machine instructions, each insn has an ID
     * Mark the target and branch as basic block boundaries */
    map<InstID, uint32_t> marks;
    map<InstID,set<InstID>> edges;
    set<InstID> notRecognised;

    InstID id;
    int32_t offset;
    InstID targetID;

    //firstly assume the first instruction is the entry
    marks[0] += BB_LEADER;

    uint32_t instrCount = instrs.size();

    for (InstID id=0; id<instrCount; id++) {

        Instruction &instr = instrs[id];

        if (instr.isControlFlow()) {
            //for control flow instructions, mark this id as terminate
            marks[id] += BB_TERMINATOR;
            //always assume the next non-nop instruction is a LEADER
            offset = 1;
            //for safety, make the instruction followed by a CJUMP with a LEADER
            if (instr.opcode == Instruction::ConditionalBranch)
                marks[id + offset] += BB_LEADER;

            while(id < instrCount - 2 &&
                instrs[id+offset].opcode == Instruction::Nop) {
                //add nop instruction to nop set.
                function.context->nopInstrs.insert(instrs[id+offset].pc);
                offset++;
            }

            marks[id + offset] += BB_LEADER;

            //for jump and conditional jump, work out the target
            if (instr.opcode == Instruction::DirectBranch ||
                instr.opcode == Instruction::ConditionalBranch) {
                PCAddress target = instr.minstr->getTargetAddress();
                if (target) {
                    //find this target in the instruction table
                    auto query = instrTable.find(target);
                    if (query == instrTable.end()) {
                        notRecognised.insert(id);
                    }
                    //if found the target instruction, mark the target as a leader
                    else {
                        targetID = (*query).second;
                        marks[targetID] += BB_LEADER;
                        edges[id].insert(targetID);
                    }
                }
                //whether it is register or memory operands, it is an indirect jump which we don't bother to perform extensive analysis on it
                else {
                    notRecognised.insert(id);
                }
            }

            //for conditional jump, always add fall through edge
            if (instr.opcode == Instruction::ConditionalBranch) {
                edges[id].insert(id+1);
            }

            //for call instructions, no more further actions, just to construct the call graph
            if (instr.opcode == Instruction::Call) {

                //Currently assume all calls would return except indirect call
                if (id+offset<instrCount)
                    edges[id].insert(id+offset);

                PCAddress callTarget = instr.minstr->getTargetAddress();
                if (callTarget) {
                    //find this target in the function map
                    auto query = functionMap.find(callTarget);
                    if (query == functionMap.end()) {
                        notRecognised.insert(id);
                    }
                    //if found in the function map
                    else {
                        Function *targetFunc = (*query).second;
                        function.calls[id] = targetFunc;
                        function.subCalls.insert(targetFunc->fid);
                    }
                }
                //whether it is register or memory operands, it is an indirect call which we don't bother to perform extensive analysis on it
                else {
                    notRecognised.insert(id);
                }
            }

            //for return instruction, add to the function's exit node
            if (instr.opcode == Instruction::Return) {
                function.returnBlocks.insert(id);
            }
        }
        /* Corner case: final instruction not a cti */
        if (id == instrCount-1 && !instr.isControlFlow()) {
            marks[id] += BB_TERMINATOR;
            cornerCase = true;
        }
    }

    //Step 2: After scanning all instructions, 
    //Based on the boundaries, construct basic blocks
    //And according to the edges, construct control flow graphs
    BlockID blockID = 0;
    BlockID trueBlockEnd = 0;
    InstID startID;
    InstID endID;
    //since all boundaries are already sorted by InstID, we just iterate through it
    for (auto mark=marks.begin(); mark != marks.end(); mark++) {

        InstID boundary = (*mark).first;
        //if mark is out of function boundary
        if (boundary >= instrCount) break;
        uint32_t property = (*mark).second;

        //if the boundary is marked as multiple LEADERS but no terminators
        //it is the start of a block
        if (property % 2 == 0) {
            //find the next LEADER or TERMINATOR for the end of the block
            /* We must make sure that there is no only nops in the block */
            auto nextMark = mark;
            nextMark++;
            if (nextMark == marks.end()) break;
            InstID blockEnd = (*nextMark).first;

            //find the first LEADER
            auto prevMark = mark;
            while(1) {
                if (prevMark == marks.begin()) break;
                prevMark--;
                if ((*prevMark).second % 2 == 1) {
                    prevMark++;
                    break;
                }
            }
            //get the true start instID of the block
            InstID trueBlockStart = (*prevMark).first;
            //trueBlockEnd carries the last oversized block
            if (trueBlockStart < trueBlockEnd)
                trueBlockStart = trueBlockEnd;

            if (blockEnd - trueBlockStart < MAX_BLOCK_INSTR_COUNT) {
                blocks.emplace_back(&function,
                                    blockID++,
                                    boundary,
                                    blockEnd,
                                    (*nextMark).second % 2 == 0);
            } else {
                trueBlockEnd = trueBlockStart + MAX_BLOCK_INSTR_COUNT;
                function.blockSplitInstrs[blockID].insert(trueBlockEnd);
                blocks.emplace_back(&function,
                                    blockID++,
                                    boundary,
                                    trueBlockEnd,
                                    1);
                //update the boundary
                boundary = trueBlockEnd;

                /* For further oversize blocks, split the block into multiple block */
                while (blockEnd - boundary > MAX_BLOCK_INSTR_COUNT) {
                    trueBlockEnd = boundary + MAX_BLOCK_INSTR_COUNT;
                    function.blockSplitInstrs[blockID].insert(trueBlockEnd);
                    blocks.emplace_back(&function,
                                        blockID++,
                                        boundary,
                                        trueBlockEnd,
                                        1);
                    boundary += MAX_BLOCK_INSTR_COUNT;
                }
                if (blockEnd != boundary) {
                    blocks.emplace_back(&function,
                                        blockID++,
                                        boundary,
                                        blockEnd,
                                        (*nextMark).second % 2 == 0);
                    trueBlockEnd = blockEnd;
                }
            }

        }
        //boundary may be marked as multiple LEADERS and only one terminator
        //it means this basic block contains only one instruction
        else if (property>1) {
            blocks.emplace_back(&function,
                                blockID++,
                                boundary,
                                boundary,
                                false);
        }
        //no need to create basic block for a single out entry
    }

    //step 3: now all basic blocks are created, link them into a CFG
    //we already got all edge information in the edge node
    //step 3.1 link instructions to block
    BasicBlock *blockArray = blocks.data();
    uint32_t bSize =blocks.size();
    for (int i=0; i<bSize; i++) {
        BasicBlock *block = blockArray + i;
        for (int j=0; j<block->size; j++) {
            block->instrs[j].block = block;
        }
    }

    if (bSize <= 1) return;

    //step 3.2 after all instructions are linked with their parent blocks, we can then link each block to block
    for (int i=0; i<bSize; i++) {
        BasicBlock *block = blockArray + i;
        if (block->fake) {
            if (i==bSize-1) continue;
            block->succ1 = blockArray + i + 1;
            blockArray[i+1].pred.insert(block);
            continue;
        }
        InstID endID = block->lastInstr()->id;
        auto query = edges.find(endID);

        if (query == edges.end()) {
            if (block->lastInstr()->isControlFlow()) {
                if (notRecognised.find(endID)==notRecognised.end()) {
                    function.terminations.insert(block->bid);
                    block->terminate = true;
                }
                else
                    function.unRecognised.insert(block->bid);
            }
        } else {
            for (auto succBlockID: (*query).second) {
                auto &instr = instrs[succBlockID];
                BasicBlock *targetBlock = instr.block;
                if (targetBlock)
                    targetBlock->pred.insert(block);
                if (block->succ1) {
                    block->succ2 = targetBlock;
                }
                else {
                    block->succ1 = targetBlock;
                }
            }
        }
    }
    //XXX: for fortran only
    if (function.name == "MAIN__") {
        for (int i=0; i<bSize; i++) {
            BasicBlock *block = blockArray + i;
            if (block->lastInstr()->opcode == Instruction::Call) {
                PCAddress target = block->lastInstr()->minstr->getTargetAddress();
                if (function.context->functionMap.find(target) != function.context->functionMap.end())
                    if (function.context->functionMap[target]->name == "_gfortran_stop_string@plt")
                        function.terminations.insert(i);
            }
        }
    }
}

//Update dominance frontiers for other nodes (called for Y)
//X.dominanceFrontier.contains(Y) iff X
//!strictly_dominates Y
//and exists Z in Y.pred, s.t. X dominates Z
static void updateDominanceFrontier(BasicBlock *Y) {
    for (BasicBlock *Z:Y->pred) {
        BasicBlock *X = Z;
        while (X &&
              //guard to entry
              (X->idom != X) &&
              //stop when you reach the idom of Y
              (X != Y->idom)) {
            X->dominanceFrontier.insert(Y);
            //traverse upwards till entry or idom of Y (loop)
            X = X->idom;
        }
    }
}

//Update post dominance frontiers for other nodes (called for Y)
//X.postDominanceFrontier.contains(Y) iff
//For X in all successors of Y
//if X does not post dominate Y
//then X's post dominance frontier is Y
static void updatePostDominanceFrontier(BasicBlock *Y) {
    BasicBlock *X = Y->succ1;
    while (X) {
        //stop when we hit the post dominator of Y
        if (X == Y->ipdom) break;
        //stop if we hit the exit
        if (X->succ1 == NULL && X->succ2 == NULL) break;
        //add to pdom frontier
        X->postDominanceFrontier.insert(Y);
        //traverse upwards towards pdom tree
        X = X->ipdom;
    }

    X = Y->succ2;
    while (X) {
        //stop when we hit the post dominator of Y
        if (X == Y->ipdom) break;
        //stop if we hit the exit
        if (X->succ1 == NULL && X->succ2 == NULL) break;
        //add to pdom frontier
        X->postDominanceFrontier.insert(Y);
        //traverse upwards towards pdom tree
        X = X->ipdom;
    }
}

void buildDominanceFrontiers(Function &function) {
    for (BasicBlock &bb : function.blocks) {
        updateDominanceFrontier(&bb);
    }
}

void buildPostDominanceFrontiers(janus::Function &function) {
    for (BasicBlock &bb : function.blocks) {
        updatePostDominanceFrontier(&bb);
    }
}

static void
buildDominanceTree(Function &function)
{
    uint32_t size = function.numBlocks;
    if (!size) return;

    /* construct a two dimensional dominator tree */
    function.domTree = new vector<BitVector>(size, BitVector(size, uint32_t(-1)));
    vector<BitVector> &domTree = *function.domTree;

    /* initialisation */
    domTree[0].clear();
    domTree[0].insert(0);

    bool converge;
    BitVector scratch(size);

    do {
        converge = true;
        for (int i=0; i< size; i++) {
            BasicBlock &bb = function.entry[i];
            /* If no predecessor, set itself as its dominator (for entry block) */
            if (bb.pred.size() == 0) {
                domTree[i].clear();
                if (bb.bid == 0) {
                    domTree[i].insert(i);
                    bb.idom = &bb;
                }
                continue;
            }
            /* create a universal set */
            scratch.setUniversal();

            /* Get intersect of all its predecessors */
            for (auto predecessor: bb.pred) {
                scratch.intersect(domTree[predecessor->bid]);
            }
            /* And also include the block itself */
            scratch.insert(i);
            /* Check convergence */
            if (scratch != domTree[i])
                converge = false;
            /* Update */
            domTree[i] = scratch;
        }
    } while (!converge);

    set<BlockID> dominators;
    for (int i=0; i<size; i++) {
        BasicBlock &bb = function.entry[i];
        domTree[i].toSTLSet(dominators);
        for (BlockID d : dominators) {
            /* If the number of dominators of BB d is 1 less than the dominators of BB i,
             * Then it means the immediate dominator of i is d */
            if (domTree[d].size() + 1 == dominators.size()) {
                bb.idom = &function.entry[d];
                break;
            }
        }
        dominators.clear();
    }
}

static void
buildPostDominanceTree(Function &function)
{
    uint32_t size = function.numBlocks;
    if (!size) return;
    if (!function.terminations.size()) return;

    BlockID termID = 0;
    bool multiExit = false;

    if (function.terminations.size() > 1) {
        /* For function with multiple exit, we create an additional fake basic block
         * So that all terminating basic block lead to this exit */
        termID = function.blocks.size();
        //create a fake node
        size++;
        multiExit = true;
    } else {
        for (auto t: function.terminations)
            termID = t;
    }

    /* construct a two dimensional post dominance tree */
    function.pdomTree = new vector<BitVector>(size, BitVector(size, uint32_t(-1)));
    vector<BitVector> &pdomTree = *function.pdomTree;

    /* initialisation from the terminator node */
    pdomTree[termID].clear();
    pdomTree[termID].insert(termID);

    bool converge;
    BitVector scratch(size);

    do {
        converge = true;
        for (int i=size-1; i>=0; i--) {
            if (i==termID) {
                pdomTree[i].clear();
                pdomTree[i].insert(i);
                continue;
            } 

            if (multiExit && function.terminations.find(i) != function.terminations.end()) {
                pdomTree[i].clear();
                pdomTree[i].insert(i);
                pdomTree[i].insert(termID);
                continue;
            }

            //normal case
            BasicBlock &bb = function.entry[i];

            /* create a universal set */
            scratch.setUniversal();

            /* Get intersect of all the two successor */
            if (bb.succ1)
                scratch.intersect(pdomTree[bb.succ1->bid]);
            if (bb.succ2)
                scratch.intersect(pdomTree[bb.succ2->bid]);
            /* And also include the block itself */
            scratch.insert(i);
            /* Check convergence */
            if (scratch != pdomTree[i])
                converge = false;
            /* Update */
            pdomTree[i] = scratch;
        }
    } while (!converge);

    set<BlockID> pdominators;
    for (int i=0; i<size; i++) {
        BasicBlock &bb = function.entry[i];
        pdomTree[i].toSTLSet(pdominators);
        for (BlockID d : pdominators) {
            /* If the number of pdominators of BB d is 1 less than the pdominators of BB i,
             * Then it means the immediate post dominator of i is d */
            if (pdomTree[d].size() + 1 == pdominators.size()) {
            	if (d < function.numBlocks) {
                    bb.ipdom = &function.entry[d];
                    break;
                }
            }
        }
        pdominators.clear();
    }
}

/* We use a global variable to reduce the stack size in this 
 * recursive function */
static uint32_t step = 0;
static void markBlock(BasicBlock *block, bool *discovered)
{
    discovered[block->bid] = true;
    block->firstVisitStep = step++;
    if (block->succ1) {
        if (!discovered[block->succ1->bid])
            markBlock(block->succ1, discovered);
    }
    if (block->succ2) {
        if (!discovered[block->succ2->bid])
            markBlock(block->succ2, discovered);
    }
    block->secondVisitStep = step++;
}

void
traverseCFG(Function &function)
{
    uint32_t size = function.numBlocks;
    /* Array to tag if a block has been visited */
    bool *discovered = new bool[size];
    memset(discovered, 0, size*sizeof(bool));
    /* Start marking from entry */
    markBlock(function.entry, discovered);
    delete[] discovered;
}

/* Study the link between functions and build call graphs */
void
buildCallGraphs(JanusContext *gc)
{
    Function *functions = gc->functions.data();
    uint32_t numFunc = gc->functions.size();

    GSTEP("Building call graphs: "<<endl);

    /* create a list of bitvectors */
    BitVector *callGraph = new BitVector[numFunc];

    for (int i=0; i<numFunc; i++) {
        callGraph[i] = BitVector(numFunc);
        for (auto sub: functions[i].subCalls) {
            callGraph[i].insert(sub);
        }
    }

    bool converge;

    do {
        converge = true;
        for (int i=0; i<numFunc; i++) {
            //merge the current with sub calls
            for (auto sub: functions[i].subCalls) {
                //get a copy
                BitVector prev = callGraph[i];
                callGraph[i].merge(callGraph[sub]);
                //check for convergence
                if (converge && prev != callGraph[i])
                    converge = false;
            }
        }
    } while(!converge);

    /* writeback subcalls */
    for (int i=0; i<numFunc; i++) {
        callGraph[i].toSTLSet(functions[i].subCalls);
    }

    delete[] callGraph;
}
