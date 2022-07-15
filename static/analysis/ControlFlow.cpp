#include <concepts>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <queue>

#include "BasicBlock.h"
#include "ControlFlow.h"
#include "Function.h"
#include "JanusContext.h"
#include "Utility.h"

using namespace std;
using namespace janus;

#define BB_LEADER 2
#define BB_TERMINATOR 1

static bool dbg = true;

class ControlFlowGraph
{
  private:
  public:
    /// The function the CFG is representing
    janus::Function &func;
    /// Entry block of the function CFG
    BasicBlock *entry;
    /// Block size
    uint32_t numBlocks;
    /// Actual storage of all the function's basic blocks
    std::vector<BasicBlock> blocks;
    /// Function Call Instructions
    std::map<janus::InstID, janus::Function *> calls;
    /// Block id which block target not determined in binary
    std::set<BlockID> unRecognised;
    /// Block id which block terminates this function
    std::set<BlockID> terminations;
    /// Block id which block ends with a return instruction
    std::set<BlockID> returnBlocks;
    /// Split point for oversized basic block (only used for dynamic
    /// modification)
    std::map<BlockID, std::set<InstID>> blockSplitInstrs;

    /// Constructor
    ControlFlowGraph(Function &, std::vector<Instruction> &,
                     std::map<PCAddress, InstID> &,
                     std::map<PCAddress, janus::Function *> &);

    /// Temporary workaround to copy all fields back into the functions
    /// after initialisation; delete after reorg is done.
    void copyToFunction();
};

