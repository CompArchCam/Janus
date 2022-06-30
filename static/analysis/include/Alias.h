/*! \file Alias.h
 *  \brief Alias analysis functions
 *
 * Currently, only the variable related dependence analysis are analysed.
 */
#ifndef _JANUS_ALIAS_
#define _JANUS_ALIAS_

#include "Loop.h"
#include "MemoryLocation.h"

enum AliasType { MustAlias, MustNotAlias, UnknownAlias };

/** \brief Alias analysis for a given loop
 */
void aliasAnalysis(janus::Loop *loop);

/** \brief Check alias relation for two memory locations
 */
void checkAliasRelation(janus::MemoryLocation &m1, janus::MemoryLocation &m2,
                        janus::Loop *loop);
/** \brief Insert the memory location and return the pointer if exist
 */
janus::MemoryLocation *getOrInsertMemLocations(janus::Loop *loop,
                                               janus::MemoryLocation *loc);
#endif
