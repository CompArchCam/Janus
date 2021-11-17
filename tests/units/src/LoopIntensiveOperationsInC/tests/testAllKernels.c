#include "../computationKernels.h"
#include "../kernels/kernelConfiguration.h"
#include "minunit/minunit.h"

/**
 * Test for all computing kernels.
 */

// ------------------------------------------------------------------ //
// Tests int
// ------------------------------------------------------------------ //
MU_TEST(test_computationKernelPolybenchCholesky_assert) {
	mu_assert_long_eq(22000000, poly_cholesky_result.initArraySum);
	mu_assert_long_eq(2001000, poly_cholesky_result.modifiedArraySum);
}
MU_TEST(test_computationKernelPolybenchCholesky_d_assert) {
	mu_assert_long_eq(22020000, poly_cholesky_d_result.initArraySum);
	mu_assert_long_eq(2001000, poly_cholesky_d_result.modifiedArraySum);
}

/*MU_TEST(test_computationKernelPolybenchLu_assert) {
	mu_assert_long_eq(22020000.000000, poly_lu_result.initArraySum);
	mu_assert_long_eq(-1624395.785872, poly_lu_result.modifiedArraySum);
}*/

MU_TEST(test_computationKernelPolybench2mm_assert) {
	mu_assert_long_eq(6050000, poly_2mm_result.initArray1Sum);
	mu_assert_long_eq(6600000, poly_2mm_result.initArray2Sum);
	mu_assert_long_eq(6600000, poly_2mm_result.initArray3Sum);
	mu_assert_long_eq(7150000, poly_2mm_result.initArray4Sum);
	mu_assert_long_eq(7150000, poly_2mm_result.initArray5Sum);

	mu_assert_long_eq(930800, poly_2mm_result.modifiedArray1Sum);
	mu_assert_long_eq(6184800, poly_2mm_result.modifiedArray2Sum);
	mu_assert_long_eq(6600300, poly_2mm_result.modifiedArray3Sum);
	mu_assert_long_eq(7150000, poly_2mm_result.modifiedArray4Sum);
	mu_assert_long_eq(18150000, poly_2mm_result.modifiedArray5Sum);
}

/*MU_TEST(test_computationKernelPolybench3mm_assert) {
	mu_assert_long_eq(11550000, poly_3mm_result.initArray1Sum);
	mu_assert_long_eq(13200000, poly_3mm_result.initArray2Sum);
	mu_assert_long_eq(15840000, poly_3mm_result.initArray3Sum);
	mu_assert_long_eq(16830000, poly_3mm_result.initArray4Sum);
	mu_assert_long_eq(11550000, poly_3mm_result.initArray5Sum);
	mu_assert_long_eq(13200000, poly_3mm_result.initArray6Sum);

	mu_assert_long_eq(12631800, poly_3mm_result.modifiedArray1Sum);
	mu_assert_long_eq(13179000, poly_3mm_result.modifiedArray2Sum);
	mu_assert_long_eq(15806000, poly_3mm_result.modifiedArray3Sum);
	mu_assert_long_eq(16803000, poly_3mm_result.modifiedArray4Sum);
	mu_assert_long_eq(707400, poly_3mm_result.modifiedArray5Sum);
	mu_assert_long_eq(2400000, poly_3mm_result.modifiedArray6Sum);
}*/

/*MU_TEST(test_computationKernelPolybenchDoitgen_assert) {
	mu_assert_long_eq(1770.000000, poly_doitgen_result.initSumSum);
	mu_assert_long_eq(566400, poly_doitgen_result.initC4Sum);
	mu_assert_long_eq(121856000, poly_doitgen_result.initASum);

	mu_assert_long_eq(5097600, poly_doitgen_result.modifiedSumSum);
	mu_assert_long_eq(566800, poly_doitgen_result.modifiedC4Sum);
	mu_assert_long_eq(215685120000, poly_doitgen_result.modifiedASum);
}*/

MU_TEST(test_computationKernelPolybenchGramschmidt_assert) {
	mu_assert_long_eq(6600000, poly_gramschmidt_result.initArray1Sum);
	mu_assert_long_eq(7920000, poly_gramschmidt_result.initArray2Sum);
	mu_assert_long_eq(6600000, poly_gramschmidt_result.initArray3Sum);

	mu_assert_long_eq(5500, poly_gramschmidt_result.modifiedArray1Sum);
	mu_assert_long_eq(3973206, poly_gramschmidt_result.modifiedArray2Sum);
	mu_assert_long_eq(400, poly_gramschmidt_result.modifiedArray3Sum);
}

