#include "computationKernels.h"
#include "kernelConfiguration.h"
#include "minunit/minunit.h"

/**
* Test for all computing kernels.
*/

// ------------------------------------------------------------------ //
// Tests int
// ------------------------------------------------------------------ //
MU_TEST(test_computationKernelPolybenchCholesky_assert) {
    mu_assert_long_eq(792120000, poly_cholesky_result.initArraySum);
    mu_assert_long_eq(437150775155, poly_cholesky_result.modifiedArraySum);
}

MU_TEST(test_computationKernelPolybenchCholesky_d_assert) {
    mu_assert_long_eq(792120000, poly_cholesky_d_result.initArraySum);
    mu_assert_long_eq(426351675155, poly_cholesky_d_result.modifiedArraySum);
}

MU_TEST(test_computationKernelPolybench2mm_assert) {
    mu_assert_long_eq(6061000, poly_2mm_result.initArray1Sum);
    mu_assert_long_eq(6612000, poly_2mm_result.initArray2Sum);
    mu_assert_long_eq(7271000, poly_2mm_result.initArray3Sum);
    mu_assert_long_eq(7878000, poly_2mm_result.initArray4Sum);
    mu_assert_long_eq(7163000, poly_2mm_result.initArray5Sum);

    mu_assert_long_eq(101710983800, poly_2mm_result.modifiedArray1Sum);
    mu_assert_long_eq(10194849800, poly_2mm_result.modifiedArray2Sum);
    mu_assert_long_eq(7279500, poly_2mm_result.modifiedArray3Sum);
    mu_assert_long_eq(7878000, poly_2mm_result.modifiedArray4Sum);
    mu_assert_long_eq(4049265389000, poly_2mm_result.modifiedArray5Sum);
}

MU_TEST(test_computationKernelPolybench3mm_assert) {
    mu_assert_long_eq(7272000, poly_3mm_result.initArray1Sum);
    mu_assert_long_eq(7932000, poly_3mm_result.initArray2Sum);
    mu_assert_long_eq(10740000, poly_3mm_result.initArray3Sum);
    mu_assert_long_eq(11564000, poly_3mm_result.initArray4Sum);
    mu_assert_long_eq(7272000, poly_3mm_result.initArray5Sum);
    mu_assert_long_eq(10024000, poly_3mm_result.initArray6Sum);

    mu_assert_long_eq(6233800, poly_3mm_result.modifiedArray1Sum);
    mu_assert_long_eq(7940000, poly_3mm_result.modifiedArray2Sum);
    mu_assert_long_eq(10719000, poly_3mm_result.modifiedArray3Sum);
    mu_assert_long_eq(11564000, poly_3mm_result.modifiedArray4Sum);
    mu_assert_long_eq(41206453600, poly_3mm_result.modifiedArray5Sum);
    mu_assert_long_eq(82800060000, poly_3mm_result.modifiedArray6Sum);
}

MU_TEST(test_computationKernelPolybenchDoitgen_assert) {
    mu_assert_long_eq(910, poly_doitgen_result.initSumSum);
    mu_assert_long_eq(222000, poly_doitgen_result.initC4Sum);
    mu_assert_long_eq(32870000, poly_doitgen_result.initASum);

    mu_assert_long_eq(1998000, poly_doitgen_result.modifiedSumSum);
    mu_assert_long_eq(222100, poly_doitgen_result.modifiedC4Sum);
    mu_assert_long_eq(36485700000, poly_doitgen_result.modifiedASum);
}

