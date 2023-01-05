/*! \file LoopSelect.h
 *  \brief Select loops for parallelisation based on different assumptions
 *  
 *  This header defines the API to generate rules for parallelisation
 */
#ifndef _PARALLEL_LOOP_SELECT_
#define _PARALLEL_LOOP_SELECT_

#include <set>
#include "janus.h"
#include "Loop.h"
#include "Function.h"

/** \brief Return the set of DOALL loop IDs from the loop pool
 *  \param jc The global context containing all the loop information
 *  \param[out] selected Recognised DOALL loops are returned here. 
 *  
 *  A DOALL loop is selected based on two assumptions:
 *  *No cross-iteration dependences.
 *  *Clear induction variables */
//void selectDOALLLoops(JanusContext *jc, std::set<LoopID> &selected);
void selectDOALLLoops(std::set<LoopID> &selected, std::vector<janus::Function>& functions, std::vector<janus::Loop>& loops, LoopAnalysisReport loopAnalysisReport, janus::Function *fmain, std::string name);

/** \brief Filter out the loops that are in the same loop nests
 *  \param jc The global context containing all the loop information
 *  \param[in,out] selected Filtered DOALL loops are read and returned here. 
 *  
 *  Only one loop in one loop nests are selected
 *  The outter loop is alreadys preferred. */
//void filterLoopNests(JanusContext *jc, std::set<LoopID> &selected);
void filterLoopNests(std::set<LoopID> &selected, std::vector<janus::Loop>& allLoops);

/** \brief Filter out the loops that are not beneficial for parallelisation
 *  based on heuristics
 *  
 *  Current heuristics are:
 *  1. Small loops are rejected for parallelisation */
//void filterLoopHeuristicBased(JanusContext *jc, std::set<LoopID> &selected);
void filterLoopHeuristicBased(JanusContext *jc, std::set<LoopID> &selected, std::vector<janus::Loop>& allLoops);

/** \brief Examine the loop's dynamic question
 *  
 *  If there are too many questions for a loop, simply reject this loop */
void
checkRuntimeQuestion(JanusContext *jc, std::set<LoopID> &selected);

/** \brief Return false if the loop is not safe to parallelise
 *  
 *  E.g. indirect calls, no induction variables, unsafe function calls */
//bool checkSafetyForParallelisation(janus::Loop &loop);
bool checkSafetyForParallelisation(janus::Loop &loop, std::vector<janus::Function>& functions);
#endif
