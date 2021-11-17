#include "computationKernels.h"
#include "kernelConfiguration.h"
#include "minunit/minunit.h"


MU_TEST(test_computationKernelPolybenchHeat3D_assert) {
    mu_assert_long_eq(51515050, poly_heat3d_result.initASum);
    mu_assert_long_eq(51515050, poly_heat3d_result.initBSum);
    mu_assert_long_eq(240583020, poly_heat3d_result.modifiedAProduct);
    mu_assert_long_eq(32597509, poly_heat3d_result.modifiedASum);
    mu_assert_long_eq(32597509, poly_heat3d_result.modifiedBSum);
}

MU_TEST_SUITE(test_suite) {
	MU_SUITE_CONFIGURE(&computationKernelPolybenchHeat3D, NULL);
	MU_RUN_TEST(test_computationKernelPolybenchHeat3D_assert);

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