#include "OptRule.h"
#include "Analysis.h"
#include "IO.h"
#include "JanusContext.h"
#include "OPT_rule_structs.h"
#include "UpdateSequence.h"
#include <cassert>
#include <iostream>
#include <queue>
#include <sstream>

//"Architecture-dependent" constant -- we want to prefetch variables this many
//load instructions
//    before they are actually needed
#define PREFETCH_CONSTANT 64

using namespace std;
using namespace janus;

#define PREFETCH_ERROR_COUNT 8

enum PREFETCH_ERRORS {
    PREFETCH_ERROR_BASE_NOT_FOUND,
    PREFETCH_ERROR_MEM_VAR_NOT_ALIVE,
    PREFETCH_ERROR_PHI_NODE_CROSSED,
    PREFETCH_ERROR_FIXED_OPND_NOT_FOUND,
    PREFETCH_ERROR_SPILL_TO_SPILL,
    PREFETCH_ERROR_NO_SLOT,
    PREFETCH_ERROR_NO_REGISTER,
    PREFETCH_ERROR_OPND_COUNT
};

const static char *PREFETCH_ERROR_NAMES[PREFETCH_ERROR_COUNT] = {
    "Base register is not live at the point of usage!",
    "Stack or absolute memory variable is not alive at the prefetch address!",
    "PHI node found during evaluation!",
    "No definition found for fixed register operand!",
    "Copying from spill slot to spill slot!",
    "No slot found!",
    "Could not find free register for instruction!",
    "Too many operands (>4)!"};
static int PREFETCH_ERROR_COUNTERS[PREFETCH_ERROR_COUNT] = {0, 0, 0, 0,
                                                            0, 0, 0};

struct PrefetchFailed {
};

template <typename X, typename A>
inline void Assert(A assertion, PREFETCH_ERRORS pe)
{
    if (!assertion) {
        int x = pe - PREFETCH_ERROR_BASE_NOT_FOUND;
        assert((x >= 0) && (x < PREFETCH_ERROR_COUNT));
        PREFETCH_ERROR_COUNTERS[x]++;
        cerr << "ERROR: " << PREFETCH_ERROR_NAMES[x] << endl;
        throw X();
    }
}

template <typename X, typename A> inline void Assert(A assertion)
{
    if (!assertion)
        throw X();
}

template <typename X, typename A> inline void Assert(A assertion, const char *s)
{
    if (!assertion) {
        cerr << "ERROR: " << s << endl;
        throw X();
    }
}

// Updates the outer limit given an additional limit (cLimit) and the current
// loop (cLoop)
inline static void updateOuterLimit(Loop *&outerLimit, Loop *cLimit,
                                    Loop *cLoop)
{
    cLimit = cLoop->lowestCommonAncestor(cLimit);
    if (!outerLimit) {
        // cLimit is the first outer limit
        outerLimit = cLimit;
    } else {
        // use the innermost one of the two
        if (outerLimit->isAncestorOf(cLimit))
            outerLimit = cLimit;
    }
}

Loop *findPrefetch(VariableState *vs, map<VariableState *, Loop *> &store)
{
    if (store.find(vs) != store.end())
        return store[vs];

    if (vs->inductionLoop) {
        // vs is an induction variable, we are done
        return store[vs] = vs->inductionLoop;
    }

    if (vs->type() == VARSTATE_PHI) {
        // non-induction phi node
        return store[vs] = NULL;
    }
    if (!vs->block->parentLoop) {
        // not defined in a loop
        return store[vs] = NULL;
    }

    if (vs->type() == VARSTATE_MEMORY_OPND) {
        // load
        if (!vs->phi.size()) {
            // from constant location
            return store[vs] = NULL;
        }

        // otherwise its prefetch is the same as the prefetch of its base
        // register
        store[vs] = findPrefetch(vs->phi[0], store);

        // If the prefetch would happen in a block not connected to this one,
        // then the value is constant here
        if (!vs->block->parentLoop->isDescendantOf(store[vs]))
            store[vs] = NULL;

        // for base-index form, check if base is constant -- IMPROVEMENT: if not
        // compute base if possible
        if ((vs->phi.size() > 1) && (store[vs]) &&
            (!store[vs]->isConstant(vs->phi[1])))
            store[vs] = NULL;

        return store[vs];
    }

    if (vs->type() == VARSTATE_INITIAL) {
        // initial value, which is always constant
        return NULL;
    }

    // Otherwise we have a normal non-induction variable defined in a loop
    auto *mi = vs->lastModified;

    // IMPROVEMENT: analyze and allow side-effect free function calls
    if (mi->fineType == INSN_CALL) {
        // Don't make any function calls
        return store[vs] = NULL;
    }

    Loop *outerLimit = NULL; // we have to choose a loop nested inside
                             // outerLimit
    Loop *loop = NULL;

    for (auto *vsi : mi->inputs) {
        if (!findPrefetch(vsi, store)) {
            // vsi does not depend on an induction variable. We must prefetch in
            // a loop where
            //     it is constant
            if (vsi->constLoop) {
                updateOuterLimit(outerLimit, vsi->constLoop->parentLoop,
                                 mi->block->parentLoop);
            } else
                updateOuterLimit(outerLimit, vsi->block->parentLoop,
                                 mi->block->parentLoop);
            continue;
        }
        Loop *l = store[vsi];

        // if it depends on an induction variable of a loop with no (ancestor)
        // relation to parentLoop, then that value comes from the last iteration
        // and hence it is constant iff we choose a loop within the LCA loop,
        // otherwise it is a non-induction non-constant variable. Hence we
        // should count it as a NULL, but note that we have to choose some loop
        // within l->LCAwith(vs->block->parentLoop)
        if (!l->isAncestorOf(vs->block->parentLoop)) {
            updateOuterLimit(outerLimit, l, mi->block->parentLoop);
            continue;
        }

        // if l is closer to vs->block than loop, choose l instead
        if (l->isDescendantOf(loop))
            loop = l;
    }

    // At this point *loop* contains:
    //    - NULL if there is no loop in which vs depends on induction variables
    //    - otherwise the innermost loop in which it does
    // And *outerLimit* points to a loop, such that we can precompute values
    // only in loops
    //     strictly nested inside it (everything is strictly nested inside NULL)
    // Hence we can prefetch iff *loop* is not NULL and it is strictly nested
    // inside *outerLimit*
    if ((!loop) || (loop == outerLimit) || (!loop->isDescendantOf(outerLimit)))
        return store[vs] = NULL; // No suitable loop found

    // we can use the constants and induction variables of loop to prefetch
    return store[vs] = loop;
}

/** Returns the length of (number of prefetchable loads on) the longest path
 * from the given load
 */
static int countDependantLoads(VariableState *vs,
                               map<VariableState *, Loop *> &prefLoads,
                               Loop *loop, map<VariableState *, int> &store)
{
    if (!loop->contains(vs->block->bid))
        return 0;

    if (store.find(vs) != store.end())
        return store[vs];
    store[vs] = 0;

    for (auto *mi : vs->dependants) {
        for (auto *vso : mi->outputs) {
            store[vs] = max(store[vs],
                            countDependantLoads(vso, prefLoads, loop, store));
        }
    }

    for (auto *vso : vs->linksToPhi) {
        if (vso->type() != VARSTATE_MEMORY_OPND)
            continue;
        store[vs] =
            max(store[vs], countDependantLoads(vso, prefLoads, loop, store));
    }

    if (prefLoads.find(vs) != prefLoads.end())
        store[vs]++;

    return store[vs];
}

