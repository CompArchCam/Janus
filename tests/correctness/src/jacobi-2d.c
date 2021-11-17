#include "computationKernels.h"
#include "kernelConfiguration.h"
#include "minunit/minunit.h"

MU_TEST(test_computationKernelPolybenchJacobi2D_assert) {
    mu_assert_double_eq(665671000.097299, poly_jacobi2d_result.initASum);
    mu_assert_double_eq(665673000.130676, poly_jacobi2d_result.initBSum);

    mu_assert_double_eq(668632079.242028, poly_jacobi2d_result.modifiedASum);
    mu_assert_double_eq(668631380.389699, poly_jacobi2d_result.modifiedBSum);
}

MU_TEST_SUITE(test_suite) {
	MU_SUITE_CONFIGURE(&computationKernelPolybenchJacobi2D, NULL);
	MU_RUN_TEST(test_computationKernelPolybenchJacobi2D_assert);

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