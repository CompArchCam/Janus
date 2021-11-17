#include "computationKernels.h"
#include "kernelConfiguration.h"
#include "minunit/minunit.h"


MU_TEST(test_computationKernelPolybenchGramschmidt_assert) {
    mu_assert_long_eq(6612000, poly_gramschmidt_result.initArray1Sum);
    mu_assert_long_eq(7932000, poly_gramschmidt_result.initArray2Sum);
    mu_assert_long_eq(6612000, poly_gramschmidt_result.initArray3Sum);

    mu_assert_long_eq(5510, poly_gramschmidt_result.modifiedArray1Sum);
    mu_assert_long_eq(3973316, poly_gramschmidt_result.modifiedArray2Sum);
    mu_assert_long_eq(401, poly_gramschmidt_result.modifiedArray3Sum);
}

MU_TEST_SUITE(test_suite) {
	MU_SUITE_CONFIGURE(&computationKernelPolybenchGramschmidt, NULL);
	MU_RUN_TEST(test_computationKernelPolybenchGramschmidt_assert);

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