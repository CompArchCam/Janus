#include "computationKernels.h"
#include "kernelConfiguration.h"
#include "minunit/minunit.h"


MU_TEST(test_computationKernelPolybench3mm_assert) {
    mu_assert_long_eq(7272000, poly_3mm_result.initArray1Sum);
    mu_assert_long_eq(7932000, poly_3mm_result.initArray2Sum);
    mu_assert_long_eq(10740000, poly_3mm_result.initArray3Sum);
    mu_assert_long_eq(11564000, poly_3mm_result.initArray4Sum);
    mu_assert_long_eq(7272000, poly_3mm_result.initArray5Sum);
    mu_assert_long_eq(10024000, poly_3mm_result.initArray6Sum);

    mu_assert_long_eq(7272000, poly_3mm_result.modifiedArray1Sum);
    mu_assert_long_eq(7932000, poly_3mm_result.modifiedArray2Sum);
    mu_assert_long_eq(10740000, poly_3mm_result.modifiedArray3Sum);
    mu_assert_long_eq(11564000, poly_3mm_result.modifiedArray4Sum);
    mu_assert_long_eq(48067920000, poly_3mm_result.modifiedArray5Sum);
    mu_assert_long_eq(82800060000, poly_3mm_result.modifiedArray6Sum);
}

MU_TEST_SUITE(test_suite) {
    MU_SUITE_CONFIGURE(&computationKernelPolybench3mm, NULL);
	MU_RUN_TEST(test_computationKernelPolybench3mm_assert);

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