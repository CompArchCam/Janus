#include "TestFunction.h"

void TestFunction::printFunctionDetails(std::vector<janus::Function> functions)
{
	for(auto f:functions)
	{
		printf("fid = %d.\n", f.fid);
		printf("minstrs.size() = %d.\n", f.minstrs.size());
		printf("instrs.size() = %d.\n", f.instrs.size());
		printf("numBlocks = %d.\n", f.numBlocks);
		printf("blocks.size() = %d.\n", f.blocks.size());
		printf("blocks[0].instrs->minstr->name = %s.\n", f.blocks[0].instrs->minstr->name);
		printf("\n\n");
	}
}
