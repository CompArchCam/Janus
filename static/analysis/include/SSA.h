/*! \file SSA.h
 *  \brief Single Static Assignment Related Analysis
 *
 *  This header contains the Single Static Assignment related construction and
 * analysis.
 */

#ifndef _Janus_SSA_
#define _Janus_SSA_

#include <concepts>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <queue>
#include <utility>

#include "Concepts.h"
#include "janus.h"

template <typename T>
concept SSARequirement = requires
{
    requires ProvidesDominanceFrontier<T>;
    requires ProvidesBasicCFG<T>;
    requires ProvidesFunctionReference<T>;
    requires std::copy_constructible<T>;
};

template <SSARequirement DomCFG>
class SSAGraph : public DomCFG
{
  public:
    SSAGraph(const DomCFG &);

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
};

/** \brief After analysis is done, this function fills the functions
 *         [equalgroup](@ref janus::Function::equalgroup) field.
 */
void saveEqualGroups(janus::Function *function);

#endif