inline static int offset(VariableState *vs, int t,
                         map<VariableState *, int> &nDL)
{
    return (((double)(nDL[vs])) / t) * PREFETCH_CONSTANT;
}

// Copy to a new variable if it is possibly needed later, otherwise simply
// define
// TODO at the moment copies every time -- room for IMPROVEMENT
static int copyIfNeeded(BasicBlock *bb, int index, VariableState *vs,
                        vector<new_instr *> &gen, int &varid)
{
    new_instr *ni = new new_instr(bb->instrs[index].pc);
    ni->def[varid] = vs->getVar();
    gen.push_back(ni);

    ni = new new_instr(X86_INS_MOV, bb->instrs[index].pc);
    ni->push_var_opnd(varid + 1);
    Operand opnd(vs->getVar());
    ni->push_opnd(varid, opnd);
    ni->inputs.push_back(varid);
    ni->outputs.push_back(varid + 1);
    gen.push_back(ni);

    varid += 2;
    return varid - 1;
}

// puts computation in gen, which puts the requested induction variable into the
// variable ID that is returned
static int getInductionVar(BasicBlock *bb, int index, VariableState *vs,
                           vector<new_instr *> &gen, int &varid)
{
    assert(vs->inductionLoop);
    assert(vs->repInductionVar);

    if (bb->alive(vs->getVar(), index) == vs) {
        // vs is alive, use it
        return copyIfNeeded(bb, index, vs, gen, varid);
    }

    for (auto *v : vs->inductionLoop->inductionVars) {
        if (bb->alive(v->getVar(), index) != v)
            continue;
        if (v->repInductionVar == vs->repInductionVar) {
            // equivalent induction variable found

            // Define (and possibly make a copy of) v
            int defvarid = copyIfNeeded(bb, index, v, gen, varid);

            // Calculate vs from v
            UpdateSequence us = (*vs->constDifference) -
                                (*v->constDifference); // update from v to vs
            return us.compute(bb, index, defvarid, gen, varid);
        }
    }

    assert("No matching induction variable found" ==
           "which should never happen");
}

// Puts computation in gen, which stores the value of the requested variable in
// offset number of iterations in the future into a variable, and returns the ID
// of the result
static int getFutureInductionVar(BasicBlock *bb, int index, VariableState *vs,
                                 int offset, vector<new_instr *> &gen,
                                 int &varid)
{
    // compute current value
    int current = getInductionVar(bb, index, vs, gen, varid);

    pair<Operand, int> upd =
        vs->update->computeNtimes(bb, index, offset, gen, varid);

    // now update contains the sum of the updates over the next offset
    // iterations. Add it to the current value to obtain future value
    new_instr *ni = new new_instr(X86_INS_ADD, bb->instrs[index].pc);
    ni->push_var_opnd(current);
    ni->push_opnd(upd.second, upd.first);
    if (upd.second >= 0)
        ni->inputs.push_back(upd.second);
    ni->outputs.push_back(current);
    gen.push_back(ni);

    // return variable ID of future value
    return current;
}

// evaluate the state vs, offset iterations into the future and insert
// computation at the given
//                                                                                         position
// vs must solely depend on induction variables and constants in loop and not
// cross any phi nodes the variable ID of the result is returned
int evaluate(BasicBlock *bb, int index, VariableState *vs, Loop *loop,
             int offset, vector<new_instr *> &gen, int &varid)
{
    if (vs->isConstant) {
        return -1;
    } else if (vs->inductionLoop == loop) {
        // induction variable found, precalculate
        return getFutureInductionVar(bb, index, vs, offset, gen, varid);
    } else if (loop->isConstant(vs)) {
        // constant variable found, try to find equivalent
        VariableState *v = bb->findEqualConstant(vs, index);
        if (v)
            return copyIfNeeded(bb, index, v, gen,
                                varid); // equivalent variable found, return

        // failed to find equivalent, compute as normal

        // if phi node, then the definition that is in this loop must be trivial
        if (vs->type() == VARSTATE_PHI) {
            for (auto *v : vs->phi) {
                if (loop->body.find(v->block->bid) == loop->body.end())
                    continue;
                if (v->type() == VARSTATE_PHI)
                    continue;

                return evaluate(bb, index, v, loop, offset, gen, varid);
            }
        }
    }

    if (vs->type() == VARSTATE_MEMORY_OPND) {
        // IMPROVEMENT: possibly optimise the case where the update is constant
        // by making use of LEA or just [base+offset] form arguments (by putting
        // the constant update as an immediate value in offset rather than
        // generate a computation for it).

        int ret = evaluate(bb, index, vs->phi[0], loop, offset, gen, varid);

        if (vs->phi.size() == 2) {
            VariableState *basevs = bb->findEqualConstant(vs->phi[1], index);
            Assert<PrefetchFailed>(basevs, PREFETCH_ERROR_BASE_NOT_FOUND);

            if (basevs != vs->phi[1]) {
                new_instr *def = new new_instr(bb->instrs[index].pc);
                def->def[varid] = basevs->getVar();

                new_instr *ni =
                    new new_instr(X86_INS_MOV, bb->instrs[index].pc);
                ni->fixedRegisters = true;

                ni->outputs.push_back(varid + 1);
                Operand opnd = Operand(vs->phi[1]->getVar());
                opnd.access = OPND_WRITE;
                ni->push_opnd(varid + 1, opnd);
                ni->def[varid + 1] = vs->phi[1]->getVar();

                ni->inputs.push_back(varid);
                opnd = Operand(basevs->getVar());
                opnd.access = OPND_READ;
                ni->push_opnd(varid, opnd);
                ni->def[varid] = basevs->getVar();

                varid += 2;

                gen.push_back(def);
                gen.push_back(ni);
            }
        }

        return ret;
    } else {
        // non-induction non-constant normal variable
        Assert<PrefetchFailed>(vs->type() == VARSTATE_NORMAL,
                               PREFETCH_ERROR_PHI_NODE_CROSSED);

        // Do the same computation as the defining machine instruction
        MachineInstruction *mi = vs->lastModified;
        new_instr *ni = new new_instr(mi, bb->instrs[index].pc);

        int ret(-1);
        if (mi->opcode == X86_INS_LEA) {
            // for LEA, the input is linked as the base register, rather than a
            // memory operand, since this represents logically correctly what
            // the instruction does. here we need to handle it separately. LEA
            // always has two operands in X86, the first one being the output

            VariableState *vsi = NULL;
            Variable var(
                (uint32_t)mi->operands[1].compress(mi->pc).getRegister());
            for (auto *vss : mi->inputs)
                if (vss->getVar() == var)
                    vsi = vss;

            assert(vsi != NULL);

            int base = evaluate(bb, index, vsi, loop, offset, gen, varid);

            // pass output
            ni->push_opnd(varid, mi->operands[0]);
            ni->outputs.push_back(varid);
            ret = varid;
            varid++;

            // pass input
            ni->push_opnd(base, mi->operands[1]);
            ni->inputs.push_back(base);

        } else if ((mi->isMOV()) && (mi->operands[0].type == OPND_REG) &&
                   (mi->operands[1].type == OPND_REG)) {
            // Register copying -- we omit this, since we will have our own
            // register allocations
            return evaluate(bb, index, mi->inputs[0], loop, offset, gen, varid);
        } else if (ni->fixedRegisters) {
            // fixed register locations -- include the location of all inputs
            // and outputs
            map<Variable, int> vars;
            for (auto *v : mi->inputs) {
                // compute inputs
                if ((v->type() == VARSTATE_STK_VAR) ||
                    (v->type() == VARSTATE_RIP_VAR)) {
                    Assert<PrefetchFailed>(bb->alive(v->getVar(), index) == v,
                                           PREFETCH_ERROR_MEM_VAR_NOT_ALIVE);
                    vars[v->getVar()] = -1;
                } else {
                    int id = evaluate(bb, index, v, loop, offset, gen, varid);
                    vars[v->getVar()] = id;
                    if (id == -1)
                        continue;
                    ni->inputs.push_back(id);
                    ni->def[id] = v->getVar();
                }
            }
            for (auto *v : mi->outputs) {
                if (v->getVar().isEFLAGS())
                    continue; // don't count EFLAGS register
                if (vars.find(v->getVar()) == vars.end()) {
                    // if variable is only output, assign id now
                    vars[v->getVar()] = varid;
                    ni->def[varid] = v->getVar();
                    varid++;
                }

                ni->outputs.push_back(vars[v->getVar()]); // mark as output
            }

            for (int i = 0; i < mi->opndCount; i++) {
                Assert<PrefetchFailed>(
                    vars.find(mi->operands[i].compress(mi->pc)) != vars.end(),
                    PREFETCH_ERROR_FIXED_OPND_NOT_FOUND);
                ni->push_opnd(vars[mi->operands[i].compress(mi->pc)],
                              mi->operands[i]);
            }

            Assert<PrefetchFailed>(vars.find(vs->getVar()) != vars.end(),
                                   PREFETCH_ERROR_FIXED_OPND_NOT_FOUND);
            ret = vars[vs->getVar()];
        } else {
            for (int i = 0; i < mi->opndCount; i++) {
                int id(-1);
                if (mi->operands[i].access & OPND_READ) {
                    VariableState *v = NULL;
                    for (auto *vv : mi->inputs) {
                        if (vv->getVar() ==
                            mi->operands[i].compress(bb->instrs[index].pc))
                            v = vv;
                    }
                    assert(v); // operand read must be an input

                    id = evaluate(bb, index, v, loop, offset, gen, varid);
                    ni->inputs.push_back(id);
                } else
                    id = varid++;

                if (mi->operands[i].access & OPND_WRITE) {
                    if (vs->getVar() ==
                        mi->operands[i].compress(bb->instrs[index].pc)) {
                        // this is the required output
                        ret = id;
                    }
                    ni->outputs.push_back(id);
                }

                // push operand
                ni->push_opnd(id, mi->operands[i]);
            }
        }

        assert(ni->opnds.size() == mi->opndCount);
        // insert clone into the program
        gen.push_back(ni);
        return ret;
    }
}

