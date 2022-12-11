/**
 * Describes code structure, after disassemble and lifting.
 * @author: jahic
 */
#ifndef Janus_SOURCE_CODE_STRUCTURE_
#define Janus_SOURCE_CODE_STRUCTURE_

#include "Function.h"

class SourceCodeStructure
{
private:
	///The name of the executable
	std::string executableName;

	//the main function of the program (not entry)
	janus::Function *main;

	///A vector of recognized functions in the executable
	std::vector<janus::Function> functions;

	// TODO: What does this data type contain?
	std::map<PCAddress, janus::Function *> functionMap;

	std::vector<std::set<LoopID>> loopNests;

public:
	SourceCodeStructure();

	// For consistency reason, only update of the full structure is possible.
	void updateBinaryStructure(janus::Function *main, std::vector<janus::Function> functions, std::map<PCAddress, janus::Function *> functionMap);

	std::vector<janus::Loop> getAllLoops();

	janus::Function getMainFunction();

	std::vector<janus::Function> getAllFunctions();

	std::map<PCAddress, janus::Function *> getFunctionMap()
	{
		return functionMap;
	}

	std::string getExecutableName()
	{
		return executableName;
	}
};

#endif
