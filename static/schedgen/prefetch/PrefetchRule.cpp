#include "PrefetchRule.h"
#include "Alias.h"
#include "JanusContext.h"
#include "Slice.h"

using namespace std;
using namespace janus;

static bool findPrefetch(Loop &loop, map<VarState *, Iterator *> &passed);
static void generatePrefetchRulesForLoop(JanusContext *jc, Loop &loop,
                                         map<VarState *, Iterator *> &passed);
static void generatePrefetchRulesForSlice(JanusContext *jc, Slice &slice,
                                          VarState *vs, Iterator *iter,
                                          Loop &loop);
static Instruction *getPrefetchInsertLocation(Slice &slice);
static bool checkAndFormatPrefetchSlice(Slice &slice, Loop &loop,
                                        Iterator *iter);
static void prepareLoopMemoryAccesses(Loop *loop);
static void prepareLoopIterators(Loop *loop);
static bool instrContainIterator(Instruction *instr, Loop *loop);
static void emitInstrCloneRules(list<Instruction *> &instrs, Instruction *front,
                                Loop &loop);

void generatePrefetchRules(JanusContext *jc)
{
    map<VarState *, Iterator *> passed;
    for (auto &loop : jc->loops) {
        if (findPrefetch(loop, passed)) {
            generatePrefetchRulesForLoop(jc, loop, passed);
            passed.clear();
        }
    }
}

static bool findPrefetch(Loop &loop, map<VarState *, Iterator *> &passed)
{
    VarState *indLoad;

    map<VarState *, MemoryLocation *> &locationTable = loop.locationTable;
    map<VarState *, VarState *> indirectMemGraph;

    /* For unsafe loops, continue to create memory locations and iterators */
    if (loop.unsafe) {
        prepareLoopMemoryAccesses(&loop);
        prepareLoopIterators(&loop);
    }

    /* we start from the memory load of the loop
     * and record every pair of memory indirection */
    for (auto read : loop.memoryReads) {
        locationTable[read->vs] = read;
        // filter out first order accesses
        if (!read->containIterator(&loop) &&
            (indLoad = read->getIndirectMemory())) {
            /* record pair when
             * load a = [b] && b is a memory access */
            indirectMemGraph[read->vs] = indLoad;
        }
    }

    /* Currently we only supports second order accesses
     * TODO: make it nth order using topological sort */
    for (auto ind : indirectMemGraph) {
        MemoryLocation *check = locationTable[ind.second];
        if (check && check->containIterator(&loop))
            passed[ind.first] = check->expr.hasIterator(&loop);
    }

    IF_LOOPLOG(
        if (passed.size()) {
            loopLog << "Loop " << dec << loop.id
                    << " found prefetch opportunities:" << endl;
        } for (auto vs
               : passed) {
            loopLog << "\t" << vs.first << " <- " << *locationTable[vs.first]
                    << " <- " << *vs.second << endl;
        })

    return (passed.size() > 0);
}

static void generatePrefetchRulesForLoop(JanusContext *jc, Loop &loop,
                                         map<VarState *, Iterator *> &passed)
{
    for (auto fetchItem : passed) {
        Slice slice(fetchItem.first, Slice::CyclicScope, &loop);
        Iterator *iter = fetchItem.second;
        if (checkAndFormatPrefetchSlice(slice, loop, iter)) {
            LOOPLOG("Slice for " << fetchItem.first << endl << slice << endl);
            generatePrefetchRulesForSlice(jc, slice, fetchItem.first, iter,
                                          loop);
        } else {
            LOOPLOG("Slice for "
                    << fetchItem.first
                    << " rejected due to complex control flow in slices" << endl
                    << endl);
        }
    }
}

