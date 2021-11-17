#ifndef _Janus_INDUCTION_
#define _Janus_INDUCTION_

#include "janus.h"
#include "Function.h"
#include "Loop.h"
#include "Operand.h"

/** \brief Analyse all variables in the loop */
void
inductionAnalysis(janus::Loop *loop);

/** \brief Retrieve the number of iterations statically. Return -1 if iteration count can't be decided */
long int
getLoopIterationCount(janus::Loop *loop);
#endif