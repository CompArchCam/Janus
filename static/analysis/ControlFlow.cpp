#include "ControlFlow.h"

#include <concepts>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <queue>

#include "BasicBlock.h"
#include "Function.h"
#include "JanusContext.h"

using namespace std;
using namespace janus;

#define BB_LEADER 2
#define BB_TERMINATOR 1

void ControlFlowGraph::buildBasicBlocks(
    std::map<PCAddress, janus::Function *> &functionMap)
{
    int cornerCase = false;
    auto &instrs = func.instrs;
    auto &instrTable = func.minstrTable;

    // Each function has a vector of machine instructions,
    // each insn has an ID Mark the target and branch as basic block boundaries
    map<InstID, uint32_t> marks;
    map<InstID, set<InstID>> edges;
    set<InstID> notRecognised;

    InstID id;
    int32_t offset;
    InstID targetID;

    // firstly assume the first instruction is the entry
    marks[0] += BB_LEADER;

    uint32_t instrCount = instrs.size();

    for (InstID id = 0; id < instrCount; id++) {
        Instruction &instr = instrs[id];

        if (instr.isControlFlow()) {
            // for control flow instructions, mark this id as terminate
            marks[id] += BB_TERMINATOR;
            // always assume the next non-nop instruction is a LEADER
            offset = 1;
            // for safety, make the instruction followed by a CJUMP with a
            // LEADER
            if (instr.opcode == Instruction::ConditionalBranch)
                marks[id + offset] += BB_LEADER;

            while (id < instrCount - 2 &&
                   instrs[id + offset].opcode == Instruction::Nop) {
                offset++;
            }

            marks[id + offset] += BB_LEADER;

            // for jump and conditional jump, work out the target
            if (instr.opcode == Instruction::DirectBranch ||
                instr.opcode == Instruction::ConditionalBranch) {
                PCAddress target = instr.minstr->getTargetAddress();
                if (target) {
                    // find this target in the instruction table
                    if (!instrTable.contains(target)) {
                        notRecognised.insert(id);
                    }
                    // if found the target instruction, mark the target as a
                    // leader
                    else {
                        targetID = instrTable.at(target);
                        marks[targetID] += BB_LEADER;
                        edges[id].insert(targetID);
                    }
                }
                // whether it is register or memory operands, it is an indirect
                // jump which we don't bother to perform extensive analysis on
                // it
                else {
                    notRecognised.insert(id);
                }
            }

            // for conditional jump, always add fall through edge
            if (instr.opcode == Instruction::ConditionalBranch) {
                edges[id].insert(id + 1);
            }

            // for call instructions, no more further actions, just to construct
            // the call graph
            if (instr.opcode == Instruction::Call) {
                // Currently assume all calls would return except indirect call
                if (id + offset < instrCount)
                    edges[id].insert(id + offset);

                PCAddress callTarget = instr.minstr->getTargetAddress();
                if (callTarget) {
                    // find this target in the function map
                    if (!functionMap.contains(callTarget)) {
                        notRecognised.insert(id);
                    }
                    // if found in the function map
                    else {
                        Function *targetFunc = functionMap.at(callTarget);
                        func.calls[id] = targetFunc;
                        func.subCalls.insert(targetFunc->fid);
                    }
                }
                // whether it is register or memory operands, it is an indirect
                // call which we don't bother to perform extensive analysis on
                // it
                else {
                    notRecognised.insert(id);
                }
            }

            // for return instruction, add to the function's exit node
            if (instr.opcode == Instruction::Return) {
                returnBlocks.insert(id);
            }
        }
        // Corner case: final instruction not a cti
        if (id == instrCount - 1 && !instr.isControlFlow()) {
            marks[id] += BB_TERMINATOR;
            cornerCase = true;
        }
    }

    // Step 2: After scanning all instructions,
    // Based on the boundaries, construct basic blocks
    // And according to the edges, construct control flow graphs
    BlockID blockID = 0;
    BlockID trueBlockEnd = 0;
    InstID startID;
    InstID endID;
    // since all boundaries are already sorted by InstID, we just iterate
    // through it
    for (auto mark = marks.begin(); mark != marks.end(); mark++) {
        InstID boundary = (*mark).first;
        // if mark is out of function boundary
        if (boundary >= instrCount)
            break;
        uint32_t property = (*mark).second;

        // if the boundary is marked as multiple LEADERS but no terminators
        // it is the start of a block
        if (property % 2 == 0) {
            // find the next LEADER or TERMINATOR for the end of the block
            /*[ > We must make sure that there is no only nops in the block <*/
            /*]*/
            auto nextMark = mark;
            nextMark++;
            if (nextMark == marks.end())
                break;
            InstID blockEnd = (*nextMark).first;

            // find the first LEADER
            auto prevMark = mark;
            while (1) {
                if (prevMark == marks.begin())
                    break;
                prevMark--;
                if ((*prevMark).second % 2 == 1) {
                    prevMark++;
                    break;
                }
            }
            // get the true start instID of the block
            InstID trueBlockStart = (*prevMark).first;
            // trueBlockEnd carries the last oversized block
            if (trueBlockStart < trueBlockEnd)
                trueBlockStart = trueBlockEnd;

            if (blockEnd - trueBlockStart < MAX_BLOCK_INSTR_COUNT) {
                blocks.emplace_back(&func, blockID++, boundary, blockEnd,
                                    (*nextMark).second % 2 == 0);
            } else {
                trueBlockEnd = trueBlockStart + MAX_BLOCK_INSTR_COUNT;
                blockSplitInstrs[blockID].insert(trueBlockEnd);
                blocks.emplace_back(&func, blockID++, boundary, trueBlockEnd,
                                    1);
                // update the boundary
                boundary = trueBlockEnd;

                // For further oversize blocks,
                // split the block into multiple block
                while (blockEnd - boundary > MAX_BLOCK_INSTR_COUNT) {
                    trueBlockEnd = boundary + MAX_BLOCK_INSTR_COUNT;
                    blockSplitInstrs[blockID].insert(trueBlockEnd);
                    blocks.emplace_back(&func, blockID++, boundary,
                                        trueBlockEnd, 1);
                    boundary += MAX_BLOCK_INSTR_COUNT;
                }
                if (blockEnd != boundary) {
                    blocks.emplace_back(&func, blockID++, boundary, blockEnd,
                                        (*nextMark).second % 2 == 0);
                    trueBlockEnd = blockEnd;
                }
            }

        }
        // boundary may be marked as multiple LEADERS and only one terminator
        // it means this basic block contains only one instruction
        else if (property > 1) {
            blocks.emplace_back(&func, blockID++, boundary, boundary, false);
        }
        // no need to create basic block for a single out entry
    }

    // step 3: now all basic blocks are created, link them into a CFG
    // we already got all edge information in the edge node
    // step 3.1 link instructions to block
    BasicBlock *blockArray = blocks.data();
    uint32_t bSize = blocks.size();
    for (int i = 0; i < bSize; i++) {
        BasicBlock *block = blockArray + i;
        for (int j = 0; j < block->size; j++) {
            block->instrs[j].block = block;
        }
    }

    if (bSize <= 1)
        return;

    // step 3.2 after all instructions are linked with their parent blocks, we
    // can then link each block to block
    for (int i = 0; i < bSize; i++) {
        BasicBlock *block = blockArray + i;
        if (block->fake) {
            if (i == bSize - 1)
                continue;
            block->succ1 = blockArray + i + 1;
            blockArray[i + 1].pred.insert(block);
            continue;
        }
        InstID endID = block->lastInstr()->id;

        if (!edges.contains(endID)) {
            if (block->lastInstr()->isControlFlow()) {
                if (!notRecognised.contains(endID)) {
                    terminations.insert(block->bid);
                    block->terminate = true;
                } else
                    unRecognised.insert(block->bid);
            }
        } else {
            for (auto succBlockID : edges.at(endID)) {
                auto &instr = instrs[succBlockID];
                BasicBlock *targetBlock = instr.block;
                if (targetBlock)
                    targetBlock->pred.insert(block);
                if (block->succ1) {
                    block->succ2 = targetBlock;
                } else {
                    block->succ1 = targetBlock;
                }
            }
        }
    }
    // XXX: for fortran only
    if (func.name == "MAIN__") {
        for (int i = 0; i < bSize; i++) {
            BasicBlock *block = blockArray + i;
            if (block->lastInstr()->opcode == Instruction::Call) {
                PCAddress target =
                    block->lastInstr()->minstr->getTargetAddress();
                if (functionMap.contains(target))
                    if (functionMap[target]->name ==
                        "_gfortran_stop_string@plt")
                        terminations.insert(i);
            }
        }
    }
}

