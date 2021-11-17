#include "computationKernels.h"
#include "kernelConfiguration.h"
#include "minunit/minunit.h"


MU_TEST(test_computationKernelPolybenchNussinov_assert) {
    mu_assert_long_eq(3332, poly_nussinov_result.initSeqSum);
    mu_assert_long_eq(0, poly_nussinov_result.initTableSum);

    mu_assert_long_eq(3332, poly_nussinov_result.modifiedSeqSum);
    mu_assert_long_eq(868399171, poly_nussinov_result.modifiedTableSum);
}

MU_TEST_SUITE(test_suite) {
	MU_SUITE_CONFIGURE(&computationKernelPolybenchNussinov, NULL);
	MU_RUN_TEST(test_computationKernelPolybenchNussinov_assert);

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