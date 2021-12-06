#include "computationKernels.h"
#include "kernelConfiguration.h"
#include "minunit/minunit.h"


MU_TEST(test_computationKernelGenericLoop2D2A_assert) {
    mu_assert_long_eq(67650, genericLoop2D2A_result.initArray1Sum);
    mu_assert_long_eq(67650, genericLoop2D2A_result.initArray2Sum);

    mu_assert_long_eq(546700, genericLoop2D2A_result.modifiedArray1Sum);
    mu_assert_long_eq(49096690, genericLoop2D2A_result.modifiedArray2Sum);
}

MU_TEST_SUITE(test_suite) {
	MU_SUITE_CONFIGURE(&computationKernelGenericLoop2D2A, NULL);
	MU_RUN_TEST(test_computationKernelGenericLoop2D2A_assert);

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