void markLive(uint64_t reg, int index /*of the starting prefetch sequence*/,
              BasicBlock *bb, vector<RegSet> &registersAvailable,
              multimap<BasicBlock *, int> &prefInd,
              vector<pair<BasicBlock *, uint64_t>> &pos,
              pair<BasicBlock *, int> defPos, set<BasicBlock *> &reached)
{
    if (reached.find(bb) != reached.end())
        return;
    reached.insert(bb);
    int lim(-1);
    if (bb == defPos.first)
        lim = defPos.second;

    // remove reg from all prefetch sequences in this block (before seq. with
    // index)
    auto pit = prefInd.equal_range(bb);
    for (auto it = pit.first; it != pit.second; it++) {
        if ((bb == pos[index].first) &&
            (pos[it->second].second >= pos[index].second))
            continue;
        if (pos[it->second].second <= lim)
            continue;

        registersAvailable[it->second].remove(reg);
    }

    if (defPos.first == bb)
        return;
    // Otherwise recurse
    for (auto *pb : bb->pred) {
        markLive(reg, index, pb, registersAvailable, prefInd, pos, defPos,
                 reached);
    }
}

#define RSP_INDEX 5
#define RBP_INDEX 6
#define SPILL_COUNT 16
#define UNASSIGNED (-(SPILL_COUNT + 1))
#define DEBUG_ASSIGNMENTS

/** \brief Struct to hold all necessary information for deciding the
 variable-register assignment.
 *
 * cavailable : set of registers currently available
 * available  : set of non-live registers (to be used for abstract variables)
 * freeUp     : the time when the given register is scheduled to be cleaned up
 * savable    : a queue of registers that are live, but not used for abstract
 variables, hence they are not in constant use, therefore they could be saved
 (elements near the front are considered to be easier to be saved)
 * slot_used  : bool array of size SPILL_COUNT marking which spill slot is
 currently being used
 * reg_save   : if the original register A is saved to register/slot B then
 (reg_save[A]==B)
 * saved_content : (reg_save[A] == B) iff (saved_content[B] == A)
 * assigned   : the A'th entry stores the register that currently contains the
 A'th abstract var
 * assignment : (assigned[A] == B) iff (assignment[B] == A)
 * lastRead   : instruction that last uses the given variable
 * definedAt  : instruction first defining or assigning the given variable
 * usedNext   : set of registers used at the very next instruction
 * relocatedOriginal : set of registers that were live through the point where
 the prefetch code
 *                     is inserted and are at a different place at any
 instruction that could
 *                     fault (or at the end)
 * liveRegs   : registers that need to be the same before and after the prefetch
 code
 * unusedRegs : registers and spill slots that are never used during this
 prefetch sequence
 * regsWritten : registers that are not the same before and after the sequence
 * originalRead : set of original registers read during this prefetch
 * nonRegVar  : if a variable is defined as a non-register location, the
 association is stored here
 */
struct AssignmentContext {
    int N, M;
    BasicBlock *bb;
    PCAddress pc;
    uint16_t rule_ID = 20;
    uint16_t rrule_ID = 0;
    set<uint32_t> cavailable;
    set<pair<int, int64_t>> freeUp;
    set<uint32_t> available;
    queue<uint32_t> savable;
    vector<bool> slot_used;
    map<int64_t, int64_t> reg_save;
    map<int64_t, int64_t> saved_content;
    vector<int64_t> assigned;
    map<int64_t, int> assignment;
    vector<int> lastRead;
    vector<int> definedAt;
    set<int64_t> usedNext;
    set<uint32_t> liveRegs;
    set<int64_t> relocatedOriginal;
    set<int64_t> unusedSpace;
    vector<RewriteRule *> rules;
    set<int64_t> regsWritten;
    set<uint32_t> originalRead;
    map<int64_t, Variable> nonRegVar;
    vector<int> lastReadOrig;
    int j;

    AssignmentContext(int N, int M) : N(N), M(M)
    {
        slot_used.resize(SPILL_COUNT + 1);
        fill(slot_used.begin(), slot_used.end(), false);
        slot_used[0] = 1;
        assigned.resize(M);
        lastRead.resize(M);
        definedAt.resize(M);
        lastReadOrig.resize(17); // for general purpose regs only
        for (int i = 0; i < 17; i++) {
            lastReadOrig[i] = -1;
        }
        for (int i = 0; i < M; i++) {
            assigned[i] = UNASSIGNED;
            lastRead[i] = -1;
            definedAt[i] = N;
        }

        for (int i = 1; i <= SPILL_COUNT; i++)
            unusedSpace.insert(-i);
    }

    void removeFreeUp(int64_t reg)
    {
        if (assignment.find(reg) == assignment.end())
            return;
        freeUp.erase({lastRead[assignment[reg]], reg});
    }

    ~AssignmentContext()
    {
        // Cleanup
        for (auto *rule : rules)
            delete rule;
    }
};

