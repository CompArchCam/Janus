#include "computationKernels.h"
#include "kernelConfiguration.h"
#include "minunit/minunit.h"

MU_TEST(test_computationKernelPolybenchFDTD2D_assert) {
    mu_assert_double_eq(31316381.591379, poly_fdtd2d_result.initExSum);
    mu_assert_double_eq(31411172.045928, poly_fdtd2d_result.initEySum);
    mu_assert_double_eq(31505962.500513, poly_fdtd2d_result.initHzSum);
    mu_assert_double_eq(217470.000000, poly_fdtd2d_result.initFictSum);

    mu_assert_double_eq(-334.393090, poly_fdtd2d_result.modifiedExSum);
    mu_assert_double_eq(1979408.351216, poly_fdtd2d_result.modifiedEySum);
    mu_assert_double_eq(108659.523848, poly_fdtd2d_result.modifiedHzSum);
    mu_assert_double_eq(217470.000000, poly_fdtd2d_result.modifiedFictSum);
}

MU_TEST_SUITE(test_suite) {
	MU_SUITE_CONFIGURE(&computationKernelPolybenchFDTD2D, NULL);
	MU_RUN_TEST(test_computationKernelPolybenchFDTD2D_assert);

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