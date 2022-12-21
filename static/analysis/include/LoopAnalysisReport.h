#ifndef _Janus_LOOP_ANALYSIS_REPORT_
#define _Janus_LOOP_ANALYSIS_REPORT_

/**
 * Analysis report of loops.
 */

class LoopAnalysisReport
{
private:
	int passedLoopId;
	bool manualLoopSelection;


public:
	LoopAnalysisReport()
	{
		this->passedLoopId=0;
		this->manualLoopSelection = false;
	}

	LoopAnalysisReport(int passedLoopId, bool manualLoopSelection);

	int getPassedLoopId()
	{
		return this->passedLoopId;
	}

	bool getManualLoopSelection()
	{
		return this->manualLoopSelection;
	}
};

#endif