inline static void markAsOverwritten(AssignmentContext &ac, int64_t reg)
{
    ac.unusedSpace.erase(reg);

    if (ac.saved_content.find(reg) == ac.saved_content.end())
        return;
    // doesn't contain original reg

    if (ac.liveRegs.find(ac.saved_content[reg]) != ac.liveRegs.end())
        cerr << "Warning! Live register has been erased..." << endl;

    ac.regsWritten.insert(ac.saved_content[reg]);

    ac.reg_save.erase(ac.saved_content[reg]);
    ac.saved_content.erase(reg);
}

static void mov_instr(int64_t dst, int64_t src, AssignmentContext &ac)
{
    if (src == dst)
        return;

    markAsOverwritten(ac, dst);

    if (ac.saved_content.find(src) != ac.saved_content.end()) {
        int64_t rs = ac.saved_content[src];
        ac.saved_content.erase(src);
        ac.reg_save[rs] = dst;
        ac.saved_content[dst] = rs;
    }
    if (ac.assignment.find(dst) != ac.assignment.end()) {
        cerr << "Warning! MOV overwrote a variable..." << endl;
        ac.removeFreeUp(dst);
        ac.assigned[ac.assignment[dst]] = UNASSIGNED;
        ac.assignment.erase(dst);
    }
    if (ac.assignment.find(src) != ac.assignment.end()) {
        int var = ac.assignment[src];

        ac.removeFreeUp(src);
        ac.freeUp.insert({ac.lastRead[var], dst});

        ac.assigned[var] = dst;
        ac.assignment.erase(src);
        ac.assignment[dst] = var;
    }

    if (dst < 0) {
        ac.slot_used[-dst] = 1;
    } else
        ac.cavailable.erase(dst);

    if (src < 0) {
        ac.slot_used[-src] = 0;
        Assert<PrefetchFailed>(dst >= 0, PREFETCH_ERROR_SPILL_TO_SPILL);
// Spill slot to register
#ifdef DEBUG_ASSIGNMENTS
        cerr << "Rule: restore register " << getRegNameX86(dst) << " from slot "
             << -src << endl;
#endif
    } else {
        ac.cavailable.insert(src);
        ac.available.insert(src);
        if (dst < 0) {
// Register to spill slot
#ifdef DEBUG_ASSIGNMENTS
            cerr << "Rule: Save register " << getRegNameX86(src) << " to slot "
                 << -dst << endl;
#endif
        } else {
// Register to register
#ifdef DEBUG_ASSIGNMENTS
            cerr << "Rule: MOV " << getRegNameX86(dst) << ", "
                 << getRegNameX86(src) << endl;
#endif
        }
    }

    RewriteRule *rule =
        OPT_INSERT_MOV_rule(ac.bb, ac.pc, ac.rule_ID++, dst, src).encode();
    ac.rules.push_back(rule);
}

// IMPROVEMENT! the save mechanism could be more efficient, only saving if the
// instruction sequence could actually fault, and restore registers manually in
// the end. Also, it could just let live registers not read during the sequence
// be overwritten if they are saved&restored anyway
static void save_original(int64_t reg, AssignmentContext &ac)
{
    cerr << "Rule: Register " << getRegNameX86(reg)
         << " should be saved in the previous prefetch"
         << " sequence to ";
    int64_t r(0);
    if (ac.unusedSpace.size()) {
        // take the last element (prefer registers over spill slots)
        auto it = ac.unusedSpace.end();
        it--;
        r = *it;
        assert(r);
        ac.unusedSpace.erase(it);
        if (r < 0) {
            cerr << "spill slot " << -r << endl;
        } else
            cerr << "register " << getRegNameX86(r) << endl;
    } else {
        cerr << "the stack" << endl;
    }

    RewriteRule *rule =
        OPT_RESTORE_REG_rule(ac.bb, ac.pc, ac.rrule_ID++, (uint32_t)reg, r)
            .encode();
    ac.rules.push_back(rule);
}

// perform a cyclic shift on ac.savable to bring to the front an element that is
// not used in the next instruction. Returns whether it was successful or not.
// IMPROVEMENT: complexity may become quadratic which may lead to suboptimal
// performance
inline static bool shift_savable(AssignmentContext &ac)
{
    queue<uint32_t> Q;

    while ((!ac.savable.empty()) &&
           (ac.usedNext.find(ac.savable.front()) != ac.usedNext.end())) {
        Q.push(ac.savable.front());
        ac.savable.pop();
    }

    bool ret(!ac.savable.empty());

    while (!Q.empty()) {
        ac.savable.push(Q.front());
        Q.pop();
    }

    return ret;
}

// finds free space, schedules cleanup at time e
inline static int64_t find_free_space(int e, AssignmentContext &ac)
{

    if (ac.cavailable.size()) {
        // unused register found
        uint32_t reg = *ac.cavailable.begin();
        ac.freeUp.insert({e, (int64_t)reg});
        ac.cavailable.erase(ac.cavailable.begin());
        return (int64_t)reg;
    }

    // Find a free spill slot
    int64_t free_slot = -1;
    for (int i = 1; i <= SPILL_COUNT; i++) {
        if (ac.slot_used[i])
            continue;
        // unused slot found
        free_slot = i;
        break;
    }

    Assert<PrefetchFailed>(free_slot >= 0, PREFETCH_ERROR_NO_SLOT);

    // Try to save a variable (Assumption: any savable variable is unlikely to
    // be in constant use)
    if (shift_savable(ac)) {
        // variable found that can be saved to a spill slot
        uint32_t reg = ac.savable.front();
        ac.savable.pop();

        if (ac.originalRead.find(reg) != ac.originalRead.end()) {
            // Save register used later to spill slot
            mov_instr(-free_slot, reg, ac);
        } else {
            // register never used => just save and restore at the start and end
            ac.relocatedOriginal.insert(reg);
            ac.reg_save.erase(ac.saved_content[reg]);
            ac.saved_content.erase(reg);
        }
        // Use reg as free space
        ac.freeUp.insert({e, (int64_t)reg});
        ac.cavailable.erase(reg);
        return (int64_t)reg;
    }

    // Otherwise use spill slot right away
    return -free_slot;
}

inline static int64_t find_free_register(int e, AssignmentContext &ac)
{
    int64_t ret = find_free_space(e, ac);
    Assert<PrefetchFailed>(ret >= 0, PREFETCH_ERROR_NO_REGISTER);
    return ret;
}

inline static int8_t correctSize(int8_t reg, Operand &opnd)
{
    if ((opnd.type == OPND_REG) && (opnd.size == 4))
        return reg + JREG_EAX - JREG_RAX;
    return reg;
}

inline static int32_t getRegOfSize(int8_t reg, int size)
{
    Assert<PrefetchFailed>((1 <= reg) && (reg <= 16));

    switch (size) {
    case 8:
        return reg;
    case 4:
        return reg - JREG_RAX + JREG_EAX;
    case 2:
        return reg - JREG_RAX + JREG_AX;
    case 1:
        if (reg < 4)
            return reg - JREG_RAX + JREG_AL;
        if (reg < 8)
            return reg - JREG_RSP + JREG_SPL;
        return reg - JREG_R8 + JREG_R8L;
    }
}

