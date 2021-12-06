#include "computationKernels.h"
#include "kernelConfiguration.h"
#include "minunit/minunit.h"


MU_TEST(test_computationKernelGenericLoop3D2A_assert) {
    mu_assert_long_eq(6110500, genericLoop3D2A_result.initArray1Sum);
    mu_assert_long_eq(6110500, genericLoop3D2A_result.initArray2Sum);

    mu_assert_long_eq(39143500, genericLoop3D2A_result.modifiedArray1Sum);
    mu_assert_long_eq(640344100, genericLoop3D2A_result.modifiedArray2Sum);
}

MU_TEST_SUITE(test_suite) {
	MU_SUITE_CONFIGURE(&computationKernelGenericLoop3D2A, NULL);
	MU_RUN_TEST(test_computationKernelGenericLoop3D2A_assert);

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