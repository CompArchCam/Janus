#include "computationKernels.h"
#include "kernelConfiguration.h"
#include "minunit/minunit.h"


MU_TEST(test_computationKernelPolybenchDoitgen_assert) {
    mu_assert_long_eq(910, poly_doitgen_result.initSumSum);
    mu_assert_long_eq(222000, poly_doitgen_result.initC4Sum);
    mu_assert_long_eq(32870000, poly_doitgen_result.initASum);

    mu_assert_long_eq(1998000, poly_doitgen_result.modifiedSumSum);
    mu_assert_long_eq(222000, poly_doitgen_result.modifiedC4Sum);
    mu_assert_long_eq(36485700000, poly_doitgen_result.modifiedASum);
}

MU_TEST_SUITE(test_suite) {
	MU_SUITE_CONFIGURE(&computationKernelPolybenchDoitgen, NULL);
	MU_RUN_TEST(test_computationKernelPolybenchDoitgen_assert);

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