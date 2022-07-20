#include "SSA.h"
#include "Arch.h"
#include "BasicBlock.h"
#include "ControlFlow.h"
#include "Function.h"
#include "JanusContext.h"
#include "Operand.h"
#include "Utility.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <queue>
#include <utility>

using namespace std;
using namespace janus;

void linkSSANodes(Function &function);

static void insertPhiNodes(Function &function, Variable var);
static void updatePhiNodes(Function &function, BlockID bid,
                           map<Variable, VarState *> &previousDefs);

static void updateSSANodes(Function &function, BlockID bid,
                           map<Variable, VarState *> &latestDefs);

/* Look up the variable state from the latest definitions, if miss, create a new
 * variable state */
static VarState *getOrInitVarState(Variable var,
                                   map<Variable, VarState *> &latestDefs,
                                   Function &function);

static void linkMemoryNodes(Variable var, VarState *vs,
                            map<Variable, VarState *> &latestDefs,
                            Function &function);

static void linkShiftedNodes(Variable var, VarState *vs,
                             map<Variable, VarState *> &latestDefs,
                             Function &function);

static void linkDependentNodes(Function &function);

static void guessCallArgument(Instruction &instr,
                              map<Variable, VarState *> &latestDefs,
                              Function &function);

static void createSuccFromPred(Function &function);

void buildSSAGraph(Function &function)
{
    if (dbg) {
        cout << "BP 1" << endl;
    }
    /* Step 1: collect variable inputs and outputs for each basic block */
    for (auto &bb : function.getCFG().blocks) {
        bb.lastStates.clear();
        for (int i = 0; i < bb.size; i++) {
            Instruction &instr = bb.instrs[i];
            for (auto vs : instr.outputs) {
                bb.lastStates[*(Variable *)vs] = vs;
            }
        }
    }

    if (dbg) {
        cout << "BP 2" << endl;
    }
    /* Step 2: Insert Phi node */
    for (auto var : function.allVars)
        insertPhiNodes(function, var);

    if (dbg) {
        cout << "BP 3" << endl;
    }
    /* Step 3: Link all instruction's input to Phi and outputs */
    linkSSANodes(function);

    if (dbg) {
        cout << "BP 4" << endl;
    }
    /* Step 3.1: From the pred computed in the previous step, create the succ
     * for each varstate */
    createSuccFromPred(function);

    if (dbg) {
        cout << "BP 5" << endl;
    }

    /* Step 4: Assign an id/version for each variable state
     * Used for human readable SSA */
    int id = 0;
    map<Variable, int> versions;
    for (auto vs : function.allStates) {
        vs->id = id++;
        Variable var = (Variable)(*vs);
        int version = versions[var];
        vs->version = version;
        versions[var] = version + 1;
    }
    if (dbg) {
        cout << "BP 6" << endl;
    }

    /* Step 5: Link dependants and mark unused variables */
    linkDependentNodes(function);
    if (dbg) {
        cout << "BP 7" << endl;
    }
}

/* Copy all the variable definitions from block orig to dest */
static void copyDefinitions(map<Variable, VarState *> *previousDefs,
                            map<Variable, VarState *> *currentDefs)
{
    // copy
    for (auto &item : *previousDefs) {
        Variable var = item.first;
        VarState *vs = item.second;
        (*currentDefs)[var] = vs;
    }
}

void linkSSANodes(Function &function)
{
    // A record that keeps track of the latest definition of variable states
    map<Variable, VarState *> latestStates;
    // A distinct record for each basic block
    map<BlockID, map<Variable, VarState *> *> globalDefs;

    BasicBlock *entry = function.getCFG().entry;

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

        if (currentDefs == NULL) {
            currentDefs = new map<Variable, VarState *>();
            globalDefs[bid] = currentDefs;
        }

        // update Phi nodes for this block from previous block
        updatePhiNodes(function, bid, *previousDefs);

        // for already visited nodes, simply update phi nodes and return
        if (visited.find(bid) != visited.end())
            continue;

        // copy definition to the current block
        copyDefinitions(previousDefs, currentDefs);

        // update SSA nodes for this block
        updateSSANodes(function, bid, *currentDefs);

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
    linkArchSpecSSANodes(function, globalDefs);

    // clear global record
    for (auto record : globalDefs) {
        if (record.second)
            record.second->clear();
    }
}