ControlFlowGraph::ControlFlowGraph(
    Function &function, std::map<PCAddress, janus::Function *> &functionMap)
    : cfg(make_unique<CFG>(function)), func(cfg->func), blocks(cfg->bs),
      unRecognised(cfg->urs), terminations(cfg->ts), returnBlocks(cfg->rbs),
      blockSplitInstrs(cfg->bsis)
{
    buildBasicBlocks(functionMap);
    numBlocks = static_cast<uint32_t>(blocks.size());
    entry = blocks.data();
    // copyToFunction();
}

ControlFlowGraph::ControlFlowGraph(
    Function *function, std::map<PCAddress, janus::Function *> &functionMap)
    : ControlFlowGraph(*function, functionMap){};

template <DomInput PCFG>
DominanceAnalysis<PCFG>::DominanceAnalysis(const PCFG &cfg) : PCFG(cfg)
{
    buildDominanceTree();
    buildDominanceFrontiers();
    PCFG::func.domTree = domTree;
}

template <DomInput PCFG>
void DominanceAnalysis<PCFG>::buildDominanceTree()
{
    uint32_t size = PCFG::numBlocks;
    if (!size)
        return;

    //[> construct a two dimensional dominator tree <]
    domTree =
        make_shared<vector<BitVector>>(size, BitVector(size, uint32_t(-1)));
    vector<BitVector> &curdomTree = *domTree;

    //[> initialisation <]
    curdomTree[0].clear();
    curdomTree[0].insert(0);

    bool converge;
    BitVector scratch(size);

    do {
        converge = true;
        for (int i = 0; i < size; i++) {
            BasicBlock &bb = PCFG::blocks[i];
            /* If no predecessor, set itself as its dominator (for entry block)
             */
            if (bb.pred.size() == 0) {
                curdomTree[i].clear();
                if (bb.bid == 0) {
                    curdomTree[i].insert(i);
                    // bb.idom = &bb;
                    idoms[&bb] = &bb;
                }
                continue;
            }
            //[> create a universal set <]
            scratch.setUniversal();

            //[> Get intersect of all its predecessors <]
            for (auto predecessor : bb.pred) {
                scratch.intersect(curdomTree[predecessor->bid]);
            }
            //[> And also include the block itself <]
            scratch.insert(i);
            //[> Check convergence <]
            if (scratch != curdomTree[i])
                converge = false;
            //[> Update <]
            curdomTree[i] = scratch;
        }
    } while (!converge);

    set<BlockID> dominators;
    for (int i = 0; i < size; i++) {
        BasicBlock &bb = PCFG::blocks[i];
        curdomTree[i].toSTLSet(dominators);
        for (BlockID d : dominators) {
            /* If the number of dominators of BB d is 1 less than the dominators
             * of BB i, Then it means the immediate dominator of i is d */
            if (curdomTree[d].size() + 1 == dominators.size()) {
                idoms[&bb] = &PCFG::blocks[d];
                break;
            }
        }
        dominators.clear();
    }
}

