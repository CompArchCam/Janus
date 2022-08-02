/* A basic block is a single entry single exit list of instructions
 * Note that this basic block structure is for analysis only
 * The end of a block does not has to be ended with control flow instructions
 */

#ifndef _Janus_BASICBLOCK_
#define _Janus_BASICBLOCK_

#include "Instruction.h"
#include "Utility.h"
#include "janus.h"

#include <map>
#include <set>
#include <vector>

namespace janus
{
class Loop;
class VarState;
class Function;

typedef uint32_t BlockID;

#define MAX_BLOCK_INSTR_COUNT 200

class BasicBlock
{
  public:
    /// Identifier for BB lookup
    BlockID bid;
    /// Number of instructions in this block
    uint32_t size;
    /// Instruction id of the first instruction
    InstID startInstID;
    /// Instruction id of the last instruction
    InstID endInstID;
    /// The predecessors of the current basic block in the CFG
    std::set<BasicBlock *> pred;
    /// First successor in the CFG
    BasicBlock *succ1;
    /// Second successor in the CFG (only available for blocks ended with
    /// conditional branches)
    BasicBlock *succ2;
    /// A fake basic block is a block that does not end with control flow
    /// instruction, but ended with an incoming branch target
    bool fake;
    /// A terminate block is a block with unknown successors (open exit)
    bool terminate;
    /// The first traverse timestep in the DFS of the CFG
    uint32_t firstVisitStep;
    /// The second traverse timestep in the DFS of the CFG
    uint32_t secondVisitStep;
    /** \brief Array of the contained instructions
     *
     * It points to the function's instruction array.
     * If you want to modify the instructions, please allocate another chunk of
     * memory
     */
    Instruction *instrs;
    /** \brief Array instructions that contain memory accesses in the basic
     * block
     *
     * Each element of the array points to an existing instruction that contains
     * memory access.
     */
    std::vector<MemoryInstruction> minstrs;
    /** \brief Last states of each variable changed (written to) in this block
     * (including phi nodes)
     */
    std::map<Variable, VarState *> lastStates;
    /** \brief Input variables used in the block without
     * initialization/assignment first */
    std::set<Variable> inputs;
    /** \brief Contains all phi nodes in this block. */
    std::vector<VarState *> phiNodes;
    /** \brief Immediate post-dominator of the current basic block. */
    BasicBlock *ipdom;
    /** \brief Basic blocks in the dominance frontier of this one.
     *
     * Set of blocks b, such that this block dominates an immediate predecessor
     * of b, but does not strictly dominate b.
     */
    // std::set<BasicBlock *> dominanceFrontier;
    /** \brief Basic blocks in the post dominance frontier of this one.
     *
     * Set of blocks b, such that this block dominates an immediate predecessor
     * of b, but does not strictly dominate b.
     */
    std::set<BasicBlock *> postDominanceFrontier;
    /// The parent function of the basic block
    Function *parentFunction;
    /** \brief Pointer to the innermost loop containing this block in its body
     */
    Loop *parentLoop;
    BasicBlock(Function *func, BlockID bid, InstID startInstID,
               InstID endInstID, bool isFake);
    /// Return the last instruction of the basic block
    Instruction *lastInstr();

    /** \brief returns true iff this dominates the given block */
    bool dominates(BasicBlock &block);
    /** \brief returns true iff this dominates the given block */
    bool dominates(BasicBlock *block);
    /// print the basic block into a dot compatible stream
    void printDot(void *outputStream);
    /** \brief Returns the SSA definition (of the given variable) used at the
     * start of the current block.
     *
     * A traversal on the basic blocks is performed to find the definition.
     */
    VarState *alive(Variable var);
    /** \brief Returns the SSA definition (of the given variable) alive at
     * (right before) the index'th instruction of the block.
     */
    VarState *alive(Variable var, int index);

    /** \brief Is the given register live just before the index'th instruction?
     *
     * Is the current value just before the index'th instruction in this block
     * in the register given ever used later (either within the same block or in
     * possible successor blocks)?
     */
    bool isRegLive(uint32_t reg, int index);

    /** \brief Returns the set of registers live just before the index'th
     * instruction.
     *
     * The i'th entry in the returned *RegSet* is set iff *isRegLive(i,index)*
     * would return true.
     */
    RegSet liveRegSet(int index);

    /** \brief If the given register is assigned just before the index'th
     * instruction, can the new value reach the calling function?
     *
     * *index* should be between 0 and size **both inclusive**. If index==size,
     * then it considers assigning just after the last instruction.
     */
    bool doesReachOutside(uint32_t reg, int index);

    /** \brief Returns the set of registers for which, if assigned just before
     * the intex'th instruction, the new value can reach the calling function.
     *
     * The i'th entry in the returned *RegSet* is set iff
     * *doesReachOutside(i,index)* would return true.
     */
    RegSet reachRegOutSet(int index);
};

} // namespace janus
#endif