static VarState *getOrInitVarState(Variable var,
                                   map<Variable, VarState *> &latestDefs,
                                   Function &function)
{
    // for constant immediate, there is no need to query, simply create a new
    // state
    if (var.type == JVAR_CONSTANT) {
        VarState *vs = new VarState(var, function.getCFG().entry, false);
        function.allStates.insert(vs);
        return vs;
    }

    // we currently assume all memory variables are different
    if (var.type == JVAR_MEMORY || var.type == JVAR_POLYNOMIAL) {
        VarState *vs = new VarState(var, function.getCFG().entry, false);
        function.allStates.insert(vs);
        linkMemoryNodes(var, vs, latestDefs, function);
        return vs;
    }

    if (var.type == JVAR_SHIFTEDCONST || var.type == JVAR_SHIFTEDREG) {
        VarState *vs = new VarState(var, function.getCFG().entry, false);
        function.allStates.insert(vs);
        linkShiftedNodes(var, vs, latestDefs, function);
        return vs;
    }
    // for the rest of variables, get latest def
    VarState *vs = latestDefs[var];
    if (!vs) {
        // uninitialized variable
        if (function.inputStates.find(var) != function.inputStates.end()) {
            latestDefs[var] = function.inputStates[var];
            return function.inputStates[var];
        }

        // not constructed yet
        vs = new VarState(var, function.getCFG().entry, false);
        function.allStates.insert(vs);
        latestDefs[var] = vs;
        function.inputStates[var] = vs;
    }
    return vs;
}

static void createSuccFromPred(Function &function)
{
    for (VarState *vs : function.allStates) {
        for (VarState *vsPred : vs->pred) {
            vsPred->succ.insert(vs);
        }
    }
}

static void updatePhiNodes(Function &function, BlockID bid,
                           map<Variable, VarState *> &previousDefs)
{
    BasicBlock &bb = function.getCFG().entry[bid];

    // Link phi node for this block
    for (auto phi : bb.phiNodes) {
        Variable var = (Variable)(*phi);
        VarState *vs = getOrInitVarState(var, previousDefs, function);
        // link phi node with previous definition of var
        // prevent self referencing
        if (phi != vs)
            phi->pred.insert(vs);
    }
}

static void updateSSANodes(Function &function, BlockID bid,
                           map<Variable, VarState *> &latestDefs)
{
    BasicBlock &bb = function.getCFG().entry[bid];
    vector<Variable> inputs;

    // Register phi nodes in the definitions
    for (auto phi : bb.phiNodes) {
        Variable var = (Variable)(*phi);
        latestDefs[var] = phi;
    }

    for (int i = 0; i < bb.size; i++) {
        Instruction &instr = bb.instrs[i];
        // update instruction inputs
        instr.getInputs(inputs);

        for (Variable var : inputs) {
            VarState *vs = getOrInitVarState(var, latestDefs, function);
            instr.inputs.push_back(vs);
            latestDefs[var] = vs;
        }
        inputs.clear();

        // update instruction output
        for (auto vs : instr.outputs) {
            Variable var = (Variable)(*vs);
            latestDefs[var] = vs;
            // for memory output, link the base, offset and disp too
            if (var.type == JVAR_MEMORY)
                linkMemoryNodes(var, vs, latestDefs, function);
        }

        // corner case, guess call argument
        if (instr.opcode == Instruction::Call) {
            guessCallArgument(instr, latestDefs, function);
            continue;
        }
    }
}

static void guessCallArgument(Instruction &instr,
                              map<Variable, VarState *> &latestDefs,
                              Function &function)
{
    vector<Variable> arguments;
    getInputCallConvention(arguments);

    // only link what is available in existing definition
    for (auto var : arguments) {
        VarState *vs = latestDefs[var];
        if (vs != NULL && (vs->lastModified != NULL || vs->pred.size() > 2)) {
            instr.inputs.push_back(vs);
        }
    }
}

static void linkMemoryNodes(Variable var, VarState *vs,
                            map<Variable, VarState *> &latestDefs,
                            Function &function)
{
    if ((int)var.base) {
        VarState *baseState = getOrInitVarState(Variable((uint32_t)var.base),
                                                latestDefs, function);
        vs->pred.insert(baseState);
    }
    if ((int)var.index) {
        VarState *indexState = getOrInitVarState(Variable((uint32_t)var.index),
                                                 latestDefs, function);
        vs->pred.insert(indexState);
    }
    if (var.value) {
        Variable ivar;
        ivar.type = JVAR_CONSTANT;
        ivar.value = var.value;
        VarState *ivs = new VarState(ivar, function.getCFG().entry, false);
        function.allStates.insert(ivs);
        vs->pred.insert(ivs);
    }
}

static void linkShiftedNodes(Variable var, VarState *vs,
                             map<Variable, VarState *> &latestDefs,
                             Function &function)
{
    Variable immedShift;
    immedShift.type = JVAR_CONSTANT;
    immedShift.value = var.shift_value;
    VarState *shiftvs =
        new VarState(immedShift, function.getCFG().entry, false);
    function.allStates.insert(shiftvs);
    vs->pred.insert(shiftvs);

    if (var.type == JVAR_SHIFTEDCONST) {
        Variable immedVal;
        immedVal.type = JVAR_CONSTANT;
        immedVal.value = var.value;
        VarState *immedvs =
            new VarState(immedVal, function.getCFG().entry, false);
        function.allStates.insert(immedvs);
        vs->pred.insert(immedvs);
    } else if (var.type == JVAR_SHIFTEDREG) {
        VarState *regState = getOrInitVarState(Variable((uint32_t)var.value),
                                               latestDefs, function);
        vs->pred.insert(regState);
    }
}

