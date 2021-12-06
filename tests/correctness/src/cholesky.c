#include "computationKernels.h"
#include "kernelConfiguration.h"
#include "minunit/minunit.h"


MU_TEST(test_computationKernelPolybenchCholesky_assert) {
    mu_assert_long_eq(792120000, poly_cholesky_result.initArraySum);
    mu_assert_long_eq(437150775155, poly_cholesky_result.modifiedArraySum);
}


MU_TEST_SUITE(test_suite) {
    MU_SUITE_CONFIGURE(&computationKernelPolybenchCholesky, NULL);
    MU_RUN_TEST(test_computationKernelPolybenchCholesky_assert);
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