static void other_instr(new_instr *ni, AssignmentContext &ac)
{
    for (auto &p : ni->opnds) {
        if (p.second == -1)
            continue; // immediate operand
        if (ac.nonRegVar.find(p.second) != ac.nonRegVar.end())
            continue; // non-register opnd
        p.first.setRegister(ac.assigned[p.second]);
    }

    if (ni->hasMemoryAccess()) {
        // Instruction could fault, we need to push to the stack all live
        // registers currently relocated from their original register
        for (auto p : ac.reg_save) {
            if (p.first == p.second)
                continue; // not relocated
            if (ac.liveRegs.find(p.first) == ac.liveRegs.end())
                continue; // not live
            ac.relocatedOriginal.insert(p.first);
        }
    }

    if (ni->type() == NEWINSTR_DEF)
        return;
    // Don't need rules for definitions, these are correct without any action

    cerr << "Rule: " << ni->print();

    // mark all output registers as overwritten
    for (int i = 0; i < ni->opnds.size(); i++) {
        if (ni->opnds[i].first.type != OPND_REG)
            continue; // not a register

        bool isOutput(0);
        for (auto x : ni->outputs)
            isOutput |= (ni->opnds[i].second == x);
        if (isOutput) {
            uint64_t reg = ni->opnds[i].first.compress(ac.pc).getRegister();
            // reg is an output register
            markAsOverwritten(ac, reg);
        }
    }

    RewriteRule *rule(NULL);
    if (ni->cloned) {
        // cloned instruction

        Assert<PrefetchFailed>(ni->opnds.size() <= 4,
                               PREFETCH_ERROR_OPND_COUNT);
        uint8_t opnds[4];
        int opc(0);
        // XXX this ordering may be incorrect if there are multiple input &
        // output operands -- change encoding in the future to match operands
        for (int i = ni->opnds.size() - 1; i >= 0; i--) {
            bool isInput(0);
            for (auto x : ni->inputs)
                isInput |= (ni->opnds[i].second == x);
            if (isInput)
                opnds[opc++] = correctSize(ac.assigned[ni->opnds[i].second],
                                           ni->cloned->operands[i]);
        }
        for (int i = ni->opnds.size() - 1; i >= 0; i--) {
            bool isOutput(0);
            for (auto x : ni->outputs)
                isOutput |= (ni->opnds[i].second == x);
            if (isOutput)
                opnds[opc++] = correctSize(ac.assigned[ni->opnds[i].second],
                                           ni->cloned->operands[i]);
        }

        rule = OPT_CLONE_INSTR_rule(ni->cloned->block, ni->cloned->pc,
                                    ac.rule_ID++, opnds,
                                    ni->type() == NEWINSTR_FIXED, ac.pc)
                   .encode();
    } else if (ni->opcode == X86_INS_PREFETCH) {
        // PREFETCH instruction
        assert(ni->opnds.size() == 1);

        rule = OPT_PREFETCH_rule(ac.bb, ac.pc, ac.rule_ID++,
                                 ni->opnds[0].first.compress(ac.pc))
                   .encode();
    } else if (ni->opcode == X86_INS_MOV) {
        // MOV instruction without spill slot (== MEM_MOV)
        assert(ni->opnds.size() == 2);
        assert(ni->opnds[0].first.type == OPND_REG);

        markAsOverwritten(ac, get_full_reg_id(ni->opnds[0].first.reg));

        rule =
            OPT_INSERT_MEM_MOV_rule(
                ac.bb, ac.pc, ac.rule_ID++,
                getRegOfSize(ni->opnds[0].first.compress(ac.pc).getRegister(),
                             ni->opnds[1].first.size),
                ni->opnds[1].first.compress(ac.pc))
                .encode();
    } else if (ni->opcode == X86_INS_ADD) {
        // ADD instruction
        assert(ni->opnds.size() == 2);
        assert(ni->opnds[0].first.type == OPND_REG);

        if ((ni->opnds[1].first.type == OPND_IMM) &&
            (ni->opnds[1].first.value > ((uint64_t)1ll << 31))) {
            // add cannot be encoded directly -> load into a register first
            int64_t reg = find_free_register(ac.j, ac);

            ac.rules.push_back(OPT_INSERT_MEM_MOV_rule(
                                   ac.bb, ac.pc, ac.rule_ID++, ((uint32_t)reg),
                                   ni->opnds[1].first.compress(ac.pc))
                                   .encode());

            ni->opnds[1].first.type = OPND_REG;
            ni->opnds[1].first.reg = reg;
        }

        bool src_opnd_type = ni->opnds[1].first.type == OPND_REG;
        rule = OPT_INSERT_ADD_rule(
                   ac.bb, ac.pc, ac.rule_ID++, src_opnd_type,
                   ((uint32_t)ni->opnds[0].first.compress(ac.pc).getRegister()),
                   ni->opnds[1].first.value)
                   .encode();
    } else if (ni->opcode == X86_INS_SUB) {
        // SUB instruction
        assert(ni->opnds.size() == 2);
        assert(ni->opnds[0].first.type == OPND_REG);

        if ((ni->opnds[1].first.type == OPND_IMM) &&
            (ni->opnds[1].first.value > ((uint64_t)1ll << 31))) {
            // add cannot be encoded directly -> load into a register first
            int64_t reg = find_free_register(ac.j, ac);

            ac.rules.push_back(OPT_INSERT_MEM_MOV_rule(
                                   ac.bb, ac.pc, ac.rule_ID++, ((uint32_t)reg),
                                   ni->opnds[1].first.compress(ac.pc))
                                   .encode());

            ni->opnds[1].first.type = OPND_REG;
            ni->opnds[1].first.reg = reg;
        }

        bool src_opnd_type = ni->opnds[1].first.type == OPND_REG;
        rule = OPT_INSERT_SUB_rule(
                   ac.bb, ac.pc, ac.rule_ID++, src_opnd_type,
                   ((uint32_t)ni->opnds[0].first.compress(ac.pc).getRegister()),
                   ni->opnds[1].first.value)
                   .encode();
    } else {
        // IMUL instruction
        assert(ni->opcode == X86_INS_IMUL);
        assert(ni->opnds.size() == 2);
        assert(ni->opnds[1].first.type == OPND_IMM);

        rule = OPT_INSERT_IMUL_rule(
                   ac.bb, ac.pc, ac.rule_ID++,
                   ((uint32_t)ni->opnds[0].first.compress(ac.pc).getRegister()),
                   ni->opnds[1].first.value)
                   .encode();
    }

    ac.rules.push_back(rule);
}

// Returns a pointer to the only VariableState in vs->phi, outside of the given
// loop if it exists NULL otherwise
static VariableState *getOnlyInitialization(VariableState *vs, Loop *loop)
{
    assert(vs->type() == VARSTATE_PHI);

    int outside_cnt(0);
    VariableState *ret = NULL;

    for (auto *v : vs->phi) {
        if (loop->contains(v->block->bid))
            continue;
        outside_cnt++;
        ret = v;
    }

    return (outside_cnt == 1) ? ret : NULL;
}

