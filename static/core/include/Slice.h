/** \file Slice.h
 *  \brief Specification and analysis related to program slices */
#ifndef _JANUS_SLICE_
#define _JANUS_SLICE_

#include "janus.h"
#include <list>
#include <set>
#include <map>
#include <iostream>

namespace janus {

class Instruction;
class VarState;
class Loop;

/** \brief Represents a list of instructions in a specific order */
class Slice
{
public:
    enum SliceScope
    {
        GeneralScope,    ///An incomplete slice, or just a list of instructions
        CyclicScope,     ///Slice inputs are previous loop iterators and loop constants
        LoopScope,       ///Slice inputs are loop constant, induction variables and memory accesses
        FunctionScope,   ///Slice inputs are function arguments and memory accesses
    };
    ///the scope of the current slice
    SliceScope scope;
    ///a doubly linked list of instructions
    std::list<Instruction*> instrs;
    ///a set of inputs for the list
    std::set<VarState*> inputs;

    Slice():scope(GeneralScope){};
    ///create a instruction slice for variable state vs
    Slice(VarState *vs, SliceScope scope, Loop *loop);
};

std::ostream& operator<<(std::ostream& out, const Slice& slice);

} /* END Janus namespace */

#endif
