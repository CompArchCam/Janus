/*! \file Affine.h
 *  \brief Affine analysis for loops
 *
 * Affine analysis is used for detecting a specific type of loops that have
 * constant loop bounds and regular memory accesses
 */
#ifndef _JANUS_AFFINE_ANALYSIS_
#define _JANUS_AFFINE_ANALYSIS_

#include "Loop.h"

/** \brief Affine analysis for a given loop
 *
 * Note that this analysis must be called after loop iterator analysis
 */
void affineAnalysis(janus::Loop *loop);
#endif