void generateOptRules(JanusContext *gc)
{
    RewriteRule rule;

    for (auto &function : gc->functions) {
        map<VariableState *, Loop *> store;
        map<VariableState *, Loop *> prefetchedLoads;
        set<int64_t> overwrittenReg;
        set<VariableState *> phiJump;
        set<Loop *> initPrefetchLoops;

        for (auto &bb : function.mblocks) {
            if (!bb.parentLoop)
                continue; // skip blocks not within any loops
            for (int i = 0; i < bb.size; i++) {
                auto &mi = bb.instrs[i];
                if (!mi.hasMemoryAccess())
                    continue;

                for (VariableState *op : mi.inputs) {
                    if (!op->block->parentLoop)
                        continue;
                    if (op->type() != VARSTATE_MEMORY_OPND)
                        continue;
                    // op contains a load instruction which is in a loop

                    Loop *loop = NULL;
                    if ((op->phi.size()) &&
                        (op->phi[0]->type() == VARSTATE_PHI) &&
                        (!op->phi[0]->inductionLoop) &&
                        (getOnlyInitialization(op->phi[0],
                                               op->block->parentLoop))) {
                        VariableState *vs = getOnlyInitialization(
                            op->phi[0], op->block->parentLoop);
                        loop = findPrefetch(vs, store);

                        // for base-index form, check if base is constant
                        //                           IMPROVEMENT: if not compute
                        //                           base if possible
                        if ((vs->phi.size() > 1) && (loop) &&
                            (!loop->isConstant(vs->phi[1])))
                            store[vs] = loop = NULL;
                        phiJump.insert(op);
                        if (initPrefetchLoops.find(loop) !=
                            initPrefetchLoops.end())
                            continue;
                        initPrefetchLoops.insert(loop);
                    } else
                        loop = findPrefetch(op, store);

                    if (!loop)
                        continue; // no prefetch

                    prefetchedLoads[op] = loop;
                }
            }
        }

        for (auto p : prefetchedLoads) {
            cerr << "Prefetchable load instruction found in loop "
                 << p.second->id << ": " << endl;

            printVarState(p.first, (void *)&cerr);
            cerr << " -> Input of instruction: ";
            for (auto *mi : p.first->dependants) {
                cerr << mi->id << " " << mi->name << endl;
            }
            cerr << endl;
        }

        // Number of loads depending on this state - only meaningful for load
        // instructions Needed to calculate offset
        map<Loop *, map<VariableState *, int>> nDL;
        map<Loop *, int> offsetT;

        for (auto p : prefetchedLoads) {
            nDL[p.second] = map<VariableState *, int>();
        }
        for (auto p : prefetchedLoads) {
            countDependantLoads(p.first, prefetchedLoads, p.second,
                                nDL[p.second]);
            // for calculating the offset
        }

        for (auto p : nDL) {
            Loop *l = p.first;
            int T(0);
            for (auto q : p.second) {
                T = max(T, q.second);
            }

            offsetT[l] = T;
        }

        vector<vector<new_instr *>> gens;
        vector<pair<BasicBlock *, uint64_t>> pos;
        vector<RegSet> registersRead;
        vector<RegSet> registersAvailable;
        vector<int> varids;

        multimap<BasicBlock *, int> prefetchIndices;

        int N(0); // number of actual prefetches

        // fill nDL
        for (auto p : prefetchedLoads) {

            try {

                int varid(0);
                vector<new_instr *> gen;
                MachineInstruction *mi;
                VariableState *toEvaluate = p.first;

                if (phiJump.find(p.first) !=
                    phiJump.end()) { // if we crossed a phi node
                    toEvaluate = getOnlyInitialization(
                        p.first->phi[0], p.first->block->parentLoop);

                    if (toEvaluate->type() == VARSTATE_NORMAL) {
                        // if it is a normal variable, then after the definition
                        mi = toEvaluate->lastModified;
                        mi++;
                    } else
                        mi = toEvaluate->block->instrs;
                    // otherwise to the start of its block (eg phi node)
                } else
                    mi = p.first->dependants[0];

                int offs = offset(p.first, offsetT[p.second], nDL[p.second]);
                // generate computation for the base address, store variable ID
                // in "result"
                int result =
                    evaluate(mi->block, mi->id - mi->block->instrs[0].id,
                             toEvaluate, p.second, offs, gen, varid);
                // generate prefetch instruction for p.first
                new_instr *ni = new new_instr(X86_INS_PREFETCH, mi->pc);
                Operand *op = new Operand(p.first->getVar());
                ni->push_opnd(result, *op);
                ni->inputs.push_back(result);

                // insert prefetch
                gen.push_back(ni);

                // Print for debugging purposes
                cerr << endl
                     << "Prefetch generated at address " << mi->pc
                     << " (offset=" << offs << "): " << endl;
                for (int i = 0; i < gen.size(); i++) {
                    cerr << gen[i]->print();
                }

                // Add to the list of actual prefetches
                gens.emplace_back(gen);
                pos.push_back({mi->block, mi->id - mi->block->instrs[0].id});

                registersRead.emplace_back();
                for (auto *ni : gen) {
                    registersRead[N].merge(ni->originalRegsRead());
                }

                // Registers originally available (as if this were the only set
                // of new instructions
                //                                                                           inserted)
                // This is refined later
                registersAvailable.emplace_back(
                    mi->block->liveRegSet(mi->id - mi->block->instrs[0].id)
                        .bits);
                registersAvailable[N].complement();

                varids.push_back(varid);

                N++;
            } catch (PrefetchFailed) {
                // Known reasons for no prefetch generation:
                //       - in some cases if only a part of a register is used
                //       (eg ax, ah, r8w)
                //       - if the alternative computation of a phi-node constant
                //       fails in evaluate
                cerr << endl << "No prefetch could be generated..." << endl;

                // IMPROVEMENT: retry at a different address or find an optimal
                // address beforehand
            }
        }

        // Refine the set of registers available as storage
        for (int i = 0; i < N; i++) {
            set<uint32_t> readSet;
            registersRead[i].toSTLSet(readSet);
            for (auto reg : readSet) {
                if (!registersAvailable[i].contains(reg))
                    continue;
                // proceed only if not marked live

                // Mark live in all prefetch code between this one and the
                // instruction defining
                //      the variable
                VariableState *vs =
                    pos[i].first->alive(Variable((uint32_t)reg));
                int vsindex =
                    (vs->type() == VARSTATE_PHI)
                        ? -1
                        : vs->lastModified->id - vs->block->instrs[0].id;
                set<BasicBlock *> reached;
                markLive(reg, i, pos[i].first, registersAvailable,
                         prefetchIndices, pos, {vs->block, vsindex}, reached);
            }
        }

        // Assigning registers to variables
        for (int i = 0; i < N; i++) {
            try {
                AssignmentContext ac(gens[i].size(), varids[i]);
                ac.bb = pos[i].first;
                ac.pc = pos[i].first->instrs[pos[i].second].pc;
                // registersAvailable[i].clear(); //XXX XXX XXX TESTING
                RegSet RS(registersAvailable[i].bits);
                RS.subtract(registersRead[i]);
                RS.toSTLSet(ac.available);
                for (auto e : ac.available)
                    if (e <=
                        16) ///////////XXX At the moment we are only using the
                            ///first 16 general purpose registers, due to
                            ///failure to encode other mov (movq, ...)
                            ///instructions in the dynamic part. This limits the
                            ///number of registers, hence it would be nice to
                            ///solve (to enable the usage of XMM, ... registers
                            ///as save slots -> IMPROVEMENT
                        ac.cavailable.insert(e);
                // Don't use the stack pointer
                ac.cavailable.erase(RSP_INDEX);
                ac.available.erase(RSP_INDEX);
                ac.cavailable.erase(RBP_INDEX);
                ac.available.erase(RBP_INDEX);

                for (auto v : ac.available)
                    if (v <= 16) ///////////XXX See previous comment
                        ac.unusedSpace.insert(v);

                // Store the registers which could be saved
                set<uint32_t> savable;
                RS = RegSet(registersAvailable[i].bits);
                RS.complement();
                RS.toSTLSet(ac.liveRegs);
                RegSet RS2(RS.bits);
                RS.subtract(registersRead[i]);
                RS2.intersect(registersRead[i]);

                // Don't use the stack pointer
                RS.remove(RSP_INDEX);
                RS2.remove(RSP_INDEX);

                // add registers not read during the prefetch (RS) first
                RS.toSTLSet(savable);
                for (auto q : savable)
                    if (q <= 16)
                        ac.savable.push(q);
                savable.clear();
                // add other live registers to the set of savable registers
                RS2.toSTLSet(savable);
                for (auto q : savable)
                    if (q <= 16)
                        ac.savable.push(q);

                registersRead[i].toSTLSet(ac.originalRead);

                // mark all registers as saved to themselves
                for (int64_t reg = 1; reg <= 16; reg++) {
                    if (ac.available.find(reg) == ac.available.end()) {
                        ac.saved_content[reg] = reg;
                        ac.reg_save[reg] = reg;
                        if (!registersAvailable[i].contains(reg))
                            ac.lastReadOrig[reg] = ac.N + 1;
                    }
                }

                int j(0);
                for (auto *ni : gens[i]) {
                    for (auto ind : ni->inputs) {
                        ac.lastRead[ind] = j;
                    }

                    for (auto ind : ni->outputs) {
                        ac.definedAt[ind] = min(ac.definedAt[ind], j);
                    }
                    if (ni->type() == NEWINSTR_DEF) {
                        for (auto p : ni->def) {
                            ac.definedAt[p.first] = j;
                        }
                    }

                    j++;
                }

                // attach to registers their last use date
                for (auto *ni : gens[i]) {
                    if (ni->type() != NEWINSTR_DEF)
                        continue;
                    for (auto p : ni->def) {
                        int reg = p.second.getRegister();
                        if (reg <= 16) {
                            ac.lastReadOrig[reg] =
                                max(ac.lastReadOrig[reg], ac.lastRead[p.first]);
                        }
                    }
                }

                // set base registers as temporary
                for (int k = 0; k < ac.M; k++) {
                    if (ac.lastRead[k] < 0)
                        ac.lastRead[k] = ac.definedAt[k] + 1;
                }

                // schedule original registers that are not live for freeUp
                for (int k = 0; k < ac.M; k++) {
                    if (ac.lastReadOrig[k] <= N)
                        continue;
                    if (!registersAvailable[i].contains(k))
                        continue;
                    ac.freeUp.insert({ac.lastReadOrig[k], k});
                }

                ac.freeUp.insert({gens[i].size(), (int64_t)-1ll});
                // abstract element to avoid dereferencing freeUp.end()
                j = 0;
                for (auto *ni : gens[i]) {
                    ac.j = j;
                    ac.usedNext.clear();
                    if (ni->type() == NEWINSTR_DEF) {
                        for (auto p : ni->def) {
                            // XXX multiple (read-only) variables assigned to
                            // the same original register?
                            if (p.second.field.type != VAR_REG) {
                                ac.nonRegVar[p.first] = p.second;
                                continue;
                            }
                            int64_t reg = (int64_t)p.second.field.value;
                            int64_t r =
                                (ac.reg_save.find(reg) == ac.reg_save.end())
                                    ? reg
                                    : ac.reg_save[reg];
                            ac.assigned[p.first] = r;
                            ac.assignment[r] = p.first;
                            if (ac.available.find(r) != ac.available.end()) {
                                ac.cavailable.erase(r);
                                ac.freeUp.insert({ac.lastRead[p.first], r});
                            }
                        }
                    } else if (ni->type() == NEWINSTR_OTHER) {
                        for (const auto &p : ni->opnds) {
                            int v = p.second;

                            if (v == -1) {
                                // Immediate operand -- nothing to do
                                continue;
                            }

                            if (ac.nonRegVar.find(v) == ac.nonRegVar.end()) {
                                if (ac.assigned[v] == UNASSIGNED) {
// v must be output only
#ifdef DEBUG_ASSIGNMENTS
                                    bool inputs_not_unassigned(true);
                                    for (int w : ni->inputs)
                                        inputs_not_unassigned &= (v != w);
                                    assert(inputs_not_unassigned);
#endif

                                    // In that case we can just allocate any
                                    // register
                                    int64_t reg =
                                        find_free_register(ac.lastRead[v], ac);
                                    ac.assigned[v] = reg;
                                    ac.assignment[reg] = v;
                                } else if (ac.assigned[v] < 0) {
                                    // variable v is currently stored in a spill
                                    // slot
                                    int64_t r =
                                        find_free_register(ac.lastRead[v], ac);
                                    mov_instr(r, ac.assigned[v], ac);
                                    // IMPROVEMENT: add r back to savable after
                                    // all arguments are obtained
                                }
                            }
                            ac.usedNext.insert(v);
                        }

// At this point all operands should be in their desired locations
#ifdef DEBUG_ASSIGNMENTS
                        for (const auto &p : ni->opnds) {
                            assert((p.second == -1) ||
                                   (ac.assigned[p.second] >= 0) ||
                                   (ac.nonRegVar.find(p.second) !=
                                    ac.nonRegVar.end()));
                        }
#endif
                    } else {
                        assert(ni->type() == NEWINSTR_FIXED);

                        // mark all necessary registers as used in the next
                        // instruction
                        for (auto &p : ni->def) {
                            if (p.first < 0)
                                continue; // skip immediate operands
                            if (ac.nonRegVar.find(p.first) !=
                                ac.nonRegVar.end())
                                continue;
                            // non-register variable
                            int64_t reg = p.second.getRegister();
                            assert(reg != X86_REG_INVALID);
                            ac.usedNext.insert(reg);
                            if ((p.second.field.type == VAR_MEM_BASE) &&
                                (p.second.field.base != X86_REG_INVALID) &&
                                (p.second.field.index != X86_REG_INVALID)) {
                                ac.usedNext.insert(p.second.field.base);
                            }
                        }

                        set<uint32_t> inputRegs;
                        for (int v : ni->inputs) {
                            if (ac.nonRegVar.find(v) != ac.nonRegVar.end())
                                continue;
                            inputRegs.insert(ni->def[v].getRegister());
                        }
                        vector<pair<int, int64_t>> assignedAsOutput;
                        // clear out everything from the registers used and copy
                        // in the data needed
                        for (auto &p : ni->def) {
                            if (p.first < 0)
                                continue; // skip immediate operands
                            if (ac.nonRegVar.find(p.first) !=
                                ac.nonRegVar.end())
                                continue;

                            int64_t reg = p.second.getRegister();
                            if (reg == ac.assigned[p.first])
                                continue;

                            // look for variables or live registers already
                            // using this register
                            if (ac.assignment.find(reg) !=
                                ac.assignment.end()) {
                                int64_t reloc = find_free_space(
                                    ac.lastRead[ac.assignment[reg]], ac);
                                if (reloc != reg)
                                    mov_instr(reloc, reg, ac);
                            }
                            if (ac.saved_content.find(reg) !=
                                ac.saved_content.end()) {
                                if (ac.lastReadOrig[ac.saved_content[reg]] <=
                                    ac.N) {
                                    int64_t reloc = find_free_space(
                                        ac.lastReadOrig[ac.saved_content[reg]],
                                        ac);
                                    if (reloc != reg)
                                        mov_instr(reloc, reg, ac);
                                } else {
                                    // register never used => just save and
                                    // restore at the start and end
                                    ac.relocatedOriginal.insert(reg);
                                    ac.reg_save.erase(ac.saved_content[reg]);
                                    ac.saved_content.erase(reg);
                                    markAsOverwritten(ac, reg);
                                }
                            }

                            assert(ac.assignment.find(reg) ==
                                   ac.assignment.end());
                            assert(ac.saved_content.find(reg) ==
                                   ac.saved_content.end());

                            if (ac.assigned[p.first] == UNASSIGNED) {
// p.first must be output only
#ifdef DEBUG_ASSIGNMENTS
                                bool inputs_not_unassigned(true);
                                for (int w : ni->inputs)
                                    inputs_not_unassigned &= (p.first != w);
                                assert(inputs_not_unassigned);
#endif
                                if (inputRegs.find(reg) != inputRegs.end()) {
                                    // an input variable already uses the same
                                    // register, meaning that this variable will
                                    // only be first defined after the
                                    // instruction will finish
                                    assignedAsOutput.push_back({p.first, reg});
                                    continue;
                                }
                                // If it is unassigned, then assign now
                                ac.assignment[reg] = p.first;
                                ac.assigned[p.first] = reg;

                                ac.cavailable.erase(reg);
                                ac.freeUp.insert({ac.lastRead[p.first], reg});
                            } else {
                                // otherwise copy to the required place
                                mov_instr(reg, ac.assigned[p.first], ac);
                            }

                            assert(ac.assignment[reg] == p.first);
                            assert(p.second.getRegister() == reg);
                        }

                        // output variables be assigned after the instruction
                        // has finished
                        for (auto p : assignedAsOutput) {
                            ac.assigned[p.first] = p.second;
                            ac.assignment[p.second] = p.first;

                            ac.cavailable.erase(p.second);
                            ac.removeFreeUp(p.first);
                            ac.freeUp.insert({ac.lastRead[p.first], p.second});
                        }
                    }

                    other_instr(ni, ac);

                    // Return registers that are not read after this point to
                    // the free register pool
                    while (ac.freeUp.begin()->first == j) {
                        int64_t r = ac.freeUp.begin()->second;
                        ac.freeUp.erase(ac.freeUp.begin());

                        if (ac.assignment.find(r) != ac.assignment.end()) {
                            ac.assigned[ac.assignment[r]] = UNASSIGNED;
                            ac.assignment.erase(r);
                        }

                        if (ac.saved_content.find(r) !=
                            ac.saved_content.end()) {
                            if (ac.lastReadOrig[ac.saved_content[r]] > j) {
                                continue;
                                // if the register holds the saved copy of a
                                // live register, it's not free
                            } else {
                                ac.reg_save.erase(ac.saved_content[r]);
                                ac.saved_content.erase(r);
                            }
                        }
                        if (r >= 0) {
                            // register
                            ac.cavailable.insert(r);
                        } else {
                            // spill slot
                            ac.slot_used[-r] = 0;
                        }
                    }
                    j++;
                }

                // Now give rules to save live registers relocated at any
                // instruction that could fault
                //  or at the end of the prefetch sequence
                for (int64_t reg : ac.relocatedOriginal) {
                    save_original(reg, ac);
                    ac.regsWritten.erase(reg);
                }

                for (int64_t reg : ac.regsWritten) {
                    if (reg <= 0)
                        continue; // spill slots do not have to be saved

                    overwrittenReg.insert(reg);
                }

                // check whether flags are to be saved in this sequence: saved
                // iff the actual live definition of eflags is used directly in
                // a machine instruction IMPROVEMENT: only save if the prefetch
                // sequence actually changes it
                bool saveFlags(0);
                Variable EFLAGS_REGISTER = Variable((uint32_t)JREG_EFLAGS);
                VariableState *vs =
                    ac.bb->alive(EFLAGS_REGISTER, ac.pc - ac.bb->instrs[0].pc);
                // if the current live definition is defined earlier in the same
                // block
                if (vs && (vs->block == ac.bb) &&
                    ((vs->type() == VARSTATE_PHI) ||
                     (vs->lastModified && (vs->lastModified->pc < ac.pc)))) {
                    for (auto *mi : vs->dependants) {
                        // used by an instruction later (in the same block)
                        if (mi->pc > ac.pc)
                            saveFlags = 1;
                    }
                }
                if (saveFlags) {
                    RewriteRule *h =
                        OPT_RESTORE_FLAGS_rule(ac.bb, ac.pc, ac.rrule_ID++)
                            .encode();
                    ac.rules.push_back(h);
                }

#ifdef DEBUG_ASSIGNMENTS
                cerr << ac.rules.size()
                     << " rules generated. Encoded versions:" << endl;
                for (auto *rule : ac.rules) {
                    rule->print(((void *)(&cerr)));
                }
#endif

                for (auto *rule : ac.rules) {
                    insertRule(0, *rule, ac.bb);
                }
            } catch (PrefetchFailed) {
                cerr << "Failed to generate rules due to too many registers "
                        "being used"
                     << endl;
            }
        }

        int saveID(0);
        // save&restore registers using the stack to comply with the calling
        // convention
        for (auto it = overwrittenReg.begin(); it != overwrittenReg.end();) {
            int64_t reg = *it;

            bool regSaved(false);
            for (int i = 0; i < function.entry->size; i++) {
                MachineInstruction &mi = function.entry->instrs[i];
                if (mi.writeReg(reg))
                    break;
                if (mi.isReturn())
                    break;
                if ((mi.opcode == X86_INS_PUSH) && (mi.readReg(reg))) {
                    // reg is saved to the stack in the entry block
                    // assume that this is using the stack to preserve the
                    // original state of the
                    //     reg during the execution of the function
                    regSaved = true;
                }
            }

            if (regSaved) {
                it = overwrittenReg.erase(it);
                continue;
            }
            cerr << "HINT - Saving register " << getRegNameX86(reg)
                 << " at the start of function " << function.fid << endl;
            // if not saved already, add instruction now to save it
            // IMPROVEMENT: recognize alignment instructions (eg AND rsp,
            // 0xfffffff0) and insert the push instructions after them
            RewriteRule *rule =
                OPT_SAVE_REG_TO_STACK_rule(
                    function.entry, function.entry->instrs[0].pc, saveID, reg)
                    .encode();
            insertRule(0, *rule, function.entry);
            delete rule;
            rule = NULL;

            saveID++;

            it++;
        }
        /*cerr << "Unrecognised: " << endl;
        for (auto bid : function.unRecognised) {
            cerr << bid << endl;
        }
        cerr << "SubCalls: " << endl;
        for (auto fid : function.subCalls) {
            cerr << fid << " " << function.context->functions[fid].name << endl;
        }*/

        for (auto &mi : function.minstrs) {
            if (!mi.isReturn())
                continue;

            saveID = 100;
            for (auto it = overwrittenReg.rbegin(); it != overwrittenReg.rend();
                 it++, saveID++) {
                int64_t reg = *it;

                cerr << "HINT - Restoring register " << getRegNameX86(reg)
                     << " at return instruction at " << mi.pc << endl;
                // insert rule to retrieve reg from the stack
                RewriteRule *rule = OPT_RESTORE_REG_FROM_STACK_rule(
                                        mi.block, mi.pc, saveID, reg)
                                        .encode();
                insertRule(0, *rule, mi.block);
                delete rule;
                rule = NULL;
            }
        }
    }

    for (int i = 0; i < PREFETCH_ERROR_COUNT; i++) {
        if (!PREFETCH_ERROR_COUNTERS[i])
            continue;
        cerr << "!!! " << PREFETCH_ERROR_NAMES[i] << " --- "
             << PREFETCH_ERROR_COUNTERS[i] << " times." << endl;
    }
}
