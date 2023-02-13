#include "Profile.h"
#include "JanusContext.h"
#include "Loop.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdio>

#include <map>

using namespace std;
using namespace janus;

//void filterParallelisableLoop(JanusContext *gc);
void filterParallelisableLoop(std::vector<janus::Loop>& loops);
#ifdef DDG
static void
//loadDDG(JanusContext *gc, Loop &loop, char *rawDDG, uint32_t size);
loadDDG(JanusContext *gc, Loop &loop, char *rawDDG, uint32_t size);
#endif

typedef struct _tempUnit {
    PCAddress addr;
    double temperature;
} TempUnit;

//void loadLoopCoverageProfiles(JanusContext *gc)
void loadLoopCoverageProfiles(std::string name, std::vector<janus::Loop>& loops)
{
    /* Read loop coverage files from loop coverage profiling */
    ifstream lcov;
    string line;
    //lcov.open(gc->name+".lcov", ios::in);
    lcov.open(name+".lcov", ios::in);
    if (lcov.good()) {
        //GSTEPCONT("\tReading "<<gc->name<<".lcov for coverage filtering"<<endl);
    	GSTEPCONT("\tReading "<<name<<".lcov for coverage filtering"<<endl);
        /* Skip the top four lines */
        getline(lcov, line);
        getline(lcov, line);
        getline(lcov, line);
        getline(lcov, line);
        int loopID, inaccurate;
        uint64_t invocation, iteration;
        float coverage;
        string name;
        printf("		loadLoopCoverageProfiles:probe 1 \n");
        while (lcov >> loopID >> coverage >> invocation>>iteration>>inaccurate>>name) {
            //Loop &loop = gc->loops[loopID-1];
        	Loop &loop = loops[loopID-1];
            loop.coverage = coverage;
            loop.invocation_count = invocation;
            loop.total_iteration_count = iteration;
            loop.inaccurate = inaccurate;
            //cout<<loopID<<" "<<coverage<<" "<<invocation<<" "<<iteration<<" "<<name<<endl;
        }
        printf("		loadLoopCoverageProfiles:probe 2 \n");
    }
    lcov.close();

    //filterParallelisableLoop(gc);
    printf("		loadLoopCoverageProfiles:filterParallelisableLoop --- CALL --- \n");
    filterParallelisableLoop(loops);
    printf("		loadLoopCoverageProfiles:filterParallelisableLoop --- RETURN --- \n");
}

//void filterParallelisableLoop(JanusContext *gc)
void filterParallelisableLoop(std::vector<janus::Loop>& loops)
{
    //for (auto &loop: gc->loops) {
	for (auto &loop: loops) {
        if (loop.coverage > JANUS_LOOP_COVERAGE_THRESHOLD &&
            loop.invocation_count > 0 &&
           (loop.total_iteration_count/loop.invocation_count) > JANUS_LOOP_MIN_ITER_COUNT) {
            cout <<"\t"<<loop.id<<" "<<loop.coverage<<" "<<(loop.total_iteration_count/loop.invocation_count)<<endl;
            loop.removed = false;
        }
        else loop.removed = true;
    }
}

//void loadLoopSelection(JanusContext *jc)
LoopAnalysisReport loadLoopSelection(std::vector<janus::Loop>& loops, std::string executableName)
{
	LoopAnalysisReport loopAnalysisReport;
    /* Read ddg files from manual specification */
    ifstream iselect;
    //iselect.open(jc->name+".loop.select", ios::in);
    iselect.open(executableName+".loop.select", ios::in);

    //if (!iselect.good()) return;
    if (!iselect.good()) return loopAnalysisReport;
    //GSTEPCONT("\tReading "<<jc->name<<".loop.select for loop selection"<<endl);
    GSTEPCONT("\tReading "<<executableName<<".loop.select for loop selection"<<endl);

    int loopID,type;
    while (iselect >> loopID >> type) {
        //Loop &loop = jc->loops[loopID-1];
    	Loop &loop = loops[loopID-1];
        loop.pass = true;
        loop.type = (LoopType)type;
    }

    /* Assign runtime ID */
    int id = 0;
    //for (auto &loop: jc->loops) {
    for (auto &loop: loops) {
        if (loop.pass) {
            loop.header.id = id++;
        }
    }
    //jc->passedLoop = id;
    //jc->manualLoopSelection = true;
    loopAnalysisReport = LoopAnalysisReport(id, true);
    return loopAnalysisReport;
}

void
loadDDGProfile(JanusContext *gc)
{
#ifdef Janus_MANUAL_ASSIST
    /* Read ddg files from manual specification */
    ifstream ifddg;
    ifddg.open(gc->name+".ddg", ios::in);
    if (ifddg.good()) {
        GSTEPCONT("\tReading "<<gc->name<<".loop.ddg for dependence analysis"<<endl);

        int instID,loopID,channel,operation;
        uint64_t addr;
        while (ifddg >> loopID >> instID >> channel>> operation >> addr) {
            SyncInfo &info = (gc->loops[loopID-1].ddg[instID])[channel];
            info.operation += operation;
            info.addr = addr;
        }
    }
    ifddg.close();
#endif
#ifdef DDG
    FILE *ddgFile;
    stringstream ss;
    char *fileBuffer;
    uint32_t fileSize;
    bool hasProfiles = false;

    /* Load loop specific profile */
    for (auto &loop: gc->loops) {
        if (gc->manualLoopSelection && !loop.pass) continue;
        ss << "Loop_"<<dec<<loop.id<<".ddg";
        ddgFile = fopen(ss.str().c_str(), "rb");
        if (ddgFile) {
            fseek(ddgFile, 0, SEEK_END);
            fileSize = ftell(ddgFile);
            rewind(ddgFile);

            fileBuffer = (char *)malloc(fileSize);
            fread(fileBuffer, fileSize, 1, ddgFile);
            cout <<"load "<<ss.str()<<" for evaluation"<<endl;
            loadDDG(gc, loop, fileBuffer, fileSize);
            hasProfiles = true;
            free(fileBuffer);
            fclose(ddgFile);
        }
        ss.str("");
    }

    gc->useProfiles = hasProfiles;

    if (!hasProfiles && !(gc->manualLoopSelection)) {
        for (auto &loop: gc->loops) {
            loop.pass = true;
        }
    }
#endif
}

#ifdef DDG
static void
loadDDG(JanusContext *gc, Loop &loop, char *rawDDG, uint32_t size)
{
    loop.pDDG = new DDG();
    loop.pDDG->header = (DDGHeader *)rawDDG;
    DDG &ddg = *loop.pDDG;
    ddg.edges = (DDGEdge *)(rawDDG + sizeof(DDGHeader) + sizeof(DDGNode) * ddg.header->node_size);
    /* Record cross iteration dependences */
    for (int i=0; i<ddg.header->cross_size; i++) {
        DDGEdge &edge = ddg.edges[i];
        /* We only record dependences on all iterations */
        if (edge.probability >= 0.9) {
            loop.depdReadInsts.insert(edge.to);
            loop.depdReadInsts.insert(edge.from);
        } else if (edge.probability >= 0.4) {
            loop.ambiguousReadInsts.insert(edge.to);
            loop.ambiguousReadInsts.insert(edge.from);
        } else {
            loop.rareDepdInsts.insert(edge.to);
            loop.rareDepdInsts.insert(edge.from);
        }
        //cout <<edge.from<<" -> "<<edge.to<<" prob: "<<edge.probability<<endl;
    }
}
#endif
