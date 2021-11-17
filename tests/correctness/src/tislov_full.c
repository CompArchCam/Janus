#include "computationKernels.h"
#include "kernelConfiguration.h"
#include "minunit/minunit.h"


MU_TEST(test_computationKernelPolybenchTislovFull_assert) {
    mu_assert_double_eq(9606.699999, poly_tislov_float_result.initBSum);
    mu_assert_double_eq(9606.699999, poly_tislov_float_result.initXSum);
    mu_assert_double_eq(16131680.304909, poly_tislov_float_result.initLSum);

    mu_assert_double_eq(1279200.000000, poly_tislov_float_result.modifiedBSum);
    mu_assert_double_eq(320000.500000, poly_tislov_float_result.modifiedXSum);
    mu_assert_double_eq(862020576.351822, poly_tislov_float_result.modifiedLSum);
}

MU_TEST_SUITE(test_suite) {
	MU_SUITE_CONFIGURE(&computationKernelPolybenchTislovFull, NULL);
	MU_RUN_TEST(test_computationKernelPolybenchTislovFull_assert);

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