MU_TEST(test_computationKernelPolybenchGramschmidt_d_assert) {
	mu_assert_long_eq(6612000, poly_gramschmidt_d_result.initArray1Sum);
	mu_assert_long_eq(7932000, poly_gramschmidt_d_result.initArray2Sum);
	mu_assert_long_eq(6612000, poly_gramschmidt_d_result.initArray3Sum);

	mu_assert_long_eq(1204510, poly_gramschmidt_d_result.modifiedArray1Sum);
	mu_assert_long_eq(3973316, poly_gramschmidt_d_result.modifiedArray2Sum);
	mu_assert_long_eq(118401, poly_gramschmidt_d_result.modifiedArray3Sum);
}


MU_TEST(test_computationKernelPolybenchTislov_assert) {
	mu_assert_long_eq(22510, poly_tislov_result.initBSum);
	mu_assert_long_eq(22510, poly_tislov_result.initXSum);
	mu_assert_long_eq(137500000, poly_tislov_result.initLSum);

	mu_assert_long_eq(517500, poly_tislov_result.modifiedBSum);
	mu_assert_long_eq(9000, poly_tislov_result.modifiedXSum);
	mu_assert_long_eq(135865752, poly_tislov_result.modifiedLSum);
}

MU_TEST(test_computationKernelGenericLoop1D2A_assert) {
	mu_assert_long_eq(460, genericLoop1D2A_result.initArray1Sum);
	mu_assert_long_eq(460, genericLoop1D2A_result.initArray2Sum);

	mu_assert_long_eq(115811.793678, genericLoop1D2A_result.modifiedArray1Sum);
	mu_assert_long_eq(3410, genericLoop1D2A_result.modifiedArray2Sum);
}

MU_TEST(test_computationKernelGenericLoop1D2AD_assert) {
	mu_assert_long_eq(560, genericLoop1D2AD_result.initArray1Sum);
	mu_assert_long_eq(560, genericLoop1D2AD_result.initArray2Sum);

	mu_assert_long_eq(154995, genericLoop1D2AD_result.modifiedArray1Sum);
	mu_assert_long_eq(4530, genericLoop1D2AD_result.modifiedArray2Sum);
}

MU_TEST(test_computationKernelGenericLoop2D2A_assert) {
	mu_assert_long_eq(55000, genericLoop2D2A_result.initArray1Sum);
	mu_assert_long_eq(55000, genericLoop2D2A_result.initArray2Sum);

	mu_assert_long_eq(440000, genericLoop2D2A_result.modifiedArray1Sum);
	mu_assert_long_eq(33221406, genericLoop2D2A_result.modifiedArray2Sum);
}

MU_TEST(test_computationKernelGenericLoop2D2AD_mu_assert) {
	mu_assert_long_eq(5510000, genericLoop2D2AD_result.initArray1Sum);
	mu_assert_long_eq(56000, genericLoop2D2AD_result.initArray2Sum);

	mu_assert_long_eq(44130000, genericLoop2D2AD_result.modifiedArray1Sum);
	mu_assert_long_eq(56000, genericLoop2D2AD_result.modifiedArray2Sum);
}

/*MU_TEST(test_computationKernelGenericLoop3D2A_assert) {
	mu_assert_long_eq(4600000, genericLoop3D2A_result.initArray1Sum);
	mu_assert_long_eq(4600000, genericLoop3D2A_result.initArray2Sum);

	mu_assert_long_eq(29500000, genericLoop3D2A_result.modifiedArray1Sum);
	mu_assert_long_eq(484000000, genericLoop3D2A_result.modifiedArray2Sum);
}*/
// ------------------------------------------------------------------ //



// ------------------------------------------------------------------ //
// Tests float
// ------------------------------------------------------------------ //
MU_TEST(test_computationKernelPolybenchCholeskyFloat_assert) {
	mu_assert_double_eq(22020000.000000, poly_cholesky_float_result.initArraySum);
	mu_assert_double_eq(1335333.000012, poly_cholesky_float_result.modifiedArraySum);
}
MU_TEST(test_computationKernelPolybenchCholesky_d_float_assert) {
	mu_assert_double_eq(22020000.000000, poly_cholesky_d_float_result.initArraySum);
	mu_assert_double_eq(1335333.000012, poly_cholesky_d_float_result.modifiedArraySum);
}

