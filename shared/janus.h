/*! \file janus.h Top level headers, shared by both static analyser and dynamic binary translator */

#ifndef _JANUS_
#define _JANUS_

#ifdef __cplusplus
extern "C" {
#endif

/* exact-width integer type */
#include <stdint.h>

#ifdef JANUS_VERBOSE
  #define IF_VERBOSE(x) x
#else
  #define IF_VERBOSE(x)
#endif

#ifdef JANUS_VERBOSE
  #define IF_VERBOSE_ELSE(x,y) x
#else
  #define IF_VERBOSE_ELSE(x,y) y
#endif

#ifdef JANUS_DEBUG_MODE
  #define IF_DEBUG(x) x
#else
  #define IF_DEBUG(x)
#endif

#ifdef JANUS_DEBUG_MODE
  #define IF_DEBUG_ELSE(x,y) x
#else
  #define IF_DEBUG_ELSE(x,y) y
#endif

/* JANUS Data Types */
typedef uintptr_t       PCAddress;
typedef uintptr_t       GRegWord;
typedef uint64_t        GBitMask;
typedef uint32_t        InstID;
typedef uint32_t        BlockID;
typedef uint32_t        FuncID;
typedef uint32_t        LoopID;

#define NUM_OF_GENERAL_REGS     16
//#define JANUS_VECT_SUPPORT

/** \brief Rewrite rule generation mode for Janus

 * In the future, we will support multiple modes together */
typedef enum _jmode {
    JNONE,
    ///loop coverage profiling mode
    JLCOV,
    ///function coverage profiling mode
    JFCOV,
    ///generic loop profiling mode
    JPROF,
    ///parallelisation mode
    JPARALLEL,
    ///vectorisation mode
    JVECTOR,
    ///software prefetch mode
    JFETCH,
    ///static analysis mode
    JANALYSIS,
    ///graph mode
    JGRAPH,
    ///single-threaded optimisation mode
    JOPT,
    //secure execution
    JSECURE,
    ///custom tool mode for Cinnamon domain specific language 
    JCUSTOM,
    //testing mode for DLL instrumentation
    JDLL
} JMode;

/* Rule ISA header defines the supported static rules */
#include "rule_isa.h"

/* The variable definition across the whole Janus platform */
#include "janus_var.h"

/* Rewrite schedule format header defines the format for the rule file */
#include "rsched_format.h"

/* Rewrite schedule feedback header */
#include "rule_feedback.h"

#ifdef __cplusplus
}
#endif

#endif
