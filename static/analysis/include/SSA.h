/*! \file SSA.h
 *  \brief Single Static Assignment Related Analysis
 *
 *  This header contains the Single Static Assignment related construction and
 * analysis.
 */

#ifndef _Janus_SSA_
#define _Janus_SSA_

#include "Arch.h"
#include "ControlFlow.h"
#include "JanusContext.h"
#include "Operand.h"
#include "Utility.h"

#include <concepts>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <queue>
#include <utility>

#include "Function.h"
#include "Loop.h"
#include "janus.h"

/** \brief After analysis is done, this function fills the functions
 *         [equalgroup](@ref janus::Function::equalgroup) field.
 */
void saveEqualGroups(janus::Function *function);

template <typename T>
concept ProvidesDominanceTree = requires(T cfg)
{
    typename T;
    // clang-format off
    // Checks for content of the class
    // Note that we can check other properties of all classes
    { cfg.idoms } -> std::convertible_to<std::unordered_map<janus::BasicBlock *, janus::BasicBlock *>>;
    { cfg.dominanceFrontiers } -> std::convertible_to<std::unordered_map<janus::BasicBlock *, std::set<janus::BasicBlock *>>>;
    { cfg.domTree } -> std::convertible_to<std::shared_ptr<std::vector<janus::BitVector>>>;
    // clang-format on
};

template <typename T>
concept ProvidesBasicCFG = requires(T cfg)
{
    typename T;

    // clang-format off
    { cfg.blocks } -> std::convertible_to<std::vector<janus::BasicBlock>&>;
    { *(cfg.entry) } -> std::convertible_to<janus::BasicBlock>;
    // clang-format on
};

template <typename T>
concept ProvidesFunctionReference = requires(T cfg)
{
    typename T;
    // clang-format off
    { cfg.func } -> std::convertible_to<janus::Function&>;
    // clang-format on
};

template <ProvidesDominanceTree DomCFG>
requires ProvidesBasicCFG<DomCFG> && ProvidesFunctionReference<DomCFG>
class SSAGraph : public DomCFG
{
  private:
    void copyDefinitions(std::map<janus::Variable, janus::VarState *> *,
                         std::map<janus::Variable, janus::VarState *> *);

    void insertPhiNodes(janus::Variable var);

    void
    updatePhiNodes(BlockID bid,
                   std::map<janus::Variable, janus::VarState *> &previousDefs);

    void linkSSANodes();

    void
    updateSSANodes(BlockID bid,
                   std::map<janus::Variable, janus::VarState *> &latestDefs);

    janus::VarState *
    getOrInitVarState(janus::Variable var,
                      std::map<janus::Variable, janus::VarState *> &latestDefs);

    void
    linkMemoryNodes(janus::Variable var, janus::VarState *vs,
                    std::map<janus::Variable, janus::VarState *> &latestDefs);

    void
    linkShiftedNodes(janus::Variable var, janus::VarState *vs,
                     std::map<janus::Variable, janus::VarState *> &latestDefs);

    void linkDependentNodes();

    void
    guessCallArgument(janus::Instruction &instr,
                      std::map<janus::Variable, janus::VarState *> &latestDefs);

    void createSuccFromPred();

    /** \brief builds the SSA graph of the given function.
     *
     * A call to this function is required in order that the SSA graph
     * consisting of *janus::MachineInstruction* and *janus::VariableState*
     * objects is linked. It is automatically called from
     * *janus::Function::translate*, which also makes sure prerequisites are
     * satisfied.
     */
    void buildSSAGraph();

    /** \brief Returns the phi node position of a given janus::Variable var.
     *  \param function Function for analysis
     *  \param var A variable for analysis
     *  \param[out] bbs Set of basic blocks that generates defination of this
     * variable \param[out] phiblocks Set of dominance frontier (blocks) of the
     * bbs.
     *
     *  It requires the inputs and outputs of each basic block is collected as
     * prerequisites.
     */
    void buildDominanceFrontierClosure(
        janus::Variable var, std::set<janus::BasicBlock *> &bbs /*OUT*/,
        std::set<janus::BasicBlock *> &phiblocks); /*OUT*/

  public:
    SSAGraph(const DomCFG &cfg) : DomCFG(cfg) { buildSSAGraph(); }
};

#endif
