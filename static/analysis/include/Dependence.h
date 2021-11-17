/*! \file Dependence.h
 *  \brief Dependence analysis functions
 *
 * Currently, only the variable related dependence analysis are analysed.
 */
#ifndef _JANUS_DEPD_
#define _JANUS_DEPD_

#include "janus.h"
#include "Variable.h"
#include "Expression.h"

/** \brief Dependence analysis for Janus variables
 *
 * Traverse the SSA graph for each phi node that the start node of the loop
 * Construct a cyclic expression for each phi node 
 */
void dependenceAnalysis(janus::Loop *loop);


/** \brief Cyclic analysis for Janus expressions
 *
 * Traverse the SSA graph and
 * Construct a cyclic expression for the given expression
 */
bool buildCyclicExpr(janus::Expr *expr, janus::Loop *loop);
#endif