#include "JanusContext.h"
#include "ControlFlow.h"
#include "Disassemble.h"
#include "Loop.h"
#include "Profile.h"
#include "SSA.h"

#include <map>
#include <string>

using namespace std;
using namespace janus;

JanusContext::JanusContext(const char* name, JMode mode)
:mode(mode), name(string(name))
{
    passedLoop = 0;
    useProfiles = false;
    manualLoopSelection = false;
    sharedOn = true;
    pltsection = false;
    //open the executable and parse according to the header
    program.open(this, name);

    //lift the binary to disassembly
    program.disassemble(this);
}
void JanusContext::translateFunctions(){
    for(auto &func: functions){
        if(!func.isExternal)
            func.translate();        //this is to ensure we analyse all functions and stack even if no loop inside
    }
}
void JanusContext::buildProgramDependenceGraph()
{
    GSTEP("Building basic blocks: ");
    uint32_t numBlocks = 0;
    /* Step 1: build CFG for each function */
    for (auto &func: functions) {
        if (func.isExecutable) {
            buildCFG(func);
            numBlocks += func.blocks.size();
        }
        else if(func.isExternal && func.minstrs.size()){//to ensure we construct BB external function stubs in plt section 
            buildCFG(func);
            numBlocks += func.blocks.size();
        }
    }
    GSTEPCONT(numBlocks<<" blocks"<<endl);

    /* Step 2: lift the disassembly to IR (the CFG must be ready) */
    GSTEP("Lifting disassembly to IR: ");
    uint32_t numInstrs = 0;
    for (auto &func: functions) {
        if (func.isExecutable && func.blocks.size()) {
            numInstrs += liftInstructions(&func);
        }
    }
    GSTEPCONT(numInstrs<<" instructions lifted"<<endl);

    /* Step 3: construct SSA graph */
    GSTEP("Building SSA graphs"<<endl);
    for (auto &func: functions) {
        if (func.isExecutable && func.blocks.size()) {
            buildSSAGraph(func);
        }
    }

    /* Step 4: construct Control Dependence Graph */
    GSTEP("Building control dependence graphs"<<endl);
    for (auto &func: functions) {
        if (func.isExecutable && func.blocks.size()) {
            buildCDG(func);
        }
    }
}

void JanusContext::analyseLoop()
{
    /* Function timer doesn't need to recognise loop */
    if (mode == JFCOV) return;

    GSTEP("Recognising loops: ");

    /* Step 1: identify loops from the control flow graph */
    for (auto &func: functions) {
        searchLoop(this, &func);
    }

    GSTEPCONT(loops.size()<<" loops recognised"<<endl);

    //for loop coverage profiling, this analysis is enough
    if (mode == JLCOV || mode == JGRAPH) return;

    GSTEP("Analysing loop relations"<<endl);

    /* Step 2: analyse loop relations within one procedure */
    for (auto &func : functions) {
        func.analyseLoopRelations();
    }

    /* Step 3: analyse loop relations across procedures */
    analyseLoopAndFunctionRelations();

    /* Step 4: load profiling information */
    if (mode == JPROF) {
        /* For automatic profiler mode, load loop coverage before analysis */
        loadLoopCoverageProfiles(this);
    }

    if (mode == JPARALLEL || mode == JANALYSIS) {
        //load loop selection from previous run
        loadLoopSelection(this);
    }

    /* Step 5: analyse each loop more in depth (Pass 1) */
    GSTEP("Analysing loops"<<endl);
    for (auto &loop: loops) {
        loop.analyse(this);
    }

    /* When analysis done for all loops, perform post analysis
     * So we can construct inter-loop relations for further analysis */
    /* Step 6: analyse each loop more in depth (Pass 2) */
    GSTEP("Analysing loops - second pass"<<endl);
    for (auto &loop: loops)
        loop.analyse2(this);

    /* Step 7: analyse each loop more in depth (Pass 3) */
    GSTEP("Analysing loops - third pass"<<endl);
    for (auto &loop: loops)
        loop.analyse3(this);
}

void JanusContext::analyseLoopLite()
{
    GSTEP("Recognising loops: ");

    for (auto &func: functions) {
        searchLoop(this, &func);
    }

    GSTEPCONT(loops.size()<<" loops recognised"<<endl);

    GSTEP("Analysing loop relations"<<endl);
    //we analyse loop relations in terms of functions
    for (auto &func : functions) {
        func.analyseLoopRelations();
    }
}

void JanusContext::analyseLoopAndFunctionRelations()
{
    for (auto &loop: loops) {
        if (loop.ancestors.size() == 0) {
            set<LoopID> nest;
            nest.insert(loop.id);

            for (auto l: loop.descendants) {
                nest.insert(l->id);
            }
            loopNests.push_back(nest);
        }
    }

    //assign nest id
    int i=0;
    for (auto &nest: loopNests) {
        for (auto lid: nest) {
            loops[lid-1].nestID = i;
        }
        i++;
    }
}
