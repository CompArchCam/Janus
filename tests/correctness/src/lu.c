#include "computationKernels.h"
#include "kernelConfiguration.h"
#include "minunit/minunit.h"


MU_TEST(test_computationKernelPolybenchLu_assert) {
    mu_assert_long_eq(22020000, poly_lu_result.initArraySum);
    mu_assert_long_eq(21495192, poly_lu_result.modifiedArraySum);
}

MU_TEST_SUITE(test_suite) {
	MU_SUITE_CONFIGURE(&computationKernelPolybenchLu, NULL);
	MU_RUN_TEST(test_computationKernelPolybenchLu_assert);

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