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
#include "Instruction.h"
#include "SSA.h"
#include "Variable.h"

template <typename T>
concept DataFlowInput = requires
{
    requires ProvidesBasicCFG<T>;
    requires ProvidesSSA<T>;
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
    ///     DataFlowSpect(std::vector<janus::Instruction*>&, const DataFlowInput auto&) 
    /// is a valid constructor.
    /// and that the type being checked can be destructed
    requires std::is_constructible_v<decltype(t),
                std::vector<janus::Instruction*>&,
                DataFlowInputType&>;

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
    { t.eqn.evaluate(std::declval<std::set<janus::BitVector>>())} -> std::convertible_to<janus::BitVector>;
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
static_assert(ProvidesDataFlowSpecs<LiveSpecs, dataflow_test::TestDFInput>);

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
static_assert(ProvidesDataFlowSpecs<AvailSpecs, dataflow_test::TestDFInput>);

template <template <typename _> typename DfSpec, DataFlowInput DfIn>
requires ProvidesDataFlowSpecs<DfSpec, DfIn>
class DataFlow : public DfIn // Same structure as other refactored analysis
{
  private:
    // Input datastructure
    DfIn &input;

    using Spec = DfSpec<DfIn>;
    using propertySet = std::set<typename Spec::Property>;

    // specification object that contains all the instance method we use
    Spec spec;

    // Stores the analysis result for each node/instruction
    std::unordered_map<janus::Instruction *, janus::BitVector> store;

    // Hash map mapping from specific property to index in BitVector
    std::unordered_map<typename Spec::Property, uint64_t> propertyIndex;

    // Hash map from bitvector index to property
    std::unordered_map<typename Spec::Property, uint64_t> indexProperty;

    // Cached results for queries
    std::unordered_map<janus::Instruction *, std::set<typename Spec::Property>>
        cachedQueries;

    // TODO: implement this bit
    /// First pass: identifies all possible "property" in question, map each
    /// property to an index in the bit vector
    void findAllProperties() {}

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

    /// Actual loop to generate all the necessary results
    void generate()
    {
        // First pass: populate *store* with all the "Properties"
        findAllProperties();

        for (auto &instr : spec.ordered()) {
            // XXX: point of discussion: both inputs and evaluate
            // should logically be able to take in variable numbers of
            // arguments.
            auto inputs = spec.inputs(&instr); // set of janus::Instruction*
            auto inputBitVectors = std::set<janus::BitVector>{};
            std::transform(inputs.begin(), inputs.end(),
                           std::back_inserter(inputBitVectors),
                           [this](auto instr) { return store[instr]; });
            store[&instr] = spec.eqn.evaluate(inputBitVectors);
        }
    }

  public:
    // Output: Get specific property at instruction
    auto getAt(janus::Instruction *instr) -> std::set<typename Spec::Property>
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
    auto getInAt(janus::Instruction *instr) -> std::set<typename Spec::Property>
    {
    }

    // Output: Get "out" properties at instruction
    auto getOutAt(janus::Instruction *instr)
        -> std::set<typename Spec::Property>
    {
    }

    // All these get methods can also have a
    // different Version that takes in the
    // InstructionID as an argument instead
};

template <DataFlowInput DfIn>
using Live = DataFlow<LiveSpecs, DfIn>;

template <DataFlowInput DfIn>
using Avail = DataFlow<AvailSpecs, DfIn>;
#endif