/*MU_TEST(test_computationKernelPolybenchLuFloat_assert) {
	mu_assert_double_eq(22020000.000000, poly_lu_float_result.initArraySum);
	mu_assert_double_eq(-1624395.785872, poly_lu_float_result.modifiedArraySum);
}*/

MU_TEST(test_computationKernelPolybench2mmFloat_assert) {
	mu_assert_double_eq(6061000.000000, poly_2mm_float_result.initArray1Sum);
	mu_assert_double_eq(6612000.000000, poly_2mm_float_result.initArray2Sum);
	mu_assert_double_eq(6610000.000000, poly_2mm_float_result.initArray3Sum);
	mu_assert_double_eq(7163000.000000, poly_2mm_float_result.initArray4Sum);
	mu_assert_double_eq(7163000.000000, poly_2mm_float_result.initArray5Sum);

	mu_assert_double_eq(85689163.090134, poly_2mm_float_result.modifiedArray1Sum);
	mu_assert_double_eq(14686342.773056, poly_2mm_float_result.modifiedArray2Sum);
	mu_assert_double_eq(6616300.000000, poly_2mm_float_result.modifiedArray3Sum);
	mu_assert_double_eq(7163000.000000, poly_2mm_float_result.modifiedArray4Sum);
	mu_assert_double_eq(18690642.160988, poly_2mm_float_result.modifiedArray5Sum);
}

MU_TEST(test_computationKernelPolybench3mmFloat_assert) {
	mu_assert_double_eq(7272000.000000, poly_3mm_float_result.initArray1Sum);
	mu_assert_double_eq(9252000.000000, poly_3mm_float_result.initArray2Sum);
	mu_assert_double_eq(10740000.000000, poly_3mm_float_result.initArray3Sum);
	mu_assert_double_eq(11564000.000000, poly_3mm_float_result.initArray4Sum);
	mu_assert_double_eq(7272000.000000, poly_3mm_float_result.initArray5Sum);
	mu_assert_double_eq(8593000.000000, poly_3mm_float_result.initArray6Sum);

	mu_assert_double_eq(6712500.000000, poly_3mm_float_result.modifiedArray1Sum);
	mu_assert_double_eq(9250000.000000, poly_3mm_float_result.modifiedArray2Sum);
	mu_assert_double_eq(10725000.000000, poly_3mm_float_result.modifiedArray3Sum);
	mu_assert_double_eq(11550000.000000, poly_3mm_float_result.modifiedArray4Sum);
	mu_assert_double_eq(37535168.441200, poly_3mm_float_result.modifiedArray5Sum);
	mu_assert_double_eq(60708551.224327, poly_3mm_float_result.modifiedArray6Sum);
}

/*MU_TEST(test_computationKernelPolybenchDoitgenFloat_assert) {
	mu_assert_double_eq(1770.000000, poly_doitgen_float_result.initSumSum);
	mu_assert_double_eq(566400, poly_doitgen_float_result.initC4Sum);
	mu_assert_double_eq(121856000, poly_doitgen_float_result.initASum);

	mu_assert_double_eq(5097600, poly_doitgen_float_result.modifiedSumSum);
	mu_assert_double_eq(566800, poly_doitgen_float_result.modifiedC4Sum);
	mu_assert_double_eq(215685120000, poly_doitgen_float_result.modifiedASum);
}*/

MU_TEST(test_computationKernelPolybenchGramschmidtFloat_assert) {
	mu_assert_double_eq(6612000.000000, poly_gramschmidt_float_result.initArray1Sum);
	mu_assert_double_eq(7932000.000000, poly_gramschmidt_float_result.initArray2Sum);
	mu_assert_double_eq(6612000.000000, poly_gramschmidt_float_result.initArray3Sum);

	mu_assert_double_eq(5509.789467, poly_gramschmidt_float_result.modifiedArray1Sum);
	mu_assert_double_eq(3978217.527086, poly_gramschmidt_float_result.modifiedArray2Sum);
	mu_assert_double_eq(699.665477, poly_gramschmidt_float_result.modifiedArray3Sum);
}

