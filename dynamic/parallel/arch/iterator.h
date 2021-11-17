/*! \file emit.h
 *  \brief Runtime handlers for loop iterators
 */
#ifndef _JANUS_PARA_ITERATOR_HANDLER_
#define _JANUS_PARA_ITERATOR_HANDLER_

#include "janus_api.h"
#include "emit.h"

/** \brief Emit induction variable initializations for variables for a given loop */
void emit_init_loop_variables(EMIT_CONTEXT, int tid);

/** \brief Emit merge procedure for variables for a given loop, only executed by the main thread */
void emit_merge_loop_variables(EMIT_CONTEXT);
#endif