#include "computationKernels.h"
#include "kernelConfiguration.h"
#include "minunit/minunit.h"


MU_TEST(test_computationKernelPolybenchGramschmidtFloat_assert) {
    mu_assert_double_eq(7562760.142851, poly_gramschmidt_float_result.initArray1Sum);
    mu_assert_double_eq(9074760.171461, poly_gramschmidt_float_result.initArray2Sum);
    mu_assert_double_eq(7562760.142851, poly_gramschmidt_float_result.initArray3Sum);

    mu_assert_double_eq(6301.698551, poly_gramschmidt_float_result.modifiedArray1Sum);
    mu_assert_double_eq(4555361.889813, poly_gramschmidt_float_result.modifiedArray2Sum);
    mu_assert_double_eq(737.342086, poly_gramschmidt_float_result.modifiedArray3Sum);
}

MU_TEST_SUITE(test_suite) {
	MU_SUITE_CONFIGURE(&computationKernelPolybenchGramschmidtFloat, NULL);
	MU_RUN_TEST(test_computationKernelPolybenchGramschmidtFloat_assert);

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