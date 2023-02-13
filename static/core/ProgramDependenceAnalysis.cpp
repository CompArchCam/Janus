/**
 * Class containing all sort of analysis related to dependence in a program.
 */

#include "ProgramDependenceAnalysis.h"
#include "ControlFlow.h"
#include "Loop.h"
#include "Profile.h"
#include "Disassemble.h"
#include "SSA.h"

using namespace janus;

ProgramDependenceAnalysis::ProgramDependenceAnalysis()
{

}


void ProgramDependenceAnalysis::buildCFGForEachFunction(std::vector<janus::Function>& functions, std::map<PCAddress, janus::Function *>& functionMap)
{
    GSTEP("Building basic blocks: ");
    uint32_t numBlocks = 0;
    /* Step 1: build CFG for each function */
    for (auto &func: functions) {
        if (func.isExecutable) {
            //buildCFG(func);
        	buildCFG(func, functionMap);
            numBlocks += func.blocks.size();
        }
    }
    GSTEPCONT(numBlocks<<" blocks"<<endl);
}

void ProgramDependenceAnalysis::liftDisassemblyToIR(std::vector<janus::Function>& functions)
{
	/* Step 2: lift the disassembly to IR (the CFG must be ready) */
	GSTEP("Lifting disassembly to IR: ");
	uint32_t numInstrs = 0;
	for (auto &func: functions) {
	    if (func.isExecutable && func.blocks.size()) {
	        numInstrs += liftInstructions(&func);
	    }
	}
	GSTEPCONT(numInstrs<<" instructions lifted"<<endl);
}

void ProgramDependenceAnalysis::constructSSA(std::vector<janus::Function>& functions)
{
	/* Step 3: construct SSA graph */
	GSTEP("Building SSA graphs"<<endl);
	for (auto &func: functions) {
	    if (func.isExecutable && func.blocks.size()) {
	        buildSSAGraph(func);
	    }
	}
}


void ProgramDependenceAnalysis::constructControlDependenceGraph(std::vector<janus::Function>& functions)
{
	/* Step 4: construct Control Dependence Graph */
	GSTEP("Building control dependence graphs"<<endl);
	for (auto &func: functions) {
	    if (func.isExecutable && func.blocks.size()) {
	        buildCDG(func);
	    }
	}
}

void ProgramDependenceAnalysis::identifyLoopsFromCFG(std::vector<janus::Function>& functions,
		std::vector<janus::Loop>& loops)
{
    GSTEP("Recognising loops: ");

    /* Step 1: identify loops from the control flow graph */
    for (auto &func: functions) {
        //searchLoop(this, &func);

    	// Updates the loops
    	searchLoop(loops, &func);
    }

    GSTEPCONT(loops.size()<<" loops recognised"<<endl);
}

void ProgramDependenceAnalysis::analyseLoopRelationsWithinProcedure(std::vector<janus::Function>& functions,
		std::vector<janus::Loop>& loops)
{
	GSTEP("Analysing loop relations"<<endl);

    for (auto &func : functions) {
        func.analyseLoopRelations(loops);
    }
}

