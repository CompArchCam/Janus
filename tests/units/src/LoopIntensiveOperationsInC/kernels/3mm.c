/**
 * Adapted from http://web.cse.ohio-state.edu/~pouchet.2/software/polybench/
 */

#include "3mm.h"
#include "kernelConfiguration.h"

void populate3mm2DFloatArray1S(float array[POLY_3MM_ARRAY_001_LENGTH_D1][POLY_3MM_ARRAY_001_LENGTH_D2], long arrayLength, long arrayWidth);
void populate3mm2DFloatArray2S(float array[POLY_3MM_ARRAY_002_LENGTH_D1][POLY_3MM_ARRAY_002_LENGTH_D2], long arrayLength, long arrayWidth);
void populate3mm2DFloatArray3S(float array[POLY_3MM_ARRAY_003_LENGTH_D1][POLY_3MM_ARRAY_003_LENGTH_D2], long arrayLength, long arrayWidth);
void populate3mm2DFloatArray4S(float array[POLY_2MM_ARRAY_004_LENGTH_D1][POLY_2MM_ARRAY_004_LENGTH_D2], long arrayLength, long arrayWidth);
void populate3mm2DFloatArray5S(float array[POLY_3MM_ARRAY_005_LENGTH_D1][POLY_3MM_ARRAY_005_LENGTH_D2], long arrayLength, long arrayWidth);
void populate3mm2DFloatArray6S(float array[POLY_3MM_ARRAY_006_LENGTH_D1][POLY_3MM_ARRAY_006_LENGTH_D2], long arrayLength, long arrayWidth);

// Polybench, 3mm
void computationKernelPolybench3mm()
{

//	_PB_NI = 180. - ARRAY_001_LENGTH, ARRAY_005_LENGTH
//	_PB_NJ = 190. - ARRAY_001_WIDTH, ARRAY_002_WIDTH, ARRAY_003_LENGTH, ARRAY_005_WIDTH, ARRAY_006_LENGTH
//	_PB_NK = 200. - ARRAY_002_LENGTH,
//	_PB_NL = 210. - ARRAY_004_WIDTH, ARRAY_006_WIDTH
//	_PB_NM = 220. - ARRAY_003_WIDTH, ARRAY_004_LENGTH
	// Stack arrays:
	long array_lenght_D1 = POLY_3MM_ARRAY_001_LENGTH_D1;
	long array_lenght_D2 = POLY_3MM_ARRAY_001_LENGTH_D2;
	float array1[array_lenght_D1][array_lenght_D2];
	populate3mm2DFloatArray1S(array1, array_lenght_D1, array_lenght_D2);

	array_lenght_D1 = POLY_3MM_ARRAY_002_LENGTH_D1;
	array_lenght_D2 = POLY_3MM_ARRAY_002_LENGTH_D2;
	float array2[array_lenght_D1][array_lenght_D2];
	populate3mm2DFloatArray2S(array2, array_lenght_D1, array_lenght_D2);

	array_lenght_D1 = POLY_3MM_ARRAY_003_LENGTH_D1;
	array_lenght_D2 = POLY_3MM_ARRAY_003_LENGTH_D2;
	float array3[array_lenght_D1][array_lenght_D2];
	populate3mm2DFloatArray3S(array3, array_lenght_D1, array_lenght_D2);

	array_lenght_D1 = POLY_3MM_ARRAY_004_LENGTH_D1;
	array_lenght_D2 = POLY_3MM_ARRAY_004_LENGTH_D2;
	float array4[array_lenght_D1][array_lenght_D2];
	populate3mm2DFloatArray4S(array4, array_lenght_D1, array_lenght_D2);

	array_lenght_D1 = POLY_3MM_ARRAY_005_LENGTH_D1;
	array_lenght_D2 = POLY_3MM_ARRAY_005_LENGTH_D2;
	float array5[array_lenght_D1][array_lenght_D2];
	populate3mm2DFloatArray5S(array5, array_lenght_D1, array_lenght_D2);

	array_lenght_D1 = POLY_3MM_ARRAY_006_LENGTH_D1;
	array_lenght_D2 = POLY_3MM_ARRAY_006_LENGTH_D2;
	float array6[array_lenght_D1][array_lenght_D2];
	populate3mm2DFloatArray6S(array6, array_lenght_D1, array_lenght_D2);

    // E := A*B
    for (int i = 0; i < POLY_3MM_ARRAY_001_LENGTH_D1; i++)
        for (int j = 0; j < POLY_3MM_ARRAY_001_LENGTH_D2; j++)
        {
        	array5[i][j] = 0.0;
            for (int k = 0; k < POLY_3MM_ARRAY_002_LENGTH_D1; ++k)
            	array5[i][j] += array1[i][k] * array2[k][j];
        }
    // F := C*D
    for (int i = 0; i < POLY_3MM_ARRAY_006_LENGTH_D1; i++)
        for (int j = 0; j < POLY_3MM_ARRAY_004_LENGTH_D2; j++)
        {
        	array6[i][j] = 0.0;
            for (int k = 0; k < POLY_3MM_ARRAY_004_LENGTH_D1; ++k)
            	array6[i][j] += array3[i][k] * array4[k][j];
        }
}

void populate3mm2DFloatArray1S(float array[POLY_3MM_ARRAY_001_LENGTH_D1][POLY_3MM_ARRAY_001_LENGTH_D2], long arrayLength, long arrayWidth)
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

void populate3mm2DFloatArray2S(float array[POLY_3MM_ARRAY_002_LENGTH_D1][POLY_3MM_ARRAY_002_LENGTH_D2], long arrayLength, long arrayWidth)
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

void populate3mm2DFloatArray3S(float array[POLY_3MM_ARRAY_003_LENGTH_D1][POLY_3MM_ARRAY_003_LENGTH_D2], long arrayLength, long arrayWidth)
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

void populate3mm2DFloatArray4S(float array[POLY_2MM_ARRAY_004_LENGTH_D1][POLY_2MM_ARRAY_004_LENGTH_D2], long arrayLength, long arrayWidth)
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

void populate3mm2DFloatArray5S(float array[POLY_3MM_ARRAY_005_LENGTH_D1][POLY_3MM_ARRAY_005_LENGTH_D2], long arrayLength, long arrayWidth)
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

void populate3mm2DFloatArray6S(float array[POLY_3MM_ARRAY_006_LENGTH_D1][POLY_3MM_ARRAY_006_LENGTH_D2], long arrayLength, long arrayWidth)
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