template <DomInput PCFG>
void DominanceAnalysis<PCFG>::buildDominanceFrontiers()
{
    for (BasicBlock &bb : PCFG::blocks) {
        updateDominanceFrontier(&bb);
    }
}

template <PostDomInput PCFG>
PostDominanceAnalysis<PCFG>::PostDominanceAnalysis(const PCFG &pcfg)
    : PCFG(pcfg)
{
    buildPostDominanceTree();
    buildPostDominanceFrontiers();
    PCFG::func.pdomTree = pdomTree;
}

template <PostDomInput PCFG>
void PostDominanceAnalysis<PCFG>::buildPostDominanceTree()
{
    uint32_t size = PCFG::blocks.size();
    if (!size)
        return;
    if (!PCFG::terminations.size())
        return;

    BlockID termID = 0;
    bool multiExit = false;

    if (PCFG::terminations.size() > 1) {
        /* For function with multiple exit, we create an additional fake basic
         * block So that all terminating basic block lead to this exit */
        termID = PCFG::blocks.size();
        // create a fake node
        size++;
        multiExit = true;
    } else {
        for (auto t : PCFG::terminations)
            termID = t;
    }

    // [> construct a two dimensional post dominance tree <]
    pdomTree =
        make_shared<vector<BitVector>>(size, BitVector(size, uint32_t(-1)));
    vector<BitVector> &curpdomTree = *pdomTree;

    //[> initialisation from the terminator node < ]
    curpdomTree[termID].clear();
    curpdomTree[termID].insert(termID);

    bool converge;
    BitVector scratch(size);

    do {
        converge = true;
        for (int i = size - 1; i >= 0; i--) {
            if (i == termID) {
                curpdomTree[i].clear();
                curpdomTree[i].insert(i);
                continue;
            }

            if (multiExit && PCFG::terminations.contains(i)) {
                curpdomTree[i].clear();
                curpdomTree[i].insert(i);
                curpdomTree[i].insert(termID);
                continue;
            }

            // normal case
            BasicBlock &bb = PCFG::blocks[i];

            //[> create a universal set <]
            scratch.setUniversal();

            //[> Get intersect of all the two successor <]
            if (bb.succ1)
                scratch.intersect(curpdomTree[bb.succ1->bid]);
            if (bb.succ2)
                scratch.intersect(curpdomTree[bb.succ2->bid]);
            //[> And also include the block itself <]
            scratch.insert(i);
            //[> Check convergence <]
            if (scratch != curpdomTree[i])
                converge = false;
            //[> Update <]
            curpdomTree[i] = scratch;
        }
    } while (!converge);

    set<BlockID> pdominators;
    for (int i = 0; i < size; i++) {
        BasicBlock &bb = PCFG::blocks[i];
        curpdomTree[i].toSTLSet(pdominators);
        for (BlockID d : pdominators) {
            /* If the number of pdominators of BB d is 1 less than the
             * pdominators of BB i, Then it means the immediate post dominator
             * of i is d
             */
            if (curpdomTree[d].size() + 1 == pdominators.size()) {
                ipdoms[&bb] = &PCFG::blocks[d];
                break;
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

template <std::derived_from<ControlFlowGraph> PCFG>
void traverseCFG(PCFG &pcfg)
{
    uint32_t size = pcfg.numBlocks;
    /* Array to tag if a block has been visited */
    bool *discovered = new bool[size];
    memset(discovered, 0, size * sizeof(bool));
    /* Start marking from entry */
    markBlock(pcfg.entry, discovered);
    delete[] discovered;
}

// Construct control flow graph for each function
void buildCFG(Function &function)
{
    /* step 1: construct a vector of basic blocks
     * and link them together */
    // auto &cfg = function.getCFG();
    auto cfg = function.getCFG();

    if (cfg.numBlocks <= 1) {
        return;
    }

    /* step 2: analyse the CFG and build dominance tree */
    /* step 3: analyse the CFG and build dominance frontiers */
    /* step 4: analyse the CFG and build post-dominance tree */
    /* step 5: analyse the CFG and build post dominance frontiers */
    auto dcfg = PostDominanceAnalysis(DominanceAnalysis(cfg));

    /* step 6: traverse the CFG */
    traverseCFG(dcfg);
}

template <CDGInput PCFG>
InstructionControlDependence<PCFG>::InstructionControlDependence(
    const PCFG &pcfg)
    : PCFG(pcfg)
{
    /* All of the instructions within a basic block are
     * immediately control dependent on the branches
     * in the post dominance frontier of the block. */
    for (auto &bb : PCFG::blocks) {
        for (int i = 0; i < bb.size; i++) {
            auto instr = &bb.instrs[i];
            for (auto cb : PCFG::postDominanceFrontiers[&bb]) {
                Instruction *ci = cb->lastInstr();
                controlDeps[instr].push_back(ci);
            }
        }
    }
}

/* Study the link between functions and build call graphs */
void buildCallGraphs(JanusContext *gc)
{
    Function *functions = gc->functions.data();
    uint32_t numFunc = gc->functions.size();

    GSTEP("Building call graphs: " << endl);

    /* create a list of bitvectors */
    BitVector *callGraph = new BitVector[numFunc];

    for (int i = 0; i < numFunc; i++) {
        callGraph[i] = BitVector(numFunc);
        for (auto sub : functions[i].subCalls) {
            callGraph[i].insert(sub);
        }
    }

    bool converge;

    do {
        converge = true;
        for (int i = 0; i < numFunc; i++) {
            // merge the current with sub calls
            for (auto sub : functions[i].subCalls) {
                // get a copy
                BitVector prev = callGraph[i];
                callGraph[i].merge(callGraph[sub]);
                // check for convergence
                if (converge && prev != callGraph[i])
                    converge = false;
            }
        }
    } while (!converge);

    /* writeback subcalls */
    for (int i = 0; i < numFunc; i++) {
        callGraph[i].toSTLSet(functions[i].subCalls);
    }

    delete[] callGraph;
}

// TODO: fix this explicit instantiation
template class InstructionControlDependence<
    PostDominanceAnalysis<DominanceAnalysis<ControlFlowGraph>>>;
