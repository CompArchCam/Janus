/**
 * Adapted from http://web.cse.ohio-state.edu/~pouchet.2/software/polybench/
 */

#include "lu.h"

// Polybench, lu
void computationKernelPolybenchLu()
{
	// Stack arrays.
	long array_lenght_D1 = POLY_LU_ARRAY_001_LENGTH_D1;
	long array_lenght_D2 = POLY_LU_ARRAY_001_LENGTH_D2;
	float array[array_lenght_D1][array_lenght_D2];
	populateLu2DFloatArrayS(array, array_lenght_D1, array_lenght_D2);

    for (int i = 0; i < POLY_LU_ARRAY_001_LENGTH_D1; i++)
        for (int j = 0; j < POLY_LU_ARRAY_001_LENGTH_D2; j++)
          array[i][j] = (float) ((i*j+1) % POLY_LU_ARRAY_001_LENGTH_D1) / POLY_LU_ARRAY_001_LENGTH_D1;
}

void populateLu2DFloatArrayS(float array[POLY_LU_ARRAY_001_LENGTH_D1][POLY_LU_ARRAY_001_LENGTH_D2], long arrayLength, long arrayWidth)
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
