#ifndef _Janus_LOOP_
#define _Janus_LOOP_

#include "janus.h"
#include "Utility.h"
#include "Variable.h"
#include "MemoryLocation.h"
#include "Iterator.h"
#include "LoopAnalysisReport.h"

#include <set>
#include <map>
#include <string>

class JanusContext;

namespace janus {
class BasicBlock;
class Function;
class DDG;

enum LoopType {
    LOOP_UNKNOWN = 0,
    LOOP_DOALL_CYCLIC,
    LOOP_DOALL_BLOCK,
    /* 3. Requires external code snipet */
    LOOP_DOALL_MANUAL,
    /* 4. Requires explicit duplication of memory */
    LOOP_DOALL_MEMORY,
    /* 5. Requires explicit lock */
    LOOP_DOALL_LOCK,
    /* 6. explicit synchronisation */
    LOOP_SYNC,
    /* 7. generic speculation loop */
    LOOP_SPEC,
    /* 8. optimised speculation */
    LOOP_SPEC_OPT,
    /* 9. hardcoded for 429.mcf, remove later */
    LOOP_MCF_HARDCODE,
    LOOP_DEPEND_DYNAMIC,
    LOOP_DEPEND_MEM
};

enum LoopExitType {
    CONDITIONAL_EXIT,
    BREAK_EXIT,
    IRREGULAR_EXIT
};

class Loop
{
    public:
    LoopID                          id;
    /** \brief loop nest id */
    int                             nestID;
    /** \brief The loop is ready for rule generation */
    bool                            pass;
    /** \brief The loop is not beneficial from coverage profiling (low coverage/iteration) */
    bool                            removed;
    /** \brief The loop is profiled unsafe at runtime */
    bool                            inaccurate;
    /** \brief The loop is currently unsafe for intended dynamic modification */
    bool                            unsafe;
    /** \brief Flag to enable runtime check */
    bool                            needRuntimeCheck;
    /** \brief Dependence graph already available */
    bool                            dependenceAvailable;
    /** \brief The loop is affine */
    bool                            affine;
    /** \brief Type of this loop */
    LoopType                        type;
    LoopExitType                    exitType;

    /* --------------------------------------------------------------
     *                    Loop Control Structures
     * ------------------------------------------------------------- */
    BasicBlock                      *start;
    std::set<BlockID>               init;
    std::set<BlockID>               body;
    std::set<BlockID>               end;
    std::set<BlockID>               check;
    std::set<BlockID>               exit;
    /* A set of blocks that needs sync */
    std::set<BlockID>               sync;
    /* A set of blocks that needs conditional modification */
    std::set<BlockID>               cond;

    /* --------------------------------------------------------------
     *                    Loop Relations
     * ------------------------------------------------------------- */
    /* Structure in the CFG */
    Function                        *parent;
    /** \brief Immediate parent of this loop */
    Loop                            *parentLoop;
    /** \brief Immediate children of this loop */
    std::set<Loop*>                 subLoops;
    /** \brief Depth in its loop nests. 0 means the outer-most loop */
    int                             level;
    /** \brief Set of loops that are ancestor of this loop in the loop relation graph */
    std::set<Loop*>                 ancestors;
    /** \brief Set of loops that are descendant(all sub-loop) of this loop in the loop relation graph */
    std::set<Loop*>                 descendants;
    std::set<FuncID>                subCalls;
    std::map<BlockID, FuncID>       calls;
    /* --------------------------------------------------------------
     *                    Loop Variables
     * ------------------------------------------------------------- */
    /** \brief Collection of states that are read-only (inputs of the loop) */
    std::set<VarState*>             initVars; 
    /** \brief Collection of variables to be made private - those that are read only
         but must be privatised for some other reason */
    std::set<Variable>              firstPrivateVars;
    /** \brief Collection of private variables (write followed by read/writes) */
    std::set<Variable>              privateVars;
    /** \brief Collection of phi variables */
    std::set<Variable>              phiVars;
    /** \brief Collection of loop bound variables */
    std::set<Variable>              checkVars;
    /** \brief Induction variables (SSA form) */
    std::set<VarState*>             phiVariables;
    /** \brief Undecided depending variables (SSA form) */
    std::set<VarState*>             undecidedPhiVariables;
    /** \brief Collection of constant phi variables created by compiler register spilling */
    std::set<Variable*>             constPhiVars;
    /** \brief All iterators of the loop */
    std::map<VarState*, Iterator>   iterators;
    /** \brief The main iterator that is associated with a loop bound  */
    Iterator                        *mainIterator;
    /** \brief Set of registers that represent the loop iterators  */
    RegSet                          iteratorRegs;
    /** \brief Iteration count from static analysis, if zero, it means the iteration count can't be determined */
    uint64_t                        staticIterCount;
    /** \brief Vector word size in this loop */
    int                             vectorWordSize;
    /** \brief Peeling distance of multiple 16 bytes, -1 means incompatitable */
    int                             peelDistance;
    /* --------------------------------------------------------------
     *                    Memory Access Information
     * ------------------------------------------------------------- */
    /** \brief memory read set of this loop */
    std::set<MemoryLocation *>      memoryReads;
    /** \brief memory write set of this loop */
    std::set<MemoryLocation *>      memoryWrites;
    /** \brief memory locations whose addresses are undecided */
    std::set<MemoryLocation *>      undecidedMemAccesses;
    /** \brief memory locations that can be decided statically */
    std::map<MemoryLocation*, std::set<MemoryLocation*>>  memoryDependences;
    /** \brief a set of recognised memory locations in the loop, indexed by array bases (common base) 
     *
     * Each array base is subject to runtime checks.
     * Non array base accesses will be treated as an array of its own */
    std::map<ExpandedExpr, std::vector<MemoryLocation *>> memoryAccesses;
    /** \brief Distinct array accesses and their memory locations in the array */
    std::map<Expr, std::vector<MemoryLocation *>> arrayAccesses;
    /** \brief Store loop iterators in terms of array bases */
    std::map<Expr, std::set<Iterator *>> arrayIterators;
    std::set<Expr>                  arrayToCheck;
    /** \brief memory location look up table, key is the variable state */
    std::map<VarState*, MemoryLocation*> locationTable;