MU_TEST(test_computationKernelPolybenchGramschmidt_assert) {
    mu_assert_long_eq(6612000, poly_gramschmidt_result.initArray1Sum);
    mu_assert_long_eq(7932000, poly_gramschmidt_result.initArray2Sum);
    mu_assert_long_eq(6612000, poly_gramschmidt_result.initArray3Sum);

    mu_assert_long_eq(5510, poly_gramschmidt_result.modifiedArray1Sum);
    mu_assert_long_eq(3973316, poly_gramschmidt_result.modifiedArray2Sum);
    mu_assert_long_eq(401, poly_gramschmidt_result.modifiedArray3Sum);
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
    mu_assert_long_eq(31510, poly_tislov_result.initBSum);
    mu_assert_long_eq(31510, poly_tislov_result.initXSum);
    mu_assert_long_eq(269570000, poly_tislov_result.initLSum);

    mu_assert_long_eq(24496500, poly_tislov_result.modifiedBSum);
    mu_assert_long_eq(7000, poly_tislov_result.modifiedXSum);
    mu_assert_long_eq(71605316890, poly_tislov_result.modifiedLSum);
}

MU_TEST(test_computationKernelGenericLoop1D2A_assert) {
    mu_assert_long_eq(180000010, genericLoop1D2A_result.initArray1Sum);
    mu_assert_long_eq(180000010, genericLoop1D2A_result.initArray2Sum);

    mu_assert_long_eq(363782569296181, genericLoop1D2A_result.modifiedArray1Sum);
    mu_assert_long_eq(1140000000, genericLoop1D2A_result.modifiedArray2Sum);
}

MU_TEST(test_computationKernelGenericLoop1D2AD_assert) {
    mu_assert_long_eq(220000010, genericLoop1D2AD_result.initArray1Sum);
    mu_assert_long_eq(220000010, genericLoop1D2AD_result.initArray2Sum);

    mu_assert_long_eq(363782405902771, genericLoop1D2AD_result.modifiedArray1Sum);
    mu_assert_long_eq(1140000020, genericLoop1D2AD_result.modifiedArray2Sum);
}

MU_TEST(test_computationKernelGenericLoop2D2A_assert) {
    mu_assert_long_eq(222000, genericLoop2D2A_result.initArray1Sum);
    mu_assert_long_eq(222000, genericLoop2D2A_result.initArray2Sum);

    mu_assert_long_eq(1786000, genericLoop2D2A_result.modifiedArray1Sum);
    mu_assert_long_eq(610046758, genericLoop2D2A_result.modifiedArray2Sum);
}

MU_TEST(test_computationKernelGenericLoop2D2AD_assert) {
    mu_assert_long_eq(319200, genericLoop2D2AD_result.initArray1Sum);
    mu_assert_long_eq(319200, genericLoop2D2AD_result.initArray2Sum);

    mu_assert_long_eq(2565600, genericLoop2D2AD_result.modifiedArray1Sum);
    mu_assert_long_eq(319200, genericLoop2D2AD_result.modifiedArray2Sum);
}

MU_TEST(test_computationKernelGenericLoop3D2A_assert) {
    mu_assert_long_eq(6110500, genericLoop3D2A_result.initArray1Sum);
    mu_assert_long_eq(6110500, genericLoop3D2A_result.initArray2Sum);

    mu_assert_long_eq(39143500, genericLoop3D2A_result.modifiedArray1Sum);
    mu_assert_long_eq(640344100, genericLoop3D2A_result.modifiedArray2Sum);
}

MU_TEST(test_computationKernelPolybenchNussinov_assert) {
    mu_assert_long_eq(3332, poly_nussinov_result.initSeqSum);
    mu_assert_long_eq(0, poly_nussinov_result.initTableSum);

    mu_assert_long_eq(3332, poly_nussinov_result.modifiedSeqSum);
    mu_assert_long_eq(868399171, poly_nussinov_result.modifiedTableSum);
}

MU_TEST(test_computationKernelPolybenchLu_assert) {
    mu_assert_long_eq(22020000, poly_lu_result.initArraySum);
    mu_assert_long_eq(21495192, poly_lu_result.modifiedArraySum);
}

