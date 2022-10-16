/*! \file SSA.h
 *  \brief Single Static Assignment Related Analysis
 *
 *  This header contains the Single Static Assignment related construction and
 * analysis.
 */

#ifndef _Janus_SSA_
#define _Janus_SSA_

#include <concepts>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <queue>
#include <utility>

#include "Arch.h"
#include "Concepts.h"
#include "MachineInstruction.h"

template <typename T>
concept SSARequirement = requires
{
    requires ProvidesDominanceFrontier<T>;
    requires ProvidesBasicCFG<T>;
    requires ProvidesFunctionReference<T>;
    requires std::copy_constructible<T>;
};

template <SSARequirement DomCFG>
class SSAGraph : public DomCFG
{
  public:
    SSAGraph(const DomCFG &);

    std::set<std::unique_ptr<janus::VarState>> ssaVars;

  private:
    uint32_t count = 0;

    void initVariables();

    void initVariables(janus::Instruction &);

    void copyDefinitions(std::map<janus::Variable, janus::VarState *> *,
                         std::map<janus::Variable, janus::VarState *> *);

    void insertPhiNodes(janus::Variable var);

    void
    updatePhiNodes(BlockID bid,
                   std::map<janus::Variable, janus::VarState *> &previousDefs);

    void linkSSANodes();

    void
    updateSSANodes(BlockID bid,
                   std::map<janus::Variable, janus::VarState *> &latestDefs);

    auto
    getOrInitVarState(janus::Variable var,
                      std::map<janus::Variable, janus::VarState *> &latestDefs)
        -> janus::VarState *;

    void
    linkMemoryNodes(janus::Variable var, janus::VarState *vs,
                    std::map<janus::Variable, janus::VarState *> &latestDefs);

    void
    linkShiftedNodes(janus::Variable var, janus::VarState *vs,
                     std::map<janus::Variable, janus::VarState *> &latestDefs);

    void linkDependentNodes();

    void
    guessCallArgument(janus::Instruction &instr,
                      std::map<janus::Variable, janus::VarState *> &latestDefs);

    void createSuccFromPred();

    /** \brief builds the SSA graph of the given function.
     *
     * A call to this function is required in order that the SSA graph
     * consisting of *janus::MachineInstruction* and *janus::VariableState*
     * objects is linked. It is automatically called from
     * *janus::Function::translate*, which also makes sure prerequisites are
     * satisfied.
     */
    void buildSSAGraph();

    /** \brief Returns the phi node position of a given janus::Variable var.
     *  \param function Function for analysis
     *  \param var A variable for analysis
     *  \param[out] bbs Set of basic blocks that generates defination of this
     * variable \param[out] phiblocks Set of dominance frontier (blocks) of the
     * bbs.
     *
     *  It requires the inputs and outputs of each basic block is collected as
     * prerequisites.
     */
    void buildDominanceFrontierClosure(
        janus::Variable var, std::set<janus::BasicBlock *> &bbs /*OUT*/,
        std::set<janus::BasicBlock *> &phiblocks); /*OUT*/
};

/** \brief After analysis is done, this function fills the functions
 *         [equalgroup](@ref janus::Function::equalgroup) field.
 */
void saveEqualGroups(janus::Function *function);

// Implementation

using namespace std;
using namespace janus;

// Function declarations resolved elsewhere
void getInputCallConvention(std::vector<janus::Variable> &v);
void linkArchSpecSSANodes(
    janus::Function &function,
    std::map<BlockID, std::map<janus::Variable, janus::VarState *> *>
        &globalDefs);

template <SSARequirement DomCFG>
SSAGraph<DomCFG>::SSAGraph(const DomCFG &domcfg) : DomCFG(domcfg)
{
    // cout << "New SSA Constructor" << endl;
    count = 0;
    initVariables();
    buildSSAGraph();
}

template <SSARequirement DomCFG>
void SSAGraph<DomCFG>::buildSSAGraph()
{
    /* Step 1: collect variable inputs and outputs for each basic block */
    for (auto &bb : DomCFG::blocks) {
        bb.lastStates.clear();
        for (int i = 0; i < bb.size; i++) {
            Instruction &instr = bb.instrs[i];
            for (auto vs : instr.outputs) {
                bb.lastStates[*(Variable *)vs] = vs;
            }
        }
    }

    /* Step 2: Insert Phi node */
    for (auto var : DomCFG::func.allVars)
        insertPhiNodes(var);

    /* Step 3: Link all instruction's input to Phi and outputs */
    linkSSANodes();

    /* Step 3.1: From the pred computed in the previous step, create the succ
     * for each varstate
     */
    createSuccFromPred();

    /* Step 4: Assign an id/version for each variable state
     * Used for human-readable SSA */
    int id = 0;
    map<Variable, int> versions;
    for (auto &vs : ssaVars) {
        vs->id = id++;
        auto var = (Variable)(*vs);
        int version = versions[var];
        vs->version = version;
        versions[var] = version + 1;
    }

    /* Step 5: Link dependants and mark unused variables */
    linkDependentNodes();
}

