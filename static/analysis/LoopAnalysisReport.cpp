#include "LoopAnalysisReport.h"

LoopAnalysisReport::LoopAnalysisReport()
{
	this->passedLoopId=0;
	this->manualLoopSelection = false;
}

LoopAnalysisReport::LoopAnalysisReport(int passedLoopId, bool manualLoopSelection)
{
	this->passedLoopId=passedLoopId;
	this->manualLoopSelection = manualLoopSelection;
}