MU_TEST(test_computationKernelPolybenchHeat3D_assert) {
    mu_assert_long_eq(51515050, poly_heat3d_result.initASum);
    mu_assert_long_eq(51515050, poly_heat3d_result.initBSum);
    mu_assert_long_eq(240583020, poly_heat3d_result.modifiedAProduct);
    mu_assert_long_eq(51515050, poly_heat3d_result.modifiedASum);
    mu_assert_long_eq(51515050, poly_heat3d_result.modifiedBSum);
}

/* ------------------------------------------------------------------ */



// ------------------------------------------------------------------ //
// Tests float
// ------------------------------------------------------------------ //


MU_TEST(test_computationKernelPolybenchHeat3DFloat_assert) {
    mu_assert_double_eq(348550.999979, poly_heat3d_float_result.initASum);
    mu_assert_double_eq(348550.999979, poly_heat3d_float_result.initBSum);
    //mu_assert_double_eq(240583020, poly_heat3d_float_result.modifiedAProduct);
    mu_assert_double_eq(28401.443425, poly_heat3d_float_result.modifiedASum);
    mu_assert_double_eq(25775.144142, poly_heat3d_float_result.modifiedBSum);
}

MU_TEST(test_computationKernelPolybenchCholeskyFloat_assert) {
    mu_assert_double_eq(907227617.164135, poly_cholesky_float_result.initArraySum);
    mu_assert_double_eq(66008999.665696, poly_cholesky_float_result.modifiedArraySum);
}
MU_TEST(test_computationKernelPolybenchCholesky_d_float_assert) {
    mu_assert_double_eq(22020000.000000, poly_cholesky_d_float_result.initArraySum);
    mu_assert_double_eq(1335333.000012, poly_cholesky_d_float_result.modifiedArraySum);
}

MU_TEST(test_computationKernelPolybenchCholeskyAlt_assert) {
    mu_assert_double_eq(5337332.999802, poly_cholesky_float_result.initArraySum);
    mu_assert_double_eq(76098.709340, poly_cholesky_float_result.modifiedArraySum);
}

MU_TEST(test_computationKernelPolybenchLuFloat_assert) {
    mu_assert_double_eq(25204600.476503, poly_lu_float_result.initArraySum);
    mu_assert_double_eq(-1624395.785872, poly_lu_float_result.modifiedArraySum);
}