void ProgramDependenceAnalysis::analyseLoopAndFunctionRelations(std::vector<janus::Loop>& loops,
		std::vector<std::set<LoopID>>& loopNests)
{
    for (auto &loop: loops) {
        if (loop.ancestors.size() == 0) {
            //set<LoopID> nest;
        	std::set<LoopID> nest;
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

LoopAnalysisReport ProgramDependenceAnalysis::loadLoopSelectionReport(std::vector<janus::Loop>& loops, std::string executableName)
{
	return loadLoopSelection(loops, executableName);
}

void ProgramDependenceAnalysis::performBasicLoopAnalysis(std::vector<janus::Loop>& loops,
		LoopAnalysisReport& loopAnalysisReport, std::vector<janus::Function>& allFunctions)
{
    GSTEP("Analysing loops - Basic"<<endl);
    for (auto &loop: loops) {
        loop.analyseBasic(loopAnalysisReport, allFunctions);
    }
}

void ProgramDependenceAnalysis::performAdvanceLoopAnalysis(std::vector<janus::Loop>& loops,
		LoopAnalysisReport& loopAnalysisReport, std::vector<janus::Function>& allFunctions)
{
    GSTEP("Analysing loops - Advance"<<endl);
    for (auto &loop: loops) {
    	loop.analyseAdvance(loopAnalysisReport, allFunctions);
    }
}

void ProgramDependenceAnalysis::performAdvanceLoopAnalysisWithReduceLoopsAliasAnalysis(std::vector<janus::Loop>& loops,
		LoopAnalysisReport& loopAnalysisReport, std::vector<janus::Function>& allFunctions)
{
    GSTEP("Analysing loops - Advance"<<endl);
    for (auto &loop: loops) {
    	loop.analyseAdvanceWithReduceLoopsAliasAnalysis(loopAnalysisReport, allFunctions);
    }
}

/*void ProgramDependenceAnalysis::reduceLoopsAliasAnalysis(std::vector<janus::Loop>& loops, LoopAnalysisReport loopAnalysisReport, std::vector<janus::Function>& allFunctions)
{
    GSTEP("Analysing loops - PROF - reduce considered loops with alias analysis"<<endl);
    for (auto &loop: loops) {
        //loop.analyseReduceLoopsAliasAnalysis(loopAnalysisReport, allFunctions);
    	loop.analyseReduceLoopsAliasAnalysis(allFunctions);
    }
}*/

//void ProgramDependenceAnalysis::performBasicPassWithBasicFunctionTranslate(std::vector<janus::Loop>& loops, LoopAnalysisReport loopAnalysisReport)
void ProgramDependenceAnalysis::performBasicPassWithBasicFunctionTranslate(std::vector<janus::Loop>& loops, std::vector<janus::Function>& allFunctions)
{
    GSTEP("Analysing loops - PROF - reduce considered loops with alias analysis"<<endl);
    for (auto &loop: loops) {
        //loop.analysePass0_Basic(loopAnalysisReport);
    	loop.analysePass0_Basic(allFunctions);
    }
}

//void ProgramDependenceAnalysis::performBasicPassWithAdvanceFunctionTranslate(std::vector<janus::Loop>& loops, LoopAnalysisReport loopAnalysisReport)
void ProgramDependenceAnalysis::performBasicPassWithAdvanceFunctionTranslate(std::vector<janus::Loop>& loops, std::vector<janus::Function>& allFunctions)
{
    GSTEP("Analysing loops - PROF - reduce considered loops with alias analysis"<<endl);
    for (auto &loop: loops) {
        //loop.analysePass0_Advance(loopAnalysisReport);
    	loop.analysePass0_Advance(allFunctions);
    }
}

void ProgramDependenceAnalysis::performLoopAnalysisPasses(std::vector<janus::Loop>& loops, LoopAnalysisReport& loopAnalysisReport, std::vector<std::set<LoopID>>& loopNests)
{
	printf("		performLoopAnalysisPasses --- START --- \n");
    /* When analysis done for all loops, perform post analysis
     * So we can construct inter-loop relations for further analysis */
    /* Step 6: analyse each loop more in depth (Pass 2) */
    GSTEP("Analysing loops - second pass"<<endl);
    for (auto &loop: loops)
    	loop.analyse2(loopAnalysisReport);
        //loop.analyse2(this);
    printf("			analyse3 --- START --- \n");
    /* Step 7: analyse each loop more in depth (Pass 3) */
    GSTEP("Analysing loops - third pass"<<endl);
    for (auto &loop: loops)
    {
    	//printf("					performLoopAnalysisPasses::loop.id = %d \n", loop.id);
    	loop.analyse3(loopAnalysisReport, loops, loopNests);
        //loop.analyse3(this);
    }
    printf("		performLoopAnalysisPasses --- DONE --- \n");
}
