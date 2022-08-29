#ifndef _JANUS_Liveness_
#define _JANUS_Liveness_

#include "DataFlow.h"

template <DataFlowInput DfIn>
class Liveness : public DataFlow<DfIn, janus::VarState>
{
  protected:
    using propertySet = typename DataFlow<DfIn, janus::VarState>::propertySet;
    using base = DataFlow<DfIn, janus::VarState>;

  public:
    // NOTE: Current implementation is purely intra-procedural and does not
    // handle about inter-procedural liveness at all
    auto dependence(janus::Instruction *instr)
        -> std::set<janus::Instruction *> override
    {
        auto bb = instr->block;

        if (instr->id != bb->endInstID) {
            return {&bb->instrs[(instr->id - bb->startInstID) + 1]};
        } else {
            if (bb->terminate) { // If current basic block is end of function
                return {};       // Return empty set
            } else {
                auto res = std::set<janus::Instruction *>{};
                if (bb->succ1)
                    res.insert(&bb->succ1->instrs[0]);
                if (bb->succ2)
                    res.insert(&bb->succ2->instrs[0]);
                return res;
            }
        }
    }

    auto dependants(janus::Instruction *instr)
        -> std::set<janus::Instruction *> override
    {
        auto startID = instr->block->startInstID;
        auto parentBB = *(instr->block);

        if (instr->id > startID) {
            // Immediate previous instruction in the BB
            return {&(parentBB.instrs[(instr->id - startID) - 1])};
        } else {
            // All end instructions of predecessor BBs
            auto res = std::set<janus::Instruction *>{};
            for (auto bb : parentBB.pred) {
                res.insert(bb->lastInstr());
            }
            return res;
        }
    }

    /**
     * For Liveness, the gen set of the instruction is the set of all (SSA)
     * variables used in the current instruction.
     *
     * @param instr is the instruction whose gen set we want to calculate
     */
    auto gen(janus::Instruction instr) -> propertySet override
    {
        auto res = propertySet{};
        std::transform(instr.inputs.begin(), instr.inputs.end(),
                       std::inserter(res, res.begin()),
                       [](auto v) { return *v; });
        return res;
    }

    /**
     * For Liveness, the kill set of the instruction is the set of all (SSA)
     * variables defined in the current instruction.
     *
     * @param instr is the instruction whose kill set we want to calculate
     */
    auto kill(janus::Instruction instr) -> propertySet override
    {
        auto res = propertySet{};
        std::transform(instr.outputs.begin(), instr.outputs.end(),
                       std::inserter(res, res.begin()),
                       [](auto v) { return *v; });
        return res;
    }

    /**
     * @brief "Meet" operator joining the set all input results
     */
    void meet(janus::BitVector &cur, janus::BitVector &rest) override
    {
        cur.merge(rest);
    }

    /**
     * For Liveness, the dataflow equation is:
     * $$
     *    \mathrm{live}(n) =
     *        ((\bigcup\limits_{i \in \mathrm{succ}(n)} \mathrm{live}(i))
     *        \setminus \mathrm{def}(n)) \cup \mathrm{ref}(n)
     * $$
     *
     * @param use           janus::BitVector of variables used by the
     * instruction. Is the gen set of the equation
     * @param def           janus::BitVector of variables defined by the
     * instruction. Is the kill set of the equation
     * @param dependence    std::set of all the instructions calculating the
     * current node would need. In thec case of liveness that would be all
     * succesors of the current instruction.
     */
    auto transferFunction(janus::Instruction instr,
                          std::vector<janus::BitVector> dependences)
        -> janus::BitVector override
    {
        janus::BitVector res{};
        // Step 1: Big Union live set of all dependences
        for (auto &bv : dependences) {
            meet(res, bv);
        }

        // Step 2: Subtract def set
        auto killBitVector = base::toBitVector(kill(instr));
        res.subtract(killBitVector);

        // Step 3: Union ref set
        auto genBitVector = base::toBitVector(gen(instr));
        res.merge(genBitVector);

        return res;
    }

    Liveness(const DfIn &dfin) : DataFlow<DfIn, janus::VarState>(dfin){};
};

#endif
