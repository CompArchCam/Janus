
#include "computationKernels.h"
#include "kernelConfiguration.h"
#include "minunit/minunit.h"


MU_TEST(test_computationKernelPolybenchCholeskyFloat_assert) {
    mu_assert_double_eq(907227617.164135, poly_cholesky_float_result.initArraySum);
    mu_assert_double_eq(66008999.665696, poly_cholesky_float_result.modifiedArraySum);
}


MU_TEST_SUITE(test_suite) {
    MU_SUITE_CONFIGURE(&computationKernelPolybenchCholeskyFloat, NULL);
    MU_RUN_TEST(test_computationKernelPolybenchCholeskyFloat_assert);
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