/*! \file schedule_format.h
 *  \brief Defines the format of the rewrite schedule
 *
 *  This header defines the layout of the rewrite schedule
 *  It defines the content of the program header, loop header for the schedule.
 *  A schedule file contains the following structure
 *  Header
 *  Loop Header
 *  Rule instructions
 *  Rule data
 */
#ifndef _JANUS_REWRITE_SCHEDULE_FORMAT_
#define _JANUS_REWRITE_SCHEDULE_FORMAT_

#include "loop_format.h"

/** \brief Rewrite schedule header */
typedef struct rsched_header {
    ///Type of the rewrite schedule
    uint32_t        ruleFileType;
    uint32_t        multiMode;
    /* Offsets */
    uint32_t        loopHeaderOffset;
    uint32_t        ruleInstOffset;
    uint32_t        ruleDataOffset;
    /* Meta information */
    uint32_t        numRules;
    uint32_t        numLoops;
    uint32_t        numFuncs;
    uint32_t        dummy;
} RSchedHeader;

/** \brief Rewrite schedule meta information
 *
 * This data is constructed once the rewrite schedule is loaded dynamically */
typedef struct _janus_info {
    JMode           mode;
    uint32_t        number_of_threads;
    uint32_t        number_of_cores;
    uint32_t        number_of_functions;
    RSchedHeader    *header;
    RSLoopHeader    *loop_header;
    uint32_t        channel;
    uint32_t        number_of_variables;
    JVarProfile     *currentProfile;
} RSchedInfo;

#endif
