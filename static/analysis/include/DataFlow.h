/** @file DataFlow.h
 *  @brief Defines a generic dataflow analysis framework, which all concrete
 *        dataflow analysis sharing a similar form should inherit from
 *
 *  For further reference, check
 *
 * Aho, A., Ullman, J., Sethi, R., & Lam, M. (2006). Compilers: Principles,
 * Techniques, and Tools. In 9.2 Introduction to Data-Flow Analysis & 9.3
 * Foundations of Data-Flow Analysis (2nd ed., pp. 597–632). Addison Wesley.
 */

#ifndef _JANUS_DataFlow_
#define _JANUS_DataFlow_

#include <algorithm>
#include <functional>
#include <iterator>
#include <map>
#include <type_traits>

#include "Concepts.h"
#include "Instruction.h"

/**
 * @brief Concept defining the requires the input to a data flow analysis must
 * fullfill
 *
 * The input to a data flow analysis must fullfill:
 *  1. Must provide the BasicBlock of the function and the relations between
 * them (implicitly or explicitly)
 *  2. Must provide the a reference to the function (in order to retrieve the
 * instructions).
 *
 * TODO: change this when we move instructions out of the function class
 */

template <typename T>
concept DataFlowInput = requires
{
    requires ProvidesBasicCFG<T>;

    // TODO: change this when we move instructions out of the function class
    requires ProvidesFunctionReference<T>;
};

template <DataFlowInput DfIn, typename Trait>
class DataFlow : public DfIn // Same structure as other refactored analysis
{
  protected:
    using propertySet = std::set<Trait>;

    auto getEmptyBitVector() -> janus::BitVector
    {
        return janus::BitVector{universeSize, false};
    }

    auto toBitVector(propertySet propSet) -> const janus::BitVector
    {
        auto res = getEmptyBitVector();
        for (auto &prop : propSet) {
            res.insert(propertyIndex.at(prop));
        }
        return res;
    }

    /* TODO: Change the structure to conform to the structure introduced by the
     * dragon book
     */

    /// Actual loop to generate all the necessary results
    void generate()
    {
        // First pass: populate *store* with all the "Properties" (all possible
        // elements)
        generateUniverseSet();

        // Second pass: Compute the properties based on the gen, kill functions
        // provided until convergence
        bool converged;
        do {
            converged = true;
            for (janus::Instruction *instr : order()) {
                auto genSet = toBitVector(gen(*instr));
                auto killSet = toBitVector(kill(*instr));

                auto inputsInstrs =
                    dependence(instr); // set of janus::Instruction*
                auto inputBitVectors = std::vector<janus::BitVector>{};
                std::transform(inputsInstrs.begin(), inputsInstrs.end(),
                               std::back_inserter(inputBitVectors),
                               [this](auto x) { return store[x]; });

                auto res = transferFunction(*instr, inputBitVectors);

                // Test convergence
                if (converged && store.contains(instr) &&
                    store.at(instr) == res) {
                    continue;
                } else {
                    store[instr] = res;
                    converged = false;
                }
            }
        } while (!converged);
    }

  private:
    // Stores the analysis result for each node/instruction
    std::unordered_map<janus::Instruction *, janus::BitVector> store;

    // Hash map mapping from specific property to index in BitVector
    std::map<Trait, uint64_t> propertyIndex;

    // Hash map from bitvector index to property
    std::unordered_map<uint64_t, Trait> indexProperty;

    // Cached results for queries
    std::unordered_map<janus::Instruction *, propertySet> cachedQueries;

    int universeSize;

    /// Function to convert bit vector into set of properies
    auto getProperties(janus::Instruction *instr) -> propertySet
    {
        auto res = propertySet{};
        auto indexSet = std::set<uint32_t>{};
        store[instr].toSTLSet(indexSet);
        for (auto id : indexSet) {
            res.insert(indexProperty[id]);
        }
        cachedQueries[instr] = res;
        return res;
    }

    /// Generate the universe set for the collection and maps bitvector index to
    /// element
    void generateUniverseSet()
    {
        propertySet allProperties{};
        for (auto instr : DfIn::func.instrs) {
            auto genset = gen(instr);
            auto killset = kill(instr);
            allProperties.insert(genset.begin(), genset.end());
            allProperties.insert(killset.begin(), killset.end());
        }
        int id = 0;
        for (auto property : allProperties) {
            propertyIndex[property] = id;
            indexProperty[id++] = property;
        }
        universeSize = propertyIndex.size();
    }

    auto order() -> std::vector<janus::Instruction *>
    {
        std::vector<janus::Instruction> &instrs = DfIn::func.instrs;
        std::vector<janus::Instruction *> topoOrder{};
        std::vector<bool> visited(instrs.size(), false);

        std::function<void(janus::Instruction *)> dfs =
            [=, &visited, &topoOrder, &dfs, this](janus::Instruction *instr) {
                visited[instr->id] = true;
                for (auto pred : dependants(instr)) {
                    if (!visited[pred->id]) {
                        dfs(pred);
                    }
                }
                topoOrder.push_back(instr);
            };

        for (auto &instr : instrs) {
            if (!visited[instr.id]) {
                dfs(&instr);
            }
        }

        std::reverse(topoOrder.begin(), topoOrder.end());

        return topoOrder;
    }

  public:
    /// Pure virtual function. Returns the dependence of the instruction in
    /// question
    /// TODO: Rename!
    virtual auto dependence(janus::Instruction *)
        -> std::set<janus::Instruction *> = 0;

    /// Pure virtual function. Returns the dependence
    /// TODO: Rename!
    virtual auto dependants(janus::Instruction *)
        -> std::set<janus::Instruction *> = 0;

    /**
     * @brief "Meet" operator joining the set all input results
     */
    virtual void meet(janus::BitVector &, janus::BitVector &) = 0;

    /**
     *  @brief Produce the "gen" set of the instruction.
     *
     * This function is used to produce the "universe" containing all
     * possible element generated by all instructions in a function before
     * creating a map between each element and a bit in the bit vector
     *
     * @param instruction whose gen set we are calculating
     */
    virtual auto gen(janus::Instruction) -> propertySet = 0;

    /// Pure virtual function. Returns the kill set of the instruction
    virtual auto kill(janus::Instruction) -> propertySet = 0;

    virtual auto transferFunction(janus::Instruction instr,
                                  std::vector<janus::BitVector> dependence)
        -> janus::BitVector = 0;

    /// @brief Query interface: Get specific property at instruction
    auto getAt(janus::Instruction *instr) -> std::set<Trait>
    {
        if (!store.contains(instr)) {
            return propertySet{};
        }

        if (cachedQueries.contains(instr)) {
            return cachedQueries.at(instr);
        } else {
            auto res = propertySet{};
            auto indexSet = std::set<uint32_t>{};
            store[instr].toSTLSet(indexSet);
            for (auto id : indexSet) {
                res.insert(indexProperty[id]);
            }
            cachedQueries[instr] = res;
            return res;
        }
    }

    /// @brief Query interface: Get "in" properties at instruction
    auto getInAt(janus::Instruction *instr) -> propertySet {}

    /// @brief Query interface: Get "out" properties at instruction
    auto getOutAt(janus::Instruction *instr) -> propertySet {}

    /// @brief Constructor; once constructed all information are available for
    /// retrieval
    DataFlow(const DfIn &input) : DfIn(input) {}
};

#endif
