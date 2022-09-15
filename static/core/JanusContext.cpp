#include "JanusContext.h"
#include "ControlFlow.h"
#include "Disassemble.h"
#include "Loop.h"
#include "Profile.h"
#include "SSA.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>

using namespace std;
using namespace janus;

JanusContext::JanusContext(const char *name, JMode mode)
    : mode(mode), name(string(name))
{
    passedLoop = 0;
    useProfiles = false;
    manualLoopSelection = false;
    sharedOn = true;
    // open the executable and parse according to the header
    program.open(this, name);

    // lift the binary to disassembly
    program.disassemble(this);
}

void JanusContext::buildProgramDependenceGraph()
{
    GSTEP("Building basic blocks: ");
    uint32_t numBlocks = 0;
    uint32_t numInstrLifted = 0;
    /* Step 1: build CFG for each function */
    for (auto &func : functions) {
        if (func.isExecutable) {
            auto preLoopAnalysis =
                make_unique<decltype(func.cfg)::element_type>(
                    SSAGraph(InstructionControlDependence(PostDominanceAnalysis(
                        DominanceAnalysis(ControlFlowGraph(func))))));
            numBlocks += preLoopAnalysis->blocks.size();
            numInstrLifted += preLoopAnalysis->instructionLiftCount;
            func.cfg = std::move(preLoopAnalysis);
        }
    }
    cout << numBlocks << " blocks" << endl;
    cout << numInstrLifted << " instructions are lifted to IR" << endl;
}

static void loopAnalysisFirstPass(JanusContext *jc)
{
    for (auto &loop : jc->loops) {
        loop.analyse(jc);
    }
}

static void loopAnalysisSecondPass(JanusContext *jc)
{
    for (auto &loop : jc->loops) {
        loop.analyse2(jc);
    }
}

static void loopAnalysisThirdPass(JanusContext *jc)
{
    for (auto &loop : jc->loops) {
        loop.analyse3(jc);
    }
}

// TODO: change the name
static void analysisAnalysis(JanusContext *jc)
{
    if (jc->mode != JANALYSIS) {
        std::cerr << "Wrong Mode: Expecting " << JANALYSIS << " Actually have "
                  << jc->mode << endl;
    }

    /* Step 1: identify loops from the control flow graph*/
    for (auto &func : jc->functions) {
        searchLoop(jc, &func);
    }
    GSTEPCONT(loops.size() << " loops recognised" << endl);

    GSTEP("Analysing loop relations" << endl);
    /* Step 2: analyse loop relations within one procedure */
    for (auto &func : jc->functions) {
        func.analyseLoopRelations();
    }

    /* Step 3: analyse loop relations across procedures */
    jc->analyseLoopAndFunctionRelations();

    /* Step 4: load loop selection from previous run*/
    loadLoopSelection(jc);

    /* Step 5: first loop analysis pass:
     *      analysis set up
     *      variable analysis
     *      dependence analysis
     *      iterator analysis */
    loopAnalysisFirstPass(jc);

    /* Step 6: second loop analysis pass:
     *      post-iterator analysis */
    loopAnalysisSecondPass(jc);

    /* Step 7: final loop analysis pass:
     *      alias analysis
     *      scratch analysis
     *      encode iterators */
    loopAnalysisThirdPass(jc);
}

static void parallelisationAnalysis(JanusContext *jc)
{
    if (jc->mode != JPARALLEL) {
        std::cerr << "Wrong Mode: Expecting " << JPARALLEL << " Actually have "
                  << jc->mode << endl;
    }

    /* Step 1: identify loops from the control flow graph*/
    for (auto &func : jc->functions) {
        searchLoop(jc, &func);
    }
    GSTEPCONT(loops.size() << " loops recognised" << endl);

    GSTEP("Analysing loop relations" << endl);
    /* Step 2: analyse loop relations within one procedure */
    for (auto &func : jc->functions) {
        func.analyseLoopRelations();
    }

    /* Step 3: analyse loop relations across procedures */
    jc->analyseLoopAndFunctionRelations();

    /* Step 4: load loop selection from previous run*/
    loadLoopSelection(jc);

    /* Step 5: first loop analysis pass:
     *      analysis set up
     *      variable analysis
     *      dependence analysis
     *      iterator analysis */
    loopAnalysisFirstPass(jc);

    /* Step 6: second loop analysis pass:
     *      post-iterator analysis */
    loopAnalysisSecondPass(jc);

    /* Step 7: final loop analysis pass:
     *      alias analysis
     *      scratch analysis
     *      encode iterators */
    loopAnalysisThirdPass(jc);
}

