/* Janus Abstract Functions */

#ifndef _Janus_FUNCTION_
#define _Janus_FUNCTION_

#include "janus.h"
#include "Executable.h"
#include "MachineInstruction.h"
#include "Instruction.h"
#include "BasicBlock.h"
#include "Utility.h"
#include "Expression.h"
#include "Variable.h"
#include "Iterator.h"
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <list>

class JanusContext;

namespace janus {
    class Function
    {
    public:
        ///function id
        FuncID                                 fid;
        ///number of bytes in byte code
        uint32_t                               size;
        ///A pointer to the byte code
        uint8_t                                *contents;
        PCAddress                              startAddress;   
        PCAddress                              endAddress;
        std::string                            name;
        bool                                   isExecutable;
        bool                                   isExternal;
        bool                                   available;
        JanusContext                           *context;
        ///If set, then it is safe to query the basic information of basic block/instructions/sub calls
        bool                                   translated;
        /* --------------------------------------------------------------
         *                       information storage
         * ------------------------------------------------------------- */
        ///Actual storage of all the function's machine instructions, read-only
        std::vector<MachineInstruction>        minstrs;
        ///Actual storage of all the function's abstract instructions, read-only
        std::vector<Instruction>               instrs;
        ///Set of variables used in this function
        std::set<Variable>                     allVars;
        ///Set of all SSA variables ever defined (or used) in this function
        std::set<VarState*>                    allStates;
        ///Entry block of the function CFG
        BasicBlock                             *entry;
        //set of all basic blocks with no incoming edge, including the entry block. this is to deal with the corner cases where we may have a basic block that does not have incoming edge known statically (it could be a target of indirect branch).
        std::set<BlockID>                      danglingBlocks;
        ///Block size
        uint32_t                               numBlocks;
        ///Actual storage of all the function's basic blocks
        std::vector<BasicBlock>                blocks;
        ///The root of the dominator tree (indexed by blockID) for the CFG in this function
        std::vector<BitVector>                 *domTree;
        ///The root of the post dominator tree (indexed by blockID) for the CFG in this function
        std::vector<BitVector>                 *pdomTree;

        /* --------------------------------------------------------------
         *                    query data structure
         * ------------------------------------------------------------- */
        ///A vector of loops IDs that are in this function.
        std::set<LoopID>                       loops;
        ///instruction look up table by PC
        std::map<PCAddress, InstID>            minstrTable;
        ///All loop iterators
        std::map<VarState*, Iterator *>        iterators;
        ///all instructions which performs a subcall
        std::map<InstID, Function *>           calls;
        ///all function id that is called by the current function
        std::set<FuncID>                       subCalls;
        ///Block id which block target not determined in binary
        std::set<BlockID>                      unRecognised;
        ///Block id which block terminates this function
        std::set<BlockID>                      terminations;
        ///Instr id which calls a long jmp
        std::set<InstID>                      longjmps;
        ///Block id which block ends with a return instruction
        std::set<BlockID>                      returnBlocks;
        ///Split point for oversized basic block (only used for dynamic modification)
        std::map<BlockID, std::set<InstID>>    blockSplitInstrs;
        ///A set of expression pointers used in this function
        std::set<Expr *>                       exprs;
        ///The initial states for all variables found in this function
        std::map<Variable, VarState*>          inputStates;

        ///all instructions which performs a subcall through jump
        std::map<InstID, Function *>           jumpToFunc;
        ///all function id that is called by the current function through jump
        std::set<FuncID>                       jumpCalls;
        /* --------------------------------------------------------------
         *                Architecture specific information
         * ------------------------------------------------------------- */
        /** \brief The size of the stack frame used for local variables, not including registers saved before making the stack frame. */
        uint32_t                               stackFrameSize;
        /** \brief The total size of the stack frame, ie typically the difference between base
         *         pointer and the stack pointer.
         *
         * Any access to [rbp-x] where *x <= totalFrameSize* is an access to the stack frame.
         */
        uint32_t                               totalFrameSize;
        bool                                   hasBasePointer;
        bool                                   implicitStack;
        bool                                   hasIndirectStackAccesses;
        PCAddress                              prologueEnd;

        /** \brief True iff the function has a constant stack pointer.
         *
         * True iff the function only modifies the stack pointer in entry nodes and termination
         * nodes and hence the stack pointer remains constant throughout the body of the function.
         */
        bool                                   hasConstantStackPointer;
        bool                                   hasConstantSP_AddSub;
        bool                                   hasConstantSP_total;
        bool                                   hasConstantSP_PushPop;
        int                                    changeSizeAddSub;
        int                                    changeSizePushPop;
        int                                    changeSizeTotal;
        bool                                   hasCanary;       //protected by Canary Value
        PCAddress                              canaryReadIns;
        PCAddress                              canaryCheckIns;
        /* Scratch Register info */
        ScratchSet                             stmRegs;
        RegSet                                 spillRegs;
        /* --------------------------------------------------------------
         *   liveness information (Only available after calling livenessAnalysis)
         * ------------------------------------------------------------- */
        /** \brief Array of liveIn register set for each instruction in this function
         *
         * To query the liveIn register for a given instruction:
         * liveReg(instr) = function.liveRegIn[instr.id] */
        RegSet                                 *liveRegIn;
        /** \brief Array of liveOut register set for each instruction in this function
         *
         * To query the liveOut register for a given instruction:
         * liveReg(instr) = function.liveRegOut[instr.id] */
        RegSet                                 *liveRegOut;

        FlagSet                                 *liveFlagIn;
        FlagSet                                 *liveFlagOut;

        /* --------------------------------------------------------------
         *                    intra-procedure information
         * ------------------------------------------------------------- */
        /* The following information is available after intra-procedure analysis */
        bool                                   recursive;
        uint32_t                               traverseStartStep;
        uint32_t                               traverseEndStep;

        Function(JanusContext *gc, FuncID fid, const Symbol &symbol, uint32_t size);
        ~Function();

        //retrieve information from instructions
        void translate();
        void visualize(void *outputStream); //output to dot format
        void printASTDot(void *outputStream); //AST output to dot format
        void print(void *outputStream);
        bool needSync();
        /** \brief Contruct the relations of the loop in this function */
        void analyseLoopRelations();
    };
}

#endif