/*MU_TEST(test_computationKernelPolybenchGramschmidt_d_float_assert) {
	mu_assert_double_eq(12336000, poly_gramschmidt_d_float_result.initArray1Sum);
	mu_assert_double_eq(14096000, poly_gramschmidt_d_float_result.initArray2Sum);
	mu_assert_double_eq(12336000, poly_gramschmidt_d_float_result.initArray3Sum);

	mu_assert_double_eq(2246310, poly_gramschmidt_d_float_result.modifiedArray1Sum);
	mu_assert_double_eq(7057716, poly_gramschmidt_d_float_result.modifiedArray2Sum);
	mu_assert_double_eq(221761, poly_gramschmidt_d_float_result.modifiedArray3Sum);
}*/

/*MU_TEST(test_computationKernelPolybenchTislov_Float_assert) {
	mu_assert_double_eq(31510, poly_tislov_float_result.initBSum);
	mu_assert_double_eq(31510, poly_tislov_float_result.initXSum);
	mu_assert_double_eq(269500000, poly_tislov_float_result.initLSum);

	mu_assert_double_eq(1004500, poly_tislov_float_result.modifiedBSum);
	mu_assert_double_eq(18446744073708178216, poly_tislov_float_result.modifiedXSum);
	mu_assert_double_eq(266302052, poly_tislov_float_result.modifiedLSum);
}*/

MU_TEST(test_computationKernelGenericLoop1D2A_Float_assert) {
	mu_assert_double_eq(560, genericLoop1D2A_float_result.initArray1Sum);
	mu_assert_double_eq(560, genericLoop1D2A_float_result.initArray2Sum);

	mu_assert_double_eq(155044.793678, genericLoop1D2A_float_result.modifiedArray1Sum);
	mu_assert_double_eq(4530.000000, genericLoop1D2A_float_result.modifiedArray2Sum);
}

MU_TEST(test_computationKernelGenericLoop1D2AD_Float_assert) {
	mu_assert_double_eq(560.000000, genericLoop1D2AD_float_result.initArray1Sum);
	mu_assert_double_eq(560.000000, genericLoop1D2AD_float_result.initArray2Sum);

	mu_assert_double_eq(155044.793678, genericLoop1D2AD_float_result.modifiedArray1Sum);
	mu_assert_double_eq(4530.000000, genericLoop1D2AD_float_result.modifiedArray2Sum);
}

MU_TEST(test_computationKernelGenericLoop2D2A_Float_assert) {
	mu_assert_double_eq(56000.000000, genericLoop2D2A_float_result.initArray1Sum);
	mu_assert_double_eq(56000.000000, genericLoop2D2A_float_result.initArray2Sum);

	mu_assert_double_eq(453000.000000, genericLoop2D2A_float_result.modifiedArray1Sum);
	mu_assert_double_eq(50833683.529556, genericLoop2D2A_float_result.modifiedArray2Sum);
}

/*MU_TEST(test_computationKernelGenericLoop2D2AD_Float_assert) {
	mu_assert_double_eq(5510000, genericLoop2D2AD_float_result.initArray1Sum);
	mu_assert_double_eq(56000, genericLoop2D2AD_float_result.initArray2Sum);

	mu_assert_double_eq(44130000, genericLoop2D2AD_float_result.modifiedArray1Sum);
	mu_assert_double_eq(315593962, genericLoop2D2AD_float_result.modifiedArray2Sum);
}*/

/*MU_TEST(test_computationKernelGenericLoop3D2A_Float_assert) {
	mu_assert_double_eq(4600000, genericLoop3D2A_float_result.initArray1Sum);
	mu_assert_double_eq(4600000, genericLoop3D2A_float_result.initArray2Sum);

	mu_assert_double_eq(29500000, genericLoop3D2A_float_result.modifiedArray1Sum);
	mu_assert_double_eq(484000000, genericLoop3D2A_float_result.modifiedArray2Sum);
}*/
// ------------------------------------------------------------------ //


