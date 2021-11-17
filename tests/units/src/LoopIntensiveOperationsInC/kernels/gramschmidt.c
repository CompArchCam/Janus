/**
 * Adapted from http://web.cse.ohio-state.edu/~pouchet.2/software/polybench/
 */

#include "gramschmidt.h"

// Polybench, gramschmidt
void computationKernelPolybenchGramschmidt()
{
	// _PB_N = 1600. ARRAY_001_WIDTH, ARRAY_002_LENGTH, ARRAY_002_WIDTH, ARRAY_003_WIDTH
	// _PB_M = 1400. ARRAY_001_LENGTH, ARRAY_003_LENGTH

	// Stack arrays:
	long array_lenght_D1 = POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1;
	long array_lenght_D2 = POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2;
	static float array1[POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1][POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2];
	populateGramschmidt2DFloatArray1S(array1, array_lenght_D1, array_lenght_D2);

	array_lenght_D1 = POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D1;
	array_lenght_D2 = POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D2;
	float array2[array_lenght_D1][array_lenght_D2];
	populateGramschmidt2DFloatArray2S(array2, array_lenght_D1, array_lenght_D2);

	array_lenght_D1 = POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D1;
	array_lenght_D2 = POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D2;
	float array3[array_lenght_D1][array_lenght_D2];
	populateGramschmidt2DFloatArray3S(array3, array_lenght_D1, array_lenght_D2);

    for (int k = 0; k < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2; k++)
    {
        int nrm = 0.0;
        for (int i = 0; i < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1; i++)
            nrm += array1[i][k] * array1[i][k];
        	//array2[k][k] = SQRT_FUN(nrm);
        	array2[k][k] = array2[k][k]/2;
        for (int i = 0; i < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1; i++)
        	array3[i][k] = array1[i][k] / array2[k][k];
        for (int j = k + 1; j < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2; j++)
        {
        	array2[k][j] = 0.0;
            for (int i = 0; i < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1; i++)
            	array2[k][j] += array3[i][k] * array1[i][j];
            for (int i = 0; i < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1; i++)
            	array1[i][j] = array1[i][j] - array3[i][k] * array2[k][j];
        }
    }
}

void populateGramschmidt2DFloatArray1S(float array[POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1][POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2], long arrayLength, long arrayWidth)
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

void populateGramschmidt2DFloatArray2S(float array[POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D1][POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D2], long arrayLength, long arrayWidth)
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

void populateGramschmidt2DFloatArray3S(float array[POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D1][POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D2], long arrayLength, long arrayWidth)
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