static void linkDependentNodes(Function &function)
{
    queue<VarState *> usedQ;
    set<VarState *> initInputs;
    set<Variable *> visitedState;
    for (auto &bb : function.getCFG().blocks) {
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

        if (visitedState.find(vs) != visitedState.end())
            continue;

        // mark the current state as used
        vs->notUsed = false;

        visitedState.insert(vs);

        // check its phi node
        for (auto phi : vs->pred) {
            if (visitedState.find(phi) == visitedState.end())
                usedQ.push(phi);
        }
    }
}

// Add all necessary phi nodes for the given variable by calculating the
// dominance frontier
void buildDominanceFrontierClosure(Function &function, Variable var,
                                   set<BasicBlock *> &bbs /*OUT*/,
                                   set<BasicBlock *> &phiblocks) /*OUT*/
{
    if (dbg) {
        cout << "\t DFC: FID = " << function.fid << endl;
    }
    if (dbg) {
        cout << "\t DFC: BP 1" << endl;
    }
    queue<BasicBlock *> q;

    if (dbg) {
        cout << "\t DFC: BP 2" << endl;
    }
    // Find all basic blocks that contain definitions of variable var
    for (auto &bb : function.getCFG().blocks) {
        if (bb.lastStates.find(var) != bb.lastStates.end()) {
            bbs.insert(&bb);
            q.push(&bb);
        }
    }
    if (dbg) {
        cout << "\t DFC: BP 3" << endl;
    }

    // BFS to generate transitive closure
    while (!q.empty()) {
        BasicBlock *bb = q.front();
        if (dbg) {
            cout << "\t\t DFC LOOP; BID = " << bb->bid << endl;
        }
        q.pop();
        if (dbg) {
            cout << "\t\t DFC LOOP; BP 1" << endl;
        }
        if (dbg) {
            cout << "\t\t DFC LOOP; bb->dominanceFrontier.size() = "
                 << bb->dominanceFrontier.size() << endl;
        }
        // insert phi node at the dominance frontier of the definition
        for (BasicBlock *cc : bb->dominanceFrontier) {
            if (dbg) {
                cout << "\t\t\t DFC INNER; BP 1" << endl;
            }
            phiblocks.insert(cc);
            if (dbg) {
                cout << "\t\t\t DFC INNER; BP 2" << endl;
            }
            if (bbs.find(cc) != bbs.end())
                continue;
            if (dbg) {
                cout << "\t\t\t DFC INNER; BP 3" << endl;
            }
            q.push(cc);
            if (dbg) {
                cout << "\t\t\t DFC INNER; BP 4" << endl;
            }
            bbs.insert(cc);
            if (dbg) {
                cout << "\t\t\t DFC INNER; BP 5" << endl;
            }
        }
        if (dbg) {
            cout << "\t\t DFC LOOP; BP 2" << endl;
        }
    }
    if (dbg) {
        cout << "\t DFC: BP 4" << endl;
    }
}

/* Insert phi node for each of the identified variable in function */
void insertPhiNodes(Function &function, Variable var)
{
    // skip memory and constant variables
    // memory variables are constructed later in the memory SSA graph
    if (var.type == JVAR_MEMORY || var.type == JVAR_POLYNOMIAL ||
        var.type == JVAR_CONSTANT || var.type == JVAR_UNKOWN)
        return;
    if (dbg) {
        cout << "\t PhiNodes: BP 1" << endl;
    }

    set<BasicBlock *> bbs;
    set<BasicBlock *> phiblocks;

    if (dbg) {
        cout << "\t PhiNodes: BP 2" << endl;
    }
    // step 1: build DF+ of the variable
    buildDominanceFrontierClosure(function, var, bbs, phiblocks);

    if (dbg) {
        cout << "\t PhiNodes: BP 3" << endl;
    }
    // step 2: insert phi nodes
    for (auto bb : phiblocks) {
        // create a variable state at each phi block for this variable
        VarState *vs = new VarState(var, bb, true);
        // record the variable state in the function's global state buffer.
        function.allStates.insert(vs);
        // update last state in this phi block
        if (bb->lastStates.find(var) == bb->lastStates.end())
            bb->lastStates[var] = vs;
        // insert phi node
        bb->phiNodes.push_back(vs);
    }
    if (dbg) {
        cout << "\t PhiNodes: BP 4" << endl;
    }
}
