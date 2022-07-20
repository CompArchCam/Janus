#include "Arch.h"
#include "BasicBlock.h"
#include "Function.h"
#include "Instruction.h"
#include "JanusContext.h"
#include "Loop.h"
#include "Utility.h"
#include <cstring>
#include <iostream>
#include <map>
#include <queue>
#include <set>

#ifdef JANUS_AARCH64
#define ARM64_INS_LDP 0x9f
#define ARM64_INS_STP 0x144
#endif

using namespace janus;
using namespace std;

void scanMemoryAccess(Function *function)
{
    for (auto &bb : function->getCFG().blocks) {
        for (int i = 0; i < bb.size; i++) {
            Instruction &instr = bb.instrs[i];
            if (instr.isMemoryAccess()) {
                bb.minstrs.emplace_back(&instr);
            }
        }
    }
}

// Identify different types of variables in the loop
void variableAnalysis(janus::Loop *loop)
{
    BasicBlock *entry = loop->parent->getCFG().entry;
    Function *parent = loop->parent;

#ifdef JANUS_AARCH64
    set<pair<Variable, Variable>>
        ldpPairs; // Set to hold memory location pairs used by ldp (and stp)
                  // instructions
#endif

    LOOPLOG("\tAnalysing loop variables" << endl);

    queue<VarState *> constQ;
    /* Step 1: recognise initial variables for the loop
     * An initial variable must be an constant variable */
    for (auto bid : loop->body) {
        BasicBlock &bb = entry[bid];
        for (int i = 0; i < bb.size; i++) {
            auto &instr = bb.instrs[i];

#ifdef JANUS_AARCH64
            /* Check for { ldp xn, xm [mem] } instructions that have 128 bit
             * memory width */
            if (instr.minstr->opcode == ARM64_INS_LDP ||
                instr.minstr->opcode == ARM64_INS_STP) {
                for (auto vi : instr.inputs) {
                    if (vi->type == JVAR_STACKFRAME) {
                        Variable mem1 = Variable(*vi);
                        Variable mem2 = Variable(*vi);
                        mem2.value += vi->size;

                        ldpPairs.insert(make_pair(mem1, mem2));
                    }
                }
            }
            if (instr.minstr->usesPropagatedStackFrame()) {
                for (auto vi : instr.inputs) {
                    if (vi->type == JVAR_STACKFRAME) {
                        loop->firstPrivateVars.insert(*(Variable *)vi);
                    }
                }
            }
#endif
            /* Check all inputs from loop instructions */
            for (auto vs : instr.inputs) {
                if (vs->type == JVAR_MEMORY || vs->type == JVAR_POLYNOMIAL) {
                    // add base and index into initVars
                    for (auto phi : vs->pred) {
                        if (loop->isInitial(phi)) {
                            loop->initVars.insert(phi);
                        }
                    }
                } else {
                    // add to initVars
                    if (loop->isInitial(vs)) {
                        loop->initVars.insert(vs);
                    }
                }
            }
            // specifically for outputs base and offset, add to initVars
            for (auto vs : instr.outputs) {
                if (vs->type == JVAR_MEMORY || vs->type == JVAR_POLYNOMIAL) {
                    for (auto phi : vs->pred) {
                        if (loop->isInitial(phi)) {
                            loop->initVars.insert(phi);
                        }
                    }
                }
            }
        }
        // add phi nodes to init variables
        for (auto vs : bb.phiNodes) {
            for (auto phi : vs->pred) {
                if (loop->isInitial(phi)) {
                    loop->initVars.insert(phi);
                }
            }
        }
    }

    // put all initial variables to a queue
    for (auto vi : loop->initVars) {
        constQ.push(vi);
    }

    /* Step 2: use BFS to find further obvious constant states (no phi nodes are
     * crossed, derived from consts) */
    while (!constQ.empty()) {
        VarState *vs = constQ.front();
        vs->constLoop = loop;
        constQ.pop();

        /* Check each instruction that use this variable as input */
        for (auto *instr : vs->dependants) {
            // if (!loop->contains(instr->block->bid)) continue;

            // 1. if all inputs of the machine instruction have been marked as
            // constant,
            // 2. the output does not modify the storage of the inputs
            // then the outputs should be constant as well
            bool constInputs(true);
            for (auto *vs : instr->inputs) {
                if (vs->type == JVAR_MEMORY || vs->type == JVAR_POLYNOMIAL ||
                    vs->type == JVAR_SHIFTEDREG ||
                    vs->type == JVAR_SHIFTEDCONST) {
                    for (auto vi : vs->pred) {
                        constInputs &= loop->isConstant(vi);
                    }
                } else
                    constInputs &= loop->isConstant(vs);
            }
            if (constInputs) {
                for (auto *vso : instr->outputs) {
                    if (vso->constLoop == loop)
                        continue;
                    // check with inputs
                    bool outputConst(true);
                    for (auto *vi : instr->inputs) {
                        if (*(Variable *)vso == *(Variable *)vi) {
                            outputConst = false;
                            break;
                        }
                    }
                    if (outputConst) {
                        vso->constLoop = loop;
                        constQ.push(vso);
                    }
                }
            }
        }
    }

    /* Step 3: recognise private variables for the loop
     * a private variable has to be:
     * 1. defined within the loop
     * 2. no phi nodes at the start of the loop */
    // conclude a set of phi variables
    set<Variable> phiVars;
    for (auto vs : loop->start->phiNodes) {
        if (!vs->notUsed)
            phiVars.insert(*(Variable *)vs);
    }

    for (auto bid : loop->body) {
        BasicBlock &bb = entry[bid];
        for (int i = 0; i < bb.size; i++) {
            auto &instr = bb.instrs[i];
            // we are only interested in stack/absolute outputs
            for (auto vs : instr.outputs) {
                if (vs->type == JVAR_REGISTER || vs->type == JVAR_STACK ||
                    vs->type == JVAR_STACKFRAME || vs->type == JVAR_ABSOLUTE) {
                    // try to find it in the phi variables
                    Variable query = *(Variable *)vs;
                    // if not found in the phi node, then it is a private
                    // variable
                    if (phiVars.find(query) == phiVars.end())
                        loop->privateVars.insert(query);
                }
            }
        }
    }

#ifdef JANUS_AARCH64
    for (auto &ldpPair : ldpPairs) {
        if (loop->privateVars.find(ldpPair.second) != loop->privateVars.end() ||
            loop->phiVars.find(ldpPair.second) != loop->phiVars.end() ||
            loop->checkVars.find(ldpPair.second) != loop->checkVars.end() ||
            loop->privateVars.find(ldpPair.first) != loop->privateVars.end() ||
            loop->phiVars.find(ldpPair.first) != loop->phiVars.end() ||
            loop->checkVars.find(ldpPair.first) != loop->checkVars.end()) {
            loop->privateVars.insert(ldpPair.first);
            loop->privateVars.insert(ldpPair.second);

            for (auto vsIter = loop->initVars.begin();
                 vsIter != loop->initVars.end()) {
                if (*(Variable *)(*vsIter) == ldpPair.first ||
                    *(Variable *)(*vsIter) == ldpPair.second) {
                    loop->initVars.erase(vsIter++);
                } else
                    ++vsIter;
            }
            JVarProfile profile1;
            profile1.var = ldpPair.first;
            profile1.type = COPY_PROFILE;
            loop->encodedVariables.push_back(profile1);

            JVarProfile profile2;
            profile2.var = ldpPair.second;
            profile2.type = COPY_PROFILE;
            loop->encodedVariables.push_back(profile2);
        }
    }

#endif
}

