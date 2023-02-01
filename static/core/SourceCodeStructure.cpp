#include "SourceCodeStructure.h"


/*std::vector<janus::Loop> SourceCodeStructure::getAllLoops()
{
	std::vector<janus::Loop> loops;
	//TODO: Implement
	return loops;
}*/

SourceCodeStructure::SourceCodeStructure()
{

}

janus::Function SourceCodeStructure::getMainFunction()
{
	return *main;
}

std::vector<janus::Function> SourceCodeStructure::getAllFunctions()
{
	return this->functions;
}

void SourceCodeStructure::updateBinaryStructure(janus::Function *main, std::vector<janus::Function>& functions, std::map<PCAddress, janus::Function *>& functionMap)
{
	// The main function of the program (not entry)
	this->main = main;

	///A vector of recognized functions in the executable
	// TODO: There is an issue with this. We need to implement an explicit copy constructor.
	// Otherwise, a destructor is called and all dynamically allocated memory is lost.
	//this->functions = functions;

	this->functionMap = functionMap;

}
