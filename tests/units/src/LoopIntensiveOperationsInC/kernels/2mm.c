/**
 * Adapted from http://web.cse.ohio-state.edu/~pouchet.2/software/polybench/
 */

#include "kernelConfiguration.h"
#include "2mm.h"

void populate2mm2DFloatArray1S(float array[POLY_2MM_ARRAY_001_LENGTH_D1][POLY_2MM_ARRAY_001_LENGTH_D2], long arrayLength, long arrayWidth);
void populate2mm2DFloatArray2S(float array[POLY_2MM_ARRAY_002_LENGTH_D1][POLY_2MM_ARRAY_002_LENGTH_D2], long arrayLength, long arrayWidth);
void populate2mm2DFloatArray3S(float array[POLY_2MM_ARRAY_003_LENGTH_D1][POLY_2MM_ARRAY_003_LENGTH_D2], long arrayLength, long arrayWidth);
void populate2mm2DFloatArray4S(float array[POLY_2MM_ARRAY_004_LENGTH_D1][POLY_2MM_ARRAY_004_LENGTH_D2], long arrayLength, long arrayWidth);
void populate2mm2DFloatArray5S(float array[POLY_2MM_ARRAY_005_LENGTH_D1][POLY_2MM_ARRAY_005_LENGTH_D2], long arrayLength, long arrayWidth);

// Polybench, 2mm
void computationKernelPolybench2mm()
{

//	_PB_NI = 180 - ARRAY_001_LENGTH, ARRAY_002_LENGTH, ARRAY_005_LENGTH
//	_PB_NJ = 190 - ARRAY_001_WIDTH, ARRAY_003_WIDTH, ARRAY_004_LENGTH
//	_PB_NK = 210 - ARRAY_002_WIDTH, ARRAY_003_LENGTH
//  _PB_NL = 220. - ARRAY_004_WIDTH, ARRAY_005_WIDTH
//	alpha = 7064880.
//	beta = 7064880.
	long alpha = 7064880;
	long beta = 7064880;

	// Stack arrays:
	long array_lenght_D1 = POLY_2MM_ARRAY_001_LENGTH_D1;
	long array_lenght_D2 = POLY_2MM_ARRAY_001_LENGTH_D2;
	float array1[array_lenght_D1][array_lenght_D2];
	populate2mm2DFloatArray1S(array1, array_lenght_D1, array_lenght_D2);

	array_lenght_D1 = POLY_2MM_ARRAY_002_LENGTH_D1;
	array_lenght_D2 = POLY_2MM_ARRAY_002_LENGTH_D2;
	float array2[array_lenght_D1][array_lenght_D2];
	populate2mm2DFloatArray2S(array2, array_lenght_D1, array_lenght_D2);

	array_lenght_D1 = POLY_2MM_ARRAY_003_LENGTH_D1;
	array_lenght_D2 = POLY_2MM_ARRAY_003_LENGTH_D2;
	float array3[array_lenght_D1][array_lenght_D2];
	populate2mm2DFloatArray3S(array3, array_lenght_D1, array_lenght_D2);

	array_lenght_D1 = POLY_2MM_ARRAY_004_LENGTH_D1;
	array_lenght_D2 = POLY_2MM_ARRAY_004_LENGTH_D2;
	float array4[array_lenght_D1][array_lenght_D2];
	populate2mm2DFloatArray4S(array4, array_lenght_D1, array_lenght_D2);

	array_lenght_D1 = POLY_2MM_ARRAY_005_LENGTH_D1;
	array_lenght_D2 = POLY_2MM_ARRAY_005_LENGTH_D2;
	float array5[array_lenght_D1][array_lenght_D2];
	populate2mm2DFloatArray5S(array5, array_lenght_D1, array_lenght_D2);

    for (int i = 0; i < POLY_2MM_ARRAY_001_LENGTH_D1; i++)
    {
        for (int j = 0; j < POLY_2MM_ARRAY_001_LENGTH_D2; j++)
        {
            array1[i][j] = 0.0;
            for (int k = 0; k < POLY_2MM_ARRAY_002_LENGTH_D2; ++k)
                array1[i][j] += alpha * array2[i][k] * array3[k][j];
        }
    }
    for (int i = 0; i < POLY_2MM_ARRAY_001_LENGTH_D1; i++)
    {
        for (int j = 0; j < POLY_2MM_ARRAY_005_LENGTH_D1; j++)
        {
        	array5[i][j] *= beta;
            for (int k = 0; k < POLY_2MM_ARRAY_001_LENGTH_D2; ++k)
            	array5[i][j] += array1[i][k] * array4[k][j];
        }
    }
}

void populate2mm2DFloatArray1S(float array[POLY_2MM_ARRAY_001_LENGTH_D1][POLY_2MM_ARRAY_001_LENGTH_D2], long arrayLength, long arrayWidth)
{
	for(long i=0; i< arrayLength;i++)
	{
		for(long j=0; j<arrayWidth; j++)
		{
			if(i>10)
				array[i][j]=i%10;
			else
				array[i][j]=i;
		}
	}
}

void populate2mm2DFloatArray2S(float array[POLY_2MM_ARRAY_002_LENGTH_D1][POLY_2MM_ARRAY_002_LENGTH_D2], long arrayLength, long arrayWidth)
{
	for(long i=0; i< arrayLength;i++)
	{
		for(long j=0; j<arrayWidth; j++)
		{
			if(i>10)
				array[i][j]=i%10;
			else
				array[i][j]=i;
		}
	}
}

void populate2mm2DFloatArray3S(float array[POLY_2MM_ARRAY_003_LENGTH_D1][POLY_2MM_ARRAY_003_LENGTH_D2], long arrayLength, long arrayWidth)
{
	for(long i=0; i< arrayLength;i++)
	{
		for(long j=0; j<arrayWidth; j++)
		{
			if(i>10)
				array[i][j]=i%10;
			else
				array[i][j]=i;
		}
	}
}

void populate2mm2DFloatArray4S(float array[POLY_2MM_ARRAY_004_LENGTH_D1][POLY_2MM_ARRAY_004_LENGTH_D2], long arrayLength, long arrayWidth)
{
	for(long i=0; i< arrayLength;i++)
	{
		for(long j=0; j<arrayWidth; j++)
		{
			if(i>10)
				array[i][j]=i%10;
			else
				array[i][j]=i;
		}
	}
}

void populate2mm2DFloatArray5S(float array[POLY_2MM_ARRAY_005_LENGTH_D1][POLY_2MM_ARRAY_005_LENGTH_D2], long arrayLength, long arrayWidth)
{
	for(long i=0; i< arrayLength;i++)
	{
		for(long j=0; j<arrayWidth; j++)
		{
			if(i>10)
				array[i][j]=i%10;
			else
				array[i][j]=i;
		}
	}
}