/* Copy all the variable definitions from block orig to dest */
template <SSARequirement DomCFG>
void SSAGraph<DomCFG>::copyDefinitions(map<Variable, VarState *> *previousDefs,
                                       map<Variable, VarState *> *currentDefs)
{
    // copy
    for (auto &item : *previousDefs) {
        Variable var = item.first;
        VarState *vs = item.second;
        (*currentDefs)[var] = vs;
    }
}

template <SSARequirement DomCFG>
void SSAGraph<DomCFG>::linkSSANodes()
{
    // A record that keeps track of the latest definition of variable states
    map<Variable, VarState *> latestStates;
    // A distinct record for each basic block
    map<BlockID, map<Variable, VarState *> *> globalDefs;

    BasicBlock *entry = DomCFG::entry;

    // We use the order BFS to link all the inputs and outputs
    queue<pair<BlockID, BlockID>> q;
    set<BlockID> visited;

    // start with the entry
    q.push(make_pair(0, 0));
    globalDefs[0] = &latestStates;

    while (!q.empty()) {
        auto pair = q.front();
        // previous block
        BlockID pbid = pair.first;
        // current block
        BlockID bid = pair.second;
        BasicBlock &bb = entry[bid];

        q.pop();

        // get previous and current definitions
        auto previousDefs = globalDefs[pbid];
        auto currentDefs = globalDefs[bid];

        if (currentDefs == nullptr) {
            currentDefs = new map<Variable, VarState *>();
            globalDefs[bid] = currentDefs;
        }

        // update Phi nodes for this block from previous block
        updatePhiNodes(bid, *previousDefs);

        // for already visited nodes, simply update phi nodes and return
        if (visited.contains(bid))
            continue;

        // copy definition to the current block
        copyDefinitions(previousDefs, currentDefs);

        // update SSA nodes for this block
        updateSSANodes(bid, *currentDefs);

        // mark the block as visited
        visited.insert(bid);

        if (bb.succ1) {
            BlockID sbid = bb.succ1->bid;
            q.push(make_pair(bid, sbid));
        }

        if (bb.succ2) {
            BlockID sbid = bb.succ2->bid;
            q.push(make_pair(bid, sbid));
        }
    }

    // after the initial definitions linked, additional modifications on the SSA
    // graph might be required the additional linking could be architecture
    // specific
    linkArchSpecSSANodes(DomCFG::func, globalDefs);

    // clear global record
    for (auto record : globalDefs) {
        if (record.second)
            record.second->clear();
    }
}

template <SSARequirement DomCFG>
VarState *
SSAGraph<DomCFG>::getOrInitVarState(Variable var,
                                    map<Variable, VarState *> &latestDefs)
{
    // for constant immediate, there is no need to query, simply create a new
    // state
    if (var.type == JVAR_CONSTANT) {
        std::unique_ptr<VarState> vs =
            make_unique<VarState>(var, DomCFG::entry, false);
        auto res = vs.get();
        ssaVars.insert(std::move(vs));
        return res;
    }

    // we currently assume all memory variables are different
    if (var.type == JVAR_MEMORY || var.type == JVAR_POLYNOMIAL) {
        std::unique_ptr<VarState> vs =
            make_unique<VarState>(var, DomCFG::entry, false);
        auto res = vs.get();
        ssaVars.insert(std::move(vs));
        linkMemoryNodes(var, res, latestDefs);
        return res;
    }

    if (var.type == JVAR_SHIFTEDCONST || var.type == JVAR_SHIFTEDREG) {
        std::unique_ptr<VarState> vs =
            make_unique<VarState>(var, DomCFG::entry, false);
        auto res = vs.get();
        ssaVars.insert(std::move(vs));
        linkShiftedNodes(var, res, latestDefs);
        return res;
    }
    // for the rest of variables, get latest def
    VarState *vs = latestDefs[var];
    if (!vs) {
        // uninitialized variable
        if (DomCFG::func.inputStates.contains(var)) {
            latestDefs[var] = DomCFG::func.inputStates[var];
            return DomCFG::func.inputStates[var];
        }

        // not constructed yet
        std::unique_ptr<VarState> vs2 =
            std::make_unique<VarState>(var, DomCFG::entry, false);
        vs = vs2.get();
        ssaVars.insert(std::move(vs2));
        latestDefs[var] = vs;
        DomCFG::func.inputStates[var] = vs;
    }
    return vs;
}

