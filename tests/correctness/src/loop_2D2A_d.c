#include "computationKernels.h"
#include "kernelConfiguration.h"
#include "minunit/minunit.h"


MU_TEST(test_computationKernelGenericLoop2D2AD_assert) {
    mu_assert_long_eq(319200, genericLoop2D2AD_result.initArray1Sum);
    mu_assert_long_eq(319200, genericLoop2D2AD_result.initArray2Sum);

    mu_assert_long_eq(2565600, genericLoop2D2AD_result.modifiedArray1Sum);
    mu_assert_long_eq(319200, genericLoop2D2AD_result.modifiedArray2Sum);
}

MU_TEST_SUITE(test_suite) {
	MU_SUITE_CONFIGURE(&computationKernelGenericLoop2D2AD, NULL);
	MU_RUN_TEST(test_computationKernelGenericLoop2D2AD_assert);

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