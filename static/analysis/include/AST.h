/* Janus Expression/ Abstract Syntax Tree Analysis */

#ifndef _JANUS_AST_
#define _JANUS_AST_

#include "Function.h"

/** \brief builds the abstract syntax tree for the given function.
 *
 * Requires the SSA graph to be constructed. 
 */
void buildASTGraph(janus::Function *function);

#endif