MU_TEST_SUITE(test_suite) {

	// ------------------------------------------------------------------ //
	// Tests int
	// ------------------------------------------------------------------ //
	MU_SUITE_CONFIGURE(&computationKernelPolybenchCholesky, NULL);
	MU_RUN_TEST(test_computationKernelPolybenchCholesky_assert);

	MU_SUITE_CONFIGURE(&computationKernelPolybenchCholeskyD, NULL);
	MU_RUN_TEST(test_computationKernelPolybenchCholesky_d_assert);


	//MU_SUITE_CONFIGURE(&computationKernelPolybench2mm, NULL);
	//MU_RUN_TEST(test_computationKernelPolybench2mm_assert);

	MU_SUITE_CONFIGURE(&computationKernelPolybenchGramschmidt, NULL);
	MU_RUN_TEST(test_computationKernelPolybenchGramschmidt_assert);
	// - Regardless if the array is allocated on heap or on stack, it should be the same.
	//MU_SUITE_CONFIGURE(&computationKernelPolybenchGramschmidtD, NULL);
	//MU_RUN_TEST(test_computationKernelPolybenchGramschmidt_d_assert);

	MU_SUITE_CONFIGURE(&computationKernelGenericLoop1D2A, NULL);
	MU_RUN_TEST(test_computationKernelGenericLoop1D2A_assert);
	// - Regardless if the array is allocated on heap or on stack, it should be the same.
	MU_SUITE_CONFIGURE(&computationKernelGenericLoop1D2AD, NULL);
	MU_RUN_TEST(test_computationKernelGenericLoop1D2AD_assert);

	MU_SUITE_CONFIGURE(&computationKernelGenericLoop2D2A, NULL);
	MU_RUN_TEST(test_computationKernelGenericLoop2D2A_assert);
	/*
	// ------------------------------------------------------------------ //
	// Tests float
	// ------------------------------------------------------------------ //
	MU_SUITE_CONFIGURE(&computationKernelPolybenchCholeskyFloat, NULL);
	MU_RUN_TEST(test_computationKernelPolybenchCholeskyFloat_assert);

	//MU_SUITE_CONFIGURE(&computationKernelPolybench2mmFloat, NULL);
	//MU_RUN_TEST(test_computationKernelPolybench2mmFloat_assert);

	MU_SUITE_CONFIGURE(&computationKernelPolybenchGramschmidtFloat, NULL);
	MU_RUN_TEST(test_computationKernelPolybenchGramschmidtFloat_assert);
	// - Regardless if the array is allocated on heap or on stack, it should be the same.
	//MU_SUITE_CONFIGURE(&computationKernelPolybenchGramschmidtDFloat, NULL);
	//MU_RUN_TEST(test_computationKernelPolybenchGramschmidt_d_Float_assert);

	MU_SUITE_CONFIGURE(&computationKernelGenericLoop1D2AFloat, NULL);
	MU_RUN_TEST(test_computationKernelGenericLoop1D2A_Float_assert);
	// - Regardless if the array is allocated on heap or on stack, it should be the same.
	MU_SUITE_CONFIGURE(&computationKernelGenericLoop1D2ADFloat, NULL);
	MU_RUN_TEST(test_computationKernelGenericLoop1D2AD_Float_assert);

	MU_SUITE_CONFIGURE(&computationKernelGenericLoop2D2AFloat, NULL);
	MU_RUN_TEST(test_computationKernelGenericLoop2D2A_Float_assert);

	// ------------------------------------------------------------------------------------
	// TODO:
	// Tests that seem to be unstable:
	// ------------------------------------------------------------------------------------
	// - Regardless if the array is allocated on heap or on stack, it should be the same.
	MU_SUITE_CONFIGURE(&computationKernelGenericLoop2D2AD, NULL);
	MU_RUN_TEST(test_computationKernelGenericLoop2D2AD_mu_assert);

	MU_SUITE_CONFIGURE(&computationKernelPolybenchTislov, NULL);
	MU_RUN_TEST(test_computationKernelPolybenchTislov_assert);

	MU_SUITE_CONFIGURE(&computationKernelPolybench3mmFloat, NULL);
	MU_RUN_TEST(test_computationKernelPolybench3mmFloat_assert);

	//MU_SUITE_CONFIGURE(&computationKernelPolybenchDoitgen, NULL);
	//MU_RUN_TEST(test_computationKernelPolybenchDoitgen_assert);

	// - Regardles if the array is allocated on heap or on stack, it should be the same.
	MU_SUITE_CONFIGURE(&computationKernelPolybenchCholeskyDFloat, NULL);
	MU_RUN_TEST(test_computationKernelPolybenchCholesky_d_float_assert);
*/
	//MU_SUITE_CONFIGURE(&computationKernelPolybenchLu, NULL);
	//MU_RUN_TEST(test_computationKernelPolybenchLu_assert);

	//MU_SUITE_CONFIGURE(&computationKernelGenericLoop3D2A, NULL);
	//MU_RUN_TEST(test_computationKernelGenericLoop3D2A_assert);
	// ------------------------------------------------------------------------------------
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