void scratchAnalysis(janus::Loop *loop)
{
    BasicBlock *entry = loop->parent->getCFG().entry;
    LOOPLOG("\tStarted scratch analysis" << endl);
    /* Work out all the registers needed for copying before loop context */
    for (auto v : loop->initVars) {
        if (v->type == JVAR_REGISTER &&
            (jreg_is_gpr(v->value) || jreg_is_simd(v->value))) {
            loop->registerToCopy.insert(v->value);
        }
    }

    LOOPLOG("\t\tRegister to copy across threads: " << loop->registerToCopy
                                                    << endl);

    /* Work out the registers needed to copy back to the main thread */
    for (auto bid : loop->body) {
        BasicBlock &bb = entry[bid];
        for (int i = 0; i < bb.size; i++) {
            Instruction &instr = bb.instrs[i];
            for (auto vo : instr.outputs) {
                if (vo->type == JVAR_REGISTER || vo->type == JVAR_STACK ||
                    vo->type == JVAR_STACKFRAME) {
                    bool neededOutside = false;
                    // if there is dependants outside of the loop
                    for (auto dep : vo->dependants) {
                        if (!loop->contains(dep->block->bid)) {
                            neededOutside = true;
                            break;
                        }
                    }

                    if (neededOutside) {
                        if (vo->type == JVAR_REGISTER)
                            loop->registerToMerge.insert(vo->value);
                        else if (vo->type == JVAR_STACK ||
                                 vo->type == JVAR_STACKFRAME)
                            loop->stackToMerge.insert(*(Variable *)vo);
                    }
                }
            }
        }
    }

    /* We also need to merge all the iterator value */
    for (auto &iter : loop->iterators) {
        VarState *vs = iter.first;
        if (vs->type == JVAR_REGISTER)
            loop->registerToMerge.insert(vs->value);
    }
    // add the check state too (because check state is modified dynamically)
    if (loop->mainIterator && loop->mainIterator->checkState &&
        loop->mainIterator->checkState->type == JVAR_REGISTER)
        loop->registerToMerge.insert(loop->mainIterator->checkState->value);

    LOOPLOG("\t\tRegister to merge from threads: " << loop->registerToMerge
                                                   << endl);
    LOOPLOG("\t\tNo. of stack variables to merge from threads: "
            << loop->stackToMerge.size() << endl);

    // TODO: update used regs
    loop->usedRegs.merge(loop->registerToCopy);
    loop->usedRegs.merge(loop->registerToMerge);
    /* We assign all general purpose registers with a mark
     * the least frequently used registers are chosen */
    map<int, int> regFreq;
    multimap<int, int> regRank;
    set<uint32_t> allRegs;
    set<uint32_t> allSIMDRegs;
    getAllGPRegisters(allRegs);
    getAllSIMDRegisters(allSIMDRegs);
    map<int, int> simdFreq;

    // initialise
    for (auto reg : allRegs) {
        regFreq[reg] = 0;
    }

    for (auto bid : loop->body) {
        BasicBlock &bb = entry[bid];
        // we need to find out the nest level from the basic block w.r.t the
        // loop assume inner loop has an iteration count of 64
        int depth = 0;
        Loop *cloop = bb.parentLoop;
        while (cloop != loop) {
            depth++;
            cloop = cloop->parentLoop;
            if (cloop == NULL)
                break;
        }

        for (int i = 0; i < bb.size; i++) {
            Instruction &instr = bb.instrs[i];

            for (auto vi : instr.inputs) {
                if (vi->type == JVAR_REGISTER) {
                    if (jreg_is_gpr(vi->value))
                        regFreq[vi->value] += (64 * depth + 1);
                    else if (jreg_is_simd(vi->value))
                        simdFreq[vi->value] += (64 * depth + 1);
                } else if (vi->type == JVAR_MEMORY ||
                           vi->type == JVAR_POLYNOMIAL) {
                    for (auto vm : vi->pred) {
                        if (vm->type == JVAR_REGISTER && jreg_is_gpr(vm->value))
                            regFreq[vm->value] += (64 * depth + 1);
                    }
                }
            }
            for (auto vo : instr.outputs) {
                if (vo->type == JVAR_REGISTER) {
                    if (jreg_is_gpr(vo->value))
                        regFreq[vo->value] += (64 * depth + 1);
                    else if (jreg_is_gpr(vo->value))
                        simdFreq[vo->value] += (64 * depth + 1);
                } else if (vo->type == JVAR_MEMORY ||
                           vo->type == JVAR_POLYNOMIAL) {
                    for (auto vm : vo->pred)
                        if (vm->type == JVAR_REGISTER && jreg_is_gpr(vm->value))
                            regFreq[vm->value] += (64 * depth + 1);
                }
            }
        }
    }

    // rank the frequency of registers
    for (auto freq : regFreq) {
        regRank.insert(make_pair(freq.second, freq.first));
    }

    for (auto simd : allSIMDRegs) {
        if (simdFreq[simd] == 0)
            loop->freeSIMDRegs.insert(simd);
    }

    // we only pick the top four least used
    int picked = 0;
    LOOPLOG("\t\tRegister pressure: " << endl);
    for (auto reg : regRank) {
        LOOPLOG("\t\t\t" << get_reg_name(reg.second) << " " << dec << reg.first
                         << endl);
    }

    for (auto reg : regRank) {
        if (picked >= 4)
            break;

        // avoid picking induction variable registers
        if (loop->iteratorRegs.contains(reg.second))
            continue;

        loop->scratchRegs.insert(reg.second);
        loop->spillRegs.insert(reg.second);
        picked++;
    }

    LOOPLOG("\t\tFound four least-used registers: " << loop->scratchRegs
                                                    << endl);
    LOOPLOG("\t\tOccupied stack frame size: " << loop->parent->totalFrameSize
                                              << endl);
}

