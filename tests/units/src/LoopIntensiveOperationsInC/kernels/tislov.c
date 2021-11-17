/**
 * Adapted from http://web.cse.ohio-state.edu/~pouchet.2/software/polybench/
 */

#include "tislov.h"

// Polybench, tislov
void computationKernelPolybenchTislov()
{
	// n=150000
	// Stack arrays:
	long array_lenght_D1 = POLY_TISLOV_ARRAY_001_LENGTH_D1;
	float b[array_lenght_D1];
	populateTislov1DFloatArray1S(b, array_lenght_D1);

	array_lenght_D1 = POLY_TISLOV_ARRAY_002_LENGTH_D1;
	float x[array_lenght_D1];
	populateTislov1DFloatArray2S(x, array_lenght_D1);

	array_lenght_D1 = POLY_TISLOV_ARRAY_003_LENGTH_D1;
	long array_lenght_D2 = POLY_TISLOV_ARRAY_003_LENGTH_D2;
	float L[array_lenght_D1][array_lenght_D2];
	populateTislov2DFloatArray3S(L, array_lenght_D1, array_lenght_D2);

    for (int i = 0; i < POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1; i++)
    {
        x[i] = - 999;
        b[i] =  i ;
        for (int j = 0; j <= i; j++)
        	L[i][j] = (int) (i+POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1-j+1)*2/POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1;
    }
}

void populateTislov1DFloatArray1S(float array[POLY_TISLOV_ARRAY_001_LENGTH_D1], long d1)
{
	for(long i=0; i< d1;i++)
	{
		if(i>10)
			array[i]=i%10;
		else
			array[i]=i;
	}
}

void populateTislov1DFloatArray2S(float array[POLY_TISLOV_ARRAY_002_LENGTH_D1], long d1)
{
	for(long i=0; i< d1;i++)
	{
		if(i>10)
			array[i]=i%10;
		else
			array[i]=i;
	}
}

void populateTislov2DFloatArray3S(float array[POLY_TISLOV_ARRAY_003_LENGTH_D1][POLY_TISLOV_ARRAY_003_LENGTH_D2], long arrayLength, long arrayWidth)
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