template <SSARequirement DomCFG>
void SSAGraph<DomCFG>::createSuccFromPred()
{
    for (auto &vs : ssaVars) {
        for (VarState *vsPred : vs->pred) {
            vsPred->succ.insert(vs.get());
        }
    }
}

template <SSARequirement DomCFG>
void SSAGraph<DomCFG>::updatePhiNodes(BlockID bid,
                                      map<Variable, VarState *> &previousDefs)
{
    BasicBlock &bb = DomCFG::entry[bid];

    // Link phi node for this block
    for (auto phi : bb.phiNodes) {
        auto var = (Variable)(*phi);
        VarState *vs = getOrInitVarState(var, previousDefs);
        // link phi node with previous definition of var
        // prevent self referencing
        if (phi != vs)
            phi->pred.insert(vs);
    }
}

template <SSARequirement DomCFG>
void SSAGraph<DomCFG>::updateSSANodes(BlockID bid,
                                      map<Variable, VarState *> &latestDefs)
{
    BasicBlock &bb = DomCFG::entry[bid];
    vector<Variable> inputs;

    // Register phi nodes in the definitions
    for (auto phi : bb.phiNodes) {
        auto var = (Variable)(*phi);
        latestDefs[var] = phi;
    }

    for (int i = 0; i < bb.size; i++) {
        Instruction &instr = bb.instrs[i];
        // update instruction inputs
        instr.getInputs(inputs);

        for (Variable var : inputs) {
            VarState *vs = getOrInitVarState(var, latestDefs);
            instr.inputs.push_back(vs);
            latestDefs[var] = vs;
        }
        inputs.clear();

        // update instruction output
        for (auto vs : instr.outputs) {
            auto var = (Variable)(*vs);
            latestDefs[var] = vs;
            // for memory output, link the base, offset and disp too
            if (var.type == JVAR_MEMORY)
                linkMemoryNodes(var, vs, latestDefs);
        }

        // corner case, guess call argument
        if (instr.opcode == Instruction::Call) {
            guessCallArgument(instr, latestDefs);
            continue;
        }
    }
}

template <SSARequirement DomCFG>
void SSAGraph<DomCFG>::guessCallArgument(Instruction &instr,
                                         map<Variable, VarState *> &latestDefs)
{
    vector<Variable> arguments;
    getInputCallConvention(arguments);

    // only link what is available in existing definition
    for (auto var : arguments) {
        VarState *vs = latestDefs[var];
        if (vs != nullptr &&
            (vs->lastModified != nullptr || vs->pred.size() > 2)) {
            instr.inputs.push_back(vs);
        }
    }
}

template <SSARequirement DomCFG>
void SSAGraph<DomCFG>::linkMemoryNodes(Variable var, VarState *vs,
                                       map<Variable, VarState *> &latestDefs)
{
    if ((int)var.base) {
        VarState *baseState =
            getOrInitVarState(Variable((uint32_t)var.base), latestDefs);
        vs->pred.insert(baseState);
    }
    if ((int)var.index) {
        VarState *indexState =
            getOrInitVarState(Variable((uint32_t)var.index), latestDefs);
        vs->pred.insert(indexState);
    }
    if (var.value) {
        Variable ivar;
        ivar.type = JVAR_CONSTANT;
        ivar.value = var.value;
        std::unique_ptr<VarState> ivs =
            std::make_unique<VarState>(ivar, DomCFG::entry, false);
        vs->pred.insert(ivs.get());
        ssaVars.insert(std::move(ivs));
    }
}

template <SSARequirement DomCFG>
void SSAGraph<DomCFG>::linkShiftedNodes(Variable var, VarState *vs,
                                        map<Variable, VarState *> &latestDefs)
{
    Variable immedShift;
    immedShift.type = JVAR_CONSTANT;
    immedShift.value = var.shift_value;
    std::unique_ptr<VarState> shiftvs =
        make_unique<VarState>(immedShift, DomCFG::entry, false);
    vs->pred.insert(shiftvs.get());
    ssaVars.insert(std::move(shiftvs));

    if (var.type == JVAR_SHIFTEDCONST) {
        Variable immedVal;
        immedVal.type = JVAR_CONSTANT;
        immedVal.value = var.value;
        std::unique_ptr<VarState> immedvs =
            make_unique<VarState>(immedVal, DomCFG::entry, false);
        vs->pred.insert(immedvs.get());
        ssaVars.insert(std::move(immedvs));
    } else if (var.type == JVAR_SHIFTEDREG) {
        VarState *regState =
            getOrInitVarState(Variable((uint32_t)var.value), latestDefs);
        vs->pred.insert(regState);
    }
}

