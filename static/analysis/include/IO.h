/*! \file IO.h
 *  \brief Printing utilities and visualisation utilities for the static tool
 */

#ifndef _Janus_DEBUG_PORT_
#define _Janus_DEBUG_PORT_

#define DUMP_LOOP_LOGS
#define DUMP_LOOP_LOGS_LEVEL_2

#include "Function.h"
#include "Loop.h"
#include "VariableState.h"
#include <fstream>
#include <iostream>

class JanusContext;

extern int stepIndex;
#ifdef DUMP_LOOP_LOGS
extern std::ofstream loopLog;
extern std::ofstream loopLog2;
#endif

#define GSTEP(msg) IF_VERBOSE(cout << stepIndex++ << ":\t" << msg)
#define GSTEPCONT(msg) IF_VERBOSE(cout << msg)
#define GSUBSTEP(msg) IF_VERBOSE(cout << stepIndex++ << ":\t\t" << msg)
/* A list of print channels */
#define GASSERT(x, msg)                                                        \
    IF_VERBOSE(if (!(x)) {                                                     \
        std::cerr << "Error: " << msg << std::endl;                            \
        std::exit(-1);                                                         \
    })

#define GASSERT_NOT_IMPLEMENTED(x, msg)                                        \
    IF_VERBOSE(if (!(x)) {                                                     \
        std:                                                                   \
            cerr << msg << " not implemented" << std::endl;                    \
    })

#define ASSERT_NOT_IMPLEMENTED(msg)                                            \
    IF_VERBOSE(std : cerr << msg << " not implemented" << std::endl;)

#ifdef DUMP_LOOP_LOGS
#define LOOPLOGLINE(x) loopLog << x << endl
#define LOOPLOG(x) loopLog << x
#define IF_LOOPLOG(x) x
#else
#define LOOPLOGLINE(x)
#define LOOPLOG(x)
#define IF_LOOPLOG(x)
#endif

#ifdef DUMP_LOOP_LOGS_LEVEL_2
#define LOOPLOG2(x) loopLog2 << x
#define IF_LOOPLOG2(x) x
#else
#define LOOPLOG2(x)
#define IF_LOOPLOG2(x)
#endif

void GIO_Init(const char *name, JMode mode);

void GIO_Exit();

void dumpDisasm(JanusContext *context);

void dumpCFG(JanusContext *context);

void dumpSSA(JanusContext *context);

void dumpLoopSSA(JanusContext *context);

void dumpLoopCFG(JanusContext *context);

void dumpAST(JanusContext *context);

void dumpLoopInfo(JanusContext *context);

void dumpBinaryInfo(JanusContext *context);

void dumpLoopRelations(JanusContext *context);

void dumpDependenceGraph(JanusContext *context);

void generateExeReport(JanusContext *context);

void generateExeReport(JanusContext *context, void *outputStream);

/** \brief print SSA Graph of a function in .dot format */
void printSSADot(janus::Function &function, void *outputStream);
/** \brief print data dependence graph of a function in .dot format */
void printDDGDot(janus::Function &function, void *outputStream);
/** \brief print SSA Graph of a loop in .dot format */
void printSSADot(janus::Loop &loop, void *outputStream);
/** \brief print DDG Graph of a loop in .dot format */
void printDDGDot(janus::Loop &loop, void *outputStream);
/** \brief print all memory operand in the loop and their alias relation */
void printAliasRelation(janus::Loop &loop, void *outputStream);
/** \brief print all induction variable of the loop */
void printInductionVars(janus::Loop &loop, void *outputStream);
/** \brief print the information of the given loop. (PLANX mode only) */
void printLoopInfo(janus::Loop &loop, void *outputStream);
#endif
