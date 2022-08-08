/* Janus Abstract Functions */

#ifndef _Janus_FUNCTION_
#define _Janus_FUNCTION_

#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "BasicBlock.h"
#include "ControlFlow.h"
#include "Executable.h"
#include "Expression.h"
#include "Instruction.h"
#include "Iterator.h"
#include "MachineInstruction.h"
#include "Utility.h"
#include "Variable.h"
#include "janus.h"

class JanusContext;

namespace janus
{
class Function
{
  public:
    Function(Function &&f) = default;
    /// function id
    FuncID fid;
    /// number of bytes in byte code
    uint32_t size;
    /// A pointer to the byte code
    uint8_t *contents;
    PCAddress startAddress;
    PCAddress endAddress;
    std::string name;
    bool isExecutable;
    bool isExternal;
    bool available;
    JanusContext *context;
    /// If set, then it is safe to query the basic information of basic
    /// block/instructions/sub calls
    bool translated;
    /* --------------------------------------------------------------
     *                       information storage
     * ------------------------------------------------------------- */
    /// Actual storage of all the function's machine instructions, read-only
    std::vector<MachineInstruction> minstrs;
    /// Actual storage of all the function's abstract instructions, read-only
    std::vector<Instruction> instrs;
    /// Set of variables used in this function
    std::set<Variable> allVars;
    /// Set of all SSA variables ever defined (or used) in this function
    std::set<VarState *> allStates;
    /// Entry block of the function CFG
    // BasicBlock *entry;
    /// Block size
    // uint32_t numBlocks;
    /// Actual storage of all the function's basic blocks
    // std::vector<BasicBlock> blocks;
    /// The root of the dominator tree (indexed by blockID) for the CFG in this
    /// function
    std::shared_ptr<std::vector<BitVector>> domTree;
    /// The root of the post dominator tree (indexed by blockID) for the CFG in
    /// this function
    std::shared_ptr<std::vector<BitVector>> pdomTree;

    /* --------------------------------------------------------------
     *                    query data structure
     * ------------------------------------------------------------- */
    /// A vector of loops IDs that are in this function.
    std::set<LoopID> loops;
    /// instruction look up table by PC
    std::map<PCAddress, InstID> minstrTable;
    /// All loop iterators
    std::map<VarState *, Iterator *> iterators;
    /// all instructions which performs a subcall
    std::map<InstID, Function *> calls;
    /// all function id that is called by the current function
    std::set<FuncID> subCalls;
    /// Block id which block target not determined in binary
    // std::set<BlockID> unRecognised;
    /// Block id which block terminates this function
    // std::set<BlockID> terminations;
    /// Block id which block ends with a return instruction
    // std::set<BlockID> returnBlocks;
    /// Split point for oversized basic block (only used for dynamic
    /// modification)
    // std::map<BlockID, std::set<InstID>> blockSplitInstrs;
    /// A set of expression pointers used in this function
    std::set<Expr *> exprs;
    /// The initial states for all variables found in this function
    std::map<Variable, VarState *> inputStates;
    /// Unique pointer to the control flow graph
    std::unique_ptr<ControlFlowGraph> cfg;
    /// Unique pointer to processed cfg; very bad indeed
    PostDominanceAnalysis<DominanceAnalysis<ControlFlowGraph>> *pcfg;

    /* --------------------------------------------------------------
     *                Architecture specific information
     * ------------------------------------------------------------- */
    /** \brief The size of the stack frame used for local variables, not
     * including registers saved before making the stack frame. */
    uint32_t stackFrameSize;
    /** \brief The total size of the stack frame, ie typically the difference
     * between base pointer and the stack pointer.
     *
     * Any access to [rbp-x] where *x <= totalFrameSize* is an access to the
     * stack frame.
     */
    uint32_t totalFrameSize;
    bool hasBasePointer;
    bool implicitStack;
    bool hasIndirectStackAccesses;
    /** \brief True iff the function has a constant stack pointer.
     *
     * True iff the function only modifies the stack pointer in entry nodes and
     * termination nodes and hence the stack pointer remains constant throughout
     * the body of the function.
     */
    bool hasConstantStackPointer;
    /* Scratch Register info */
    ScratchSet stmRegs;
    RegSet spillRegs;
    /* --------------------------------------------------------------
     *   liveness information (Only available after calling livenessAnalysis)
     * ------------------------------------------------------------- */
    /** \brief Array of liveIn register set for each instruction in this
     * function
     *
     * To query the liveIn register for a given instruction:
     * liveReg(instr) = function.liveRegIn[instr.id] */
    RegSet *liveRegIn;
    /** \brief Array of liveOut register set for each instruction in this
     * function
     *
     * To query the liveOut register for a given instruction:
     * liveReg(instr) = function.liveRegOut[instr.id] */
    RegSet *liveRegOut;

    /* --------------------------------------------------------------
     *                    intra-procedure information
     * ------------------------------------------------------------- */
    /* The following information is available after intra-procedure analysis */
    bool recursive;
    uint32_t traverseStartStep;
    uint32_t traverseEndStep;

    Function(JanusContext *gc, FuncID fid, const Symbol &symbol, uint32_t size);
    ~Function();

    // retrieve information from instructions
    void translate();
    void visualize(void *outputStream);   // output to dot format
    void printASTDot(void *outputStream); // AST output to dot format
    void print(void *outputStream);
    bool needSync();
    /** \brief Contruct the relations of the loop in this function */
    void analyseLoopRelations();
    /** \brief creates the cfg is not present; otherwise return the cfg object
     */
    ControlFlowGraph &getCFG();
};
} // namespace janus

#endif