static void loadProfilingAnalysis(JanusContext *jc)
{
    if (jc->mode != JPROF) {
        std::cerr << "Wrong Mode: Expecting " << JPROF << " Actually have "
                  << jc->mode << endl;
    }

    /* Step 1: identify loops from the control flow graph*/
    for (auto &func : jc->functions) {
        searchLoop(jc, &func);
    }
    GSTEPCONT(loops.size() << " loops recognised" << endl);

    GSTEP("Analysing loop relations" << endl);
    /* Step 2: analyse loop relations within one procedure */
    for (auto &func : jc->functions) {
        func.analyseLoopRelations();
    }

    /* Step 3: analyse loop relations across procedures */
    jc->analyseLoopAndFunctionRelations();

    /* Step 4: load loop selection from previous run*/
    loadLoopCoverageProfiles(jc);

    /* Step 5: first loop analysis pass:
     *      analysis set up
     *      variable analysis
     *      dependence analysis
     *      iterator analysis */
    loopAnalysisFirstPass(jc);

    /* Step 6: second loop analysis pass:
     *      post-iterator analysis */
    loopAnalysisSecondPass(jc);

    /* Step 7: final loop analysis pass:
     *      alias analysis
     *      scratch analysis
     *      encode iterators */
    loopAnalysisThirdPass(jc);
}

static void loopCoverageAnalysis(JanusContext *jc)
{
    /* Step 1: identify loops from the control flow graph */
    GSTEP("Recognising loops: ");
    for (auto &func : jc->functions) {
        searchLoop(jc, &func);
    }
    GSTEPCONT(loops.size() << " loops recognised" << endl);
}

static void controlFlowGraphAnalysis(JanusContext *jc)
{
    /* Step 1: identify loops from the control flow graph */
    GSTEP("Recognising loops: ");
    for (auto &func : jc->functions) {
        searchLoop(jc, &func);
    }
    GSTEPCONT(loops.size() << " loops recognised" << endl);
}

void JanusContext::analyseLoop()
{
    /* Function timer doesn't need to recognise loop */
    switch (mode) {
    case JFCOV:
        return;
    case JLCOV: {
        loopCoverageAnalysis(this);
        return;
    };
    case JGRAPH: {
        controlFlowGraphAnalysis(this);
        return;
    };
    case JPARALLEL: {
        parallelisationAnalysis(this);
        return;
    };
    case JANALYSIS: {
        analysisAnalysis(this);
        return;
    };
    case JPROF: {
        loadProfilingAnalysis(this);
        return;
    }
    default:
        break;
    }
    GSTEP("Recognising loops: ");

    /* Step 1: identify loops from the control flow graph */
    for (auto &func : functions) {
        searchLoop(this, &func);
    }

    GSTEPCONT(loops.size() << " loops recognised" << endl);

    // for loop coverage profiling, this analysis is enough
    if (mode == JLCOV || mode == JGRAPH)
        return;

    GSTEP("Analysing loop relations" << endl);

    /* Step 2: analyse loop relations within one procedure */
    for (auto &func : functions) {
        func.analyseLoopRelations();
    }

    /* Step 3: analyse loop relations across procedures */
    analyseLoopAndFunctionRelations();

    /* Step 5: analyse each loop more in depth (Pass 1) */
    GSTEP("Analysing loops" << endl);
    for (auto &loop : loops) {
        loop.analyse(this);
    }

    /* When analysis done for all loops, perform post analysis
     * So we can construct inter-loop relations for further analysis */
    /* Step 6: analyse each loop more in depth (Pass 2) */
    GSTEP("Analysing loops - second pass" << endl);
    for (auto &loop : loops)
        loop.analyse2(this);

    /* Step 7: analyse each loop more in depth (Pass 3) */
    GSTEP("Analysing loops - third pass" << endl);
    for (auto &loop : loops)
        loop.analyse3(this);
}

void JanusContext::analyseLoopLite()
{
    GSTEP("Recognising loops: ");

    for (auto &func : functions) {
        searchLoop(this, &func);
    }

    GSTEPCONT(loops.size() << " loops recognised" << endl);

    GSTEP("Analysing loop relations" << endl);
    // we analyse loop relations in terms of functions
    for (auto &func : functions) {
        func.analyseLoopRelations();
    }
}

void JanusContext::analyseLoopAndFunctionRelations()
{
    for (auto &loop : loops) {
        if (loop.ancestors.size() == 0) {
            set<LoopID> nest;
            nest.insert(loop.id);

            for (auto l : loop.descendants) {
                nest.insert(l->id);
            }
            loopNests.push_back(nest);
        }
    }

    // assign nest id
    int i = 0;
    for (auto &nest : loopNests) {
        for (auto lid : nest) {
            loops[lid - 1].nestID = i;
        }
        i++;
    }
}
