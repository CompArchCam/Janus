#include "computationKernels.h"
#include "kernelConfiguration.h"
#include "minunit/minunit.h"


MU_TEST(test_computationKernelPolybench2mm_assert) {
    mu_assert_long_eq(5510000, poly_2mm_result.initArray1Sum);
    mu_assert_long_eq(6612000, poly_2mm_result.initArray2Sum);
    mu_assert_long_eq(6610000, poly_2mm_result.initArray3Sum);
    mu_assert_long_eq(7163000, poly_2mm_result.initArray4Sum);
    mu_assert_long_eq(7163000, poly_2mm_result.initArray5Sum);

    mu_assert_long_eq(109264300000, poly_2mm_result.modifiedArray1Sum);
    mu_assert_long_eq(6612000, poly_2mm_result.modifiedArray2Sum);
    mu_assert_long_eq(6610000, poly_2mm_result.modifiedArray3Sum);
    mu_assert_long_eq(7163000, poly_2mm_result.modifiedArray4Sum);
    mu_assert_long_eq(4049265389000, poly_2mm_result.modifiedArray5Sum);
}

MU_TEST_SUITE(test_suite) {

    MU_SUITE_CONFIGURE(&computationKernelPolybench2mm, NULL);
	MU_RUN_TEST(test_computationKernelPolybench2mm_assert);
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