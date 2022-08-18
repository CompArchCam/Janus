#include "SSA.h"
#include "Arch.h"

#include <iostream>
#include <map>
#include <queue>
#include <utility>

using namespace std;
using namespace janus;

template <SSARequirement DomCFG>
SSAGraph<DomCFG>::SSAGraph(const DomCFG &domcfg) : DomCFG(domcfg)
{
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
    for (auto vs : DomCFG::func.allStates) {
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
        auto *vs = new VarState(var, DomCFG::entry, false);
        DomCFG::func.allStates.insert(vs);
        return vs;
    }

    // we currently assume all memory variables are different
    if (var.type == JVAR_MEMORY || var.type == JVAR_POLYNOMIAL) {
        auto *vs = new VarState(var, DomCFG::entry, false);
        DomCFG::func.allStates.insert(vs);
        linkMemoryNodes(var, vs, latestDefs);
        return vs;
    }

    if (var.type == JVAR_SHIFTEDCONST || var.type == JVAR_SHIFTEDREG) {
        auto *vs = new VarState(var, DomCFG::entry, false);
        DomCFG::func.allStates.insert(vs);
        linkShiftedNodes(var, vs, latestDefs);
        return vs;
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
        vs = new VarState(var, DomCFG::entry, false);
        DomCFG::func.allStates.insert(vs);
        latestDefs[var] = vs;
        DomCFG::func.inputStates[var] = vs;
    }
    return vs;
}

template <SSARequirement DomCFG>
void SSAGraph<DomCFG>::createSuccFromPred()
{
    for (VarState *vs : DomCFG::func.allStates) {
        for (VarState *vsPred : vs->pred) {
            vsPred->succ.insert(vs);
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
        auto *ivs = new VarState(ivar, DomCFG::entry, false);
        DomCFG::func.allStates.insert(ivs);
        vs->pred.insert(ivs);
    }
}

template <SSARequirement DomCFG>
void SSAGraph<DomCFG>::linkShiftedNodes(Variable var, VarState *vs,
                                        map<Variable, VarState *> &latestDefs)
{
    Variable immedShift;
    immedShift.type = JVAR_CONSTANT;
    immedShift.value = var.shift_value;
    auto *shiftvs = new VarState(immedShift, DomCFG::entry, false);
    DomCFG::func.allStates.insert(shiftvs);
    vs->pred.insert(shiftvs);

    if (var.type == JVAR_SHIFTEDCONST) {
        Variable immedVal;
        immedVal.type = JVAR_CONSTANT;
        immedVal.value = var.value;
        auto *immedvs = new VarState(immedVal, DomCFG::entry, false);
        DomCFG::func.allStates.insert(immedvs);
        vs->pred.insert(immedvs);
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
        VarState *vs = new VarState(var, bb, true);
        // record the variable state in the function's global state buffer.
        DomCFG::func.allStates.insert(vs);
        // update last state in this phi block
        if (!bb->lastStates.contains(var))
            bb->lastStates[var] = vs;
        // insert phi node
        bb->phiNodes.push_back(vs);
    }
}

// TODO: fix this by either moving the implementations to header
template class SSAGraph<
    PostDominanceAnalysis<DominanceAnalysis<ControlFlowGraph>>>;
