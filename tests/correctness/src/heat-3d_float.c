#include "computationKernels.h"
#include "kernelConfiguration.h"
#include "minunit/minunit.h"


MU_TEST(test_computationKernelPolybenchHeat3DFloat_assert) {
    mu_assert_double_eq(348550.999979, poly_heat3d_float_result.initASum);
    mu_assert_double_eq(348550.999979, poly_heat3d_float_result.initBSum);
    //mu_assert_double_eq(240583020, poly_heat3d_float_result.modifiedAProduct);
    mu_assert_double_eq(28401.443425, poly_heat3d_float_result.modifiedASum);
    mu_assert_double_eq(25775.144142, poly_heat3d_float_result.modifiedBSum);
}

MU_TEST_SUITE(test_suite) {
	MU_SUITE_CONFIGURE(&computationKernelPolybenchHeat3DFloat, NULL);
	MU_RUN_TEST(test_computationKernelPolybenchHeat3DFloat_assert);

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