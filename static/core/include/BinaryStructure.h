/**
 * Describes binary structure, after disassembly and lifting.
 * @author: jahic
 */

#ifndef _BINARY_STRUCTURE_
#define _BINARY_STRUCTURE_

class BinaryStructure
{
private:
	//the main function of the program (not entry)
	janus::Function *main;

	///A vector of recognized functions in the executable
	std::vector<janus::Function> functions;

public:
	BinaryStructure();s
};

#endif
