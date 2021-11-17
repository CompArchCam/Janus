#include "computationKernels.h"
#include "kernelConfiguration.h"
#include "minunit/minunit.h"


MU_TEST(test_computationKernelGenericLoop1D2AD_assert) {
    mu_assert_long_eq(220000010, genericLoop1D2AD_result.initArray1Sum);
    mu_assert_long_eq(220000010, genericLoop1D2AD_result.initArray2Sum);

    mu_assert_long_eq(363782405902771, genericLoop1D2AD_result.modifiedArray1Sum);
    mu_assert_long_eq(1140000020, genericLoop1D2AD_result.modifiedArray2Sum);
}

MU_TEST_SUITE(test_suite) {
	MU_SUITE_CONFIGURE(&computationKernelGenericLoop1D2AD, NULL);
	MU_RUN_TEST(test_computationKernelGenericLoop1D2AD_assert);

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