template <SSARequirement DomCFG>
void SSAGraph<DomCFG>::linkDependentNodes()
{
    queue<VarState *> usedQ;
    set<VarState *> initInputs;
    set<Variable *> visitedState;
    for (auto &bb : DomCFG::blocks) {
        for (int i = 0; i < bb.size; i++) {
            Instruction &instr = bb.instrs[i];
            for (auto vs : instr.inputs) {
                initInputs.insert(vs);
                // link dependents
                if (vs->type == JVAR_POLYNOMIAL || vs->type == JVAR_MEMORY) {
                    for (auto vi : vs->pred)
                        vi->dependants.insert(&instr);
                }
                vs->dependants.insert(&instr);
            }
            // mark all memory output as used
            for (auto vs : instr.outputs) {
                if (vs->type == JVAR_MEMORY) {
                    for (auto vi : vs->pred) {
                        vs->dependants.insert(&instr);
                        initInputs.insert(vs);
                    }
                }
            }
        }
    }

    // push the inputs into used queue, these are the used states from
    // instructions
    for (auto n : initInputs)
        usedQ.push(n);

    // mark all traversed nodes as "used", the untravesed node are unused
    while (!usedQ.empty()) {
        VarState *vs = usedQ.front();
        usedQ.pop();

        if (visitedState.contains(vs))
            continue;

        // mark the current state as used
        vs->notUsed = false;

        visitedState.insert(vs);

        // check its phi node
        for (auto phi : vs->pred) {
            if (!visitedState.contains(phi))
                usedQ.push(phi);
        }
    }
}

// Add all necessary phi nodes for the given variable by calculating the
// dominance frontier
template <SSARequirement DomCFG>
void SSAGraph<DomCFG>::buildDominanceFrontierClosure(
    Variable var, set<BasicBlock *> &bbs, set<BasicBlock *> &phiblocks)
{
    queue<BasicBlock *> q;

    // Find all basic blocks that contain definitions of variable var
    for (auto &bb : DomCFG::func.getCFG().blocks) {
        if (bb.lastStates.contains(var)) {
            bbs.insert(&bb);
            q.push(&bb);
        }
    }

    // BFS to generate transitive closure
    while (!q.empty()) {
        BasicBlock *bb = q.front();
        q.pop();
        // insert phi node at the dominance frontier of the definition
        for (BasicBlock *cc : DomCFG::dominanceFrontiers[bb]) {
            phiblocks.insert(cc);
            if (bbs.contains(cc))
                continue;
            q.push(cc);
            bbs.insert(cc);
        }
    }
}

template <SSARequirement DomCFG>
/* Insert phi node for each of the identified variable in function */
void SSAGraph<DomCFG>::insertPhiNodes(Variable var)
{
    // skip memory and constant variables
    // memory variables are constructed later in the memory SSA graph
    if (var.type == JVAR_MEMORY || var.type == JVAR_POLYNOMIAL ||
        var.type == JVAR_CONSTANT || var.type == JVAR_UNKOWN)
        return;

    set<BasicBlock *> bbs;
    set<BasicBlock *> phiblocks;

    // step 1: build DF+ of the variable
    buildDominanceFrontierClosure(var, bbs, phiblocks);

    // step 2: insert phi nodes
    for (auto bb : phiblocks) {
        // create a variable state at each phi block for this variable
        std::unique_ptr<VarState> vs = make_unique<VarState>(var, bb, true);
        // update last state in this phi block
        if (!bb->lastStates.contains(var))
            bb->lastStates[var] = vs.get();
        // insert phi node
        bb->phiNodes.push_back(vs.get());
        // record the variable state in the function's global state buffer.
        ssaVars.insert(std::move(vs));
    }
}

#include "janus_arch.h"

template <SSARequirement DomCFG>
void SSAGraph<DomCFG>::initVariables()
{
    // lift individual instruction
    int cnt = 0;
    for (auto &instr : DomCFG::func.instrs) {
        cnt++;
        initVariables(instr);
    }
    count += cnt;
}