    /* --------------------------------------------------------------
     *                    Rewrite Schedule Information
     * ------------------------------------------------------------- */
    ///Rewrite schedule loop header
    RSLoopHeader                    header;
    ///The set of registers needed to copy between each thread
    RegSet                          registerToCopy;
    ///The set of registers needed to merge after threads finish
    RegSet                          registerToMerge;
    ///The set of registers needed to merge after threads finish, but need to track which thread last modified the register
    RegSet                          registerToConditionalMerge;
    ///The set of free SIMD registers
    RegSet                          freeSIMDRegs;
    ///The set of stack elements needed to merge after threads finish
    std::set<Variable>              stackToMerge;
    ///The set of stack elements needed to merge after threads finish
    std::set<Variable>              stackToConditionalMerge;
    ///Four least used registers used in this loop
    ScratchSet                      scratchRegs;
    ///Registers that need to be spilled when using scratch registers
    RegSet                          spillRegs;
    ///Registers that are used
    RegSet                          usedRegs;
    /** \brief A vector of encoded JVar profiles
     *
     * The encoded variables is directly copied into the rewrite schedule and read by the dynamic component */
    std::vector<JVarProfile>        encodedVariables;
    /* --------------------------------------------------------------
     *                    Profiling Information (JProfile mode)
     * ------------------------------------------------------------- */
    //std::set<PCAddress>             depdReadInsts;
    //std::set<PCAddress>             ambiguousReadInsts;
    //std::set<PCAddress>             rareDepdInsts;
    //std::map<Variable, VarProfile>  dependingVariables;
    //std::map<Variable, VarProfile>  privateVariables;
    //std::map<InstID, std::map<int,SyncInfo>> ddg;
    float                           coverage;
    uint64_t                        invocation_count;
    uint64_t                        total_iteration_count;
    /** \brief Reveals the hot path from profiling */
    std::map<BlockID, double>       temperature;
    Loop(BasicBlock *start, Function *parent);
    /** \brief perform full Janus static analysis on this loop (first pass) */
    //void                 analyse(JanusContext *gc);
    // The only difference between basic and advance analysis is that the advance analysis
    // calls also translateAdvance on the parent function, while basic analysis
    // calls only translateBasic.
    void analyseBasic(LoopAnalysisReport loopAnalysisReport, std::vector<janus::Function>& allFunctions);
    void analyseAdvance(LoopAnalysisReport loopAnalysisReport, std::vector<janus::Function>& allFunctions);
    // Had to separate analyze into analyseBasic/analyseAdvance, analyseReduceLoopsAliasAnalysis, and
    // analysePass0, due to dependencies on different steps.
    //void analyseReduceLoopsAliasAnalysis(LoopAnalysisReport loopAnalysisReport, std::vector<janus::Function>& allFunctions);
    void analyseReduceLoopsAliasAnalysis(std::vector<janus::Function>& allFunctions);
    //void Loop::analysePass0(LoopAnalysisReport loopAnalysisReport);
    // The only difference between analysePass0_Basic and analysePass0_Advance is that
    // analysePass0_Basic calls translateBasic on functions, while
    // analysePass0_Advance calls translateBasic and translateAdvance.
    //void analysePass0_Basic(LoopAnalysisReport loopAnalysisReport, std::vector<janus::Function>& allFunctions);
    void analysePass0_Basic(std::vector<janus::Function>& allFunctions);
    //void analysePass0_Advance(LoopAnalysisReport loopAnalysisReport, std::vector<janus::Function>& allFunctions);
    void analysePass0_Advance(std::vector<janus::Function>& allFunctions);

