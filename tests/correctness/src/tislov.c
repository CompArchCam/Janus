#include "computationKernels.h"
#include "kernelConfiguration.h"
#include "minunit/minunit.h"


MU_TEST(test_computationKernelPolybenchTislov_assert) {
    mu_assert_long_eq(31510, poly_tislov_result.initBSum);
    mu_assert_long_eq(31510, poly_tislov_result.initXSum);
    mu_assert_long_eq(269570000, poly_tislov_result.initLSum);

    mu_assert_long_eq(24496500, poly_tislov_result.modifiedBSum);
    mu_assert_long_eq(7000, poly_tislov_result.modifiedXSum);
    mu_assert_long_eq(71605316890, poly_tislov_result.modifiedLSum);
}

MU_TEST_SUITE(test_suite) {
	MU_SUITE_CONFIGURE(&computationKernelPolybenchTislov, NULL);
	MU_RUN_TEST(test_computationKernelPolybenchTislov_assert);

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