#ifdef JANUS_X86
/* Generate output states for a given instruction */
template <SSARequirement DomCFG>
void SSAGraph<DomCFG>::initVariables(janus::Instruction &instr)
{
    MachineInstruction *minstr = instr.minstr;
    BasicBlock *block = instr.block;
    if (!minstr || !block)
        return;

    if (minstr->fineType == INSN_NOP)
        return;

    for (int i = 0; i < minstr->opndCount; i++) {
        Variable var = minstr->operands[i].lift(instr.pc + instr.minstr->size);
        // record this variable
        DomCFG::func.allVars.insert(var);
        // add outputs to the instruction
        if (minstr->operands[i].access == OPND_WRITE ||
            minstr->operands[i].access == OPND_READ_WRITE) {
            std::unique_ptr<VarState> vs =
                std::make_unique<VarState>(var, block, &instr);
            instr.outputs.push_back(vs.get());
            ssaVars.insert(std::move(vs));
        }
    }

    // if the instruction modifies eflag, add to the output too
    if (minstr->writeControlFlag()) {
        Variable var((uint32_t)0);
        var.type = JVAR_CONTROLFLAG;
        DomCFG::func.allVars.insert(var);
        std::unique_ptr<VarState> vs =
            std::make_unique<VarState>(var, block, &instr);
        instr.outputs.push_back(vs.get());
        ssaVars.insert(std::move(vs));
    }

    // imul
    if ((minstr->opcode == X86_INS_IMUL || minstr->opcode == X86_INS_IDIV ||
         minstr->opcode == X86_INS_MUL || minstr->opcode == X86_INS_DIV) &&
        minstr->opndCount == 1) {
        // implicit EAX:EDX output
        Variable var((uint32_t)JREG_RAX);
        std::unique_ptr<VarState> vs =
            std::make_unique<VarState>(var, block, &instr);
        instr.outputs.push_back(vs.get());
        ssaVars.insert(std::move(vs));
        Variable var2((uint32_t)JREG_RDX);
        std::unique_ptr<VarState> vs2 =
            std::make_unique<VarState>(var2, block, &instr);
        instr.outputs.push_back(vs2.get());
        ssaVars.insert(std::move(vs2));
    }

    // call
    else if (minstr->opcode == X86_INS_CALL) {
        // force the calling function to generate RAX output
        Variable var((uint32_t)JREG_RAX);
        std::unique_ptr<VarState> vs =
            make_unique<VarState>(var, block, &instr);
        instr.outputs.push_back(vs.get());
        ssaVars.insert(std::move(vs));
        Variable var2((uint32_t)JREG_XMM0);
        std::unique_ptr<VarState> vs2 =
            std::make_unique<VarState>(var2, block, &instr);
        instr.outputs.push_back(vs2.get());
        ssaVars.insert(std::move(vs2));
    }

    // push or pop
    else if (minstr->opcode == X86_INS_PUSH || minstr->opcode == X86_INS_POP) {
        Variable var((uint32_t)JREG_RSP);
        std::unique_ptr<VarState> vs =
            make_unique<VarState>(var, block, &instr);
        instr.outputs.push_back(vs.get());
        ssaVars.insert(std::move(vs));
        if (minstr->opcode == X86_INS_POP) {
            for (int i = 0; i < minstr->opndCount; i++) {
                Variable var =
                    minstr->operands[i].lift(instr.pc + instr.minstr->pc);
                vs = std::make_unique<VarState>(var, block, &instr);
                instr.outputs.push_back(vs.get());
                ssaVars.insert(std::move(vs));
            }
        }
    }

    else if (minstr->opcode == X86_INS_CDQ || minstr->opcode == X86_INS_CDQE) {
        Variable var((uint32_t)JREG_RAX);
        std::unique_ptr<VarState> vs =
            std::make_unique<VarState>(var, block, &instr);
        instr.outputs.push_back(vs.get());
        ssaVars.insert(std::move(vs));
    }

    else if (minstr->opcode == X86_INS_MOVDQU ||
             minstr->opcode == X86_INS_MOVDQA) {
        Variable var = minstr->operands[0].lift(instr.pc + instr.minstr->pc);
        minstr->operands[0].access = OPND_WRITE;
        minstr->operands[1].access = OPND_READ;
        std::unique_ptr<VarState> vs =
            std::make_unique<VarState>(var, block, &instr);
        instr.outputs.push_back(vs.get());
        ssaVars.insert(std::move(vs));
    }
}

#endif

#endif