    /** \brief perform full Janus static analysis on this loop (second pass) */
    //void                 analyse2(JanusContext *gc);
    void                 analyse2(LoopAnalysisReport loopAnalysisReport);
    /** \brief perform full Janus static analysis on this loop (third pass) */
    //void                 analyse3(JanusContext *gc);
    //void                 analyse3(LoopAnalysisReport loopAnalysisReport);
    void 				 analyse3(LoopAnalysisReport loopAnalysisReport, std::vector<janus::Loop>& allLoops, std::vector<std::set<LoopID>>& loopNests);
    /** \brief return true if the loop body contains the basic block */
    bool                 contains(BlockID bid);
    /** \brief return true if the loop body contains the instruction */
    bool                 contains(Instruction &instr);
    void                 name(void *outputStream);
    void                 print(void *outputStream);
    void                 printDot(void *outputStream); /* output .dot format */
    /** \brief print the abstract syntax tree of the loop */
    void                 printASTDot(void *outputStream);
    void                 fullPrint(void *outputStream);
    /** \brief returns whether the value of vs is (assumed to be) the same for every iteration
     *
     * After variableAnalysis(this) has been called, this function returns whether vs holds the 
     * same value in every iteration of the loop.
     *
     * Complexity: *O(L)*, where *L = max(this->level, vs->constLoop->level)*,
     *             as an LCA with *vs->constLoop* may be calculated.
     *
     * Note, that false positives are sometimes possible, see VarState::dependsOnMemory.
     */
    bool                 isConstant(VarState *vs);
    /** \brief returns whether the value of vs is only dependent on the induction variable of the loop
     *
     * It traverses back in the SSA graph. If vs is calculated by vs = foo(i, C1, C2).
     * It is treated as an independent variable
     */
    bool                 isIndependent(VarState *vs);
    /** \brief returns whether the value of vs is freshly loaded from outside of the loop */
    bool                 isInitial(VarState *vs);
    /** \brief returns whether v is the main induction variable with check condtions */
    bool                 isMainInduction(VarState *vs);
    /** \brief Returns whether this loop is an ancestor of l.
     *
     * Returns false if l==NULL.
     */
    bool                 isAncestorOf(Loop* l);
    /** \brief Returns whether this loop is a descendant of l.
     *
     * a->hasAsAncestor(b)==b->isAncestorOf(a) for all (a,b != NULL)
     *
     * This function returns true if l==NULL
     */
    bool                 isDescendantOf(Loop* l);
    /** \brief Return the Lowest Common Ancestor with l in the loop graph.
     * 
     * Returns the innermost loop A, such that both this and l are nested within A (possibly 
     * through multiple levels of nesting).
     *
     * If *this* and *l* have no common ancestor, they are in different functions or l==NULL, then
     * NULL is returned.
     */
    Loop                 *lowestCommonAncestor(Loop* l);
    /** \brief Return a variable state whose absolute storage is constant.*/
    VarState             *getAbsoluteConstant(VarState *vs);
    /** \brief Return a variable state whose the storage is constant and non-immediate.*/
    VarState             *getAbsoluteStorage(VarState *vs);
    /** \brief Return the encoded profile for the variable : init, range, check etc. */
    JVarProfile          *getEncodedProfile(VarState *vs);
    private:
    bool analysed;
};

} /* END Janus namespace */

void
linkParentLoops(janus::Function *function);

//void searchLoop(JanusContext *gc, janus::Function *function);
//void searchLoop(janus::Function *function);
void searchLoop(std::vector<janus::Loop>* loops, janus::Function *function);
#endif
