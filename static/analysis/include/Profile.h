#ifndef _Janus_PROFILE_
#define _Janus_PROFILE_

#include "Loop.h"

class JanusContext;

void
getBodyTemperature(JanusContext *gc, janus::Loop *loop);

//void
//loadLoopSelection(JanusContext *gc);

LoopAnalysisReport loadLoopSelection(std::vector<janus::Loop>& loops, std::string executableName);

void
loadDDGProfile(JanusContext *gc);

//void loadLoopCoverageProfiles(JanusContext *gc);
void loadLoopCoverageProfiles(std::string name, std::vector<janus::Loop>& loops);

//filter out the loops that are not beneficial
void
filterParallelisableLoop(JanusContext *gc);

#define JANUS_LOOP_COVERAGE_THRESHOLD 1.0
#define JANUS_LOOP_MIN_ITER_COUNT 15

#endif
