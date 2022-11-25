/**
 * Describes code structure, after disassemble and lifting.
 * @author: jahic
 */

#include "Function.h"

#ifndef _CODE_STRUCTURE_
#define _CODE_STRUCTURE_

class BinaryStructure
{
private:
	//the main function of the program (not entry)
	janus::Function *main;

	///A vector of recognized functions in the executable
	std::vector<janus::Function> functions;

public:
	BinaryStructure();

	// For consistency reason, only update of the full structure is possible.
	void updateBinaryStructure(janus::Function *main, std::vector<janus::Function> functions);
};

#endif
