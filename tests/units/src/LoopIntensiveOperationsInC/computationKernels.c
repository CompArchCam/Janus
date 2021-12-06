/**
 * Implementation of computing intensive applications and patterns.
 */

#include <unistd.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>     /* srand, rand, malloc */


#include "populateArrays.h"
#include "computationKernels.h"

// Polybench, cholesky
void computationKernelPolybenchCholesky()
{
	printf("		--- computationKernelPolybenchCholesky.\n");

	ARRAY_LENGTH_D1 = POLY_CHOLESKY_ARRAY_001_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_CHOLESKY_ARRAY_001_LENGTH_D2;
	static int array_cholesky[POLY_CHOLESKY_ARRAY_001_LENGTH_D1][POLY_CHOLESKY_ARRAY_001_LENGTH_D2];

	populate2DIntArrayS(array_cholesky, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	long sum = sum2DIntArrayS(array_cholesky, POLY_CHOLESKY_ARRAY_001_LENGTH_D1, POLY_CHOLESKY_ARRAY_001_LENGTH_D2);
	// - Log the initial array sum.
	poly_cholesky_result.initArraySum=sum;

	printf("		--- sum=%lu.\n", sum);

    for (int i = 0; i < ARRAY_LENGTH_D1; i++)
    {
        for (int j = 0; j <= i; j++)
        {
        	array_cholesky[i][j] = (int)(-j % ARRAY_LENGTH_D1) / ARRAY_LENGTH_D1 + 1;
        }
        for (int j = i+1; j < ARRAY_LENGTH_D1; j++)
        {
        	array_cholesky[i][j] = 0;
        }
        array_cholesky[i][i] = 1;
    }
    //printf("	--- Populate arrays.\n");

    // Calculate the sum after array modification.
    sum = sum2DIntArrayS(array_cholesky, POLY_CHOLESKY_ARRAY_001_LENGTH_D1, POLY_CHOLESKY_ARRAY_001_LENGTH_D2);
    // - Log the modified array sum.
    poly_cholesky_result.modifiedArraySum=sum;
    printf("		--- sum=%lu.\n", sum);
}

// Polybench, cholesky
void computationKernelPolybenchCholeskyFloat()
{
	printf("		--- computationKernelPolybenchCholeskyFloat.\n");

	ARRAY_LENGTH_D1 = POLY_CHOLESKY_ARRAY_001_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_CHOLESKY_ARRAY_001_LENGTH_D2;
	static float array_cholesky[POLY_CHOLESKY_ARRAY_001_LENGTH_D1][POLY_CHOLESKY_ARRAY_001_LENGTH_D2];

	populate2DFloatArrayS(array_cholesky, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	double sum = sum2DFloatArrayS(array_cholesky, POLY_CHOLESKY_ARRAY_001_LENGTH_D1, POLY_CHOLESKY_ARRAY_001_LENGTH_D2);
	// - Log the initial array sum.
	poly_cholesky_float_result.initArraySum=sum;

	printf("		--- sum=%f.\n", sum);

    for (int i = 0; i < ARRAY_LENGTH_D1; i++)
    {
        for (int j = 0; j <= i; j++)
        {
        	array_cholesky[i][j] = (float)(-j % ARRAY_LENGTH_D1) / ARRAY_LENGTH_D1 + 1;
        }
        for (int j = i+1; j < ARRAY_LENGTH_D1; j++)
        {
        	array_cholesky[i][j] = 0;
        }
        array_cholesky[i][i] = 1;
    }
    //printf("	--- Populate arrays.\n");

    // Calculate the sum after array modification.
    sum = sum2DFloatArrayS(array_cholesky, POLY_CHOLESKY_ARRAY_001_LENGTH_D1, POLY_CHOLESKY_ARRAY_001_LENGTH_D2);
    // - Log the modified array sum.
    poly_cholesky_float_result.modifiedArraySum=sum;
    printf("		--- sum=%f.\n", sum);
}

// Polybench, cholesky
// - Dynamically allocated arrays.
void computationKernelPolybenchCholeskyD()
{
	printf("		--- computationKernelPolybenchCholeskyD.\n");

	ARRAY_LENGTH_D1 = POLY_CHOLESKY_ARRAY_001_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_CHOLESKY_ARRAY_001_LENGTH_D2;
	int** array_cholesky;

	allocate2DIntArrayD(&array_cholesky, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	populate2DIntArrayD(&array_cholesky, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);
	//printf("	--- array_cholesky populated.\n");

	// Calculate the sum before array modification.
	long sum = sum2DIntArrayD(array_cholesky, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);
	// - Log the initial array sum.
	poly_cholesky_d_result.initArraySum=sum;

	printf("		--- sum=%lu.\n", sum);

    for (int i = 0; i < ARRAY_LENGTH_D1; i++)
    {
        for (int j = 0; j <= i; j++)
        {
        	array_cholesky[i][j] = (int)(-j % ARRAY_LENGTH_D1) / ARRAY_LENGTH_D1 + 1;
        }
        for (int j = i+1; j < ARRAY_LENGTH_D1; j++)
        {
        	array_cholesky[i][j] = 0;
        }
        array_cholesky[i][i] = 1;
    }
    //printf("	--- Populate arrays.\n");

    // Calculate the sum after array modification.
    sum = sum2DIntArrayD(array_cholesky, POLY_CHOLESKY_ARRAY_001_LENGTH_D1, POLY_CHOLESKY_ARRAY_001_LENGTH_D2);
    // - Log the modified array sum.
    poly_cholesky_d_result.modifiedArraySum=sum;
    printf("		--- sum=%lu.\n", sum);

    // Free the memory, avoid memory leak.
	for(int i=0; i<ARRAY_LENGTH_D1; i++)
		free(array_cholesky[i]);

	free(array_cholesky);
}

// Polybench, cholesky
// - Dynamically allocated arrays.
void computationKernelPolybenchCholeskyDFloat()
{
	printf("		--- computationKernelPolybenchCholeskyDFloat.\n");

	ARRAY_LENGTH_D1 = POLY_CHOLESKY_ARRAY_001_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_CHOLESKY_ARRAY_001_LENGTH_D2;
	float** array_cholesky;

	allocate2DFloatArrayD(&array_cholesky, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	populate2DFloatArrayD(&array_cholesky, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);
	//printf("	--- array_cholesky populated.\n");

	// Calculate the sum before array modification.
	double sum = sum2DFloatArrayD(array_cholesky, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);
	// - Log the initial array sum.
	poly_cholesky_d_float_result.initArraySum=sum;

	printf("		--- sum=%f.\n", sum);

    for (int i = 0; i < ARRAY_LENGTH_D1; i++)
    {
        for (int j = 0; j <= i; j++)
        {
        	array_cholesky[i][j] = (float)(-j % ARRAY_LENGTH_D1) / ARRAY_LENGTH_D1 + 1;
        }
        for (int j = i+1; j < ARRAY_LENGTH_D1; j++)
        {
        	array_cholesky[i][j] = 0;
        }
        array_cholesky[i][i] = 1;
    }
    //printf("	--- Populate arrays.\n");

    // Calculate the sum after array modification.
    sum = sum2DFloatArrayD(array_cholesky, POLY_CHOLESKY_ARRAY_001_LENGTH_D1, POLY_CHOLESKY_ARRAY_001_LENGTH_D2);
    // - Log the modified array sum.
    poly_cholesky_d_float_result.modifiedArraySum=sum;
    printf("		--- sum=%f.\n", sum);

    // Free the memory, avoid memory leak.
	for(int i=0; i<ARRAY_LENGTH_D1; i++)
		free(array_cholesky[i]);

	free(array_cholesky);
}

// Polybench, lu
void computationKernelPolybenchLu()
{
	printf("		--- computationKernelPolybenchLu.\n");
	// Stack arrays.
	ARRAY_LENGTH_D1 = POLY_LU_ARRAY_001_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_LU_ARRAY_001_LENGTH_D2;
	static int array_lu[POLY_LU_ARRAY_001_LENGTH_D1][POLY_LU_ARRAY_001_LENGTH_D2];
	populate2DIntArrayS(array_lu, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	long sum = sum2DIntArrayS(array_lu, POLY_LU_ARRAY_001_LENGTH_D1, POLY_LU_ARRAY_001_LENGTH_D2);
	// - Log the initial array sum.
	poly_lu_result.initArraySum=sum;
	printf("		--- sum=%lu.\n", sum);

    for(int i = 0; i < POLY_LU_ARRAY_001_LENGTH_D1; i++)
        for (int j = 0; j < POLY_LU_ARRAY_001_LENGTH_D2; j++)
        	array_lu[i][j] = (int) ((i*j+1) % POLY_LU_ARRAY_001_LENGTH_D1) / POLY_LU_ARRAY_001_LENGTH_D1;

    for(int i = 0; i < POLY_LU_ARRAY_001_LENGTH_D1; i++) {
      for (int j = 0; j <i; j++) {
         for (int k = 0; k < j; k++) {
        	 // array_lu[i][j] -= array_lu[i][k] * array_lu[k][j]; // Original
        	 array_lu[i][j] -= (array_lu[i][k] * array_lu[k][j])/POLY_LU_ARRAY_001_LENGTH_D1; // Modified
         }
         // array_lu[i][j] /= array_lu[j][j]; // Original
         array_lu[i][j] /= array_lu[j][j]; // Modified
      }
     for(int j = i; j < POLY_LU_ARRAY_001_LENGTH_D1; j++) {
         for(int k = 0; k < i; k++) {
        	 //array_lu[i][j] -= array_lu[i][k] * array_lu[k][j]; // Original
        	 array_lu[i][j] -= (array_lu[i][k] * array_lu[k][j])/POLY_LU_ARRAY_001_LENGTH_D1; // Modified
         }
      }
    }

    // Calculate the sum after array modification.
    sum = sum2DIntArrayS(array_lu, POLY_LU_ARRAY_001_LENGTH_D1, POLY_LU_ARRAY_001_LENGTH_D2);
    // - Log the modified array sum.
    poly_lu_result.modifiedArraySum=sum;
    printf("		--- sum=%lu.\n", sum);
}

// Polybench, lu
void computationKernelPolybenchLuFloat()
{
	printf("		--- computationKernelPolybenchLuFloat.\n");
	// Stack arrays.
	ARRAY_LENGTH_D1 = POLY_LU_ARRAY_001_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_LU_ARRAY_001_LENGTH_D2;
	static float array_lu[POLY_LU_ARRAY_001_LENGTH_D1][POLY_LU_ARRAY_001_LENGTH_D2];
	populate2DFloatArrayS(array_lu, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	double sum = sum2DFloatArrayS(array_lu, POLY_LU_ARRAY_001_LENGTH_D1, POLY_LU_ARRAY_001_LENGTH_D2);
	// - Log the initial array sum.
	poly_lu_float_result.initArraySum=sum;
	printf("		--- sum=%f.\n", sum);

    for(int i = 0; i < POLY_LU_ARRAY_001_LENGTH_D1; i++)
        for (int j = 0; j < POLY_LU_ARRAY_001_LENGTH_D2; j++)
        	array_lu[i][j] = (float) ((i*j+1) % POLY_LU_ARRAY_001_LENGTH_D1) / POLY_LU_ARRAY_001_LENGTH_D1;

    for(int i = 0; i < POLY_LU_ARRAY_001_LENGTH_D1; i++) {
      for (int j = 0; j <i; j++) {
         for (int k = 0; k < j; k++) {
        	 // array_lu[i][j] -= array_lu[i][k] * array_lu[k][j]; // Original
        	 array_lu[i][j] -= (float)  (array_lu[i][k] * array_lu[k][j])/POLY_LU_ARRAY_001_LENGTH_D1; // Modified
         }
         // array_lu[i][j] /= array_lu[j][j]; // Original
         array_lu[i][j] = (float) array_lu[i][j]/array_lu[j][j]; // Modified
      }
     for(int j = i; j < POLY_LU_ARRAY_001_LENGTH_D1; j++) {
         for(int k = 0; k < i; k++) {
        	 //array_lu[i][j] -= array_lu[i][k] * array_lu[k][j]; // Original
        	 array_lu[i][j] -= (float) (array_lu[i][k] * array_lu[k][j])/POLY_LU_ARRAY_001_LENGTH_D1; // Modified
         }
      }
    }

    // Calculate the sum after array modification.
    sum = sum2DFloatArrayS(array_lu, POLY_LU_ARRAY_001_LENGTH_D1, POLY_LU_ARRAY_001_LENGTH_D2);
    // - Log the modified array sum.
    poly_lu_float_result.modifiedArraySum=sum;
    printf("		--- sum=%f.\n", sum);
}

// Polybench, 2mm
void computationKernelPolybench2mm()
{
	printf("		--- computationKernelPolybench2mm.\n");
//	_PB_NI = 180 - ARRAY_001_LENGTH, ARRAY_002_LENGTH, ARRAY_005_LENGTH
//	_PB_NJ = 190 - ARRAY_001_WIDTH, ARRAY_003_WIDTH, ARRAY_004_LENGTH
//	_PB_NK = 210 - ARRAY_002_WIDTH, ARRAY_003_LENGTH
//  _PB_NL = 220. - ARRAY_004_WIDTH, ARRAY_005_WIDTH
//	alpha = 7064880.
//	beta = 7064880.
	//long alpha = 7064880; // Original
	long alpha = 3; // Modified
	//long beta = 7064880; // Original
	long beta = 3; // Modified

	// Stack arrays:
	ARRAY_LENGTH_D1 = POLY_2MM_ARRAY_001_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_2MM_ARRAY_001_LENGTH_D2;
	static int array1_2mm[POLY_2MM_ARRAY_001_LENGTH_D1][POLY_2MM_ARRAY_001_LENGTH_D2];
	populate2DIntArrayS(array1_2mm, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	long sum = sum2DIntArrayS(array1_2mm, POLY_2MM_ARRAY_001_LENGTH_D1, POLY_2MM_ARRAY_001_LENGTH_D2);
	// - Log the initial array sum.
	poly_2mm_result.initArray1Sum=sum;
	printf("		--- sum array1=%lu.\n", sum);

	ARRAY_LENGTH_D1 = POLY_2MM_ARRAY_002_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_2MM_ARRAY_002_LENGTH_D2;
	static int array2_2mm[POLY_2MM_ARRAY_002_LENGTH_D1][POLY_2MM_ARRAY_002_LENGTH_D2];
	populate2DIntArrayS(array2_2mm, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DIntArrayS(array2_2mm, POLY_2MM_ARRAY_002_LENGTH_D1, POLY_2MM_ARRAY_002_LENGTH_D2);
	// - Log the initial array sum.
	poly_2mm_result.initArray2Sum=sum;
	printf("		--- sum array2=%lu.\n", sum);

	ARRAY_LENGTH_D1 = POLY_2MM_ARRAY_003_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_2MM_ARRAY_003_LENGTH_D2;
	static int array3_2mm[POLY_2MM_ARRAY_003_LENGTH_D1][POLY_2MM_ARRAY_003_LENGTH_D2];
	populate2DIntArrayS(array3_2mm, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DIntArrayS(array3_2mm, POLY_2MM_ARRAY_003_LENGTH_D1, POLY_2MM_ARRAY_003_LENGTH_D2);
	// - Log the initial array sum.
	poly_2mm_result.initArray3Sum=sum;
	printf("		--- sum array3=%lu.\n", sum);

	ARRAY_LENGTH_D1 = POLY_2MM_ARRAY_004_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_2MM_ARRAY_004_LENGTH_D2;
	static int array4_2mm[POLY_2MM_ARRAY_003_LENGTH_D2][POLY_2MM_ARRAY_004_LENGTH_D2];
	populate2DIntArrayS(array4_2mm, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DIntArrayS(array4_2mm, POLY_2MM_ARRAY_004_LENGTH_D1, POLY_2MM_ARRAY_004_LENGTH_D2);
	// - Log the initial array sum.
	poly_2mm_result.initArray4Sum=sum;
	printf("		--- sum array4=%lu.\n", sum);

	ARRAY_LENGTH_D1 = POLY_2MM_ARRAY_005_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_2MM_ARRAY_005_LENGTH_D2;
	static int array5_2mm[POLY_2MM_ARRAY_005_LENGTH_D1][POLY_2MM_ARRAY_005_LENGTH_D2];
	populate2DIntArrayS(array5_2mm, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DIntArrayS(array5_2mm, POLY_2MM_ARRAY_005_LENGTH_D1, POLY_2MM_ARRAY_005_LENGTH_D2);
	// - Log the initial array sum.
	poly_2mm_result.initArray5Sum=sum;
	printf("		--- sum array5=%lu.\n", sum);

    for (int i = 0; i < POLY_2MM_ARRAY_001_LENGTH_D1; i++)
    {
        for (int j = 0; j < POLY_2MM_ARRAY_001_LENGTH_D2; j++)
        {
            array1_2mm[i][j] = 1.0;
            for (int k = 0; k < POLY_2MM_ARRAY_002_LENGTH_D2; ++k)
            {
            	// array1_2mm[i][j] += alpha * array2_2mm[i][k] * array3_2mm[k][j]; // Original
            	array1_2mm[i][j] += (alpha * array2_2mm[i][k] * array3_2mm[k][j])/POLY_2MM_ARRAY_002_LENGTH_D2; // Modified
            }

        }
    }
    for (int i = 0; i < POLY_2MM_ARRAY_001_LENGTH_D1; i++)
    {
        for (int j = 0; j < POLY_2MM_ARRAY_005_LENGTH_D1; j++)
        {
        	array5_2mm[i][j] *= beta;
            for (int k = 0; k < POLY_2MM_ARRAY_001_LENGTH_D2; ++k)
            {
            	//array5_2mm[i][j] += array1_2mm[i][k] * array4_2mm[k][j]; // Original
            	array5_2mm[i][j] += (array1_2mm[i][k]/POLY_2MM_ARRAY_001_LENGTH_D1) * (array4_2mm[k][j]/POLY_2MM_ARRAY_001_LENGTH_D2); // Modified
            }
        }
    }

    // Calculate the sum after array modification.
    sum = sum2DIntArrayS(array1_2mm, POLY_2MM_ARRAY_001_LENGTH_D1, POLY_2MM_ARRAY_001_LENGTH_D2);
    // - Log the modified array sum.
    poly_2mm_result.modifiedArray1Sum=sum;
    printf("		--- sum array1=%lu.\n", sum);

    sum = sum2DIntArrayS(array2_2mm, POLY_2MM_ARRAY_002_LENGTH_D1, POLY_2MM_ARRAY_002_LENGTH_D2);
    // - Log the modified array sum.
    poly_2mm_result.modifiedArray2Sum=sum;
    printf("		--- sum array2=%lu.\n", sum);

    sum = sum2DIntArrayS(array3_2mm, POLY_2MM_ARRAY_003_LENGTH_D1, POLY_2MM_ARRAY_003_LENGTH_D2);
    // - Log the modified array sum.
    poly_2mm_result.modifiedArray3Sum=sum;
    printf("		--- sum array3=%lu.\n", sum);

    sum = sum2DIntArrayS(array4_2mm, POLY_2MM_ARRAY_004_LENGTH_D1, POLY_2MM_ARRAY_004_LENGTH_D2);
    // - Log the modified array sum.
    poly_2mm_result.modifiedArray4Sum=sum;
    printf("		--- sum array4=%lu.\n", sum);

    sum = sum2DIntArrayS(array5_2mm, POLY_2MM_ARRAY_005_LENGTH_D1, POLY_2MM_ARRAY_005_LENGTH_D2);
    // - Log the modified array sum.
    poly_2mm_result.modifiedArray5Sum=sum;
    printf("		--- sum array5=%lu.\n", sum);
}

// Polybench, 2mm
void computationKernelPolybench2mmFloat()
{
	printf("		--- computationKernelPolybench2mmFloat.\n");
//	_PB_NI = 180 - ARRAY_001_LENGTH, ARRAY_002_LENGTH, ARRAY_005_LENGTH
//	_PB_NJ = 190 - ARRAY_001_WIDTH, ARRAY_003_WIDTH, ARRAY_004_LENGTH
//	_PB_NK = 210 - ARRAY_002_WIDTH, ARRAY_003_LENGTH
//  _PB_NL = 220. - ARRAY_004_WIDTH, ARRAY_005_WIDTH
//	alpha = 7064880.
//	beta = 7064880.
	//long alpha = 7064880; // Original
	long alpha = 3; // Modified
	//long beta = 7064880; // Original
	long beta = 3; // Modified

	// Stack arrays:
	ARRAY_LENGTH_D1 = POLY_2MM_ARRAY_001_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_2MM_ARRAY_001_LENGTH_D2;
	static float array1_2mm[POLY_2MM_ARRAY_001_LENGTH_D1][POLY_2MM_ARRAY_001_LENGTH_D2];
	populate2DFloatArrayS(array1_2mm, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	double sum = sum2DFloatArrayS(array1_2mm, POLY_2MM_ARRAY_001_LENGTH_D1, POLY_2MM_ARRAY_001_LENGTH_D2);
	// - Log the initial array sum.
	poly_2mm_float_result.initArray1Sum=sum;
	printf("		--- sum array1=%f.\n", sum);

	ARRAY_LENGTH_D1 = POLY_2MM_ARRAY_002_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_2MM_ARRAY_002_LENGTH_D2;
	static float array2_2mm[POLY_2MM_ARRAY_002_LENGTH_D1][POLY_2MM_ARRAY_002_LENGTH_D2];
	populate2DFloatArrayS(array2_2mm, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DFloatArrayS(array2_2mm, POLY_2MM_ARRAY_002_LENGTH_D1, POLY_2MM_ARRAY_002_LENGTH_D2);
	// - Log the initial array sum.
	poly_2mm_float_result.initArray2Sum=sum;
	printf("		--- sum array2=%f.\n", sum);

	ARRAY_LENGTH_D1 = POLY_2MM_ARRAY_003_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_2MM_ARRAY_003_LENGTH_D2;
	static float array3_2mm[POLY_2MM_ARRAY_003_LENGTH_D1][POLY_2MM_ARRAY_003_LENGTH_D2];
	populate2DFloatArrayS(array3_2mm, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DFloatArrayS(array3_2mm, POLY_2MM_ARRAY_003_LENGTH_D1, POLY_2MM_ARRAY_003_LENGTH_D2);
	// - Log the initial array sum.
	poly_2mm_float_result.initArray3Sum=sum;
	printf("		--- sum array3=%f.\n", sum);

	ARRAY_LENGTH_D1 = POLY_2MM_ARRAY_004_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_2MM_ARRAY_004_LENGTH_D2;
	static float array4_2mm[POLY_2MM_ARRAY_003_LENGTH_D2][POLY_2MM_ARRAY_004_LENGTH_D2];
	populate2DFloatArrayS(array4_2mm, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DFloatArrayS(array4_2mm, POLY_2MM_ARRAY_004_LENGTH_D1, POLY_2MM_ARRAY_004_LENGTH_D2);
	// - Log the initial array sum.
	poly_2mm_float_result.initArray4Sum=sum;
	printf("		--- sum array4=%f.\n", sum);

	ARRAY_LENGTH_D1 = POLY_2MM_ARRAY_005_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_2MM_ARRAY_005_LENGTH_D2;
	static float array5_2mm[POLY_2MM_ARRAY_005_LENGTH_D1][POLY_2MM_ARRAY_005_LENGTH_D2];
	populate2DFloatArrayS(array5_2mm, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DFloatArrayS(array5_2mm, POLY_2MM_ARRAY_005_LENGTH_D1, POLY_2MM_ARRAY_005_LENGTH_D2);
	// - Log the initial array sum.
	poly_2mm_float_result.initArray5Sum=sum;
	printf("		--- sum array5=%f.\n", sum);

    for (int i = 0; i < POLY_2MM_ARRAY_001_LENGTH_D1; i++)
    {
        for (int j = 0; j < POLY_2MM_ARRAY_001_LENGTH_D2; j++)
        {
            array1_2mm[i][j] = 1.0;
            for (int k = 0; k < POLY_2MM_ARRAY_002_LENGTH_D2; ++k)
            {
            	// array1_2mm[i][j] += alpha * array2_2mm[i][k] * array3_2mm[k][j]; // Original
            	array1_2mm[i][j] += (alpha * array2_2mm[i][k] * array3_2mm[k][j])/POLY_2MM_ARRAY_002_LENGTH_D2; // Modified
            }

        }
    }
    for (int i = 0; i < POLY_2MM_ARRAY_001_LENGTH_D1; i++)
    {
        for (int j = 0; j < POLY_2MM_ARRAY_005_LENGTH_D1; j++)
        {
        	array5_2mm[i][j] *= beta;
            for (int k = 0; k < POLY_2MM_ARRAY_001_LENGTH_D2; ++k)
            {
            	//array5_2mm[i][j] += array1_2mm[i][k] * array4_2mm[k][j]; // Original
            	array5_2mm[i][j] += (float) (array1_2mm[i][k]/POLY_2MM_ARRAY_001_LENGTH_D1) * (array4_2mm[k][j]/POLY_2MM_ARRAY_001_LENGTH_D2); // Modified
            }
        }
    }

    // Calculate the sum after array modification.
    sum = sum2DFloatArrayS(array1_2mm, POLY_2MM_ARRAY_001_LENGTH_D1, POLY_2MM_ARRAY_001_LENGTH_D2);
    // - Log the modified array sum.
    poly_2mm_float_result.modifiedArray1Sum=sum;
    printf("		--- sum array1=%f.\n", sum);

    sum = sum2DFloatArrayS(array2_2mm, POLY_2MM_ARRAY_002_LENGTH_D1, POLY_2MM_ARRAY_002_LENGTH_D2);
    // - Log the modified array sum.
    poly_2mm_float_result.modifiedArray2Sum=sum;
    printf("		--- sum array2=%f.\n", sum);

    sum = sum2DFloatArrayS(array3_2mm, POLY_2MM_ARRAY_003_LENGTH_D1, POLY_2MM_ARRAY_003_LENGTH_D2);
    // - Log the modified array sum.
    poly_2mm_float_result.modifiedArray3Sum=sum;
    printf("		--- sum array3=%f.\n", sum);

    sum = sum2DFloatArrayS(array4_2mm, POLY_2MM_ARRAY_004_LENGTH_D1, POLY_2MM_ARRAY_004_LENGTH_D2);
    // - Log the modified array sum.
    poly_2mm_float_result.modifiedArray4Sum=sum;
    printf("		--- sum array4=%f.\n", sum);

    sum = sum2DFloatArrayS(array5_2mm, POLY_2MM_ARRAY_005_LENGTH_D1, POLY_2MM_ARRAY_005_LENGTH_D2);
    // - Log the modified array sum.
    poly_2mm_float_result.modifiedArray5Sum=sum;
    printf("		--- sum array5=%f.\n", sum);
}

// Polybench, 3mm
void computationKernelPolybench3mm()
{
	printf("		--- computationKernelPolybench3mm.\n");
//	_PB_NI = 180. - ARRAY_001_LENGTH, ARRAY_005_LENGTH
//	_PB_NJ = 190. - ARRAY_001_WIDTH, ARRAY_002_WIDTH, ARRAY_003_LENGTH, ARRAY_005_WIDTH, ARRAY_006_LENGTH
//	_PB_NK = 200. - ARRAY_002_LENGTH,
//	_PB_NL = 210. - ARRAY_004_WIDTH, ARRAY_006_WIDTH
//	_PB_NM = 220. - ARRAY_003_WIDTH, ARRAY_004_LENGTH
	// Stack arrays:
	ARRAY_LENGTH_D1 = POLY_3MM_ARRAY_001_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_3MM_ARRAY_001_LENGTH_D2;
	static int array1_3mm[POLY_3MM_ARRAY_001_LENGTH_D1][POLY_3MM_ARRAY_001_LENGTH_D2];
	populate2DIntArrayS(array1_3mm, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	long sum = sum2DIntArrayS(array1_3mm, POLY_3MM_ARRAY_001_LENGTH_D1, POLY_3MM_ARRAY_001_LENGTH_D2);
	// - Log the initial array sum.
	poly_3mm_result.initArray1Sum=sum;
	printf("		--- sum array1=%lu.\n", sum);

	ARRAY_LENGTH_D1 = POLY_3MM_ARRAY_002_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_3MM_ARRAY_002_LENGTH_D2;
	static int array2_3mm[POLY_3MM_ARRAY_002_LENGTH_D1][POLY_3MM_ARRAY_002_LENGTH_D2];
	populate2DIntArrayS(array2_3mm, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DIntArrayS(array2_3mm, POLY_3MM_ARRAY_002_LENGTH_D1, POLY_3MM_ARRAY_002_LENGTH_D2);
	// - Log the initial array sum.
	poly_3mm_result.initArray2Sum=sum;
	printf("		--- sum array2=%lu.\n", sum);

	ARRAY_LENGTH_D1 = POLY_3MM_ARRAY_003_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_3MM_ARRAY_003_LENGTH_D2;
	static int array3_3mm[POLY_3MM_ARRAY_003_LENGTH_D1][POLY_3MM_ARRAY_003_LENGTH_D2];
	populate2DIntArrayS(array3_3mm, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DIntArrayS(array3_3mm, POLY_3MM_ARRAY_003_LENGTH_D1, POLY_3MM_ARRAY_003_LENGTH_D2);
	// - Log the initial array sum.
	poly_3mm_result.initArray3Sum=sum;
	printf("		--- sum array3=%lu.\n", sum);

	ARRAY_LENGTH_D1 = POLY_3MM_ARRAY_004_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_3MM_ARRAY_004_LENGTH_D2;
	static int array4_3mm[POLY_3MM_ARRAY_004_LENGTH_D1][POLY_3MM_ARRAY_004_LENGTH_D2];
	populate2DIntArrayS(array4_3mm, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DIntArrayS(array4_3mm, POLY_3MM_ARRAY_004_LENGTH_D1, POLY_3MM_ARRAY_004_LENGTH_D2);
	// - Log the initial array sum.
	poly_3mm_result.initArray4Sum=sum;
	printf("		--- sum array4=%lu.\n", sum);

	ARRAY_LENGTH_D1 = POLY_3MM_ARRAY_005_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_3MM_ARRAY_005_LENGTH_D2;
	static int array5_3mm[POLY_3MM_ARRAY_005_LENGTH_D1][POLY_3MM_ARRAY_005_LENGTH_D2];
	populate2DIntArrayS(array5_3mm, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DIntArrayS(array5_3mm, POLY_3MM_ARRAY_005_LENGTH_D1, POLY_3MM_ARRAY_005_LENGTH_D2);
	// - Log the initial array sum.
	poly_3mm_result.initArray5Sum=sum;
	printf("		--- sum array5=%lu.\n", sum);

	ARRAY_LENGTH_D1 = POLY_3MM_ARRAY_006_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_3MM_ARRAY_006_LENGTH_D2;
	static int array6_3mm[POLY_3MM_ARRAY_006_LENGTH_D1][POLY_3MM_ARRAY_006_LENGTH_D2];
	populate2DIntArrayS(array6_3mm, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DIntArrayS(array6_3mm, POLY_3MM_ARRAY_006_LENGTH_D1, POLY_3MM_ARRAY_006_LENGTH_D2);
	// - Log the initial array sum.
	poly_3mm_result.initArray6Sum=sum;
	printf("		--- sum array6=%lu.\n", sum);

    // E := A*B
    for (int i = 0; i < POLY_3MM_ARRAY_001_LENGTH_D1; i++)
        for (int j = 0; j < POLY_3MM_ARRAY_001_LENGTH_D2; j++)
        {
        	array5_3mm[i][j] = 0.0;
            for (int k = 0; k < POLY_3MM_ARRAY_002_LENGTH_D1; ++k)
            {
            	// array5_3mm[i][j] += array1_3mm[i][k] * array2_3mm[k][j]; // Original
            	array5_3mm[i][j] += (array1_3mm[i][k] * array2_3mm[k][j])/POLY_3MM_ARRAY_002_LENGTH_D1; // Modified
            }
        }
    // F := C*D
    for (int i = 0; i < POLY_3MM_ARRAY_006_LENGTH_D1; i++)
        for (int j = 0; j < POLY_3MM_ARRAY_004_LENGTH_D2; j++)
        {
        	array6_3mm[i][j] = 1.0;
            for (int k = 0; k < POLY_3MM_ARRAY_004_LENGTH_D1; ++k)
            {
            	// array6_3mm[i][j] += array3_3mm[i][k] * array4_3mm[k][j]; // Original
            	array6_3mm[i][j] += (array3_3mm[i][k] * array4_3mm[k][j])/POLY_3MM_ARRAY_002_LENGTH_D2; // Modified
            }
        }
    // Calculate the sum after array modification.
	sum = sum2DIntArrayS(array1_3mm, POLY_3MM_ARRAY_001_LENGTH_D1, POLY_3MM_ARRAY_001_LENGTH_D2);
    // - Log the modified array sum.
    poly_3mm_result.modifiedArray1Sum=sum;
	printf("		--- sum array1=%lu.\n", sum);

	sum = sum2DIntArrayS(array2_3mm, POLY_3MM_ARRAY_002_LENGTH_D1, POLY_3MM_ARRAY_002_LENGTH_D2);
    // - Log the modified array sum.
    poly_3mm_result.modifiedArray2Sum=sum;
	printf("		--- sum array2=%lu.\n", sum);

	sum = sum2DIntArrayS(array3_3mm, POLY_3MM_ARRAY_003_LENGTH_D1, POLY_3MM_ARRAY_003_LENGTH_D2);
    // - Log the modified array sum.
    poly_3mm_result.modifiedArray3Sum=sum;
	printf("		--- sum array3=%lu.\n", sum);

	sum = sum2DIntArrayS(array4_3mm, POLY_3MM_ARRAY_004_LENGTH_D1, POLY_3MM_ARRAY_004_LENGTH_D2);
    // - Log the modified array sum.
    poly_3mm_result.modifiedArray4Sum=sum;
	printf("		--- sum array4=%lu.\n", sum);

	sum = sum2DIntArrayS(array5_3mm, POLY_3MM_ARRAY_005_LENGTH_D1, POLY_3MM_ARRAY_005_LENGTH_D2);
    // - Log the modified array sum.
    poly_3mm_result.modifiedArray5Sum=sum;
	printf("		--- sum array5=%lu.\n", sum);

	sum = sum2DIntArrayS(array6_3mm, POLY_3MM_ARRAY_006_LENGTH_D1, POLY_3MM_ARRAY_006_LENGTH_D2);
    // - Log the modified array sum.
    poly_3mm_result.modifiedArray6Sum=sum;
	printf("		--- sum array6=%lu.\n", sum);
}

// Polybench, 3mm
void computationKernelPolybench3mmFloat()
{
	printf("		--- computationKernelPolybench3mmFloat.\n");
//	_PB_NI = 180. - ARRAY_001_LENGTH, ARRAY_005_LENGTH
//	_PB_NJ = 190. - ARRAY_001_WIDTH, ARRAY_002_WIDTH, ARRAY_003_LENGTH, ARRAY_005_WIDTH, ARRAY_006_LENGTH
//	_PB_NK = 200. - ARRAY_002_LENGTH,
//	_PB_NL = 210. - ARRAY_004_WIDTH, ARRAY_006_WIDTH
//	_PB_NM = 220. - ARRAY_003_WIDTH, ARRAY_004_LENGTH
	// Stack arrays:
	ARRAY_LENGTH_D1 = POLY_3MM_ARRAY_001_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_3MM_ARRAY_001_LENGTH_D2;
	static float array1_3mm[POLY_3MM_ARRAY_001_LENGTH_D1][POLY_3MM_ARRAY_001_LENGTH_D2];
	populate2DFloatArrayS(array1_3mm, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	double sum = sum2DFloatArrayS(array1_3mm, POLY_3MM_ARRAY_001_LENGTH_D1, POLY_3MM_ARRAY_001_LENGTH_D2);
	// - Log the initial array sum.
	poly_3mm_float_result.initArray1Sum=sum;
	printf("		--- sum array1=%f.\n", sum);

	ARRAY_LENGTH_D1 = POLY_3MM_ARRAY_002_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_3MM_ARRAY_002_LENGTH_D2;
	static float array2_3mm[POLY_3MM_ARRAY_002_LENGTH_D1][POLY_3MM_ARRAY_002_LENGTH_D2];
	populate2DFloatArrayS(array2_3mm, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DFloatArrayS(array2_3mm, POLY_3MM_ARRAY_002_LENGTH_D1, POLY_3MM_ARRAY_002_LENGTH_D2);
	// - Log the initial array sum.
	poly_3mm_float_result.initArray2Sum=sum;
	printf("		--- sum array2=%f.\n", sum);

	ARRAY_LENGTH_D1 = POLY_3MM_ARRAY_003_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_3MM_ARRAY_003_LENGTH_D2;
	static float array3_3mm[POLY_3MM_ARRAY_003_LENGTH_D1][POLY_3MM_ARRAY_003_LENGTH_D2];
	populate2DFloatArrayS(array3_3mm, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DFloatArrayS(array3_3mm, POLY_3MM_ARRAY_003_LENGTH_D1, POLY_3MM_ARRAY_003_LENGTH_D2);
	// - Log the initial array sum.
	poly_3mm_float_result.initArray3Sum=sum;
	printf("		--- sum array3=%f.\n", sum);

	ARRAY_LENGTH_D1 = POLY_3MM_ARRAY_004_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_3MM_ARRAY_004_LENGTH_D2;
	static float array4_3mm[POLY_3MM_ARRAY_004_LENGTH_D1][POLY_3MM_ARRAY_004_LENGTH_D2];
	populate2DFloatArrayS(array4_3mm, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DFloatArrayS(array4_3mm, POLY_3MM_ARRAY_004_LENGTH_D1, POLY_3MM_ARRAY_004_LENGTH_D2);
	// - Log the initial array sum.
	poly_3mm_float_result.initArray4Sum=sum;
	printf("		--- sum array4=%f.\n", sum);

	ARRAY_LENGTH_D1 = POLY_3MM_ARRAY_005_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_3MM_ARRAY_005_LENGTH_D2;
	static float array5_3mm[POLY_3MM_ARRAY_005_LENGTH_D1][POLY_3MM_ARRAY_005_LENGTH_D2];
	populate2DFloatArrayS(array5_3mm, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DFloatArrayS(array5_3mm, POLY_3MM_ARRAY_005_LENGTH_D1, POLY_3MM_ARRAY_005_LENGTH_D2);
	// - Log the initial array sum.
	poly_3mm_float_result.initArray5Sum=sum;
	printf("		--- sum array5=%f.\n", sum);

	ARRAY_LENGTH_D1 = POLY_3MM_ARRAY_006_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_3MM_ARRAY_006_LENGTH_D2;
	static float array6_3mm[POLY_3MM_ARRAY_006_LENGTH_D1][POLY_3MM_ARRAY_006_LENGTH_D2];
	populate2DFloatArrayS(array6_3mm, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DFloatArrayS(array6_3mm, POLY_3MM_ARRAY_006_LENGTH_D1, POLY_3MM_ARRAY_006_LENGTH_D2);
	// - Log the initial array sum.
	poly_3mm_float_result.initArray6Sum=sum;
	printf("		--- sum array6=%f.\n", sum);

    // E := A*B
    for (int i = 0; i < POLY_3MM_ARRAY_001_LENGTH_D1; i++)
        for (int j = 0; j < POLY_3MM_ARRAY_001_LENGTH_D2; j++)
        {
        	array5_3mm[i][j] = 0.0;
            for (int k = 0; k < POLY_3MM_ARRAY_002_LENGTH_D1; ++k)
            {
            	// array5_3mm[i][j] += array1_3mm[i][k] * array2_3mm[k][j]; // Original
            	array5_3mm[i][j] += (array1_3mm[i][k] * array2_3mm[k][j])/POLY_3MM_ARRAY_002_LENGTH_D1; // Modified
            }
        }
    // F := C*D
    for (int i = 0; i < POLY_3MM_ARRAY_006_LENGTH_D1; i++)
        for (int j = 0; j < POLY_3MM_ARRAY_004_LENGTH_D2; j++)
        {
        	array6_3mm[i][j] = 1.0;
            for (int k = 0; k < POLY_3MM_ARRAY_004_LENGTH_D1; ++k)
            {
            	// array6_3mm[i][j] += array3_3mm[i][k] * array4_3mm[k][j]; // Original
            	array6_3mm[i][j] += (array3_3mm[i][k] * array4_3mm[k][j])/POLY_3MM_ARRAY_002_LENGTH_D2; // Modified
            }
        }
    // Calculate the sum after array modification.
	sum = sum2DFloatArrayS(array1_3mm, POLY_3MM_ARRAY_001_LENGTH_D1, POLY_3MM_ARRAY_001_LENGTH_D2);
    // - Log the modified array sum.
	poly_3mm_float_result.modifiedArray1Sum=sum;
	printf("		--- sum array1=%f.\n", sum);

	sum = sum2DFloatArrayS(array2_3mm, POLY_3MM_ARRAY_002_LENGTH_D1, POLY_3MM_ARRAY_002_LENGTH_D2);
    // - Log the modified array sum.
	poly_3mm_float_result.modifiedArray2Sum=sum;
	printf("		--- sum array2=%f.\n", sum);

	sum = sum2DFloatArrayS(array3_3mm, POLY_3MM_ARRAY_003_LENGTH_D1, POLY_3MM_ARRAY_003_LENGTH_D2);
    // - Log the modified array sum.
	poly_3mm_float_result.modifiedArray3Sum=sum;
	printf("		--- sum array3=%f.\n", sum);

	sum = sum2DFloatArrayS(array4_3mm, POLY_3MM_ARRAY_004_LENGTH_D1, POLY_3MM_ARRAY_004_LENGTH_D2);
    // - Log the modified array sum.
	poly_3mm_float_result.modifiedArray4Sum=sum;
	printf("		--- sum array4=%f.\n", sum);

	sum = sum2DFloatArrayS(array5_3mm, POLY_3MM_ARRAY_005_LENGTH_D1, POLY_3MM_ARRAY_005_LENGTH_D2);
    // - Log the modified array sum.
	poly_3mm_float_result.modifiedArray5Sum=sum;
	printf("		--- sum array5=%f.\n", sum);

	sum = sum2DFloatArrayS(array6_3mm, POLY_3MM_ARRAY_006_LENGTH_D1, POLY_3MM_ARRAY_006_LENGTH_D2);
    // - Log the modified array sum.
    poly_3mm_float_result.modifiedArray6Sum=sum;
	printf("		--- sum array6=%f.\n", sum);
}

// Polybench, doitgen
void computationKernelPolybenchDoitgen()
{
	printf("		--- computationKernelPolybenchDoitgen.\n");
    // _PB_NR = 150.
	// _PB_NQ = 140.
	// _PB_NP = 1600.748019
	// Stack arrays:
	ARRAY_LENGTH_D1 = POLY_DOITGEN_ARRAY_001_LENGTH_D1;
	static int sum[POLY_DOITGEN_ARRAY_001_LENGTH_D1];
	populate1DIntArrayS(sum, ARRAY_LENGTH_D1);
	//printf("		--- populate1DFloatArrayS populated.\n");

	// Calculate the sum before array modification.
	long sumArray = sum1DIntArrayS(sum, POLY_DOITGEN_ARRAY_001_LENGTH_D1);
	// - Log the initial array sum.
	poly_doitgen_result.initSumSum=sumArray;
	printf("		--- sum sum=%lu.\n", sumArray);

	ARRAY_LENGTH_D1 = POLY_DOITGEN_ARRAY_002_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_DOITGEN_ARRAY_002_LENGTH_D2;
	//printf("		--- ARRAY_LENGTH_D1=%lu, ARRAY_LENGTH_D2=%lu.\n", ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);
	static int C4[POLY_DOITGEN_ARRAY_002_LENGTH_D1][POLY_DOITGEN_ARRAY_002_LENGTH_D2];

	populate2DIntArrayS(C4, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sumArray = sum2DIntArrayS(C4, POLY_DOITGEN_ARRAY_002_LENGTH_D1, POLY_DOITGEN_ARRAY_002_LENGTH_D2);
	// - Log the initial array sum.
	poly_doitgen_result.initC4Sum=sumArray;
	printf("		--- sum C4=%lu.\n", sumArray);

	//printf("		--- populate2DFloatArrayS populated.\n");

	ARRAY_LENGTH_D1 = POLY_DOITGEN_ARRAY_003_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_DOITGEN_ARRAY_003_LENGTH_D2;
	ARRAY_LENGTH_D3 = POLY_DOITGEN_ARRAY_003_LENGTH_D3;
	static int A[POLY_DOITGEN_ARRAY_003_LENGTH_D1][POLY_DOITGEN_ARRAY_003_LENGTH_D2][POLY_DOITGEN_ARRAY_003_LENGTH_D3];
	populate3DIntArrayS(A, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2, ARRAY_LENGTH_D3);

	// Calculate the sum before array modification.
	sumArray = sum3DIntArrayS(A, POLY_DOITGEN_ARRAY_003_LENGTH_D1, POLY_DOITGEN_ARRAY_003_LENGTH_D2, POLY_DOITGEN_ARRAY_003_LENGTH_D3);
	// - Log the initial array sum.
	poly_doitgen_result.initASum=sumArray;
	printf("		--- sum A=%lu.\n", sumArray);

	//printf("		--- populate3DFloatArrayS populated.\n");

    for (int r = 0; r < ARRAY_LENGTH_D1; r++)
        for (int q = 0; q < ARRAY_LENGTH_D2; q++)
        {
            for (int p = 0; p < ARRAY_LENGTH_D3; p++)
            {
                sum[p] = 0.0;
                for (int s = 0; s < ARRAY_LENGTH_D3; s++)
                    sum[p] += A[r][q][s] * C4[s][p];
            }
            for (int p = 0; p < ARRAY_LENGTH_D3; p++)
                A[r][q][p] = sum[p];
        }

    // Calculate the sum after array modification.
    sumArray = sum1DIntArrayS(sum, POLY_DOITGEN_ARRAY_001_LENGTH_D1);
   	printf("		--- sum sum=%lu.\n", sumArray);
    // - Log the modified array sum.
   	poly_doitgen_result.modifiedSumSum=sumArray;

   	sumArray = sum2DIntArrayS(C4, POLY_DOITGEN_ARRAY_002_LENGTH_D1, POLY_DOITGEN_ARRAY_002_LENGTH_D2);
   	printf("		--- sum C4=%lu.\n", sumArray);
    // - Log the modified array sum.
   	poly_doitgen_result.modifiedC4Sum=sumArray;

   	sumArray = sum3DIntArrayS(A, POLY_DOITGEN_ARRAY_003_LENGTH_D1, POLY_DOITGEN_ARRAY_003_LENGTH_D2, POLY_DOITGEN_ARRAY_003_LENGTH_D3);
   	printf("		--- sum A=%lu.\n", sumArray);
    // - Log the modified array sum.
   	poly_doitgen_result.modifiedASum=sumArray;
}

// Polybench, doitgen
void computationKernelPolybenchDoitgenFloat()
{
	printf("		--- computationKernelPolybenchDoitgenFloat.\n");
    // _PB_NR = 150.
	// _PB_NQ = 140.
	// _PB_NP = 1600.748019
	// Stack arrays:
	ARRAY_LENGTH_D1 = POLY_DOITGEN_ARRAY_001_LENGTH_D1;
	static float sum[POLY_DOITGEN_ARRAY_001_LENGTH_D1];
	populate1DFloatArrayS(sum, ARRAY_LENGTH_D1);
	//printf("		--- populate1DFloatArrayS populated.\n");

	// Calculate the sum before array modification.
	double sumArray = sum1DFloatArrayS(sum, POLY_DOITGEN_ARRAY_001_LENGTH_D1);
	// - Log the initial array sum.
	poly_doitgen_float_result.initSumSum=sumArray;
	printf("		--- sum sum=%f.\n", sumArray);

	ARRAY_LENGTH_D1 = POLY_DOITGEN_ARRAY_002_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_DOITGEN_ARRAY_002_LENGTH_D2;
	//printf("		--- ARRAY_LENGTH_D1=%lu, ARRAY_LENGTH_D2=%lu.\n", ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);
	static float C4[POLY_DOITGEN_ARRAY_002_LENGTH_D1][POLY_DOITGEN_ARRAY_002_LENGTH_D2];

	populate2DFloatArrayS(C4, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sumArray = sum2DFloatArrayS(C4, POLY_DOITGEN_ARRAY_002_LENGTH_D1, POLY_DOITGEN_ARRAY_002_LENGTH_D2);
	// - Log the initial array sum.
	poly_doitgen_float_result.initC4Sum=sumArray;
	printf("		--- sum C4=%f.\n", sumArray);

	//printf("		--- populate2DFloatArrayS populated.\n");

	ARRAY_LENGTH_D1 = POLY_DOITGEN_ARRAY_003_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_DOITGEN_ARRAY_003_LENGTH_D2;
	ARRAY_LENGTH_D3 = POLY_DOITGEN_ARRAY_003_LENGTH_D3;
	static float A[POLY_DOITGEN_ARRAY_003_LENGTH_D1][POLY_DOITGEN_ARRAY_003_LENGTH_D2][POLY_DOITGEN_ARRAY_003_LENGTH_D3];
	populate3DFloatArrayS(A, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2, ARRAY_LENGTH_D3);

	// Calculate the sum before array modification.
	sumArray = sum3DFloatArrayS(A, POLY_DOITGEN_ARRAY_003_LENGTH_D1, POLY_DOITGEN_ARRAY_003_LENGTH_D2, POLY_DOITGEN_ARRAY_003_LENGTH_D3);
	// - Log the initial array sum.
	poly_doitgen_float_result.initASum=sumArray;
	printf("		--- sum A=%f.\n", sumArray);

	//printf("		--- populate3DFloatArrayS populated.\n");

    for (int r = 0; r < ARRAY_LENGTH_D1; r++)
        for (int q = 0; q < ARRAY_LENGTH_D2; q++)
        {
            for (int p = 0; p < ARRAY_LENGTH_D3; p++)
            {
                sum[p] = 0.0;
                for (int s = 0; s < ARRAY_LENGTH_D3; s++)
                    sum[p] += A[r][q][s] * C4[s][p];
            }
            for (int p = 0; p < ARRAY_LENGTH_D3; p++)
                A[r][q][p] = sum[p];
        }

    // Calculate the sum after array modification.
    sumArray = sum1DFloatArrayS(sum, POLY_DOITGEN_ARRAY_001_LENGTH_D1);
   	printf("		--- sum sum=%f.\n", sumArray);
    // - Log the modified array sum.
   	poly_doitgen_float_result.modifiedSumSum=sumArray;

   	sumArray = sum2DFloatArrayS(C4, POLY_DOITGEN_ARRAY_002_LENGTH_D1, POLY_DOITGEN_ARRAY_002_LENGTH_D2);
   	printf("		--- sum C4=%f.\n", sumArray);
    // - Log the modified array sum.
   	poly_doitgen_float_result.modifiedC4Sum=sumArray;

   	sumArray = sum3DFloatArrayS(A, POLY_DOITGEN_ARRAY_003_LENGTH_D1, POLY_DOITGEN_ARRAY_003_LENGTH_D2, POLY_DOITGEN_ARRAY_003_LENGTH_D3);
   	printf("		--- sum A=%f.\n", sumArray);
    // - Log the modified array sum.
   	poly_doitgen_float_result.modifiedASum=sumArray;
}

// Polybench, gramschmidt
void computationKernelPolybenchGramschmidt()
{
	printf("		--- computationKernelPolybenchGramschmidt.\n");
	// _PB_N = 1600. ARRAY_001_WIDTH, ARRAY_002_LENGTH, ARRAY_002_WIDTH, ARRAY_003_WIDTH
	// _PB_M = 1400. ARRAY_001_LENGTH, ARRAY_003_LENGTH

	// Stack arrays:
	ARRAY_LENGTH_D1 = POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2;
	static int array1_gramschmidt[POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1][POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2];
	populate2DIntArrayS(array1_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	long sum = sum2DIntArrayS(array1_gramschmidt, POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1, POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2);
	// - Log the initial array sum.
	poly_gramschmidt_result.initArray1Sum=sum;
	printf("		--- sum array1=%lu.\n", sum);

	ARRAY_LENGTH_D1 = POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D2;
	static int array2_gramschmidt[POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D1][POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D2];
	populate2DIntArrayS(array2_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DIntArrayS(array2_gramschmidt, POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D1, POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D2);
	// - Log the initial array sum.
	poly_gramschmidt_result.initArray2Sum=sum;
	printf("		--- sum array2=%lu.\n", sum);

	ARRAY_LENGTH_D1 = POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D2;
	static int array3_gramschmidt[POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D1][POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D2];
	populate2DIntArrayS(array3_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DIntArrayS(array3_gramschmidt, POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D1, POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D2);
	// - Log the initial array sum.
	poly_gramschmidt_result.initArray3Sum=sum;
	printf("		--- sum array3=%lu.\n", sum);


	#ifdef LOG_RESULTS
		// Appending to the end of file.
		#ifdef LOG_RESULTS_BINARY
		logFile = fopen("computationKernelPolybenchGramschmidtArray1_gramschmidtLog.txt", "w+");
		#endif /* MACRO */

		#ifdef LOG_RESULTS_JPAR
		logFile = fopen("computationKernelPolybenchGramschmidtArray1_gramschmidtLog_JPAR.txt", "w+");
		#endif /* MACRO */

	write2DIntToFile(array1_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);
	fclose(logFile);
	#endif /* MACRO */

	#ifdef LOG_RESULTS
		// Appending to the end of file.
		#ifdef LOG_RESULTS_BINARY
		logFile = fopen("computationKernelPolybenchGramschmidtArray2_gramschmidtLog.txt", "w+");
		#endif /* MACRO */

		#ifdef LOG_RESULTS_JPAR
		logFile = fopen("computationKernelPolybenchGramschmidtArray2_gramschmidtLog_JPAR.txt", "w+");
		#endif /* MACRO */

	write2DIntToFile(array2_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);
	fclose(logFile);
	#endif /* MACRO */

	#ifdef LOG_RESULTS
		// Appending to the end of file.
		#ifdef LOG_RESULTS_BINARY
		logFile = fopen("computationKernelPolybenchGramschmidtArray3_gramschmidtLog.txt", "w+");
		#endif /* MACRO */

		#ifdef LOG_RESULTS_JPAR
		logFile = fopen("computationKernelPolybenchGramschmidtArray3_gramschmidtLog_JPAR.txt", "w+");
		#endif /* MACRO */

	write2DIntToFile(array3_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);
	fclose(logFile);
	#endif /* MACRO */

    for (int k = 0; k < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2; k++)
    {
        int nrm = 0.0;
        for (int i = 0; i < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1; i++)
            nrm += array1_gramschmidt[i][k] * array1_gramschmidt[i][k]/( array1_gramschmidt[i][k]+i+1);
        	//array2[k][k] = SQRT_FUN(nrm);
        	if(nrm>0)
        		array2_gramschmidt[k][k] = sqrt(nrm);
        for (int i = 0; i < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1; i++)
        {
        	array3_gramschmidt[i][k] = array1_gramschmidt[i][k] / array2_gramschmidt[k][k]; // Original.
        	array3_gramschmidt[i][k] = (int) array3_gramschmidt[i][k];
        	//array3_gramschmidt[i][k] = (0.01 + array3_gramschmidt[i][k])*0.00001;
        	//array3_gramschmidt[i][k] = array1_gramschmidt[i][k] / (array2_gramschmidt[k][k]+1); // Modified.
        	//printf("array1[%d][%d]=%f.\n", i, k, array1_gramschmidt[i][k]);
        	//printf("array2[%d][%d]=%f.\n", k, k, array2[k][k]);
        	//printf("array3[%d][%d]=%f.\n", i, k, array3[i][k]);
        }
        for (int j = k + 1; j < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2; j++)
        {
        	array2_gramschmidt[k][j] = 0.0;
            for (int i = 0; i < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1; i++)
            {
            	//array2_gramschmidt[k][j] += array3_gramschmidt[i][k] * array1_gramschmidt[i][j]; // Original
            	array2_gramschmidt[k][j] += (array3_gramschmidt[i][k] * array1_gramschmidt[i][j])/POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2; // Modified
            	array2_gramschmidt[k][j] = (int) array2_gramschmidt[k][j];
            }
            for (int i = 0; i < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1; i++)
            {
            	array1_gramschmidt[i][j] = (array1_gramschmidt[i][j] - array3_gramschmidt[i][k] * array2_gramschmidt[k][j]); // Original
            	// -- Modifications --- //
            	array1_gramschmidt[i][j] = (array1_gramschmidt[i][j] - array3_gramschmidt[i][k] * array2_gramschmidt[k][j])/POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1;
            	array1_gramschmidt[i][j] = (int) array1_gramschmidt[i][j];
            	if((array1_gramschmidt[i][j]>1000) || (array1_gramschmidt[i][j] <-1000))
            		array1_gramschmidt[i][j]=123;
            	// -------------------- //
            }

        }
    }

	for(long i=0; i< POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1;i++)
	{
		for(long j=0; j<POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2; j++)
		{
			//printf("array1[%d][%d] = %f.\n",i,j, array1_gramschmidt[i][j]);
		}
	}

	#ifdef LOG_RESULTS
		// Appending to the end of file.
		#ifdef LOG_RESULTS_BINARY
		logFile = fopen("computationKernelPolybenchGramschmidtArray1_gramschmidtModifiedLog.txt", "w+");
		#endif /* MACRO */

		#ifdef LOG_RESULTS_JPAR
		logFile = fopen("computationKernelPolybenchGramschmidtArray1_gramschmidtModifiedLog_JPAR.txt", "w+");
		#endif /* MACRO */

	write2DIntToFile(array1_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);
	fclose(logFile);
	#endif /* MACRO */

	#ifdef LOG_RESULTS
		// Appending to the end of file.
		#ifdef LOG_RESULTS_BINARY
		logFile = fopen("computationKernelPolybenchGramschmidtArray2_gramschmidtModifiedLog.txt", "w+");
		#endif /* MACRO */

		#ifdef LOG_RESULTS_JPAR
		logFile = fopen("computationKernelPolybenchGramschmidtArray2_gramschmidtModifiedLog_JPAR.txt", "w+");
		#endif /* MACRO */

	write2DIntToFile(array2_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);
	fclose(logFile);
	#endif /* MACRO */

	#ifdef LOG_RESULTS
		// Appending to the end of file.
		#ifdef LOG_RESULTS_BINARY
		logFile = fopen("computationKernelPolybenchGramschmidtArray3_gramschmidtModifiedLog.txt", "w+");
		#endif /* MACRO */

		#ifdef LOG_RESULTS_JPAR
		logFile = fopen("computationKernelPolybenchGramschmidtArray3_gramschmidtModifiedLog_JPAR.txt", "w+");
		#endif /* MACRO */

	write2DIntToFile(array3_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);
	fclose(logFile);
	#endif /* MACRO */

    // Calculate the sum after array modification.
    sum = sum2DIntArrayS(array1_gramschmidt, POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1, POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2);
    printf("		--- sum array1=%lu.\n", sum);
    // - Log the modified array sum.
    poly_gramschmidt_result.modifiedArray1Sum=sum;

    sum = sum2DIntArrayS(array2_gramschmidt, POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D1, POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D2);
    printf("		--- sum array2=%lu.\n", sum);
    // - Log the modified array sum.
    poly_gramschmidt_result.modifiedArray2Sum=sum;

   	sum = sum2DIntArrayS(array3_gramschmidt, POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D1, POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D2);
   	printf("		--- sum array3=%lu.\n", sum);
    // - Log the modified array sum.
    poly_gramschmidt_result.modifiedArray3Sum=sum;
}

// Polybench, gramschmidt
void computationKernelPolybenchGramschmidtFloat()
{
	printf("		--- computationKernelPolybenchGramschmidtFloat.\n");
	// _PB_N = 1600. ARRAY_001_WIDTH, ARRAY_002_LENGTH, ARRAY_002_WIDTH, ARRAY_003_WIDTH
	// _PB_M = 1400. ARRAY_001_LENGTH, ARRAY_003_LENGTH

	// Stack arrays:
	ARRAY_LENGTH_D1 = POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2;
	static float array1_gramschmidt[POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1][POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2];
	populate2DFloatArrayS(array1_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	double sum = sum2DFloatArrayS(array1_gramschmidt, POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1, POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2);
	// - Log the initial array sum.
	poly_gramschmidt_float_result.initArray1Sum=sum;
	printf("		--- sum array1=%f.\n", sum);

	ARRAY_LENGTH_D1 = POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D2;
	static float array2_gramschmidt[POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D1][POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D2];
	populate2DFloatArrayS(array2_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DFloatArrayS(array2_gramschmidt, POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D1, POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D2);
	// - Log the initial array sum.
	poly_gramschmidt_float_result.initArray2Sum=sum;
	printf("		--- sum array2=%f.\n", sum);

	ARRAY_LENGTH_D1 = POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D2;
	static float array3_gramschmidt[POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D1][POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D2];
	populate2DFloatArrayS(array3_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DFloatArrayS(array3_gramschmidt, POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D1, POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D2);
	// - Log the initial array sum.
	poly_gramschmidt_float_result.initArray3Sum=sum;
	printf("		--- sum array3=%f.\n", sum);


	#ifdef LOG_RESULTS
		// Appending to the end of file.
		#ifdef LOG_RESULTS_BINARY
		logFile = fopen("computationKernelPolybenchGramschmidtArray1_gramschmidtLog.txt", "w+");
		#endif /* MACRO */

		#ifdef LOG_RESULTS_JPAR
		logFile = fopen("computationKernelPolybenchGramschmidtArray1_gramschmidtLog_JPAR.txt", "w+");
		#endif /* MACRO */

	write2DFloatToFile(array1_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);
	fclose(logFile);
	#endif /* MACRO */

	#ifdef LOG_RESULTS
		// Appending to the end of file.
		#ifdef LOG_RESULTS_BINARY
		logFile = fopen("computationKernelPolybenchGramschmidtArray2_gramschmidtLog.txt", "w+");
		#endif /* MACRO */

		#ifdef LOG_RESULTS_JPAR
		logFile = fopen("computationKernelPolybenchGramschmidtArray2_gramschmidtLog_JPAR.txt", "w+");
		#endif /* MACRO */

	write2DFloatToFile(array2_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);
	fclose(logFile);
	#endif /* MACRO */

	#ifdef LOG_RESULTS
		// Appending to the end of file.
		#ifdef LOG_RESULTS_BINARY
		logFile = fopen("computationKernelPolybenchGramschmidtArray3_gramschmidtLog.txt", "w+");
		#endif /* MACRO */

		#ifdef LOG_RESULTS_JPAR
		logFile = fopen("computationKernelPolybenchGramschmidtArray3_gramschmidtLog_JPAR.txt", "w+");
		#endif /* MACRO */

	write2DFloatToFile(array3_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);
	fclose(logFile);
	#endif /* MACRO */

    for (int k = 0; k < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2; k++)
    {
        int nrm = 0.0;
        for (int i = 0; i < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1; i++)
            nrm += (float) array1_gramschmidt[i][k] * array1_gramschmidt[i][k]/( array1_gramschmidt[i][k]+i+1);
        	//array2[k][k] = SQRT_FUN(nrm);
        	if(nrm>0)
        		array2_gramschmidt[k][k] = sqrt(nrm);
        for (int i = 0; i < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1; i++)
        {
        	array3_gramschmidt[i][k] = (float) array1_gramschmidt[i][k] / array2_gramschmidt[k][k]; // Original.
        	array3_gramschmidt[i][k] = (float) array3_gramschmidt[i][k];
        	//array3_gramschmidt[i][k] = (0.01 + array3_gramschmidt[i][k])*0.00001;
        	//array3_gramschmidt[i][k] = array1_gramschmidt[i][k] / (array2_gramschmidt[k][k]+1); // Modified.
        	//printf("array1[%d][%d]=%f.\n", i, k, array1_gramschmidt[i][k]);
        	//printf("array2[%d][%d]=%f.\n", k, k, array2[k][k]);
        	//printf("array3[%d][%d]=%f.\n", i, k, array3[i][k]);
        }
        for (int j = k + 1; j < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2; j++)
        {
        	array2_gramschmidt[k][j] = 0.0;
            for (int i = 0; i < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1; i++)
            {
            	//array2_gramschmidt[k][j] += array3_gramschmidt[i][k] * array1_gramschmidt[i][j]; // Original
            	array2_gramschmidt[k][j] += (float) (array3_gramschmidt[i][k] * array1_gramschmidt[i][j])/POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2; // Modified
            	array2_gramschmidt[k][j] = (float) array2_gramschmidt[k][j];
            }
            for (int i = 0; i < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1; i++)
            {
            	array1_gramschmidt[i][j] = (array1_gramschmidt[i][j] - array3_gramschmidt[i][k] * array2_gramschmidt[k][j]); // Original
            	// -- Modifications --- //
            	array1_gramschmidt[i][j] = (float) (array1_gramschmidt[i][j] - array3_gramschmidt[i][k] * array2_gramschmidt[k][j])/POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1;
            	array1_gramschmidt[i][j] = (float)  array1_gramschmidt[i][j];
            	if((array1_gramschmidt[i][j]>1000) || (array1_gramschmidt[i][j] <-1000))
            		array1_gramschmidt[i][j]=123;
            	// -------------------- //
            }

        }
    }

	for(long i=0; i< POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1;i++)
	{
		for(long j=0; j<POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2; j++)
		{
			//printf("array1[%d][%d] = %f.\n",i,j, array1_gramschmidt[i][j]);
		}
	}

	#ifdef LOG_RESULTS
		// Appending to the end of file.
		#ifdef LOG_RESULTS_BINARY
		logFile = fopen("computationKernelPolybenchGramschmidtArray1_gramschmidtModifiedLog.txt", "w+");
		#endif /* MACRO */

		#ifdef LOG_RESULTS_JPAR
		logFile = fopen("computationKernelPolybenchGramschmidtArray1_gramschmidtModifiedLog_JPAR.txt", "w+");
		#endif /* MACRO */

	write2DFloatToFile(array1_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);
	fclose(logFile);
	#endif /* MACRO */

	#ifdef LOG_RESULTS
		// Appending to the end of file.
		#ifdef LOG_RESULTS_BINARY
		logFile = fopen("computationKernelPolybenchGramschmidtArray2_gramschmidtModifiedLog.txt", "w+");
		#endif /* MACRO */

		#ifdef LOG_RESULTS_JPAR
		logFile = fopen("computationKernelPolybenchGramschmidtArray2_gramschmidtModifiedLog_JPAR.txt", "w+");
		#endif /* MACRO */

	write2DFloatToFile(array2_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);
	fclose(logFile);
	#endif /* MACRO */

	#ifdef LOG_RESULTS
		// Appending to the end of file.
		#ifdef LOG_RESULTS_BINARY
		logFile = fopen("computationKernelPolybenchGramschmidtArray3_gramschmidtModifiedLog.txt", "w+");
		#endif /* MACRO */

		#ifdef LOG_RESULTS_JPAR
		logFile = fopen("computationKernelPolybenchGramschmidtArray3_gramschmidtModifiedLog_JPAR.txt", "w+");
		#endif /* MACRO */

	write2DFloatToFile(array3_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);
	fclose(logFile);
	#endif /* MACRO */

    // Calculate the sum after array modification.
    sum = sum2DFloatArrayS(array1_gramschmidt, POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1, POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2);
    printf("		--- sum array1=%f.\n", sum);
    // - Log the modified array sum.
    poly_gramschmidt_float_result.modifiedArray1Sum=sum;

    sum = sum2DFloatArrayS(array2_gramschmidt, POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D1, POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D2);
    printf("		--- sum array2=%f.\n", sum);
    // - Log the modified array sum.
    poly_gramschmidt_float_result.modifiedArray2Sum=sum;

   	sum = sum2DFloatArrayS(array3_gramschmidt, POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D1, POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D2);
   	printf("		--- sum array3=%f.\n", sum);
    // - Log the modified array sum.
   	poly_gramschmidt_float_result.modifiedArray3Sum=sum;
}

// Polybench, gramschmidt
// - Dynamically allocated arrays
void computationKernelPolybenchGramschmidtD()
{
	// _PB_N = 1600. ARRAY_001_WIDTH, ARRAY_002_LENGTH, ARRAY_002_WIDTH, ARRAY_003_WIDTH
	//// _PB_M = 1400. ARRAY_001_LENGTH, ARRAY_003_LENGTH
	printf("		--- computationKernelPolybenchGramschmidtD.\n");
	// Stack arrays:
	ARRAY_LENGTH_D1 = POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2;

	int** array1_gramschmidt;
	allocate2DIntArrayD(&array1_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	//printf("		--- array1_gramschmidt allocated.\n");

	populate2DIntArrayD(&array1_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	//printf("		--- array1_gramschmidt populated.\n");

	// Calculate the sum before array modification.
	long sum = sum2DIntArrayD(array1_gramschmidt, POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1, POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2);
	// - Log the initial array sum.
	poly_gramschmidt_d_result.initArray1Sum=sum;
	printf("		--- sum array1=%lu.\n", sum);

	ARRAY_LENGTH_D1 = POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D2;
	int** array2_gramschmidt;
	allocate2DIntArrayD(&array2_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	populate2DIntArrayD(&array2_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DIntArrayD(array2_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);
	// - Log the initial array sum.
	poly_gramschmidt_d_result.initArray2Sum=sum;
	printf("		--- sum array2=%lu.\n", sum);

	ARRAY_LENGTH_D1 = POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D2;
	int** array3_gramschmidt;
	allocate2DIntArrayD(&array3_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	populate2DIntArrayD(&array3_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DIntArrayD(array3_gramschmidt, POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D1, POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D2);
	// - Log the initial array sum.
	poly_gramschmidt_d_result.initArray3Sum=sum;
	printf("		--- sum array3=%lu.\n", sum);

    for (int k = 0; k < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2; k++)
    {
        int nrm = 0.0;
        for (int i = 0; i < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1; i++)
            nrm += array1_gramschmidt[i][k] * array1_gramschmidt[i][k]/( array1_gramschmidt[i][k]+i+1);
        	//array2[k][k] = SQRT_FUN(nrm);
        	if(nrm>0)
        		array2_gramschmidt[k][k] = sqrt(nrm);
        for (int i = 0; i < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1; i++)
        {
        	array3_gramschmidt[i][k] = array1_gramschmidt[i][k] / array2_gramschmidt[k][k]; // Original.
        	//array3_gramschmidt[i][k] = array1_gramschmidt[i][k] / (array2_gramschmidt[k][k]+1); // Modified.
        	//printf("array1[%d][%d]=%f.\n", i, k, array1_gramschmidt[i][k]);
        	//printf("array2[%d][%d]=%f.\n", k, k, array2[k][k]);
        	//printf("array3[%d][%d]=%f.\n", i, k, array3[i][k]);
        }
        for (int j = k + 1; j < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2; j++)
        {
        	array2_gramschmidt[k][j] = 0.0;
            for (int i = 0; i < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1; i++)
            {
            	//array2_gramschmidt[k][j] += array3_gramschmidt[i][k] * array1_gramschmidt[i][j]; // Original
            	array2_gramschmidt[k][j] += (array3_gramschmidt[i][k] * array1_gramschmidt[i][j])/POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2; // Modified
            }
            for (int i = 0; i < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1; i++)
            {
            	array1_gramschmidt[i][j] = (array1_gramschmidt[i][j] - array3_gramschmidt[i][k] * array2_gramschmidt[k][j]); // Original
            	// -- Modifications --- //
            	array1_gramschmidt[i][j] = 1+(array1_gramschmidt[i][j] - array3_gramschmidt[i][k] * array2_gramschmidt[k][j])/POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1;

            	if((array1_gramschmidt[i][j]>1000) || (array1_gramschmidt[i][j] <-1000))
            		array1_gramschmidt[i][j]=123;
            	// -------------------- //
            }

        }
    }

	for(long i=0; i< POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1;i++)
	{
		for(long j=0; j<POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2; j++)
		{
			//printf("array1[%d][%d] = %f.\n",i,j, array1_gramschmidt[i][j]);
		}
	}

    // Calculate the sum after array modification.
    sum = sum2DIntArrayD(array1_gramschmidt, POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1, POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2);
    printf("		--- sum array1=%lu.\n", sum);
    // - Log the modified array sum.
    poly_gramschmidt_d_result.modifiedArray1Sum=sum;

    sum = sum2DIntArrayD(array2_gramschmidt, POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D1, POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D2);
    printf("		--- sum array2=%lu.\n", sum);
    poly_gramschmidt_d_result.modifiedArray2Sum=sum;

   	sum = sum2DIntArrayD(array3_gramschmidt, POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D1, POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D2);
   	printf("		--- sum array3=%lu.\n", sum);
   	poly_gramschmidt_d_result.modifiedArray3Sum=sum;

   	// Free the memory, avoid memory leak.
	for(int i=0; i<POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1; i++)
		free(array1_gramschmidt[i]);
	for(int i=0; i<POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D1; i++)
			free(array2_gramschmidt[i]);
	for(int i=0; i<POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D1; i++)
			free(array3_gramschmidt[i]);
}

// Polybench, gramschmidt
// - Dynamically allocated arrays
void computationKernelPolybenchGramschmidtDFloat()
{
	// _PB_N = 1600. ARRAY_001_WIDTH, ARRAY_002_LENGTH, ARRAY_002_WIDTH, ARRAY_003_WIDTH
	//// _PB_M = 1400. ARRAY_001_LENGTH, ARRAY_003_LENGTH
	printf("		--- computationKernelPolybenchGramschmidtDFloat.\n");
	// Stack arrays:
	ARRAY_LENGTH_D1 = POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2;

	float** array1_gramschmidt;
	allocate2DFloatArrayD(&array1_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	//printf("		--- array1_gramschmidt allocated.\n");

	populate2DFloatArrayD(&array1_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	//printf("		--- array1_gramschmidt populated.\n");

	// Calculate the sum before array modification.
	double sum = sum2DFloatArrayD(array1_gramschmidt, POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1, POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2);
	// - Log the initial array sum.
	poly_gramschmidt_d_float_result.initArray1Sum=sum;
	printf("		--- sum array1=%f.\n", sum);

	ARRAY_LENGTH_D1 = POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D2;
	float** array2_gramschmidt;
	allocate2DFloatArrayD(&array2_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	populate2DFloatArrayD(&array2_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DFloatArrayD(array2_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);
	// - Log the initial array sum.
	poly_gramschmidt_d_float_result.initArray2Sum=sum;
	printf("		--- sum array2=%f.\n", sum);

	ARRAY_LENGTH_D1 = POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D2;
	float** array3_gramschmidt;
	allocate2DFloatArrayD(&array3_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	populate2DFloatArrayD(&array3_gramschmidt, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DFloatArrayD(array3_gramschmidt, POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D1, POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D2);
	// - Log the initial array sum.
	poly_gramschmidt_d_float_result.initArray3Sum=sum;
	printf("		--- sum array3=%f.\n", sum);

    for (int k = 0; k < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2; k++)
    {
        int nrm = 0.0;
        for (int i = 0; i < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1; i++)
            nrm += array1_gramschmidt[i][k] * array1_gramschmidt[i][k]/( array1_gramschmidt[i][k]+i+1);
        	//array2[k][k] = SQRT_FUN(nrm);
        	if(nrm>0)
        		array2_gramschmidt[k][k] = sqrt(nrm);
        for (int i = 0; i < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1; i++)
        {
        	array3_gramschmidt[i][k] = array1_gramschmidt[i][k] / array2_gramschmidt[k][k]; // Original.
        	//array3_gramschmidt[i][k] = array1_gramschmidt[i][k] / (array2_gramschmidt[k][k]+1); // Modified.
        	//printf("array1[%d][%d]=%f.\n", i, k, array1_gramschmidt[i][k]);
        	//printf("array2[%d][%d]=%f.\n", k, k, array2[k][k]);
        	//printf("array3[%d][%d]=%f.\n", i, k, array3[i][k]);
        }
        for (int j = k + 1; j < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2; j++)
        {
        	array2_gramschmidt[k][j] = 0.0;
            for (int i = 0; i < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1; i++)
            {
            	//array2_gramschmidt[k][j] += array3_gramschmidt[i][k] * array1_gramschmidt[i][j]; // Original
            	array2_gramschmidt[k][j] += (array3_gramschmidt[i][k] * array1_gramschmidt[i][j])/POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2; // Modified
            }
            for (int i = 0; i < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1; i++)
            {
            	array1_gramschmidt[i][j] = (array1_gramschmidt[i][j] - array3_gramschmidt[i][k] * array2_gramschmidt[k][j]); // Original
            	// -- Modifications --- //
            	array1_gramschmidt[i][j] = 1+(array1_gramschmidt[i][j] - array3_gramschmidt[i][k] * array2_gramschmidt[k][j])/POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1;

            	if((array1_gramschmidt[i][j]>1000) || (array1_gramschmidt[i][j] <-1000))
            		array1_gramschmidt[i][j]=123;
            	// -------------------- //
            }

        }
    }

	for(long i=0; i< POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1;i++)
	{
		for(long j=0; j<POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2; j++)
		{
			//printf("array1[%d][%d] = %f.\n",i,j, array1_gramschmidt[i][j]);
		}
	}

    // Calculate the sum after array modification.
    sum = sum2DFloatArrayD(array1_gramschmidt, POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1, POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2);
    printf("		--- sum array1=%f.\n", sum);
    // - Log the modified array sum.
    poly_gramschmidt_d_float_result.modifiedArray1Sum=sum;

    sum = sum2DFloatArrayD(array2_gramschmidt, POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D1, POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D2);
    printf("		--- sum array2=%f.\n", sum);
    poly_gramschmidt_d_float_result.modifiedArray2Sum=sum;

   	sum = sum2DFloatArrayD(array3_gramschmidt, POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D1, POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D2);
   	printf("		--- sum array3=%f.\n", sum);
   	poly_gramschmidt_d_float_result.modifiedArray3Sum=sum;

   	// Free the memory, avoid memory leak.
	for(int i=0; i<POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1; i++)
		free(array1_gramschmidt[i]);
	for(int i=0; i<POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D1; i++)
			free(array2_gramschmidt[i]);
	for(int i=0; i<POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D1; i++)
			free(array3_gramschmidt[i]);
}


/**
 * Allocate and populate arrays.
 * @author: Jahic
 */
// Polybench, tislov
void computationKernelPolybenchTislov()
{
	printf("		--- computationKernelPolybenchTislov.\n");
	// n=150000
	// Stack arrays:
	ARRAY_LENGTH_D1 = POLY_TISLOV_ARRAY_001_LENGTH_D1;
	static int b_tislov[POLY_TISLOV_ARRAY_001_LENGTH_D1];
	populate1DIntArrayS(b_tislov, ARRAY_LENGTH_D1);

	// Calculate the sum before array modification.
	long sum = sum1DIntArrayS(b_tislov, POLY_TISLOV_ARRAY_001_LENGTH_D1);
	// - Log the initial array sum.
	poly_tislov_result.initBSum=sum;
	printf("		--- sum sum=%lu.\n", sum);

	ARRAY_LENGTH_D1 = POLY_TISLOV_ARRAY_002_LENGTH_D1;
	static int x_tislov[POLY_TISLOV_ARRAY_002_LENGTH_D1];
	populate1DIntArrayS(x_tislov, ARRAY_LENGTH_D1);

	// Calculate the sum before array modification.
	sum = sum1DIntArrayS(x_tislov, POLY_TISLOV_ARRAY_002_LENGTH_D1);
	// - Log the initial array sum.
	poly_tislov_result.initXSum=sum;
	printf("		--- sum x_tislov=%lu.\n", sum);

	ARRAY_LENGTH_D1 = POLY_TISLOV_ARRAY_003_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_TISLOV_ARRAY_003_LENGTH_D2;
	static int L_tislov[POLY_TISLOV_ARRAY_003_LENGTH_D1][POLY_TISLOV_ARRAY_003_LENGTH_D2];
	populate2DIntArrayS(L_tislov, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DIntArrayS(L_tislov, POLY_TISLOV_ARRAY_003_LENGTH_D1, POLY_TISLOV_ARRAY_003_LENGTH_D2);
	// - Log the initial array sum.
	poly_tislov_result.initLSum=sum;
	printf("		--- sum L_tislov=%lu.\n", sum);

    for (int i = 0; i < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1; i++)
    {
    	//x_tislov[i] = - 999;
    	x_tislov[i] = - 9;
    	b_tislov[i] =  i ;
        for (int j = 0; j <= i; j++)
        	L_tislov[i][j] = (int) (i+POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1-j+1)*2/POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1;
    }

    // Calculate the sum after array modification.
    sum = sum1DIntArrayS(b_tislov, POLY_TISLOV_ARRAY_001_LENGTH_D1);
    printf("		--- sum b_tislov=%lu.\n", sum);
    // - Log the modified array sum.
    poly_tislov_result.modifiedBSum=sum;

	sum = sum1DIntArrayS(x_tislov, POLY_TISLOV_ARRAY_002_LENGTH_D1);
	printf("		--- sum x_tislov=%lu.\n", sum);
    // - Log the modified array sum.
	poly_tislov_result.modifiedXSum=sum;

   	sum = sum2DIntArrayS(L_tislov, POLY_TISLOV_ARRAY_003_LENGTH_D1, POLY_TISLOV_ARRAY_003_LENGTH_D2);
   	printf("		--- sum L_tislov=%lu.\n", sum);
    // - Log the modified array sum.
   	poly_tislov_result.modifiedLSum=sum;
}

// Polybench, tislov
void computationKernelPolybenchTislovFloat()
{
	printf("		--- computationKernelPolybenchTislovFloat.\n");
	// n=150000
	// Stack arrays:
	ARRAY_LENGTH_D1 = POLY_TISLOV_ARRAY_001_LENGTH_D1;
	static float b_tislov[POLY_TISLOV_ARRAY_001_LENGTH_D1];
	populate1DFloatArrayS(b_tislov, ARRAY_LENGTH_D1);

	// Calculate the sum before array modification.
	double sum = sum1DFloatArrayS(b_tislov, POLY_TISLOV_ARRAY_001_LENGTH_D1);
	// - Log the initial array sum.
	poly_tislov_float_result.initBSum=sum;
	printf("		--- sum sum=%f.\n", sum);

	ARRAY_LENGTH_D1 = POLY_TISLOV_ARRAY_002_LENGTH_D1;
	static float x_tislov[POLY_TISLOV_ARRAY_002_LENGTH_D1];
	populate1DFloatArrayS(x_tislov, ARRAY_LENGTH_D1);

	// Calculate the sum before array modification.
	sum = sum1DFloatArrayS(x_tislov, POLY_TISLOV_ARRAY_002_LENGTH_D1);
	// - Log the initial array sum.
	poly_tislov_float_result.initXSum=sum;
	printf("		--- sum x_tislov=%f.\n", sum);

	ARRAY_LENGTH_D1 = POLY_TISLOV_ARRAY_003_LENGTH_D1;
	ARRAY_LENGTH_D2 = POLY_TISLOV_ARRAY_003_LENGTH_D2;
	static float L_tislov[POLY_TISLOV_ARRAY_003_LENGTH_D1][POLY_TISLOV_ARRAY_003_LENGTH_D2];
	populate2DFloatArrayS(L_tislov, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DFloatArrayS(L_tislov, POLY_TISLOV_ARRAY_003_LENGTH_D1, POLY_TISLOV_ARRAY_003_LENGTH_D2);
	// - Log the initial array sum.
	poly_tislov_float_result.initLSum=sum;
	printf("		--- sum L_tislov=%f.\n", sum);

    for (int i = 0; i < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1; i++)
    {
    	x_tislov[i] = - 999;
    	b_tislov[i] =  i ;
        for (int j = 0; j <= i; j++)
        	L_tislov[i][j] = (float) (i+POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1-j+1)*2/POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1;
    }

    // Calculate the sum after array modification.
    sum = sum1DFloatArrayS(b_tislov, POLY_TISLOV_ARRAY_001_LENGTH_D1);
    printf("		--- sum b_tislov=%f.\n", sum);
    // - Log the modified array sum.
    poly_tislov_float_result.modifiedBSum=sum;

	sum = sum1DFloatArrayS(x_tislov, POLY_TISLOV_ARRAY_002_LENGTH_D1);
	printf("		--- sum x_tislov=%f.\n", sum);
    // - Log the modified array sum.
	poly_tislov_float_result.modifiedXSum=sum;

   	sum = sum2DFloatArrayS(L_tislov, POLY_TISLOV_ARRAY_003_LENGTH_D1, POLY_TISLOV_ARRAY_003_LENGTH_D2);
   	printf("		--- sum L_tislov=%f.\n", sum);
    // - Log the modified array sum.
   	poly_tislov_float_result.modifiedLSum=sum;
}

// Generic 1D loop, 2 arrays.
void computationKernelGenericLoop1D2A()
{
	printf("		--- computationKernelGenericLoop1D2A.\n");
	// Stack arrays:
	ARRAY_LENGTH_D1 = GENERIC_ARRAY_001_D1;
	static int array1_generic[GENERIC_ARRAY_001_D1];
	populate1DIntArrayS(array1_generic, ARRAY_LENGTH_D1);
	//printf("		--- populate1DFloatArrayS populated.\n");

	// Calculate the sum before array modification.
	long sumArray = sum1DIntArrayS(array1_generic, GENERIC_ARRAY_001_D1);
	// - Log the initial array sum.
	genericLoop1D2A_result.initArray1Sum=sumArray;
	printf("		--- sum array1_generic=%lu.\n", sumArray);

	// Stack arrays:
	ARRAY_LENGTH_D1 = GENERIC_ARRAY_001_D1;
	static int array2_generic[GENERIC_ARRAY_001_D1];
	populate1DIntArrayS(array2_generic, ARRAY_LENGTH_D1);
	//printf("		--- populate1DFloatArrayS populated.\n");

	// Calculate the sum before array modification.
	sumArray = sum1DIntArrayS(array2_generic, GENERIC_ARRAY_001_D1);
	// - Log the initial array sum.
	genericLoop1D2A_result.initArray2Sum=sumArray;
	printf("		--- sum array2_generic=%lu.\n", sumArray);

	for (int i = 0; i < ARRAY_LENGTH_D1; i++)
	{
		array2_generic[i] += array2_generic[i] * array2_generic[i];
		int tmp = ARRAY_LENGTH_D1%(i+1);
		array1_generic[i] = (1+(array2_generic[i] * (tmp)))*(i+1)/(ARRAY_LENGTH_D1-i);
	}

	//printf("		--- computationKernelGenericLoop1D2A computing complete.\n");

	// Calculate the sum after array modification.
	sumArray = sum1DIntArrayS(array1_generic, GENERIC_ARRAY_001_D1);
	printf("		--- sum array1_generic=%lu.\n", sumArray);
    // - Log the modified array sum.
	genericLoop1D2A_result.modifiedArray1Sum=sumArray;

	sumArray = sum1DIntArrayS(array2_generic, GENERIC_ARRAY_001_D1);
	printf("		--- sum array2_generic=%lu.\n", sumArray);
    // - Log the modified array sum.
	genericLoop1D2A_result.modifiedArray2Sum=sumArray;
}

// Generic 1D loop, 2 arrays.
void computationKernelGenericLoop1D2AFloat()
{
	printf("		--- computationKernelGenericLoop1D2AFloat.\n");
	// Stack arrays:
	ARRAY_LENGTH_D1 = GENERIC_ARRAY_001_D1;
	static float array1_generic[GENERIC_ARRAY_001_D1];
	populate1DFloatArrayS(array1_generic, ARRAY_LENGTH_D1);
	//printf("		--- populate1DFloatArrayS populated.\n");

	// Calculate the sum before array modification.
	double sumArray = sum1DFloatArrayS(array1_generic, GENERIC_ARRAY_001_D1);
	// - Log the initial array sum.
	genericLoop1D2A_float_result.initArray1Sum=sumArray;
	printf("		--- sum array1_generic=%f.\n", sumArray);

	// Stack arrays:
	ARRAY_LENGTH_D1 = GENERIC_ARRAY_001_D1;
	static float array2_generic[GENERIC_ARRAY_001_D1];
	populate1DFloatArrayS(array2_generic, ARRAY_LENGTH_D1);
	//printf("		--- populate1DFloatArrayS populated.\n");

	// Calculate the sum before array modification.
	sumArray = sum1DFloatArrayS(array2_generic, GENERIC_ARRAY_001_D1);
	// - Log the initial array sum.
	genericLoop1D2A_float_result.initArray2Sum=sumArray;
	printf("		--- sum array2_generic=%f.\n", sumArray);

	for (int i = 0; i < ARRAY_LENGTH_D1; i++)
	{
		array2_generic[i] += array2_generic[i] * array2_generic[i];
		int tmp = ARRAY_LENGTH_D1%(i+1);
		array1_generic[i] = (float) (1+(array2_generic[i] * (tmp)))*(i+1)/(ARRAY_LENGTH_D1-i);
	}

	//printf("		--- computationKernelGenericLoop1D2A computing complete.\n");

	// Calculate the sum after array modification.
	sumArray = sum1DFloatArrayS(array1_generic, GENERIC_ARRAY_001_D1);
	printf("		--- sum array1_generic=%f.\n", sumArray);
    // - Log the modified array sum.
	genericLoop1D2A_float_result.modifiedArray1Sum=sumArray;

	sumArray = sum1DFloatArrayS(array2_generic, GENERIC_ARRAY_001_D1);
	printf("		--- sum array2_generic=%f.\n", sumArray);
    // - Log the modified array sum.
	genericLoop1D2A_float_result.modifiedArray2Sum=sumArray;
}

// Generic 1D loop, 2 arrays.
// - Dynamically allocated arrays
void computationKernelGenericLoop1D2AD()
{
	printf("		--- computationKernelGenericLoop1D2AD.\n");
	// Dynamic arrays:
	ARRAY_LENGTH_D1 = GENERIC_ARRAY_001_D1;
	int* array1_generic;
	allocate1DIntArrayD(&array1_generic, ARRAY_LENGTH_D1);

	populate1DIntArrayD(&array1_generic, ARRAY_LENGTH_D1);
	//printf("		--- populate1DFloatArrayS populated.\n");

	// Calculate the sum before array modification.
	long sumArray = sum1DIntArrayD(array1_generic, ARRAY_LENGTH_D1);
	// - Log the initial array sum.
	genericLoop1D2AD_result.initArray1Sum=sumArray;
	printf("		--- sum sum=%lu.\n", sumArray);

	// Stack arrays:
	int* array2_generic;
	allocate1DIntArrayD(&array2_generic, ARRAY_LENGTH_D1);

	populate1DIntArrayD(&array2_generic, ARRAY_LENGTH_D1);
	//printf("		--- populate1DFloatArrayS populated.\n");

	// Calculate the sum before array modification.
	sumArray = sum1DIntArrayD(array2_generic, ARRAY_LENGTH_D1);
	// - Log the initial array sum.
	genericLoop1D2AD_result.initArray2Sum=sumArray;
	printf("		--- sum sum=%lu.\n", sumArray);

	//printf("		--- populate3DFloatArrayS populated.\n");

	for (int i = 0; i < ARRAY_LENGTH_D1; i++)
	{
		array2_generic[i] += array2_generic[i] * array2_generic[i];
		int tmp = ARRAY_LENGTH_D1%(i+1);
		array1_generic[i] = (1+(array2_generic[i] * (tmp)))*(i+1)/(ARRAY_LENGTH_D1-i);
	}

	// Calculate the sum after array modification.
	sumArray = sum1DIntArrayD(array1_generic, GENERIC_ARRAY_001_D1);
	printf("		--- sum array1_generic=%lu.\n", sumArray);
    // - Log the modified array sum.
	genericLoop1D2AD_result.modifiedArray1Sum=sumArray;

	sumArray = sum1DIntArrayD(array2_generic, GENERIC_ARRAY_001_D1);
	printf("		--- sum array2_generic=%lu.\n", sumArray);
    // - Log the modified array sum.
	genericLoop1D2AD_result.modifiedArray2Sum=sumArray;
}

// Generic 1D loop, 2 arrays.
// - Dynamically allocated arrays
void computationKernelGenericLoop1D2ADFloat()
{
	printf("		--- computationKernelGenericLoop1D2ADFloat.\n");
	// Dynamic arrays:
	ARRAY_LENGTH_D1 = GENERIC_ARRAY_001_D1;
	float* array1_generic;
	allocate1DFloatArrayD(&array1_generic, ARRAY_LENGTH_D1);

	populate1DFloatArrayD(&array1_generic, ARRAY_LENGTH_D1);
	//printf("		--- populate1DFloatArrayS populated.\n");

	// Calculate the sum before array modification.
	double sumArray = sum1DFloatArrayD(array1_generic, ARRAY_LENGTH_D1);
	// - Log the initial array sum.
	genericLoop1D2AD_float_result.initArray1Sum=sumArray;
	printf("		--- sum sum=%f.\n", sumArray);

	// Stack arrays:
	float* array2_generic;
	allocate1DFloatArrayD(&array2_generic, ARRAY_LENGTH_D1);

	populate1DFloatArrayD(&array2_generic, ARRAY_LENGTH_D1);
	//printf("		--- populate1DFloatArrayS populated.\n");

	// Calculate the sum before array modification.
	sumArray = sum1DFloatArrayD(array2_generic, ARRAY_LENGTH_D1);
	// - Log the initial array sum.
	genericLoop1D2AD_float_result.initArray2Sum=sumArray;
	printf("		--- sum sum=%f.\n", sumArray);

	//printf("		--- populate3DFloatArrayS populated.\n");

	for (int i = 0; i < ARRAY_LENGTH_D1; i++)
	{
		array2_generic[i] += array2_generic[i] * array2_generic[i];
		int tmp = ARRAY_LENGTH_D1%(i+1);
		array1_generic[i] = (float) (1+(array2_generic[i] * (tmp)))*(i+1)/(ARRAY_LENGTH_D1-i);
	}

	// Calculate the sum after array modification.
	sumArray = sum1DFloatArrayD(array1_generic, GENERIC_ARRAY_001_D1);
	printf("		--- sum array1_generic=%f.\n", sumArray);
    // - Log the modified array sum.
	genericLoop1D2AD_float_result.modifiedArray1Sum=sumArray;

	sumArray = sum1DFloatArrayD(array2_generic, GENERIC_ARRAY_001_D1);
	printf("		--- sum array2_generic=%f.\n", sumArray);
    // - Log the modified array sum.
	genericLoop1D2AD_float_result.modifiedArray2Sum=sumArray;
}

// Generic 2D loop, 2 arrays.
void computationKernelGenericLoop2D2A()
{
	printf("		--- computationKernelGenericLoop2D2A.\n");
	// Stack arrays:
	ARRAY_LENGTH_D1 = GENERIC_ARRAY_001_D1;
	ARRAY_LENGTH_D2 = GENERIC_ARRAY_001_D2;
	static int array1_generic[GENERIC_ARRAY_001_D1][GENERIC_ARRAY_001_D2];
	populate2DIntArrayS(array1_generic, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	long sum = sum2DIntArrayS(array1_generic, GENERIC_ARRAY_001_D1, GENERIC_ARRAY_001_D2);
	// - Log the initial array sum.
	genericLoop2D2A_result.initArray1Sum=sum;
	printf("		--- sum array1=%lu.\n", sum);

	ARRAY_LENGTH_D1 = GENERIC_ARRAY_002_D1;
	ARRAY_LENGTH_D2 = GENERIC_ARRAY_002_D2;
	static int array2_generic[GENERIC_ARRAY_002_D1][GENERIC_ARRAY_002_D2];
	populate2DIntArrayS(array2_generic, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DIntArrayS(array2_generic, GENERIC_ARRAY_002_D1, GENERIC_ARRAY_002_D2);
	// - Log the initial array sum.
	genericLoop2D2A_result.initArray2Sum=sum;
	printf("		--- sum array2=%lu.\n", sum);

	for (int i = 0; i < GENERIC_ARRAY_001_D1; i++)
	{
		for (int j = 0; j < GENERIC_ARRAY_001_D2; j++)
		{
			array1_generic[i][j] += array1_generic[i][j] * array1_generic[i][j];

			for (int k = 1+i; k < GENERIC_ARRAY_002_D1; k++)
			{
				for (int l = 0; l < GENERIC_ARRAY_001_D2; l++)
				{
					int tmp1 = GENERIC_ARRAY_001_D2%(l+1);
					int tmp2 = GENERIC_ARRAY_001_D2%(k+1);
					array2_generic[k][l] += (array2_generic[i][j] + tmp1 +1)/(array2_generic[i][j] + tmp2 +1) ;
				}
			}
		}
	}

	// Calculate the sum after array modification.
	sum = sum2DIntArrayS(array1_generic, GENERIC_ARRAY_001_D1, GENERIC_ARRAY_001_D2);
	printf("		--- sum array1_generic=%lu.\n", sum);
    // - Log the modified array sum.
	genericLoop2D2A_result.modifiedArray1Sum=sum;

	sum = sum2DIntArrayS(array2_generic, GENERIC_ARRAY_002_D1, GENERIC_ARRAY_002_D2);
	printf("		--- sum array2_generic=%lu.\n", sum);
    // - Log the modified array sum.
	genericLoop2D2A_result.modifiedArray2Sum=sum;
}

// Generic 2D loop, 2 arrays.
void computationKernelGenericLoop2D2AFloat()
{
	printf("		--- computationKernelGenericLoop2D2AFloat.\n");
	// Stack arrays:
	ARRAY_LENGTH_D1 = GENERIC_ARRAY_001_D1;
	ARRAY_LENGTH_D2 = GENERIC_ARRAY_001_D2;
	static float array1_generic[GENERIC_ARRAY_001_D1][GENERIC_ARRAY_001_D2];
	populate2DFloatArrayS(array1_generic, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	double sum = sum2DFloatArrayS(array1_generic, GENERIC_ARRAY_001_D1, GENERIC_ARRAY_001_D2);
	// - Log the initial array sum.
	genericLoop2D2A_float_result.initArray1Sum=sum;
	printf("		--- sum array1=%f.\n", sum);

	ARRAY_LENGTH_D1 = GENERIC_ARRAY_002_D1;
	ARRAY_LENGTH_D2 = GENERIC_ARRAY_002_D2;
	static float array2_generic[GENERIC_ARRAY_002_D1][GENERIC_ARRAY_002_D2];
	populate2DFloatArrayS(array2_generic, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DFloatArrayS(array2_generic, GENERIC_ARRAY_002_D1, GENERIC_ARRAY_002_D2);
	// - Log the initial array sum.
	genericLoop2D2A_float_result.initArray2Sum=sum;
	printf("		--- sum array2=%f.\n", sum);

	for (int i = 0; i < GENERIC_ARRAY_001_D1; i++)
	{
		for (int j = 0; j < GENERIC_ARRAY_001_D2; j++)
		{
			array1_generic[i][j] += array1_generic[i][j] * array1_generic[i][j];

			for (int k = 1+i; k < GENERIC_ARRAY_002_D1; k++)
			{
				for (int l = 0; l < GENERIC_ARRAY_001_D2; l++)
				{
					int tmp1 = GENERIC_ARRAY_001_D2%(l+1);
					int tmp2 = GENERIC_ARRAY_001_D2%(k+1);
					array2_generic[k][l] += (float) (array2_generic[i][j] + tmp1 +1)/(array2_generic[i][j] + tmp2 +1) ;
				}
			}
		}
	}

	// Calculate the sum after array modification.
	sum = sum2DFloatArrayS(array1_generic, GENERIC_ARRAY_001_D1, GENERIC_ARRAY_001_D2);
	printf("		--- sum array1_generic=%f.\n", sum);
    // - Log the modified array sum.
	genericLoop2D2A_float_result.modifiedArray1Sum=sum;

	sum = sum2DFloatArrayS(array2_generic, GENERIC_ARRAY_002_D1, GENERIC_ARRAY_002_D2);
	printf("		--- sum array2_generic=%f.\n", sum);
    // - Log the modified array sum.
	genericLoop2D2A_float_result.modifiedArray2Sum=sum;
}

// Generic 2D loop, 2 arrays.
// - Dynamic allocation of arrays.
void computationKernelGenericLoop2D2AD()
{
	printf("		--- computationKernelGenericLoop2D2AD.\n");
	// Stack arrays:
	ARRAY_LENGTH_D1 = GENERIC_ARRAY_D_001_D1;
	ARRAY_LENGTH_D2 = GENERIC_ARRAY_D_001_D2;
	int** array1_generic;
	allocate2DIntArrayD(&array1_generic, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	populate2DIntArrayD(&array1_generic, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	long sum = sum2DIntArrayD(array1_generic, GENERIC_ARRAY_D_001_D1, GENERIC_ARRAY_D_001_D2);
	// - Log the initial array sum.
	genericLoop2D2AD_result.initArray1Sum=sum;
	printf("		--- sum array1=%lu.\n", sum);

	ARRAY_LENGTH_D1 = GENERIC_ARRAY_D_002_D1;
	ARRAY_LENGTH_D2 = GENERIC_ARRAY_D_002_D2;
	int** array2_generic;
	allocate2DIntArrayD(&array2_generic, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	populate2DIntArrayD(&array2_generic, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DIntArrayD(array2_generic, GENERIC_ARRAY_D_002_D1, GENERIC_ARRAY_D_002_D2);
	// - Log the initial array sum.
	genericLoop2D2AD_result.initArray2Sum=sum;
	printf("		--- sum array2=%lu.\n", sum);

	for (int i = 0; i < GENERIC_ARRAY_D_001_D1; i++)
	{
		for (int j = 0; j < GENERIC_ARRAY_D_001_D2; j++)
		{
			array1_generic[i][j] += array1_generic[i][j] * array1_generic[i][j];

			for (int k = 1+i; k < GENERIC_ARRAY_D_002_D1; k++)
			{
				for (int l = 0; l < GENERIC_ARRAY_D_002_D2; l++)
				{
					int tmp1 = GENERIC_ARRAY_D_001_D2%(l+1);
					int tmp2 = GENERIC_ARRAY_D_001_D2%(k+1);
					array2_generic[k][l] += 0.0001*((array2_generic[i][j] + tmp1 +1)/(array2_generic[i][j] + tmp2 +1));
				}
			}
		}
	}

	// Calculate the sum after array modification.
	sum = sum2DIntArrayD(array1_generic, GENERIC_ARRAY_D_001_D1, GENERIC_ARRAY_D_001_D2);
	printf("		--- sum array1_generic=%lu.\n", sum);
    // - Log the modified array sum.
	genericLoop2D2AD_result.modifiedArray1Sum=sum;

	sum = sum2DIntArrayD(array2_generic, GENERIC_ARRAY_D_002_D1, GENERIC_ARRAY_D_002_D2);
	printf("		--- sum array2_generic=%lu.\n", sum);
    // - Log the modified array sum.
	genericLoop2D2AD_result.modifiedArray2Sum=sum;
}

// Generic 2D loop, 2 arrays.
// - Dynamic allocation of arrays.
void computationKernelGenericLoop2D2ADFloat()
{
	printf("		--- computationKernelGenericLoop2D2ADFloat.\n");
	// Stack arrays:
	ARRAY_LENGTH_D1 = GENERIC_ARRAY_D_001_D1;
	ARRAY_LENGTH_D2 = GENERIC_ARRAY_D_001_D2;
	float** array1_generic;
	allocate2DFloatArrayD(&array1_generic, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	populate2DFloatArrayD(&array1_generic, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	double sum = sum2DFloatArrayD(array1_generic, GENERIC_ARRAY_D_001_D1, GENERIC_ARRAY_D_001_D2);
	// - Log the initial array sum.
	genericLoop2D2AD_float_result.initArray1Sum=sum;
	printf("		--- sum array1=%f.\n", sum);

	ARRAY_LENGTH_D1 = GENERIC_ARRAY_D_002_D1;
	ARRAY_LENGTH_D2 = GENERIC_ARRAY_D_002_D2;
	float** array2_generic;
	allocate2DFloatArrayD(&array2_generic, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	populate2DFloatArrayD(&array2_generic, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2);

	// Calculate the sum before array modification.
	sum = sum2DFloatArrayD(array2_generic, GENERIC_ARRAY_D_002_D1, GENERIC_ARRAY_D_002_D2);
	// - Log the initial array sum.
	genericLoop2D2AD_float_result.initArray2Sum=sum;
	printf("		--- sum array2=%f.\n", sum);

	for (int i = 0; i < GENERIC_ARRAY_D_001_D1; i++)
	{
		for (int j = 0; j < GENERIC_ARRAY_D_001_D2; j++)
		{
			array1_generic[i][j] += (float) array1_generic[i][j] * array1_generic[i][j];

			for (int k = 1+i; k < GENERIC_ARRAY_D_002_D1; k++)
			{
				for (int l = 0; l < GENERIC_ARRAY_D_002_D2; l++)
				{
					int tmp1 = GENERIC_ARRAY_D_001_D2%(l+1);
					int tmp2 = GENERIC_ARRAY_D_001_D2%(k+1);
					array2_generic[k][l] += (float) (array2_generic[i][j] + tmp1 +1)/(array2_generic[i][j] + tmp2 +1);
				}
			}
		}
	}

	// Calculate the sum after array modification.
	sum = sum2DFloatArrayD(array1_generic, GENERIC_ARRAY_D_001_D1, GENERIC_ARRAY_D_001_D2);
	printf("		--- sum array1_generic=%f.\n", sum);
    // - Log the modified array sum.
	genericLoop2D2AD_float_result.modifiedArray1Sum=sum;

	sum = sum2DFloatArrayD(array2_generic, GENERIC_ARRAY_D_002_D1, GENERIC_ARRAY_D_002_D2);
	printf("		--- sum array2_generic=%f.\n", sum);
    // - Log the modified array sum.
	genericLoop2D2AD_float_result.modifiedArray2Sum=sum;
}

// Generic 3D loop, 2 arrays.
void computationKernelGenericLoop3D2A()
{
	printf("		--- computationKernelGenericLoop3D2A.\n");
	ARRAY_LENGTH_D1 = GENERIC_ARRAY_001_D1;
	ARRAY_LENGTH_D2 = GENERIC_ARRAY_001_D2;
	ARRAY_LENGTH_D3 = GENERIC_ARRAY_001_D3;
	static int array1_generic[GENERIC_ARRAY_001_D1][GENERIC_ARRAY_001_D2][GENERIC_ARRAY_001_D3];
	populate3DIntArrayS(array1_generic, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2, ARRAY_LENGTH_D3);

	// Calculate the sum before array modification.
	long sumArray = sum3DIntArrayS(array1_generic, GENERIC_ARRAY_001_D1, GENERIC_ARRAY_001_D2, GENERIC_ARRAY_001_D3);
	// - Log the initial array sum.
	genericLoop3D2A_result.initArray1Sum=sumArray;
	printf("		--- sum array1_generic=%lu.\n", sumArray);

	ARRAY_LENGTH_D1 = GENERIC_ARRAY_002_D1;
	ARRAY_LENGTH_D2 = GENERIC_ARRAY_002_D2;
	ARRAY_LENGTH_D3 = GENERIC_ARRAY_002_D3;
	static int array2_generic[GENERIC_ARRAY_002_D1][GENERIC_ARRAY_002_D2][GENERIC_ARRAY_002_D3];
	populate3DIntArrayS(array2_generic, ARRAY_LENGTH_D1, GENERIC_ARRAY_002_D2, GENERIC_ARRAY_002_D3);

	// Calculate the sum before array modification.
	sumArray = sum3DIntArrayS(array2_generic, GENERIC_ARRAY_002_D1, GENERIC_ARRAY_002_D2, GENERIC_ARRAY_002_D3);
	// - Log the initial array sum.
	genericLoop3D2A_result.initArray2Sum=sumArray;
	printf("		--- sum array2_generic=%lu.\n", sumArray);

	for (int i = 0; i < GENERIC_ARRAY_001_D1; i++)
	{
		for (int j = 0; j < GENERIC_ARRAY_001_D2; j++)
		{
			for (int k = 0; k < GENERIC_ARRAY_001_D3; k++)
			{
				array1_generic[i][j][k] = array1_generic[i][j][k]*array1_generic[i][j][k];
				array2_generic[i][j][k] = array1_generic[i][j][k] + 2*array1_generic[i][j][k]*array2_generic[i][j][k] + array2_generic[i][j][k]*array2_generic[i][j][k];
			}
		}
	}

	// Calculate the sum after array modification.
	sumArray = sum3DIntArrayS(array1_generic, GENERIC_ARRAY_001_D1, GENERIC_ARRAY_001_D2, GENERIC_ARRAY_001_D3);
	printf("		--- sum array1_generic=%lu.\n", sumArray);
    // - Log the modified array sum.
	genericLoop3D2A_result.modifiedArray1Sum=sumArray;

	sumArray = sum3DIntArrayS(array2_generic, GENERIC_ARRAY_002_D1, GENERIC_ARRAY_002_D2, GENERIC_ARRAY_002_D3);
	printf("		--- sum array2_generic=%lu.\n", sumArray);
    // - Log the modified array sum.
	genericLoop3D2A_result.modifiedArray2Sum=sumArray;
}

// Generic 3D loop, 2 arrays.
void computationKernelGenericLoop3D2AFloat()
{
	printf("		--- computationKernelGenericLoop3D2A.\n");
	ARRAY_LENGTH_D1 = GENERIC_ARRAY_001_D1;
	ARRAY_LENGTH_D2 = GENERIC_ARRAY_001_D2;
	ARRAY_LENGTH_D3 = GENERIC_ARRAY_001_D3;
	static float array1_generic[GENERIC_ARRAY_001_D1][GENERIC_ARRAY_001_D2][GENERIC_ARRAY_001_D3];
	populate3DFloatArrayS(array1_generic, ARRAY_LENGTH_D1, ARRAY_LENGTH_D2, ARRAY_LENGTH_D3);

	// Calculate the sum before array modification.
	double sumArray = sum3DFloatArrayS(array1_generic, GENERIC_ARRAY_001_D1, GENERIC_ARRAY_001_D2, GENERIC_ARRAY_001_D3);
	// - Log the initial array sum.
	genericLoop3D2A_float_result.initArray1Sum=sumArray;
	printf("		--- sum array1_generic=%f.\n", sumArray);

	ARRAY_LENGTH_D1 = GENERIC_ARRAY_002_D1;
	ARRAY_LENGTH_D2 = GENERIC_ARRAY_002_D2;
	ARRAY_LENGTH_D3 = GENERIC_ARRAY_002_D3;
	static float array2_generic[GENERIC_ARRAY_002_D1][GENERIC_ARRAY_002_D2][GENERIC_ARRAY_002_D3];
	populate3DFloatArrayS(array2_generic, ARRAY_LENGTH_D1, GENERIC_ARRAY_002_D2, GENERIC_ARRAY_002_D3);

	// Calculate the sum before array modification.
	sumArray = sum3DFloatArrayS(array2_generic, GENERIC_ARRAY_002_D1, GENERIC_ARRAY_002_D2, GENERIC_ARRAY_002_D3);
	// - Log the initial array sum.
	genericLoop3D2A_float_result.initArray2Sum=sumArray;
	printf("		--- sum array2_generic=%f.\n", sumArray);

	for (int i = 0; i < GENERIC_ARRAY_001_D1; i++)
	{
		for (int j = 0; j < GENERIC_ARRAY_001_D2; j++)
		{
			for (int k = 0; k < GENERIC_ARRAY_001_D3; k++)
			{
				array1_generic[i][j][k] = array1_generic[i][j][k]*array1_generic[i][j][k];
				array2_generic[i][j][k] = (float) array1_generic[i][j][k] + 2*array1_generic[i][j][k]*array2_generic[i][j][k] + array2_generic[i][j][k]*array2_generic[i][j][k];
			}
		}
	}

	// Calculate the sum after array modification.
	sumArray = sum3DFloatArrayS(array1_generic, GENERIC_ARRAY_001_D1, GENERIC_ARRAY_001_D2, GENERIC_ARRAY_001_D3);
	printf("		--- sum array1_generic=%f.\n", sumArray);
    // - Log the modified array sum.
	genericLoop3D2A_float_result.modifiedArray1Sum=sumArray;

	sumArray = sum3DFloatArrayS(array2_generic, GENERIC_ARRAY_002_D1, GENERIC_ARRAY_002_D2, GENERIC_ARRAY_002_D3);
	printf("		--- sum array2_generic=%f.\n", sumArray);
    // - Log the modified array sum.
	genericLoop3D2A_float_result.modifiedArray2Sum=sumArray;
}

void allocate1DIntArrayD(int** array, long arrayLength)
{
	*array = (int*) malloc(arrayLength*sizeof(int));
}

void allocate1DFloatArrayD(float** array, long arrayLength)
{
	*array = (float*) malloc(arrayLength*sizeof(float));
}

void allocate2DIntArrayD(int*** array, long arrayLength, long arrayWidth)
{
    // Allocate pointers for rows.
	*array  = (int**) malloc(arrayLength*sizeof(int*));

    for(long i=0; i< arrayLength;i++)
    {
		(*array)[i] = (int*) malloc(arrayWidth*sizeof(int));
    }
}

void allocate2DFloatArrayD(float*** array, long arrayLength, long arrayWidth)
{
    // Allocate pointers for rows.
	*array  = (float**) malloc(arrayLength*sizeof(int*));

    for(long i=0; i< arrayLength;i++)
    {
		(*array)[i] = (float*) malloc(arrayWidth*sizeof(float));
    }
}


void populate1DIntArrayS(int array[ARRAY_LENGTH_D1], long d1)
{
	for(long i=0; i< d1;i++)
	{
		if(i>10)
			array[i]=i%10;
		else
			array[i]=i;
	}
}

void populate1DFloatArrayS(float array[ARRAY_LENGTH_D1], long d1)
{
	for(long i=0; i< d1;i++)
	{
		if(i>10)
			array[i]=i%10+1;
		else
			array[i]=i+1;
	}
}

void populate2DIntArrayS(int array[ARRAY_LENGTH_D1][ARRAY_LENGTH_D2], long arrayLength, long arrayWidth)
{
	for(long i=0; i< arrayLength;i++)
	{
		for(long j=0; j<arrayWidth; j++)
		{
			if(i>4)
				array[i][j]=i%10+1;
			else
				array[i][j]=i+1;
		}
	}
}

void populate2DFloatArrayS(float array[ARRAY_LENGTH_D1][ARRAY_LENGTH_D2], long arrayLength, long arrayWidth)
{
	for(long i=0; i< arrayLength;i++)
	{
		for(long j=0; j<arrayWidth; j++)
		{
			if(i>10)
				array[i][j]=(i%10)+1;
			else
				array[i][j]=i+1;
			//printf("array[%d][%d] = %f.\n",i,j, array[i][j]);
		}
	}
}

void populate3DIntArrayS(int array[ARRAY_LENGTH_D1][ARRAY_LENGTH_D2][ARRAY_LENGTH_D3], long D1, long D2, long D3)
{
	for(long i=0; i< D1;i++)
	{
		for(long j=0; j<D2; j++)
		{
			for(long k=0; k<D3; k++)
			{
				if(i>10)
					array[i][j][k]=i%10;
				else
					array[i][j][k]=i;
				//if(array[i][j][k]!=0)
					//printf("array[i][j][k] = %f.\n", array[i][j][k]);
			}
		}

	}
}

void populate3DFloatArrayS(float array[ARRAY_LENGTH_D1][ARRAY_LENGTH_D2][ARRAY_LENGTH_D3], long D1, long D2, long D3)
{
	for(long i=0; i< D1;i++)
	{
		for(long j=0; j<D2; j++)
		{
			for(long k=0; k<D3; k++)
			{
				if(i>10)
					array[i][j][k]=i%10;
				else
					array[i][j][k]=i;
				//if(array[i][j][k]!=0)
					//printf("array[i][j][k] = %f.\n", array[i][j][k]);
			}
		}

	}
}

void populate2DIntArrayD(int*** array, long arrayLength, long arrayWidth)
{
	for(long i=0; i< arrayLength;i++)
	{
		for(long j=0; j<arrayWidth; j++)
		{
			if(i>10)
				(*array)[i][j]=i%10+1;
			else
				(*array)[i][j]=i+1;
		}
	}
}

void populate1DIntArrayD(int** array, long arrayLength)
{
	for(long i=0; i< arrayLength;i++)
	{
		if(i>10)
			(*array)[i]=i%10+1;
		else
			(*array)[i]=i+1;
	}
}

void populate1DFloatArrayD(float** array, long arrayLength)
{
	for(long i=0; i< arrayLength;i++)
	{
		if(i>10)
			(*array)[i]=i%10+1;
		else
			(*array)[i]=i+1;
	}
}

void populate2DFloatArrayD(float*** array, long arrayLength, long arrayWidth)
{
	for(long i=0; i< arrayLength;i++)
	{
		for(long j=0; j<arrayWidth; j++)
		{
			if(i>10)
				(*array)[i][j]=i%10+1;
			else
				(*array)[i][j]=i+1;
		}
	}
}

void print2DIntArrayD(int** array, long arrayLength, long arrayWidth)
{
	for(long i=0; i< arrayLength;i++)
	{
		for(long j=0; j<arrayWidth; j++)
		{
			printf("array[%lu][%lu]=%d.\n", i, j, array[i][j]);
		}
	}
}

void print2DFloatArrayD(float** array, long arrayLength, long arrayWidth)
{
	for(long i=0; i< arrayLength;i++)
	{
		for(long j=0; j<arrayWidth; j++)
		{
			printf("array[%lu][%lu]=%f.\n", i, j, array[i][j]);
		}
	}
}

void print2DIntArrayS(int array[ARRAY_LENGTH_D1][ARRAY_LENGTH_D2], long arrayLength, long arrayWidth)
{
	for(long i=0; i< arrayLength;i++)
	{
		for(long j=0; j<arrayWidth; j++)
		{
			printf("array[%lu][%lu]=%d.\n", i, j, array[i][j]);
		}
	}
}

void print2DFloatArrayS(float array[ARRAY_LENGTH_D1][ARRAY_LENGTH_D2], long arrayLength, long arrayWidth)
{
	for(long i=0; i< arrayLength;i++)
	{
		for(long j=0; j<arrayWidth; j++)
		{
			printf("array[%lu][%lu]=%f.\n", i, j, array[i][j]);
		}
	}
}

long sum1DIntArrayD(int* array, long arrayLength)
{
	long sum = 0;

	for(long i=0; i< arrayLength;i++)
	{
		sum = sum+array[i];
	}

    return sum;
}

double sum1DFloatArrayD(float* array, long arrayLength)
{
	double sum = 0;

	for(long i=0; i< arrayLength;i++)
	{
		sum = sum+array[i];
	}

    return sum;
}

double sum2DFloatArrayD(float** array, long arrayLength, long arrayWidth)
{
	double sum = 0;

	for(long i=0; i< arrayLength;i++)
	{
		for(long j=0; j<arrayWidth; j++)
			sum = sum+array[i][j];
	}

    return sum;
}

long sum2DIntArrayD(int** array, long arrayLength, long arrayWidth)
{
	long sum = 0;

	for(long i=0; i< arrayLength;i++)
	{
		for(long j=0; j<arrayWidth; j++)
			sum = sum+array[i][j];
	}

    return sum;
}

long sum1DIntArrayS(int array[ARRAY_LENGTH_D1], long arrayLength)
{
	long sum = 0;

	for(long i=0; i< arrayLength;i++)
	{
		sum = sum+array[i];
	}

    return sum;
}

long sum2DIntArrayS(int array[ARRAY_LENGTH_D1][ARRAY_LENGTH_D2], long arrayLength, long arrayWidth)
{
	long sum = 0;

	for(long i=0; i< arrayLength;i++)
	{
		for(long j=0; j<arrayWidth; j++)
			sum = sum+array[i][j];
	}

    return sum;
}

long sum3DIntArrayS(int array[ARRAY_LENGTH_D1][ARRAY_LENGTH_D2][ARRAY_LENGTH_D3], long D1, long D2, long D3)
{
	long sum = 0;

	for(long i=0; i< D1;i++)
	{
		for(long j=0; i< D2;j++)
		{
			for(long k=0; k<D3; k++)
				sum = sum+array[i][j][k];
		}
	}

    return sum;
}

double sum1DFloatArrayS(float array[ARRAY_LENGTH_D1], long arrayLength)
{
	double sum = 0;

	for(long i=0; i< arrayLength;i++)
	{
		sum = sum+array[i];
	}

    return sum;
}

double sum2DFloatArrayS(float array[ARRAY_LENGTH_D1][ARRAY_LENGTH_D2], long D1, long D2)
{
	double sum = 0;

	for(long i=0; i< D1;i++)
	{
		for(long j=0; j<D2; j++)
			sum = sum+array[i][j];
	}

    return sum;
}

double sum3DFloatArrayS(float array[ARRAY_LENGTH_D1][ARRAY_LENGTH_D2][ARRAY_LENGTH_D3], long D1, long D2, long D3)
{
	double sum = 0;

	for(long i=0; i< D1;i++)
	{
		for(long j=0; j< D2;j++)
		{
			for(long k=0; k<D3; k++)
				sum = sum+array[i][j][k];
		}
	}

    return sum;
}

#ifdef LOG_RESULTS
void write2DIntToFile(int array[ARRAY_LENGTH_D1][ARRAY_LENGTH_D2], long arrayLength, long arrayWidth)
{
	for(long i=0; i< arrayLength;i++)
	{
		for(long j=0; j<arrayWidth; j++)
		{
			fprintf(logFile, "%d ", array[i][j]);
		}
		fprintf(logFile,"\n");
	}
}

void appendStringToFile(char* fileName, char* text)
{
	// Appending to the end of file.
	FILE* logFileTemp = fopen(fileName, "a+");

	fprintf(logFileTemp,"%s\n", text);

	fclose(logFileTemp);
}
#endif /* MACRO */