static bool checkAndFormatPrefetchSlice(Slice &slice, Loop &loop,
                                        Iterator *iter)
{
    if (!slice.instrs.size())
        return false;
    // currently we only allow slices within only one basic block
    // inserting instructions at different basic block requires control
    // dependent instruction to be added in slice currently we don't support
    // control flow in slices
    BasicBlock *block = slice.instrs.front()->block;
    int count = 0;

    Instruction *toRemove = NULL;
    // check if all instructions belong to the same block
    for (auto instr : slice.instrs) {
        if (block != instr->block) {
            count++;
            toRemove = instr;
        }
    }

    if (count == 1) {
        slice.instrs.remove(toRemove);
    } else if (count > 1)
        return false;

    /* sort the instruction list based on the instruction id */
    map<InstID, Instruction *> temp;
    for (auto instr : slice.instrs) {
        temp[instr->id] = instr;
    }
    // clear the previous slice
    slice.instrs.clear();
    // add back to slice
    for (auto instrP : temp) {
        slice.instrs.push_back(instrP.second);
    }
    return true;
}

static void generatePrefetchRulesForSlice(JanusContext *jc, Slice &slice,
                                          VarState *vs, Iterator *iter,
                                          Loop &loop)
{
    RewriteRule rule;
    // work out the insertion point
    Instruction *trigger = getPrefetchInsertLocation(slice);

    Instruction *front = slice.instrs.front();

    if (!trigger) {
        LOOPLOG("\tError: could not find a prefetch insertion point!" << endl);
    }

    if (front && !instrContainIterator(front, &loop)) {
        list<Instruction *> preCopy;
        preCopy.push_back(front);
        slice.instrs.pop_front();
        // copy the instruction list for slices that doesn't start with iterator
        // references
        while (!slice.instrs.empty()) {
            Instruction *instr = slice.instrs.front();
            if (instr && instrContainIterator(instr, &loop)) {
                // update front instruction
                front = instr;
                break;
            } else {
                preCopy.push_back(instr);
                slice.instrs.pop_front();
            }
        }

        // insert INSTR_CLONE rule
        emitInstrCloneRules(preCopy, trigger, loop);
    }

    if (iter->strideKind == Iterator::INTEGER) {
        bool foundStart = false;
        // for constant stride, we can simply modify the memory operands
        // without inserting new instructions or use additional registers

        for (auto vi : front->inputs) {
            if (vi->type == JVAR_MEMORY) {
                MemoryLocation *ml = loop.locationTable[vi];
                if (ml && ml->containIterator(&loop)) {
                    rule = RewriteRule(MEM_PREFETCH, trigger->block->instrs->pc,
                                       trigger->pc, trigger->id);
                    // update the memory operand to be prefetched
                    JVar toFetch = *(JVar *)vi;
                    // add prefetch distance
                    toFetch.value += iter->stride * PREFETCH_CONSTANT;
                    // encode jvar in rewrite rule
                    encodeJVar(toFetch, rule);
                    insertRule(loop.id, rule, trigger->block);

                    // clone and update the front instruction to be copied with
                    // PREFETCH_CONSTANT/2
                    rule = RewriteRule(INSTR_UPDATE, trigger->block->instrs->pc,
                                       trigger->pc, trigger->id);
                    rule.ureg0.up = 1; // relative mode
                    // prefetch distance
                    rule.reg1 = iter->stride * (PREFETCH_CONSTANT / 2);
                    insertRule(loop.id, rule, trigger->block);
                    foundStart = true;

                    slice.instrs.pop_front();
                }
            }
        }

        if (!foundStart)
            return;
        if (slice.instrs.size() > 1) {
            emitInstrCloneRules(slice.instrs, trigger, loop);
        }

        // at last, generate the final prefetch rewrite rule
        rule = RewriteRule(MEM_PREFETCH, trigger->block->instrs->pc,
                           trigger->pc, trigger->id);
        JVar toFetch2 = *(JVar *)vs;
        encodeJVar(toFetch2, rule);
        insertRule(loop.id, rule, trigger->block);
    }
}

static Instruction *getPrefetchInsertLocation(Slice &slice)
{
    return slice.instrs.front();
}

static bool instrContainIterator(Instruction *instr, Loop *loop)
{
    if (!instr)
        return false;
    for (auto vi : instr->inputs) {
        if (vi->type == JVAR_MEMORY) {
            MemoryLocation *ml = loop->locationTable[vi];
            if (ml && ml->containIterator(loop))
                return true;
        }
    }
    return false;
}