MU_TEST(test_computationKernelPolybench2mmFloat_assert) {
    mu_assert_double_eq(6932530.130947, poly_2mm_float_result.initArray1Sum);
    mu_assert_double_eq(7562760.142851, poly_2mm_float_result.initArray2Sum);
    mu_assert_double_eq(8318530.157173, poly_2mm_float_result.initArray3Sum);
    mu_assert_double_eq(9011990.170252, poly_2mm_float_result.initArray4Sum);
    mu_assert_double_eq(8192990.154755, poly_2mm_float_result.initArray5Sum);

    mu_assert_double_eq(1041715.237999, poly_2mm_float_result.modifiedArray1Sum);
    mu_assert_double_eq(7084382.107496, poly_2mm_float_result.modifiedArray2Sum);
    mu_assert_double_eq(8306320.156598, poly_2mm_float_result.modifiedArray3Sum);
    mu_assert_double_eq(9011990.170252, poly_2mm_float_result.modifiedArray4Sum);
    mu_assert_double_eq(21554794.254911, poly_2mm_float_result.modifiedArray5Sum);
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

MU_TEST(test_computationKernelPolybenchDoitgenFloat_assert) {
    mu_assert_double_eq(1770.000000, poly_doitgen_float_result.initSumSum);
    mu_assert_double_eq(566400, poly_doitgen_float_result.initC4Sum);
    mu_assert_double_eq(121856000, poly_doitgen_float_result.initASum);

    mu_assert_double_eq(5097600, poly_doitgen_float_result.modifiedSumSum);
    mu_assert_double_eq(566800, poly_doitgen_float_result.modifiedC4Sum);
    mu_assert_double_eq(215685120000, poly_doitgen_float_result.modifiedASum);
}

MU_TEST(test_computationKernelPolybenchGramschmidtFloat_assert) {
    mu_assert_double_eq(7562760.142851, poly_gramschmidt_float_result.initArray1Sum);
    mu_assert_double_eq(9074760.171461, poly_gramschmidt_float_result.initArray2Sum);
    mu_assert_double_eq(7562760.142851, poly_gramschmidt_float_result.initArray3Sum);

    mu_assert_double_eq(6301.698551, poly_gramschmidt_float_result.modifiedArray1Sum);
    mu_assert_double_eq(4555361.889813, poly_gramschmidt_float_result.modifiedArray2Sum);
    mu_assert_double_eq(737.342086, poly_gramschmidt_float_result.modifiedArray3Sum);
}

MU_TEST(test_computationKernelPolybenchGramschmidt_d_float_assert) {
    mu_assert_double_eq(12336000, poly_gramschmidt_d_float_result.initArray1Sum);
    mu_assert_double_eq(14096000, poly_gramschmidt_d_float_result.initArray2Sum);
    mu_assert_double_eq(12336000, poly_gramschmidt_d_float_result.initArray3Sum);

    mu_assert_double_eq(2246310, poly_gramschmidt_d_float_result.modifiedArray1Sum);
    mu_assert_double_eq(7057716, poly_gramschmidt_d_float_result.modifiedArray2Sum);
    mu_assert_double_eq(221761, poly_gramschmidt_d_float_result.modifiedArray3Sum);
}

MU_TEST(test_computationKernelPolybenchTislov_Float_assert) {
    mu_assert_double_eq(31510, poly_tislov_float_result.initBSum);
    mu_assert_double_eq(31510, poly_tislov_float_result.initXSum);
    mu_assert_double_eq(269500000, poly_tislov_float_result.initLSum);

    mu_assert_double_eq(1004500, poly_tislov_float_result.modifiedBSum);
    mu_assert_double_eq(18446744073708178216, poly_tislov_float_result.modifiedXSum);
    mu_assert_double_eq(266302052, poly_tislov_float_result.modifiedLSum);
}


MU_TEST(test_computationKernelPolybenchTislovFull_assert) {
    mu_assert_double_eq(9606.699999, poly_tislov_float_result.initBSum);
    mu_assert_double_eq(9606.699999, poly_tislov_float_result.initXSum);
    mu_assert_double_eq(16131680.304909, poly_tislov_float_result.initLSum);

    mu_assert_double_eq(1279200.000000, poly_tislov_float_result.modifiedBSum);
    mu_assert_double_eq(320000.500000, poly_tislov_float_result.modifiedXSum);
    mu_assert_double_eq(862020576.351822, poly_tislov_float_result.modifiedLSum);
}

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

MU_TEST(test_computationKernelGenericLoop2D2AD_Float_assert) {
    mu_assert_double_eq(5510000, genericLoop2D2AD_float_result.initArray1Sum);
    mu_assert_double_eq(56000, genericLoop2D2AD_float_result.initArray2Sum);

    mu_assert_double_eq(44130000, genericLoop2D2AD_float_result.modifiedArray1Sum);
    mu_assert_double_eq(315593962, genericLoop2D2AD_float_result.modifiedArray2Sum);
}

MU_TEST(test_computationKernelGenericLoop3D2A_Float_assert) {
    mu_assert_double_eq(4600000, genericLoop3D2A_float_result.initArray1Sum);
    mu_assert_double_eq(4600000, genericLoop3D2A_float_result.initArray2Sum);

    mu_assert_double_eq(29500000, genericLoop3D2A_float_result.modifiedArray1Sum);
    mu_assert_double_eq(483999999, genericLoop3D2A_float_result.modifiedArray2Sum);
}

MU_TEST(test_computationKernelPolybenchJacobi2D_assert) {
    mu_assert_double_eq(665671000.097299, poly_jacobi2d_result.initASum);
    mu_assert_double_eq(665673000.130676, poly_jacobi2d_result.initBSum);

    mu_assert_double_eq(668632079.242028, poly_jacobi2d_result.modifiedASum);
    mu_assert_double_eq(668631380.389699, poly_jacobi2d_result.modifiedBSum);
}

MU_TEST(test_computationKernelPolybenchFDTD2D_assert) {
    mu_assert_double_eq(31316381.591379, poly_fdtd2d_result.initExSum);
    mu_assert_double_eq(31411172.045928, poly_fdtd2d_result.initEySum);
    mu_assert_double_eq(31505962.500513, poly_fdtd2d_result.initHzSum);
    mu_assert_double_eq(217470.000000, poly_fdtd2d_result.initFictSum);

    mu_assert_double_eq(408586796.008320, poly_fdtd2d_result.modifiedExSum);
    mu_assert_double_eq(436451740.355241, poly_fdtd2d_result.modifiedEySum);
    mu_assert_double_eq(50415011.005579, poly_fdtd2d_result.modifiedHzSum);
    mu_assert_double_eq(217470.000000, poly_fdtd2d_result.modifiedFictSum);
}
// ------------------------------------------------------------------ //


MU_TEST_SUITE(test_suite) {

	// ------------------------------------------------------------------ //
	// Tests int
	// ------------------------------------------------------------------ //
	//MU_SUITE_CONFIGURE(&computationKernelPolybenchCholesky, NULL);
	//MU_RUN_TEST(test_computationKernelPolybenchCholesky_assert);

	//MU_SUITE_CONFIGURE(&computationKernelPolybenchCholeskyD, NULL);
	//MU_RUN_TEST(test_computationKernelPolybenchCholesky_d_assert);

    //MU_SUITE_CONFIGURE(&computationKernelPolybench2mm, NULL);
	//MU_RUN_TEST(test_computationKernelPolybench2mm_assert);

    //MU_SUITE_CONFIGURE(&computationKernelPolybench3mm, NULL);
	//MU_RUN_TEST(test_computationKernelPolybench3mm_assert);

	//MU_SUITE_CONFIGURE(&computationKernelPolybenchGramschmidt, NULL);
	//MU_RUN_TEST(test_computationKernelPolybenchGramschmidt_assert);

	// - Regardless if the array is allocated on heap or on stack, it should be the same.
	//MU_SUITE_CONFIGURE(&computationKernelPolybenchGramschmidtD, NULL);
	//MU_RUN_TEST(test_computationKernelPolybenchGramschmidt_d_assert);

	//MU_SUITE_CONFIGURE(&computationKernelGenericLoop1D2A, NULL);
	//MU_RUN_TEST(test_computationKernelGenericLoop1D2A_assert);
	// - Regardless if the array is allocated on heap or on stack, it should be the same.
	//MU_SUITE_CONFIGURE(&computationKernelGenericLoop1D2AD, NULL);
	//MU_RUN_TEST(test_computationKernelGenericLoop1D2AD_assert);

	//MU_SUITE_CONFIGURE(&computationKernelGenericLoop2D2A, NULL);
	//MU_RUN_TEST(test_computationKernelGenericLoop2D2A_assert);


	//MU_SUITE_CONFIGURE(&computationKernelGenericLoop2D2AD, NULL);
	//MU_RUN_TEST(test_computationKernelGenericLoop2D2AD_assert);
	
	//MU_SUITE_CONFIGURE(&computationKernelPolybenchTislov, NULL);
	//MU_RUN_TEST(test_computationKernelPolybenchTislov_assert);

	//MU_SUITE_CONFIGURE(&computationKernelPolybenchDoitgen, NULL);
	//MU_RUN_TEST(test_computationKernelPolybenchDoitgen_assert);


	//MU_SUITE_CONFIGURE(&computationKernelGenericLoop3D2A, NULL);
	//MU_RUN_TEST(test_computationKernelGenericLoop3D2A_assert);

	//MU_SUITE_CONFIGURE(&computationKernelPolybenchLu, NULL);
	//MU_RUN_TEST(test_computationKernelPolybenchLu_assert);

	//MU_SUITE_CONFIGURE(&computationKernelPolybenchNussinov, NULL);
	//MU_RUN_TEST(test_computationKernelPolybenchNussinov_assert);

	//MU_SUITE_CONFIGURE(&computationKernelPolybenchHeat3D, NULL);
	//MU_RUN_TEST(test_computationKernelPolybenchHeat3D_assert);
	// ------------------------------------------------------------------ //
	// Tests float
	// ------------------------------------------------------------------ //

	//MU_SUITE_CONFIGURE(&computationKernelPolybenchFDTD2D, NULL);
	//MU_RUN_TEST(test_computationKernelPolybenchFDTD2D_assert);

	//MU_SUITE_CONFIGURE(&computationKernelPolybenchJacobi2D, NULL);
	//MU_RUN_TEST(test_computationKernelPolybenchJacobi2D_assert);

	//MU_SUITE_CONFIGURE(&computationKernelPolybenchHeat3DFloat, NULL);
	//MU_RUN_TEST(test_computationKernelPolybenchHeat3DFloat_assert);

	//MU_SUITE_CONFIGURE(&computationKernelPolybenchTislovFull, NULL);
	//MU_RUN_TEST(test_computationKernelPolybenchTislovFull_assert);

    //MU_SUITE_CONFIGURE(&computationKernelPolybenchLuFloat, NULL);
    //MU_RUN_TEST(test_computationKernelPolybenchLuFloat_assert);

	//MU_SUITE_CONFIGURE(&computationKernelPolybenchCholeskyFloat, NULL);
	//MU_RUN_TEST(test_computationKernelPolybenchCholeskyFloat_assert);

	//MU_SUITE_CONFIGURE(&computationKernelPolybenchCholeskyAlt, NULL);
	//MU_RUN_TEST(test_computationKernelPolybenchCholeskyAlt_assert);

	//MU_SUITE_CONFIGURE(&computationKernelPolybench2mmFloat, NULL);
	//MU_RUN_TEST(test_computationKernelPolybench2mmFloat_assert);

	//MU_SUITE_CONFIGURE(&computationKernelPolybenchGramschmidtFloat, NULL);
	//MU_RUN_TEST(test_computationKernelPolybenchGramschmidtFloat_assert);
	// - Regardless if the array is allocated on heap or on stack, it should be the same.
	//MU_SUITE_CONFIGURE(&computationKernelPolybenchGramschmidtDFloat, NULL);
	//MU_RUN_TEST(test_computationKernelPolybenchGramschmidt_d_Float_assert);

	//MU_SUITE_CONFIGURE(&computationKernelGenericLoop1D2AFloat, NULL);
	//MU_RUN_TEST(test_computationKernelGenericLoop1D2A_Float_assert);
	// - Regardless if the array is allocated on heap or on stack, it should be the same.
	//MU_SUITE_CONFIGURE(&computationKernelGenericLoop1D2ADFloat, NULL);
	//MU_RUN_TEST(test_computationKernelGenericLoop1D2AD_Float_assert);

	//MU_SUITE_CONFIGURE(&computationKernelGenericLoop2D2AFloat, NULL);
	//MU_RUN_TEST(test_computationKernelGenericLoop2D2A_Float_assert);

/*
	// ------------------------------------------------------------------------------------
	// TODO:
	// Tests that seem to be unstable:
	// ------------------------------------------------------------------------------------
	// - Regardless if the array is allocated on heap or on stack, it should be the same.


	MU_SUITE_CONFIGURE(&computationKernelPolybench3mmFloat, NULL);
	MU_RUN_TEST(test_computationKernelPolybench3mmFloat_assert);


	// - Regardles if the array is allocated on heap or on stack, it should be the same.
	MU_SUITE_CONFIGURE(&computationKernelPolybenchCholeskyDFloat, NULL);
	MU_RUN_TEST(test_computationKernelPolybenchCholesky_d_float_assert);
*/

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
