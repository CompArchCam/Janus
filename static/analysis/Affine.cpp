#include "Affine.h"

using namespace janus;
using namespace std;

void affineAnalysis(janus::Loop *loop)
{
    if (loop->staticIterCount <= 0)
        return;
    if (loop->unsafe)
        return;

    // no break statement in the loop body
    if (loop->exit.size() > 1)
        return;

    // assume the memory accesses are already constructed
    // for (auto &memBase: loop->memoryAccesses) {
    //     ScalarEvolution se(memBase)
    // }

    // cout<<"loop "<<loop->id<<" is affine loop with bound
    // "<<dec<<loop->staticIterCount <<endl;
    loop->affine = true;
}