static void emitInstrCloneRules(list<Instruction *> &instrs,
                                Instruction *trigger, Loop &loop)
{
    RewriteRule rule;
    Instruction *startInstr = instrs.front();
    InstID prev = startInstr->id - 1;
    int step = 0;

    for (auto it = instrs.begin(); it != instrs.end(); it++) {
        // scan continuous code for instruction duplicate
        Instruction *instr = *it;
        // found a non-continuous instruction, emit a instruction duplicate
        // rewrite rule
        if (instr->id != prev + 1) {
            // terminate prev
            rule = RewriteRule(INSTR_CLONE, trigger->block->instrs->pc,
                               trigger->pc, trigger->id);
            // instruction relatively to trigger instruction
            rule.ureg0.up = 1; // relative mode
            rule.ureg0.down = startInstr->id - trigger->id;
            rule.ureg1.down = step;
            // update start and step
            startInstr = instr;
            step = 0;
            prev = instr->id;
            insertRule(loop.id, rule, trigger->block);
        } else {
            step++;
            prev++;
        }
    }
    // finish the closure
    rule = RewriteRule(INSTR_CLONE, trigger->block->instrs->pc, trigger->pc,
                       trigger->id);
    rule.ureg0.up = 1; // relative mode
    rule.ureg0.down = startInstr->id - trigger->id;
    rule.ureg1.down = step + 1;
    insertRule(loop.id, rule, trigger->block);
}

static void prepareLoopMemoryAccesses(Loop *loop)
{
    Function *parent = loop->parent;
    BasicBlock *entry = parent->getCFG().entry;
    // a temporary dictionary holding intermediate scevs
    map<VarState *, SCEV> ranges;

    LOOPLOG2("---------------------------------------------------------"
             << endl);
    LOOPLOG2("Loop " << dec << loop->id << " Prefetch Memory Analysis" << endl);

    /* Read set */
    auto &readSets = loop->memoryReads;
    /* Write set */
    auto &writeSets = loop->memoryWrites;

    for (auto bid : loop->body) {
        BasicBlock &bb = entry[bid];
        for (auto &mi : bb.minstrs) {
            // skip for load effective address
            if (mi.mem->type != JVAR_MEMORY)
                continue;

            LOOPLOG2("\t\t" << mi << endl);
            // create a memory variable for each memory instruction
            MemoryLocation *memVar = new MemoryLocation(&mi, loop);
            LOOPLOG2("\t\t\tMemory expression: " << memVar->expr
                                                 << " constructed" << endl);
            // create a memory variable for each memory instruction
            memVar->getSCEV();
            LOOPLOG2("\t\t\tScalar evolution: " << *memVar->escev << endl);
            // insert the memory location into the loop array accesses
            MemoryLocation *location = getOrInsertMemLocations(loop, memVar);
            // add to read and write set
            if (mi.type == MemoryInstruction::Read ||
                mi.type == MemoryInstruction::ReadAndRead) {
                location->readBy.insert(mi.instr);
                readSets.insert(location);
            } else if (mi.type == MemoryInstruction::Write) {
                location->writeFrom.insert(mi.instr);
                writeSets.insert(location);
            } else if (mi.type == MemoryInstruction::ReadAndWrite) {
                location->readBy.insert(mi.instr);
                readSets.insert(location);
                location->writeFrom.insert(mi.instr);
                writeSets.insert(location);
            }
        }
    }
    LOOPLOG2("\n\t"
             << "Identified read set: " << endl);
    IF_LOOPLOG2(for (auto memLoc
                     : readSets) { loopLog2 << "\t\t" << *memLoc << endl; });
    LOOPLOG2("\t"
             << "Identified write set: " << endl);
    IF_LOOPLOG2(for (auto memLoc
                     : writeSets) { loopLog2 << "\t\t" << *memLoc << endl; });
    LOOPLOG2("\tRecognised array bases:" << endl);
    IF_LOOPLOG2(for (auto &memBase
                     : loop->arrayAccesses) {
        loopLog2 << "\t\t" << memBase.first << " ";
        // print the accessed instructions
        for (auto ml : memBase.second) {
            if (ml) {
                for (auto r : ml->readBy)
                    loopLog2 << "r" << dec << r->id << " ";
                for (auto w : ml->writeFrom)
                    loopLog2 << "w" << dec << w->id << " ";
            }
        }
        loopLog2 << endl;
    });
}

static void prepareLoopIterators(Loop *loop) {}
