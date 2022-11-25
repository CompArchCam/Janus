#include "SourceCodeStructure.h"


std::vector<janus::Loop> SourceCodeStructure::getAllLoops()
{
	std::vector<janus::Loop> loops:
	//TODO: Implement
	return loops;
}

Function SourceCodeStructure::getMainFunction()
{
	return *main;
}

std::vector<janus::Function> SourceCodeStructure::getAllFunctions()
{
	return this->functions;
}

void SourceCodeStructure::updateBinaryStructure(janus::Function *main, std::vector<janus::Function> functions, std::map<PCAddress, janus::Function *>* functionMap)
{

}
