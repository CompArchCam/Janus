#ifndef _JANUS_DataFlow_
#define _JANUS_DataFlow_

/**
 * API design: Since all applications of the data flow framework are
 * calculating a property of the program on a per instruction level,
 * it makes sense to expose a few get methods (providing both the
 * final results, as well as the in/out properties) as final output
 * interface.
 *
 * On the other hand, the overhead cost to compare and store expressions
 * is quite significant. Hence I recommend the use of get methods
 * to provide flexibility on the specific data structure used to store
 * the final results.
 */
#include <algorithm>
#include <iterator>
#include <map>
#include <type_traits>

#include "Concepts.h"
#include "ControlFlow.h"
#include "Expression.h"
#include "Instruction.h"
#include "SSA.h"
#include "Variable.h"

template <typename T>
concept DataFlowInput = requires
{
    requires ProvidesBasicCFG<T>;
    // requires ProvidesSSA<T>;
};

namespace dataflow_test
{
using TestDFInput = SSAGraph<DominanceAnalysis<ControlFlowGraph>>;
static_assert(DataFlowInput<TestDFInput>);
} // namespace dataflow_test

template <template <typename T> typename DataFlowSpec,
          typename DataFlowInputType>
concept ProvidesDataFlowSpecs = requires(DataFlowSpec<DataFlowInputType> t)
{
    requires DataFlowInput<DataFlowInputType>;

    /// Must define the type the dataflow analysis outputs
    typename decltype(t)::Property;

    // clang-format off

    /// This checks that 
    ///     DataFlowSpec(std::vector<janus::Instruction*>&, const DataFlowInput auto&) 
    /// is a valid constructor.
    /// and that DataFlowSpec is also destructible
    requires std::constructible_from<
                // Type to construct
                decltype(t),
                // Arguments {
                    std::vector<janus::Instruction*>&,
                    DataFlowInputType&
                // }
                >;
    // NOTE: { t } -> std::constructible_from<...> would probably be better
    // but unfortunately, { t } -> ... syntax can only check the *reference*
    // to the type of t against concepts, and no reference can be constructed
    // by a constructor

    requires std::is_destructible_v<decltype(t)>;

    // *ordered()* method should produce a container that
    // provides begin and end iterators, the iterator should at
    // least provide read access (hence allowing access through a
    // ranged based for loop)
    { t.ordered().begin() } -> std::input_iterator<>;
    { t.ordered().end() } -> std::input_iterator<>;

    /// DataFlowSpec should specify an equation that takes in
    /// 1 or many bit vectors as input and produce a single processed
    /// bit vector as output
    { t.eqn.evaluate(std::declval<std::set<janus::BitVector>>()) } -> std::convertible_to<janus::BitVector>;
    // clang-format on
};

template <DataFlowInput DfIn>
class LiveSpecs
{
  private:
    DfIn &inputs;
    std::vector<janus::Instruction *> instrs;

  public:
    using Property = janus::VarState;
    LiveSpecs(std::vector<janus::Instruction *> &, DfIn &in)
        : inputs(in), instrs(instrs){};
    // TODO: This is just a place holder, replace with implementation
    auto ordered() -> std::vector<janus::VarState>;
};

template <DataFlowInput DfIn>
class AvailSpecs
{
  private:
    DfIn &inputs;
    std::vector<janus::Instruction *> instrs;

  public:
    using Property = janus::Expr;
    AvailSpecs(std::vector<janus::Instruction *> &instrs, DfIn &in)
        : inputs(in), instrs(instrs){};
    // TODO: This is just a place holder, replace with implementation
    auto ordered() -> std::vector<janus::VarState>;
};

template <DataFlowInput DfIn, typename Property>
class DataFlow : public DfIn // Same structure as other refactored analysis
{
  protected:
    using propertySet = std::set<Property>;

  private:
    // Input datastructure
    DfIn &input;

    // Stores the analysis result for each node/instruction
    std::unordered_map<janus::Instruction *, janus::BitVector> store;

    // Hash map mapping from specific property to index in BitVector
    std::unordered_map<Property, uint64_t> propertyIndex;

    // Hash map from bitvector index to property
    std::unordered_map<uint64_t, Property> indexProperty;

    // Cached results for queries
    std::unordered_map<janus::Instruction *, std::set<Property>> cachedQueries;

    // TODO: implement this bit
    /// First pass: identifies all possible "property" in question, map each
    /// property to an index in the bit vector
    void fillBitVector() {}

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

