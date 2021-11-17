#include "computationKernels.h"
#include "kernelConfiguration.h"
#include "minunit/minunit.h"


MU_TEST(test_conditionalMergingTest1_assert) {
    mu_assert_long_eq(450010, conditionalMergeTest1_result.initArray1Sum);
    mu_assert_long_eq(168, conditionalMergeTest1_result.x);
    mu_assert_long_eq(298, conditionalMergeTest1_result.y);
    mu_assert_long_eq(3, conditionalMergeTest1_result.z);
    mu_assert_long_eq(4700010, conditionalMergeTest1_result.modifiedArray1Sum);
}


MU_TEST_SUITE(test_suite) {
    MU_SUITE_CONFIGURE(&conditionalMergingTest1, NULL);
    MU_RUN_TEST(test_conditionalMergingTest1_assert);
}

int main() {

	#ifdef LOG_RESULTS
	// Restart the file
	logFile = fopen("log.txt", "w+");
	fclose(logFile);
	#endif /* MACRO */

	MU_RUN_SUITE(test_suite);
	MU_REPORT();
	return MU_EXIT_CODE;
}