#include "JanusContext.h"
#include "SchedGen.h"
#include "ProgramDependenceAnalysis.h"
#include "SourceCodeStructure.h"
#include "LoopAnalysisReport.h"

#include "minunit.h"

#include "Profile.h"

#include <string.h>

using namespace std;


MU_TEST(test_openAndParseBinary_testIf)
{
	char *executableName = "../../tests/simpleBinExamples/testIf/build/testIf";

	janus::ExecutableBinaryStructure executableBinaryStructure = janus::ExecutableBinaryStructure(string(executableName));

	mu_assert_int_eq(29, executableBinaryStructure.sections.size());
	mu_assert_long_eq(84, executableBinaryStructure.symbols.size());
	mu_assert_long_eq(0, executableBinaryStructure.crossSectionRef.size());
}

MU_TEST(test_openAndParseBinary_testSimpleLoop)
{
	// TODO: change to relative path. For some reason, ifstream fails to open the relative path.
	char *executableName = "../../tests/simpleBinExamples/testSimpleLoop/build/testSimpleLoop";

	janus::ExecutableBinaryStructure executableBinaryStructure = janus::ExecutableBinaryStructure(string(executableName));

	mu_assert_int_eq(29, executableBinaryStructure.sections.size());
	mu_assert_long_eq(83, executableBinaryStructure.symbols.size());
	mu_assert_long_eq(0, executableBinaryStructure.crossSectionRef.size());
}

MU_TEST(test_openAndParseBinary_x86_gcc_cholesky)
{
	char *executableName = "../../tests/polybench/x86-gcc/cholesky";

	janus::ExecutableBinaryStructure executableBinaryStructure = janus::ExecutableBinaryStructure(string(executableName));

	mu_assert_int_eq(31, executableBinaryStructure.sections.size());
	mu_assert_long_eq(118, executableBinaryStructure.symbols.size());
	mu_assert_long_eq(0, executableBinaryStructure.crossSectionRef.size());
}

void openAndParseBinary()
{

}

MU_TEST_SUITE(test_suite) {

    MU_SUITE_CONFIGURE(&openAndParseBinary, NULL);
	MU_RUN_TEST(test_openAndParseBinary_testIf);
	MU_RUN_TEST(test_openAndParseBinary_testSimpleLoop);
	MU_RUN_TEST(test_openAndParseBinary_x86_gcc_cholesky);
}

int main()
{

	#ifdef LOG_RESULTS
	// Restart the file
	logFile = fopen("log.txt", "w+");
	fclose(logFile);
	#endif // MACRO

	MU_RUN_SUITE(test_suite);
	MU_REPORT();

	return MU_EXIT_CODE;
}
