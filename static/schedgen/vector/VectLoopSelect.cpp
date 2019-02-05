#include "VectLoopSelect.h"
#include "capstone/capstone.h"
#include <set>

using namespace janus;

void
setSupportedVectorOpcodes(set<InstOp> &supported_opcodes, set<InstOp> &singles, set<InstOp> &doubles)
{
#ifdef JANUS_X86
    //Single precision
    singles.insert(X86_INS_MOVAPS);
    singles.insert(X86_INS_ADDPS);
    singles.insert(X86_INS_SUBPS);
    singles.insert(X86_INS_MULPS);
    singles.insert(X86_INS_DIVPS);
    //SSE
    singles.insert(X86_INS_VMULSS);
    singles.insert(X86_INS_VDIVSS);
    singles.insert(X86_INS_VADDSS);
    singles.insert(X86_INS_VSUBSS);
    singles.insert(X86_INS_VMOVSS);
    singles.insert(X86_INS_ADDSS);
    singles.insert(X86_INS_SUBSS);
    singles.insert(X86_INS_MOVSS);
    singles.insert(X86_INS_MULSS);
    singles.insert(X86_INS_DIVSS);

    //Double precision
    doubles.insert(X86_INS_MOVAPD);
    doubles.insert(X86_INS_ADDPD);
    doubles.insert(X86_INS_SUBPD);
    doubles.insert(X86_INS_MULPD);
    doubles.insert(X86_INS_DIVPD);
    //SSE
    doubles.insert(X86_INS_VMULSD);
    doubles.insert(X86_INS_VDIVSD);
    doubles.insert(X86_INS_VADDSD);
    doubles.insert(X86_INS_VSUBSD);
    doubles.insert(X86_INS_VMOVSD);
    doubles.insert(X86_INS_ADDSD);
    doubles.insert(X86_INS_SUBSD);
    doubles.insert(X86_INS_MOVSD);
    doubles.insert(X86_INS_MULSD);
    doubles.insert(X86_INS_DIVSD);
    doubles.insert(X86_INS_CVTSI2SD);
    doubles.insert(X86_INS_VCVTSI2SD);

    supported_opcodes.insert(singles.begin(), singles.end());
    supported_opcodes.insert(doubles.begin(), doubles.end());

    supported_opcodes.insert(X86_INS_PXOR);
    supported_opcodes.insert(X86_INS_VPXOR);
#endif
}

static int
getVectorWordSize(Instruction &instr, set<InstOp> &singles, set<InstOp> &doubles) {
    if (singles.find(instr.minstr->opcode) != singles.end())
        return 4;
    if (doubles.find(instr.minstr->opcode) != doubles.end())
        return 8;
    return -1;
}

bool
isVectorRuntimeCompatible(Loop &loop, set<InstOp> &supported_opcode, set<InstOp> &singles, set<InstOp> &doubles)
{
    Function *parent = loop.parent;
    /* Get entry block of the CFG */
    BasicBlock *entry = parent->entry;
    bool foundVector = false;
    //examine each loop instructions
#ifdef JANUS_X86
    for (auto bid: loop.body) {
        BasicBlock &bb = entry[bid];
        for (int i=0; i<bb.size; i++) {
            Instruction &instr = bb.instrs[i];
            if (instr.minstr->isXMMInstruction()) {
                foundVector = true;
                //check supported opcode
                if (supported_opcode.find(instr.minstr->opcode) == supported_opcode.end()) {
                    LOOPLOGLINE("loop "<<dec<<loop.id<<" unsupported opcode: "<<bb.instrs[i]);
                    return false;
                }

                //check vector size
                int wordSize = getVectorWordSize(instr, singles, doubles);
                if (wordSize > 0) {
                    if (!loop.vectorWordSize) loop.vectorWordSize = wordSize;
                    else if (loop.vectorWordSize != wordSize) {
                        LOOPLOGLINE("loop "<<dec<<loop.id<<" contains different vector word size: "<<bb.instrs[i]);
                        return false;
                    }
                }

                //check reduction read
            } else {
                //check if it is supported non-vector instruction
                
            }
        }
    }
#endif
    return foundVector;
}

static bool
checkLoopIterator(Iterator &iter)
{
    //currently we are only processing on add iterator
    //if (!(iter.vs && iter.vs->lastModified && iter.vs->lastModified->opcode == Instruction::Add))
    //    return false;
    if (iter.kind == Iterator::INDUCTION_IMM && iter.stride <= 0)
        return false;

    if (iter.kind != Iterator::INDUCTION_IMM && iter.kind != Iterator::INDUCTION_VAR)
        return false;

    //the update instruction can be retrieved
    if (!iter.getUpdateInstr()) return false;
    return true;
}

static bool
checkLoopIterators(Loop &loop)
{
    for (auto &iter: loop.iterators) {
        if (!checkLoopIterator(iter.second)) return false;
    }
    return true;
}

void
selectVectorisableLoop(JanusContext *gc, set<Loop *> &selected_loops, set<InstOp> &supported_opcode, set<InstOp> &singles, set<InstOp> &doubles)
{
    for (auto &loop : gc->loops) {
        //Condition 1: we only look at innermost loops
        if (loop.descendants.size()) {
            LOOPLOGLINE("loop "<<dec<<loop.id<<" not innermost.");
            continue;
        }
        //Condition 2: must contain a main induction variable
        if (!loop.mainIterator) {
            LOOPLOGLINE("loop "<<dec<<loop.id<<" does not have main iterator");
            continue;
        }
        //Condition 2: no internal function calls
        if (loop.subCalls.size()) {
            LOOPLOGLINE("loop "<<dec<<loop.id<<" internal function calls.");
            continue;
        }
        //Condition 3: no ambiguous memory addresses
        if (loop.undecidedMemAccesses.size()) {
            LOOPLOGLINE("loop "<<dec<<loop.id<<" undecided memory access.");
            continue;
        }
        //Condition 4: unsafe for induction analysis
        if (loop.unsafe) {
            LOOPLOGLINE("loop "<<dec<<loop.id<<" unsafe.");
            continue;
        }
        //Condition 5: loop boundary decided and larger than 4
        if (loop.staticIterCount < 4) {
            LOOPLOGLINE("loop "<<dec<<loop.id<<" static iter count:"<<loop.staticIterCount);
            continue;
        }
        //currently we only support one block optimisation
        //For more than one block, not yet implemented
        if (loop.body.size()!=1) {
            LOOPLOGLINE("loop "<<dec<<loop.id<<" multiple basic blocks.");
            continue;
        }
        if (!isVectorRuntimeCompatible(loop, supported_opcode, singles, doubles)) {
            LOOPLOGLINE("loop "<<dec<<loop.id<<" not currently handled in Janus runtime.");
            continue;
        }

        //check whether induction variable is supported
        if (!checkLoopIterators(loop)) {
            LOOPLOGLINE("loop "<<dec<<loop.id<<" induction variable support not yet implemented.");
            continue;
        }
        selected_loops.insert(&loop);
    }
}

void
printVectorisableLoops(set<Loop *> &selected_loops) {
    cout << "Vectorisable loops: " << endl;
    for (auto loop: selected_loops) {
        cout << loop->id << " ";
    }
    cout << endl;
}
