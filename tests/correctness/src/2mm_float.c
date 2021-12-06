#include "computationKernels.h"
#include "kernelConfiguration.h"
#include "minunit/minunit.h"


MU_TEST(test_computationKernelPolybench2mmFloat_assert) {
    mu_assert_double_eq(6302300.119042, poly_2mm_float_result.initArray1Sum);
    mu_assert_double_eq(7562760.142851, poly_2mm_float_result.initArray2Sum);
    mu_assert_double_eq(7562300.142884, poly_2mm_float_result.initArray3Sum);
    mu_assert_double_eq(8192990.154755, poly_2mm_float_result.initArray4Sum);
    mu_assert_double_eq(8192990.154755, poly_2mm_float_result.initArray5Sum);

    mu_assert_double_eq(1119154.084802, poly_2mm_float_result.modifiedArray1Sum);
    mu_assert_double_eq(7562760.142851, poly_2mm_float_result.modifiedArray2Sum);
    mu_assert_double_eq(7562300.142884, poly_2mm_float_result.modifiedArray3Sum);
    mu_assert_double_eq(8192990.154755, poly_2mm_float_result.modifiedArray4Sum);
    mu_assert_double_eq(24588125.244570, poly_2mm_float_result.modifiedArray5Sum);
}

MU_TEST_SUITE(test_suite) {
	MU_SUITE_CONFIGURE(&computationKernelPolybench2mmFloat, NULL);
	MU_RUN_TEST(test_computationKernelPolybench2mmFloat_assert);

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