#include "Slice.h"
#include "Function.h"
#include "Instruction.h"
#include "Loop.h"
#include "Variable.h"

#include <queue>

using namespace std;
using namespace janus;

Slice::Slice(VarState *vs, SliceScope scope, Loop *loop) : scope(scope)
{
    queue<VarState *> q;
    set<VarState *> visited;
    q.push(vs);

    /* Perform BFS on all predecessors of vs and avoid duplicates */
    if (scope == CyclicScope) {
        while (!q.empty()) {
            VarState *v = q.front();
            q.pop();
            // skip visited node
            if (visited.find(v) != visited.end())
                continue;
            visited.insert(v);
            // if we found an instruction that generate the state, add to the
            // slice
            if (v->lastModified) {
                if (loop->contains(*v->lastModified)) {
                    instrs.push_front(v->lastModified);
                    for (auto vi : loop->parent->getCFG().getSSAVarRead(
                             *v->lastModified)) {
                        q.push(vi);
                    }
                } else {
                    // state defined outside of the loop, simply add to
                    // inputs
                    inputs.insert(v);
                }
            } else {
                // stop if we hit a loop iterator
                if (v->isPHI && loop->iterators.contains(v))
                    continue;
                for (auto pred : v->pred) {
                    q.push(pred);
                }
            }
        }
    } else {
        while (!q.empty()) {
            VarState *v = q.front();
            q.pop();
            // skip visited node
            if (visited.find(v) != visited.end())
                continue;
            visited.insert(v);
            // if we found an instruction that generate the state, add to
            // the slice
            if (v->lastModified) {
                instrs.push_front(v->lastModified);
                for (auto vi :
                     loop->parent->getCFG().getSSAVarRead(*v->lastModified)) {
                    q.push(vi);
                }
            } else {
                for (auto pred : v->pred)
                    q.push(pred);
            }
        }
    }
}

ostream &janus::operator<<(ostream &out, const Slice &slice)
{
    for (auto l : slice.instrs) {
        if (l)
            out << *l << endl;
    }
    return out;
}
