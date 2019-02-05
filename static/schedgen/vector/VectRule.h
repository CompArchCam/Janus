/*! \file VectRule.h
 *  \brief Rule generation for automatic vectorisation
 *  
 *  This header defines the API to generate rules for vectorisation
 */
#ifndef _VECTOR_RULE_GEN_
#define _VECTOR_RULE_GEN_

#include "SchedGenInt.h"
#include "Loop.h"

/** \brief Generate VECTOR related rules */
void
generateVectorRules(JanusContext *gc);
/** \brief Check if all loops memory accesses are aligned */
bool alignmentAnalysis(janus::Loop &loop);
/** \brief Whether this instruction is after the induction variable update */
bool checkPostIteratorUpdate(janus::Instruction &instr);

#endif
