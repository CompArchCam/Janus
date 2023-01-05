#ifndef _PREFETCH_RULE_GEN_
#define _PREFETCH_RULE_GEN_

#include "SchedGenInt.h"

//"Architecture-dependent" constant -- we want to prefetch variables this many load instructions
//    before they are actually needed
#define PREFETCH_CONSTANT 64

/** \brief Generate prefetching rewrite rules */
//void generatePrefetchRules(JanusContext *jc);
void generatePrefetchRules(JanusContext *jc, std::vector<janus::Loop>& loops);
#endif