ControlFlowGraph::ControlFlowGraph(
    Function &function, std::vector<Instruction> &instrs,
    std::map<PCAddress, InstID> &instrTable,
    std::map<PCAddress, janus::Function *> &functionMap)
    : func{function}
{
    /* Each function has a vector of machine instructions, each insn has an ID
     * Mark the target and branch as basic block boundaries */
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
                        auto targetFunc = functionMap.at(callTarget);
                        calls[id] = targetFunc;
                        // TODO: check if we need this
                        function.calls[id] = targetFunc;
                        function.subCalls.insert(targetFunc->fid);
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
        /* Corner case: final instruction not a cti */
        if (id == instrCount - 1 && !instr.isControlFlow()) {
            marks[id] += BB_TERMINATOR;
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
            /* We must make sure that there is no only nops in the block */
            auto nextMark = std::next(mark);
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
                blocks.emplace_back(&function, blockID++, boundary, blockEnd,
                                    (*nextMark).second % 2 == 0);
            } else {
                trueBlockEnd = trueBlockStart + MAX_BLOCK_INSTR_COUNT;

                blockSplitInstrs[blockID].insert(trueBlockEnd);
                blocks.emplace_back(&function, blockID++, boundary,
                                    trueBlockEnd, 1);
                // update the boundary
                boundary = trueBlockEnd;

                /* For further oversize blocks, split the block into multiple
                 * block */
                while (blockEnd - boundary > MAX_BLOCK_INSTR_COUNT) {
                    trueBlockEnd = boundary + MAX_BLOCK_INSTR_COUNT;

                    blockSplitInstrs[blockID].insert(trueBlockEnd);
                    blocks.emplace_back(&function, blockID++, boundary,
                                        trueBlockEnd, 1);
                    boundary += MAX_BLOCK_INSTR_COUNT;
                }
                if (blockEnd != boundary) {
                    blocks.emplace_back(&function, blockID++, boundary,
                                        blockEnd, (*nextMark).second % 2 == 0);
                    trueBlockEnd = blockEnd;
                }
            }

        }
        // boundary may be marked as multiple LEADERS and only one terminator
        // it means this basic block contains only one instruction
        else if (property > 1) {
            blocks.emplace_back(&function, blockID++, boundary, boundary,
                                false);
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

    if (bSize <= 1) {
        cout << "cfg constructor early exit" << endl;
        entry = blocks.data();
        numBlocks = static_cast<uint32_t>(blocks.size());
        cout << numBlocks << endl;
        copyToFunction();
        cout << "Copied to function" << endl;
        return;
    }

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
        InstID blockEndID = block->lastInstr()->id;
        auto query = edges.find(blockEndID);

        if (!edges.contains(blockEndID)) {
            if (block->lastInstr()->isControlFlow()) {
                if (notRecognised.contains(blockEndID)) {
                    terminations.insert(block->bid);

                    // XXX: block.terminate is redundant
                    block->terminate = true;
                } else {
                    unRecognised.insert(block->bid);
                }
            }
        } else {
            const auto succBlocksIDs = edges.at(blockEndID);
            for (auto succBlockID : succBlocksIDs) {
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
    if (function.name == "MAIN__") {
        for (int i = 0; i < bSize; i++) {
            BasicBlock *block = blockArray + i;
            if (block->lastInstr()->opcode == Instruction::Call) {
                PCAddress target =
                    block->lastInstr()->minstr->getTargetAddress();
                if (function.context->functionMap.contains(target) &&
                    function.context->functionMap[target]->name ==
                        "_gfortran_stop_string@plt") {
                    terminations.insert(i);
                }
            }
        }
    }
    entry = blocks.data();
    numBlocks = static_cast<uint32_t>(blocks.size());
    copyToFunction();
}

void ControlFlowGraph::copyToFunction()
{
    func.entry = entry;
    func.numBlocks = numBlocks;
    func.blocks = blocks;
    // func.calls = calls;
    func.unRecognised = unRecognised;
    func.terminations = terminations;
    func.returnBlocks = returnBlocks;
    func.blockSplitInstrs = blockSplitInstrs;
}

template <std::derived_from<ControlFlowGraph> ProcessedCFG>
class DominanceAnalysis : public ProcessedCFG
{
  private:
    // Update dominance frontiers for other nodes (called for Y)
    // X.dominanceFrontier.contains(Y) iff X
    //! strictly_dominates Y
    // and exists Z in Y.pred, s.t. X dominates Z
    static void updateDominanceFrontier(BasicBlock *Y)
    {
        for (BasicBlock *Z : Y->pred) {
            BasicBlock *X = Z;
            while (X &&
                   // guard to entry
                   (X->idom != X) &&
                   // stop when you reach the idom of Y
                   (X != Y->idom)) {
                if (dbg)
                    cout << "\t\t\t" << X->bid
                         << "->domFront.size(): " << X->dominanceFrontier.size()
                         << endl;
                X->dominanceFrontier.insert(Y);
                // traverse upwards till entry or idom of Y (loop)
                X = X->idom;
            }
        }
    }

  public:
    /// Constructor
    DominanceAnalysis(const ProcessedCFG &);

    /// Constructor with a bit of conversion
    // DominanceAnalysis(const ProcessedCFG &);

    /// The root of the dominator tree (indexed by blockID) for the CFG in this
    /// function
    std::shared_ptr<std::vector<BitVector>> domTree;

    /// Temporary workaround to copy all fields back into the functions
    /// after initialisation; delete after reorg is done.
    void copyToFunction();

    /// builds the dominance frontiers of the basic blocks
    void buildDominanceFrontiers();
};

template <std::derived_from<ControlFlowGraph> PCFG>
DominanceAnalysis<PCFG>::DominanceAnalysis(const PCFG &cfg)
    : PCFG(std::move(cfg))
{
    uint32_t size = ControlFlowGraph::numBlocks;
    if (!size) {
        cout << "\t\t numBlocks = 0" << endl;
        buildDominanceFrontiers();
        cout << "\t\t Dominance Frontier Built" << endl;
        copyToFunction();
        return;
    }

    /* construct a two dimensional dominator tree */
    // make_shared is equivalent to the old 'new' statement, with the additional
    // benefit of not needing to be explicitly deleted when done
    domTree = make_shared<std::vector<BitVector>>(
        vector<BitVector>(size, BitVector(size, uint32_t(-1))));
    vector<BitVector> &curDomTree = *domTree;

    cout << "df1" << endl;

    /* initialisation */
    curDomTree[0].clear();
    curDomTree[0].insert(0);

    bool converge;
    BitVector scratch(size);

    cout << "df2" << endl;

    do {
        converge = true;
        for (int i = 0; i < size; i++) {
            cout << "df3 " << i << endl;
            BasicBlock &bb = ControlFlowGraph::blocks[i];
            /* If no predecessor, set itself as its dominator (for entry block)
             */
            if (bb.pred.size() == 0) {
                curDomTree[i].clear();
                if (bb.bid == 0) {
                    curDomTree[i].insert(i);
                    bb.idom = &bb;
                }
                continue;
            }
            /* create a universal set */
            scratch.setUniversal();

            /* Get intersect of all its predecessors */
            for (auto predecessor : bb.pred) {
                scratch.intersect(curDomTree[predecessor->bid]);
            }
            /* And also include the block itself */
            scratch.insert(i);
            /* Check convergence */
            if (scratch != curDomTree[i])
                converge = false;
            /* Update */
            curDomTree[i] = scratch;
        }
    } while (!converge);

    set<BlockID> dominators;
    for (int i = 0; i < size; i++) {
        BasicBlock &bb = ControlFlowGraph::blocks[i];
        curDomTree[i].toSTLSet(dominators);
        for (BlockID d : dominators) {
            /* If the number of dominators of BB d is 1 less than the dominators
             * of BB i, Then it means the immediate dominator of i is d */
            if (curDomTree[d].size() + 1 == dominators.size()) {
                bb.idom = &(ControlFlowGraph::blocks[d]);
                break;
            }
        }
        dominators.clear();
    }

    cout << "\t\t Dominanec Tree End" << endl;
    buildDominanceFrontiers();
    cout << "\t\t Dominance Frontier Built" << endl;
    copyToFunction();
}

template <std::derived_from<ControlFlowGraph> PCFG>
void DominanceAnalysis<PCFG>::buildDominanceFrontiers()
{
    for (BasicBlock &bb : ControlFlowGraph::blocks) {
        updateDominanceFrontier(&bb);
    }
}

template <std::derived_from<ControlFlowGraph> PCFG>
void DominanceAnalysis<PCFG>::copyToFunction()
{
    ControlFlowGraph::func.domTree = domTree.get();
}

template <std::derived_from<ControlFlowGraph> PCFG>
class PostDominanceAnalysis : public PCFG
{
  private:
    void updatePostDominanceFrontier(BasicBlock *Y)
    {
        if (dbg)
            cout << "\t\t\t" << 1 << endl;
        BasicBlock *X = Y->succ1;
        while (X) {
            // stop when we hit the post dominator of Y
            if (dbg)
                cout << "\t\t\t" << X->bid << ": " << 1 << endl;
            if (X == Y->ipdom)
                break;
            if (dbg)
                cout << "\t\t\t" << X->bid << ": " << 2 << endl;
            // stop if we hit the exit
            if (X->succ1 == NULL && X->succ2 == NULL)
                break;
            if (dbg)
                cout << "\t\t\t" << X->bid << ": " << 3 << endl;
            // add to pdom frontier
            if (dbg && X)
                cout << "\t\t\t X is not null" << endl;
            if (dbg && Y)
                cout << "\t\t\t Y is not null" << endl;
            if (dbg && X)
                cout << "\t\t\t X->postDom.size(): "
                     << X->postDominanceFrontier.size() << endl;
            X->postDominanceFrontier.insert(Y);
            if (dbg)
                cout << "\t\t\t X inserted Y" << endl;
            if (dbg)
                cout << "\t\t\t" << X->bid << ": " << 4 << endl;
            // traverse upwards towards pdom tree
            X = X->ipdom;
            if (dbg && X)
                cout << "\t\t\t" << X->bid << ": " << 5 << endl;
            if (dbg && X && X->bid == 0)
                cout << "\t\t\t Special case: bid = 0, check validity";
            if (dbg && X && X->bid == 0)
                cout << "\t\t\t" << X << "vs" << ControlFlowGraph::entry
                     << endl;
        }

        if (dbg)
            cout << "\t\t\t" << 2 << endl;
        X = Y->succ2;
        while (X) {
            // stop when we hit the post dominator of Y
            if (dbg)
                cout << "\t\t\t" << X->bid << ": " << 1 << endl;
            if (X == Y->ipdom)
                break;
            if (dbg)
                cout << "\t\t\t" << X->bid << ": " << 2 << endl;
            // stop if we hit the exit
            if (X->succ1 == NULL && X->succ2 == NULL)
                break;
            if (dbg)
                cout << "\t\t\t" << X->bid << ": " << 3 << endl;
            // add to pdom frontier
            if (dbg && X)
                cout << "\t\t\t X is not null" << endl;
            if (dbg && Y)
                cout << "\t\t\t Y is not null" << endl;
            if (dbg && X)
                cout << "\t\t\t X->postDom.size(): "
                     << X->postDominanceFrontier.size() << endl;
            X->postDominanceFrontier.insert(Y);
            if (dbg)
                cout << "\t\t\t X inserted Y" << endl;
            if (dbg)
                cout << "\t\t\t" << X->bid << ": " << 4 << endl;
            // traverse upwards towards pdom tree
            X = X->ipdom;
            if (dbg && X)
                cout << "\t\t\t" << X->bid << ": " << 5 << endl;
            if (dbg && X && X->bid == 0)
                cout << "\t\t\t Special case: bid = 0, check validity";
            if (dbg && X && X->bid == 0)
                cout << "\t\t\t" << X << "vs" << ControlFlowGraph::entry
                     << endl;
        }
    }
    // Update post dominance frontiers for other nodes (called for Y)
    // X.postDominanceFrontier.contains(Y) iff
    // For X in all successors of Y
    // if X does not post dominate Y
    // then X's post dominance frontier is Y

    void buildPostDominanceFrontiers()
    {
        for (BasicBlock &bb : ControlFlowGraph::blocks) {
            updatePostDominanceFrontier(&bb);
        }
    }

    void copyToFunction();

    void cleanup()
    {
        buildPostDominanceFrontiers();
        copyToFunction();
    }

  public:
    PostDominanceAnalysis(const PCFG &);

    /// The root of the post dominator tree (indexed by blockID) for the CFG in
    /// this function
    std::shared_ptr<std::vector<BitVector>> pdomTree;
};

template <std::derived_from<ControlFlowGraph> PCFG>
PostDominanceAnalysis<PCFG>::PostDominanceAnalysis(const PCFG &pcfg)
    : PCFG(std::move(pcfg))
{
    uint32_t size = ControlFlowGraph::numBlocks;
    if (!size) {
        // TODO: cleanup this bit
        cout << "numBlocks = 0" << endl;
        buildPostDominanceFrontiers();
        cout << "\t\t Post Dominance Frontier Built" << endl;
        copyToFunction();
        return;
    }

    if (!ControlFlowGraph::terminations.size()) {
        // TODO: cleanup this bit
        cout << "terminations = 0" << endl;
        buildPostDominanceFrontiers();
        cout << "\t\t Post Dominance Frontier Built" << endl;
        copyToFunction();
        return;
    }

    BlockID termID = 0;
    bool multiExit = false;

    if (ControlFlowGraph::terminations.size() > 1) {
        /* For function with multiple exit, we create an additional fake basic
         * block So that all terminating basic block lead to this exit */
        termID = ControlFlowGraph::blocks.size();
        // create a fake node
        size++;
        multiExit = true;
    } else {
        for (auto t : ControlFlowGraph::terminations)
            termID = t;
    }

    /* construct a two dimensional post dominance tree */
    pdomTree =
        make_shared<vector<BitVector>>(size, BitVector(size, uint32_t(-1)));
    vector<BitVector> &pdomTreeDref = *pdomTree;

    /* initialisation from the terminator node */
    pdomTreeDref[termID].clear();
    pdomTreeDref[termID].insert(termID);

    bool converge;
    BitVector scratch(size);

    do {
        converge = true;
        for (int i = size - 1; i >= 0; i--) {
            if (i == termID) {
                pdomTreeDref[i].clear();
                pdomTreeDref[i].insert(i);
                continue;
            }

            if (multiExit && ControlFlowGraph::terminations.contains(i)) {
                pdomTreeDref[i].clear();
                pdomTreeDref[i].insert(i);
                pdomTreeDref[i].insert(termID);
                continue;
            }

            // normal case
            BasicBlock &bb = ControlFlowGraph::blocks[i];

            /* create a universal set */
            scratch.setUniversal();

            /* Get intersect of all the two successor */
            if (bb.succ1)
                scratch.intersect(pdomTreeDref[bb.succ1->bid]);
            if (bb.succ2)
                scratch.intersect(pdomTreeDref[bb.succ2->bid]);
            /* And also include the block itself */
            scratch.insert(i);
            /* Check convergence */
            if (scratch != pdomTreeDref[i])
                converge = false;
            /* Update */
            pdomTreeDref[i] = scratch;
        }
    } while (!converge);

    set<BlockID> pdominators;
    for (int i = 0; i < size; i++) {
        BasicBlock &bb = ControlFlowGraph::blocks[i];
        pdomTreeDref[i].toSTLSet(pdominators);
        for (BlockID d : pdominators) {
            /* If the number of pdominators of BB d is 1 less than the
             * pdominators of BB i, Then it means the immediate post dominator
             * of i is d */
            if (pdomTreeDref[d].size() + 1 == pdominators.size()) {
                bb.ipdom = &(ControlFlowGraph::blocks[d]);
                break;
            }
        }
        pdominators.clear();
    }

    cout << "\t\t End of buildPostDominanceFrontiers" << endl;
    buildPostDominanceFrontiers();
    cout << "\t\t Post Dominance Frontier Built" << endl;
    copyToFunction();
}

template <std::derived_from<ControlFlowGraph> PCFG>
void PostDominanceAnalysis<PCFG>::copyToFunction()
{
    ControlFlowGraph::func.pdomTree = pdomTree.get();
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

template <std::derived_from<ControlFlowGraph> PCFG> void traverseCFG(PCFG &pcfg)
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
    if (dbg)
        cout << function.fid << ": start" << endl;
    /* step 1: construct a vector of basic blocks
     * and link them together */
    auto cfg = ControlFlowGraph(function, function.instrs, function.minstrTable,
                                function.context->functionMap);
    if (dbg)
        cout << function.fid << ": cfg built" << endl;

    if (cfg.numBlocks <= 1) {
        if (dbg)
            cout << function.fid << ": early exit" << endl;
        return;
    }

    /* step 2: analyse the CFG and build dominance tree */
    /* step 3: analyse the CFG and build dominance frontiers */
    /* step 4: analyse the CFG and build post-dominance tree */
    /* step 5: analyse the CFG and build post dominance frontiers */
    auto dcfg = PostDominanceAnalysis(DominanceAnalysis(cfg));
    if (dbg)
        cout << function.fid << ": dcfg built" << endl;

    /* step 6: traverse the CFG */
    traverseCFG(dcfg);
    if (dbg)
        cout << function.fid << ": dcfg traversed" << endl;
}

// Construct control dependence graph for each function
void buildCDG(Function &function)
{
    /* All of the instructions within a basic block are
     * immediately control dependent on the branches
     * in the post dominance frontier of the block. */
    for (auto &bb : function.blocks) {
        for (int i = 0; i < bb.size; i++) {
            Instruction &instr = bb.instrs[i];
            for (auto cb : bb.postDominanceFrontier) {
                Instruction *ci = cb->lastInstr();
                instr.controlDep.push_back(ci);
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

//// OLD

// static void buildBasicBlocks(Function &function);

// static void buildDominanceTree(Function &function);

// static void buildPostDominanceTree(Function &function);

// void traverseCFG(Function &function)
//{
// uint32_t size = function.numBlocks;
//[> Array to tag if a block has been visited <]
// bool *discovered = new bool[size];
// memset(discovered, 0, size * sizeof(bool));
//[> Start marking from entry <]
// markBlock(function.entry, discovered);
// delete[] discovered;
//}

//// Construct control flow graph for each function
// void buildCFGOld(Function &function)
//{
/* step 1: construct a vector of basic blocks
 * and link them together */
// buildBasicBlocks(function);
//[> set function entry <]
// function.entry = function.blocks.data();
// function.numBlocks = function.blocks.size();

// if (function.numBlocks <= 1)
// return;

//[> step 2: analyse the CFG and build dominance tree <]
// buildDominanceTree(function);

//[> step 3: analyse the CFG and build post-dominance tree <]
// buildPostDominanceTree(function);

//[> step 4: analyse the CFG and build dominance frontiers <]
// buildDominanceFrontiers(function);

//[> step 5: analyse the CFG and build post dominance frontiers <]
// buildPostDominanceFrontiers(function);

//[> step 6: traverse the CFG <]
// traverseCFG(function);
//}

// static void buildBasicBlocks(Function &function)
//{
// int cornerCase = false;
// auto &instrs = function.instrs;
// auto &instrTable = function.minstrTable;
// auto &functionMap = function.context->functionMap;
// auto &blocks = function.blocks;

/* Each function has a vector of machine instructions, each insn has an ID
 * Mark the target and branch as basic block boundaries */
// map<InstID, uint32_t> marks;
// map<InstID, set<InstID>> edges;
// set<InstID> notRecognised;

// InstID id;
// int32_t offset;
// InstID targetID;

//// firstly assume the first instruction is the entry
// marks[0] += BB_LEADER;

// uint32_t instrCount = instrs.size();

// for (InstID id = 0; id < instrCount; id++) {

// Instruction &instr = instrs[id];

// if (instr.isControlFlow()) {
//// for control flow instructions, mark this id as terminate
// marks[id] += BB_TERMINATOR;
//// always assume the next non-nop instruction is a LEADER
// offset = 1;
//// for safety, make the instruction followed by a CJUMP with a
//// LEADER
// if (instr.opcode == Instruction::ConditionalBranch)
// marks[id + offset] += BB_LEADER;

// while (id < instrCount - 2 &&
// instrs[id + offset].opcode == Instruction::Nop) {
// offset++;
//}

// marks[id + offset] += BB_LEADER;

//// for jump and conditional jump, work out the target
// if (instr.opcode == Instruction::DirectBranch ||
// instr.opcode == Instruction::ConditionalBranch) {
// PCAddress target = instr.minstr->getTargetAddress();
// if (target) {
//// find this target in the instruction table
// auto query = instrTable.find(target);
// if (query == instrTable.end()) {
// notRecognised.insert(id);
//}
//// if found the target instruction, mark the target as a
//// leader
// else {
// targetID = (*query).second;
// marks[targetID] += BB_LEADER;
// edges[id].insert(targetID);
//}
//}
//// whether it is register or memory operands, it is an indirect
//// jump which we don't bother to perform extensive analysis on
//// it
// else {
// notRecognised.insert(id);
//}
//}

//// for conditional jump, always add fall through edge
// if (instr.opcode == Instruction::ConditionalBranch) {
// edges[id].insert(id + 1);
//}

//// for call instructions, no more further actions, just to construct
//// the call graph
// if (instr.opcode == Instruction::Call) {

//// Currently assume all calls would return except indirect call
// if (id + offset < instrCount)
// edges[id].insert(id + offset);

// PCAddress callTarget = instr.minstr->getTargetAddress();
// if (callTarget) {
//// find this target in the function map
// auto query = functionMap.find(callTarget);
// if (query == functionMap.end()) {
// notRecognised.insert(id);
//}
//// if found in the function map
// else {
// Function *targetFunc = (*query).second;
// function.calls[id] = targetFunc;
// function.subCalls.insert(targetFunc->fid);
//}
//}
//// whether it is register or memory operands, it is an indirect
//// call which we don't bother to perform extensive analysis on
//// it
// else {
// notRecognised.insert(id);
//}
//}

//// for return instruction, add to the function's exit node
// if (instr.opcode == Instruction::Return) {
// function.returnBlocks.insert(id);
//}
//}
//[> Corner case: final instruction not a cti <]
// if (id == instrCount - 1 && !instr.isControlFlow()) {
// marks[id] += BB_TERMINATOR;
// cornerCase = true;
//}
//}

//// Step 2: After scanning all instructions,
//// Based on the boundaries, construct basic blocks
//// And according to the edges, construct control flow graphs
// BlockID blockID = 0;
// BlockID trueBlockEnd = 0;
// InstID startID;
// InstID endID;
//// since all boundaries are already sorted by InstID, we just iterate
//// through it
// for (auto mark = marks.begin(); mark != marks.end(); mark++) {

// InstID boundary = (*mark).first;
//// if mark is out of function boundary
// if (boundary >= instrCount)
// break;
// uint32_t property = (*mark).second;

//// if the boundary is marked as multiple LEADERS but no terminators
//// it is the start of a block
// if (property % 2 == 0) {
//// find the next LEADER or TERMINATOR for the end of the block
//[> We must make sure that there is no only nops in the block <]
// auto nextMark = mark;
// nextMark++;
// if (nextMark == marks.end())
// break;
// InstID blockEnd = (*nextMark).first;

//// find the first LEADER
// auto prevMark = mark;
// while (1) {
// if (prevMark == marks.begin())
// break;
// prevMark--;
// if ((*prevMark).second % 2 == 1) {
// prevMark++;
// break;
//}
//}
//// get the true start instID of the block
// InstID trueBlockStart = (*prevMark).first;
//// trueBlockEnd carries the last oversized block
// if (trueBlockStart < trueBlockEnd)
// trueBlockStart = trueBlockEnd;

// if (blockEnd - trueBlockStart < MAX_BLOCK_INSTR_COUNT) {
// blocks.emplace_back(&function, blockID++, boundary, blockEnd,
//(*nextMark).second % 2 == 0);
//} else {
// trueBlockEnd = trueBlockStart + MAX_BLOCK_INSTR_COUNT;
// function.blockSplitInstrs[blockID].insert(trueBlockEnd);
// blocks.emplace_back(&function, blockID++, boundary,
// trueBlockEnd, 1);
//// update the boundary
// boundary = trueBlockEnd;

/* For further oversize blocks, split the block into multiple
 * block */
// while (blockEnd - boundary > MAX_BLOCK_INSTR_COUNT) {
// trueBlockEnd = boundary + MAX_BLOCK_INSTR_COUNT;
// function.blockSplitInstrs[blockID].insert(trueBlockEnd);
// blocks.emplace_back(&function, blockID++, boundary,
// trueBlockEnd, 1);
// boundary += MAX_BLOCK_INSTR_COUNT;
//}
// if (blockEnd != boundary) {
// blocks.emplace_back(&function, blockID++, boundary,
// blockEnd, (*nextMark).second % 2 == 0);
// trueBlockEnd = blockEnd;
//}
//}

//}
//// boundary may be marked as multiple LEADERS and only one terminator
//// it means this basic block contains only one instruction
// else if (property > 1) {
// blocks.emplace_back(&function, blockID++, boundary, boundary,
// false);
//}
//// no need to create basic block for a single out entry
//}

//// step 3: now all basic blocks are created, link them into a CFG
//// we already got all edge information in the edge node
//// step 3.1 link instructions to block
// BasicBlock *blockArray = blocks.data();
// uint32_t bSize = blocks.size();
// for (int i = 0; i < bSize; i++) {
// BasicBlock *block = blockArray + i;
// for (int j = 0; j < block->size; j++) {
// block->instrs[j].block = block;
//}
//}

// if (bSize <= 1)
// return;

//// step 3.2 after all instructions are linked with their parent blocks, we
//// can then link each block to block
// for (int i = 0; i < bSize; i++) {
// BasicBlock *block = blockArray + i;
// if (block->fake) {
// if (i == bSize - 1)
// continue;
// block->succ1 = blockArray + i + 1;
// blockArray[i + 1].pred.insert(block);
// continue;
//}
// InstID endID = block->lastInstr()->id;
// auto query = edges.find(endID);

// if (query == edges.end()) {
// if (block->lastInstr()->isControlFlow()) {
// if (notRecognised.find(endID) == notRecognised.end()) {
// function.terminations.insert(block->bid);
// block->terminate = true;
//} else
// function.unRecognised.insert(block->bid);
//}
//} else {
// for (auto succBlockID : (*query).second) {
// auto &instr = instrs[succBlockID];
// BasicBlock *targetBlock = instr.block;
// if (targetBlock)
// targetBlock->pred.insert(block);
// if (block->succ1) {
// block->succ2 = targetBlock;
//} else {
// block->succ1 = targetBlock;
//}
//}
//}
//}
//// XXX: for fortran only
// if (function.name == "MAIN__") {
// for (int i = 0; i < bSize; i++) {
// BasicBlock *block = blockArray + i;
// if (block->lastInstr()->opcode == Instruction::Call) {
// PCAddress target =
// block->lastInstr()->minstr->getTargetAddress();
// if (function.context->functionMap.find(target) !=
// function.context->functionMap.end())
// if (function.context->functionMap[target]->name ==
//"_gfortran_stop_string@plt")
// function.terminations.insert(i);
//}
//}
//}
//}

//// Update dominance frontiers for other nodes (called for Y)
//// X.dominanceFrontier.contains(Y) iff X
////! strictly_dominates Y
//// and exists Z in Y.pred, s.t. X dominates Z
// static void updateDominanceFrontier(BasicBlock *Y)
//{
// for (BasicBlock *Z : Y->pred) {
// BasicBlock *X = Z;
// while (X &&
//// guard to entry
//(X->idom != X) &&
//// stop when you reach the idom of Y
//(X != Y->idom)) {
// X->dominanceFrontier.insert(Y);
//// traverse upwards till entry or idom of Y (loop)
// X = X->idom;
//}
//}
//}

//// Update post dominance frontiers for other nodes (called for Y)
//// X.postDominanceFrontier.contains(Y) iff
//// For X in all successors of Y
//// if X does not post dominate Y
//// then X's post dominance frontier is Y
// static void updatePostDominanceFrontier(BasicBlock *Y)
//{
// BasicBlock *X = Y->succ1;
// while (X) {
//// stop when we hit the post dominator of Y
// if (X == Y->ipdom)
// break;
//// stop if we hit the exit
// if (X->succ1 == NULL && X->succ2 == NULL)
// break;
//// add to pdom frontier
// X->postDominanceFrontier.insert(Y);
//// traverse upwards towards pdom tree
// X = X->ipdom;
//}

// X = Y->succ2;
// while (X) {
//// stop when we hit the post dominator of Y
// if (X == Y->ipdom)
// break;
//// stop if we hit the exit
// if (X->succ1 == NULL && X->succ2 == NULL)
// break;
//// add to pdom frontier
// X->postDominanceFrontier.insert(Y);
//// traverse upwards towards pdom tree
// X = X->ipdom;
//}
//}

// void buildDominanceFrontiers(Function &function)
//{
// for (BasicBlock &bb : function.blocks) {
// updateDominanceFrontier(&bb);
//}
//}

// void buildPostDominanceFrontiers(janus::Function &function)
//{
// for (BasicBlock &bb : function.blocks) {
// updatePostDominanceFrontier(&bb);
//}
//}

// static void buildDominanceTree(Function &function)
//{
// uint32_t size = function.numBlocks;
// if (!size)
// return;

//[> construct a two dimensional dominator tree <]
// function.domTree =
// new vector<BitVector>(size, BitVector(size, uint32_t(-1)));
// vector<BitVector> &domTree = *function.domTree;

//[> initialisation <]
// domTree[0].clear();
// domTree[0].insert(0);

// bool converge;
// BitVector scratch(size);

// do {
// converge = true;
// for (int i = 0; i < size; i++) {
// BasicBlock &bb = function.entry[i];
/* If no predecessor, set itself as its dominator (for entry block)
 */
// if (bb.pred.size() == 0) {
// domTree[i].clear();
// if (bb.bid == 0) {
// domTree[i].insert(i);
// bb.idom = &bb;
//}
// continue;
//}
//[> create a universal set <]
// scratch.setUniversal();

//[> Get intersect of all its predecessors <]
// for (auto predecessor : bb.pred) {
// scratch.intersect(domTree[predecessor->bid]);
//}
//[> And also include the block itself <]
// scratch.insert(i);
//[> Check convergence <]
// if (scratch != domTree[i])
// converge = false;
//[> Update <]
// domTree[i] = scratch;
//}
//} while (!converge);

// set<BlockID> dominators;
// for (int i = 0; i < size; i++) {
// BasicBlock &bb = function.entry[i];
// domTree[i].toSTLSet(dominators);
// for (BlockID d : dominators) {
/* If the number of dominators of BB d is 1 less than the dominators
 * of BB i, Then it means the immediate dominator of i is d */
// if (domTree[d].size() + 1 == dominators.size()) {
// bb.idom = &function.entry[d];
// break;
//}
//}
// dominators.clear();
//}
//}

// static void buildPostDominanceTree(Function &function)
//{
// uint32_t size = function.numBlocks;
// if (!size)
// return;
// if (!function.terminations.size())
// return;

// BlockID termID = 0;
// bool multiExit = false;

// if (function.terminations.size() > 1) {
/* For function with multiple exit, we create an additional fake basic
 * block So that all terminating basic block lead to this exit */
// termID = function.blocks.size();
//// create a fake node
// size++;
// multiExit = true;
//} else {
// for (auto t : function.terminations)
// termID = t;
//}

//[> construct a two dimensional post dominance tree <]
// function.pdomTree =
// new vector<BitVector>(size, BitVector(size, uint32_t(-1)));
// vector<BitVector> &pdomTree = *function.pdomTree;

//[> initialisation from the terminator node <]
// pdomTree[termID].clear();
// pdomTree[termID].insert(termID);

// bool converge;
// BitVector scratch(size);

// do {
// converge = true;
// for (int i = size - 1; i >= 0; i--) {
// if (i == termID) {
// pdomTree[i].clear();
// pdomTree[i].insert(i);
// continue;
//}

// if (multiExit &&
// function.terminations.find(i) != function.terminations.end()) {
// pdomTree[i].clear();
// pdomTree[i].insert(i);
// pdomTree[i].insert(termID);
// continue;
//}

//// normal case
// BasicBlock &bb = function.entry[i];

//[> create a universal set <]
// scratch.setUniversal();

//[> Get intersect of all the two successor <]
// if (bb.succ1)
// scratch.intersect(pdomTree[bb.succ1->bid]);
// if (bb.succ2)
// scratch.intersect(pdomTree[bb.succ2->bid]);
//[> And also include the block itself <]
// scratch.insert(i);
//[> Check convergence <]
// if (scratch != pdomTree[i])
// converge = false;
//[> Update <]
// pdomTree[i] = scratch;
//}
//} while (!converge);

// set<BlockID> pdominators;
// for (int i = 0; i < size; i++) {
// BasicBlock &bb = function.entry[i];
// pdomTree[i].toSTLSet(pdominators);
// for (BlockID d : pdominators) {
/* If the number of pdominators of BB d is 1 less than the
 * pdominators of BB i, Then it means the immediate post dominator
 * of i is d */
// if (pdomTree[d].size() + 1 == pdominators.size()) {
// bb.ipdom = &function.entry[d];
// break;
//}
//}
// pdominators.clear();
//}
//}
