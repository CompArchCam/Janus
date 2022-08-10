#ifndef _Janus_CONTROLFLOW_
#define _Janus_CONTROLFLOW_

// Control flow related analysis

#include <concepts>
#include <memory>
#include <unordered_map>

#include "BasicBlock.h"
#include "Concepts.h"

class JanusContext;

/** \brief build the control flow graph*/
void buildCFG(janus::Function &function);

void buildCallGraphs(JanusContext *gc);

class CFG
{
  public:
    /// Private variable for blocks
    std::vector<janus::BasicBlock> bs;
    /// Private variable for unRecognised blocks
    std::set<BlockID> urs;
    /// Private variable for termination blocks
    std::set<BlockID> ts;
    /// Private variable for returnBlocks blocks
    std::set<BlockID> rbs;
    /// Private variable for blockSplitInstrs
    std::map<BlockID, std::set<InstID>> bsis;
    /// The function the CFG is representing
    janus::Function &func;
    /// Constructor
    CFG(janus::Function &f) : bs{}, urs{}, ts{}, rbs{}, bsis{}, func(f){};
};

/** \brief Data structure representing a basic block
 *
 */
class ControlFlowGraph
{
  private:
    void buildBasicBlocks(std::map<PCAddress, janus::Function *> &);

  public:
    /// Actual storage of cfg
    std::shared_ptr<CFG> cfg;
    /// The function the CFG is representing
    janus::Function &func;
    /// Entry block of the function CFG
    janus::BasicBlock *entry;
    /// Block size
    uint32_t numBlocks;
    /// Actual storage of all the function's basic blocks
    std::vector<janus::BasicBlock> &blocks;
    /// Block id which block target not determined in binary
    std::set<BlockID> &unRecognised;
    /// Block id which block terminates this function
    std::set<BlockID> &terminations;
    /// Block id which block ends with a return instruction
    std::set<BlockID> &returnBlocks;
    /// Split point for oversized basic block (only used for dynamic
    /// modification)
    std::map<BlockID, std::set<InstID>> &blockSplitInstrs;

    /// Constructor
    ControlFlowGraph(janus::Function &,
                     std::map<PCAddress, janus::Function *> &);
    /// Constructor
    ControlFlowGraph(janus::Function *,
                     std::map<PCAddress, janus::Function *> &);
};

template <typename T>
concept DomInput = requires(T cfg)
{
    requires ProvidesBasicCFG<T>;
    requires ProvidesTerminationBlocks<T>;
    requires ProvidesFunctionReference<T>;
};

template <DomInput ProcessedCFG>
class DominanceAnalysis : public ProcessedCFG
{
  private:
    // Update dominance frontiers for other nodes (called for Y)
    // X.dominanceFrontier.contains(Y) iff X
    //! strictly_dominates Y
    // and exists Z in Y.pred, s.t. X dominates Z
    void updateDominanceFrontier(janus::BasicBlock *Y)
    {
        for (janus::BasicBlock *Z : Y->pred) {
            janus::BasicBlock *X = Z;
            while (X && idoms.contains(X) &&
                   // guard to entry
                   (idoms[X] != X) && idoms.contains(Y) &&
                   // stop when you reach the idom of Y
                   (X != idoms[Y])) {
                dominanceFrontiers[X].insert(Y);
                // traverse upwards till entry or idom of Y (loop)
                X = idoms[X];
            }
        }
    }

    void buildDominanceTree();

    /// builds the dominance frontiers of the basic blocks
    void buildDominanceFrontiers();

  public:
    /// Constructor
    DominanceAnalysis(const ProcessedCFG &);

    /// The root of the dominator tree (indexed by blockID) for the CFG in this
    /// function
    std::shared_ptr<std::vector<janus::BitVector>> domTree;

    /// (Hash)Map storing the immediate dominators of each basic blocks;
    /// replaces the *janus::BasicBlock::idom* field
    /// TODO: rename required
    std::unordered_map<janus::BasicBlock *, janus::BasicBlock *> idoms;

    /// (Hash)Map storing the dominance frontier of each basic block; replaces
    /// the *janus::BasicBlock::dominanceFrontier* in the original
    /// implementation
    /// TODO: rename required
    std::unordered_map<janus::BasicBlock *, std::set<janus::BasicBlock *>>
        dominanceFrontiers;
};

template <typename T>
concept PostDomInput = requires(T cfg)
{
    requires ProvidesBasicCFG<T>;
    requires ProvidesTerminationBlocks<T>;
    requires ProvidesFunctionReference<T>;
};

template <PostDomInput PCFG>
class PostDominanceAnalysis : public PCFG
{
  private:
    // Update post dominance frontiers for other nodes (called for Y)
    // X.postDominanceFrontier.contains(Y) iff
    // For X in all successors of Y
    // if X does not post dominate Y
    // then X's post dominance frontier is Y
    void updatePostDominanceFrontier(janus::BasicBlock *Y)
    {
        auto updateDomFrontier = [this](auto *x, auto *xSucc) {
            while (x) {
                // stop when we hit the post dominator of Y
                if (x == ipdoms[xSucc])
                    break;
                // stop if we hit the exit
                if (x->succ1 == nullptr && x->succ2 == nullptr)
                    break;
                // add to pdom frontier
                postDominanceFrontiers[x].insert(xSucc);
                // traverse upwards towards pdom tree
                x = ipdoms[x];
            }
        };

        updateDomFrontier(Y->succ1, Y);
        updateDomFrontier(Y->succ2, Y);
    }

    void buildPostDominanceFrontiers()
    {
        for (janus::BasicBlock &bb : ControlFlowGraph::blocks) {
            updatePostDominanceFrontier(&bb);
        }
    }

    void buildPostDominanceTree();

  public:
    PostDominanceAnalysis(const PCFG &);

    /// (Hash)Map storing the immediate dominators of each basic blocks;
    /// replaces the *janus::BasicBlock::idom* field
    /// TODO: rename required
    std::unordered_map<janus::BasicBlock *, janus::BasicBlock *> ipdoms;

    /// (Hash)Map storing the dominance frontier of each basic block; replaces
    /// the *janus::BasicBlock::dominanceFrontier* in the original
    /// implementation
    /// TODO: rename required
    std::unordered_map<janus::BasicBlock *, std::set<janus::BasicBlock *>>
        postDominanceFrontiers;

    /// The root of the post dominator tree (indexed by blockID) for the CFG in
    /// this function
    std::shared_ptr<std::vector<janus::BitVector>> pdomTree;
};

template <typename T>
concept CDGInput = requires
{
    requires ProvidesBasicCFG<T>;
    requires ProvidesPostDominanceFrontier<T>;
};

template <CDGInput PCFG>
class InstructionControlDependence : PCFG
{
  public:
    std::unordered_map<janus::Instruction *, std::vector<janus::Instruction *>>
        controlDeps;

    InstructionControlDependence(const PCFG &);
};

template <std::derived_from<ControlFlowGraph> PCFG>
void traverseCFG(PCFG &pcfg);

#endif