void variableAnalysis(janus::Function *function)
{
    for (auto &instr : function->instrs) {
        for (auto vi : instr.inputs) {
            if (vi->type == JVAR_REGISTER)
                instr.regReads.insert(vi->value);
            else if (vi->type == JVAR_MEMORY || vi->type == JVAR_POLYNOMIAL) {
                for (auto vm : vi->pred)
                    if (vm->type == JVAR_REGISTER)
                        instr.regReads.insert(vm->value);
            }
        }
        for (auto vo : instr.outputs) {
            if (vo->type == JVAR_REGISTER)
                instr.regWrites.insert(vo->value);
            else if (vo->type == JVAR_MEMORY || vo->type == JVAR_POLYNOMIAL) {
                for (auto vm : vo->pred)
                    if (vm->type == JVAR_REGISTER)
                        instr.regReads.insert(vm->value);
            }
        }
    }
}

void livenessAnalysis(Function *function)
{
    BasicBlock *entry = function->getCFG().entry;
    uint32_t numBlocks = function->getCFG().numBlocks;
    uint32_t numInstrs = function->instrs.size();

    if (function->liveRegIn != NULL || function->liveRegOut != NULL)
        return;

    /* Allocate register sets of outputs */
    RegSet *iLiveRegIn = new RegSet[numInstrs];
    RegSet *iLiveRegOut = new RegSet[numInstrs];

    /* Allocate temporary register set arrays */
    RegSet *regDefs = new RegSet[numBlocks];
    RegSet *regUses = new RegSet[numBlocks];
    RegSet *liveRegIn = new RegSet[numBlocks];
    RegSet *liveRegOut = new RegSet[numBlocks];

    RegSet returnSet;
    /* init */
    memset(regDefs, 0, numBlocks * sizeof(RegSet));
    memset(regUses, 0, numBlocks * sizeof(RegSet));
    memset(liveRegIn, 0, numBlocks * sizeof(RegSet));
    memset(liveRegOut, 0, numBlocks * sizeof(RegSet));

#ifdef JANUS_X86
    returnSet.insert(JREG_RAX);
#elif JANUS_AARCH64
    returnSet.insert(JREG_X0);
#endif

    /* Step 1: conclude the register USE DEF sets */
    for (int b = 0; b < numBlocks; b++) {
        BasicBlock &bb = entry[b];
        for (int i = 0; i < bb.size; i++) {
            Instruction &instr = bb.instrs[i];
            // skip invisible reads within the same block
            regUses[b].merge(instr.regReads - regDefs[b]);
            regDefs[b].merge(instr.regWrites);
        }
    }

    bool converge;

    /* Step 2: live analysis at basic block granularity */
    do {
        converge = true;

        for (int i = 0; i < numBlocks; i++) {
            // copy both register and stack in-out states
            RegSet regInOld = liveRegIn[i];
            RegSet regOutOld = liveRegOut[i];
            // take reference
            RegSet &in = liveRegIn[i];
            RegSet &out = liveRegOut[i];

            BasicBlock &bb = entry[i];
            if (bb.succ1)
                out.merge(liveRegIn[bb.succ1->bid]);
            if (bb.succ2)
                out.merge(liveRegIn[bb.succ2->bid]);
            if (bb.terminate)
                out.merge(returnSet);

            // for each node in CFG
            // in[B] = use[B] + out[B] - def[B]
            in = regUses[i] + (out - regDefs[i]);

            if (!(regInOld == in && regOutOld == out)) {
                converge = false;
            }
        }
        // until it converges
    } while (!converge);

    /* Step 3: conclude liveIn and liveOut for each instruction */
    for (int b = 0; b < numBlocks; b++) {
        BasicBlock &bb = entry[b];
        // get liveOut at the end of the block
        RegSet liveOut = liveRegOut[b];
        RegSet liveIn;
        for (int i = bb.size - 1; i >= 0; i--) {
            Instruction &instr = bb.instrs[i];

            iLiveRegOut[instr.id] = liveOut;
            liveIn = liveOut - instr.regWrites + instr.regReads;
            iLiveRegIn[instr.id] = liveIn;
            liveOut = liveIn;
        }
    }

    function->liveRegIn = iLiveRegIn;
    function->liveRegOut = iLiveRegOut;
}
