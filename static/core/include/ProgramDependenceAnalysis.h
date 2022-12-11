#ifndef _JANUS_PROGRAM_DEPENDENCE_ANALYSIS_
#define _JANUS_PROGRAM_DEPENDENCE_ANALYSIS_

#include "Function.h"
#include "LoopAnalysisReport.h"

namespace janus {


class ProgramDependenceAnalysis
{
public:
	ProgramDependenceAnalysis();

	void buildCFGForEachFunction(std::vector<janus::Function>& functions);
	void liftDisassemblyToIR(std::vector<janus::Function>& functions);
	void constructSSA(std::vector<janus::Function>& functions);
	void constructControlDependenceGraph(std::vector<janus::Function>& functions);

	// Identify loops from the control flow graph
	std::vector<janus::Loop> identifyLoopsFromCFG(std::vector<janus::Function> functions);

	// Analyze loop relations within one procedure
	void analyseLoopRelationsWithinProcedure(std::vector<janus::Function>& functions, std::vector<janus::Loop>* loops);

    // Analyze loop relations across procedures
	std::vector<std::set<LoopID>> analyseLoopAndFunctionRelations(std::vector<janus::Loop>& loops);

	LoopAnalysisReport loadLoopSelectionReport(std::vector<janus::Loop>& loops, std::string executableName);

	// The difference between these two analysis types is that performBasicLoopAnalysis calls analyseBasic,
	// while performAdvanceLoopAnalysis calls analyseAdvance.
	void performBasicLoopAnalysis(std::vector<janus::Loop>& loops, LoopAnalysisReport loopAnalysisReport);
	void performAdvanceLoopAnalysis(std::vector<janus::Loop>& loops, LoopAnalysisReport loopAnalysisReport);

	void reduceLoopsAliasAnalysis(std::vector<janus::Loop>& loops, LoopAnalysisReport loopAnalysisReport);
	//void performBasicPass(std::vector<janus::Loop> loops, LoopAnalysisReport loopAnalysisReport);
	void performBasicPassWithBasicFunctionTranslate(std::vector<janus::Loop>& loops, LoopAnalysisReport loopAnalysisReport);
	void performBasicPassWithAdvanceFunctionTranslate(std::vector<janus::Loop>& loops, LoopAnalysisReport loopAnalysisReport);

	void performLoopAnalysisPasses(std::vector<janus::Loop>& loops, LoopAnalysisReport loopAnalysisReport);
};

} /* END Janus namespace */

#endif