    auto toBitVector(propertySet propSet) -> janus::BitVector
    {
        auto res = janus::BitVector{};
        for (auto prop : propSet) {
            res.insert(propertyIndex.at(prop));
        }
        return res;
    }

    /// Actual loop to generate all the necessary results
    void generate()
    {
        // First pass: populate *store* with all the "Properties"
        fillBitVector();

        for (auto instr : order()) {
            // XXX: point of discussion: both inputs and evaluate
            // should logically be able to take in variable numbers of
            // arguments.
            auto genSet = toBitVector(generate(*instr));
            auto killSet = toBitVector(kill(*instr));

            auto inputsInstrs = inputs(&instr); // set of janus::Instruction*
            auto inputBitVectors = std::vector<janus::BitVector>{};
            std::transform(inputsInstrs.begin(), inputsInstrs.end(),
                           std::back_inserter(inputBitVectors),
                           [this](auto instr) { return store[instr]; });
            store[&instr] = evaluate(inputBitVectors);
        }
    }

  public:
    /// Pure virtual function. Returns the dependence of the instruction in
    /// question
    virtual auto dependence(janus::Instruction)
        -> std::set<janus::Instruction> = 0;

    /// Pure virtual function. Returns the gen set of the instruction in
    /// question
    virtual auto gen(janus::Instruction) -> propertySet = 0;

    virtual auto kill(janus::Instruction) -> propertySet = 0;

    virtual auto order() -> std::vector<janus::Instruction *> = 0;

    virtual auto evaluate(janus::BitVector gen, janus::BitVector kill,
                          std::vector<janus::BitVector> dependence)
        -> janus::BitVector = 0;

    // Output: Get specific property at instruction
    auto getAt(janus::Instruction *instr) -> std::set<Property>
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

    // Output: Get "in" properties at instruction
    auto getInAt(janus::Instruction *instr) -> std::set<Property> {}

    // Output: Get "out" properties at instruction
    auto getOutAt(janus::Instruction *instr) -> std::set<Property> {}

    // All these get methods can also have a
    // different Version that takes in the
    // InstructionID as an argument instead
};

template <DataFlowInput DfIn>
class Live : public DataFlow<DfIn, janus::VarState>
{
  protected:
    using propertySet = typename DataFlow<DfIn, janus::VarState>::propertySet;

  private:
    auto topoSortWithCycleHandling();

  public:
    // NOTE: Current implementation is purely intra-procedural and does not care
    // about inter-procedural liveness at all
    auto dependence(janus::Instruction instr)
        -> std::set<janus::Instruction> override
    {
        auto bb = instr.block;
        if (instr.id != bb->endInstID) {
            return {bb->instrs[(instr.id - bb->startInstID) + 1]};
        } else {
            if (bb->terminate) { // If current basic block is end of function
                return {};       // Return empty set
            } else {
                auto res = std::set<janus::Instruction>{};
                if (bb->succ1)
                    res.insert(bb->succ1->instrs[0]);
                if (bb->succ2)
                    res.insert(bb->succ2->instrs[0]);
                return res;
            }
        }
    }

    auto gen(janus::Instruction instr) -> propertySet override
    {
        return instr.inputs;
    }

    auto kill(janus::Instruction instr) -> propertySet override
    {
        return instr.outputs;
    }

    auto order() -> std::vector<janus::Instruction *> override
    {
        return topoSortWithCycleHandling();
    }

    /// For Live, the dataflow equation is:
    /// \mathrm{live}(n) = ((\bigcup\limits_{i \in \mathrm{succ}(n)}
    /// \mathrm{live}(i) \\ \mathrm{def}(n)) \cup \mathrm{ref}(n)
    auto evaluate(janus::BitVector gen, janus::BitVector kill,
                  std::vector<janus::BitVector> dependences)
        -> janus::BitVector override
    {
        janus::BitVector res{};
        // Step 1: Big Union live set of all dependences
        for (auto &bv : dependences) {
            res.merge(bv);
        }
        // Step 2: Subtract def set
        res.subtract(kill);

        // Step 3: Union ref set
        res.merge(gen);

        return res;
    }
};

template <DataFlowInput DfIn>
class Avail : public DataFlow<DfIn, janus::Expr>
{
  protected:
    using propertySet = typename DataFlow<DfIn, janus::VarState>::propertySet;

  public:
};

#endif
