/**
 * Adapted from http://web.cse.ohio-state.edu/~pouchet.2/software/polybench/
 */

#include "cholesky.h"

// Polybench, cholesky
void computationKernelPolybenchCholesky()
{
	printf("computationKernelPolybenchCholesky.\n");

	long array_lenght_D1 = POLY_CHOLESKY_ARRAY_001_LENGTH_D1;
	long array_lenght_D2 = POLY_CHOLESKY_ARRAY_001_LENGTH_D2;
	int array[array_lenght_D1][array_lenght_D2];

	populateCholesky2DIntArrayS(array, array_lenght_D1, array_lenght_D2);
	printf("	- Populate arrays.\n");
	printf("	- ARRAY_LENGTH = %lu; ARRAY_WIDTH = %lu.\n", array_lenght_D1, array_lenght_D2);
    for (int i = 0; i < array_lenght_D1; i++)
    {
        for (int j = 0; j <= i; j++)
        {
        	array[i][j] = (float)(-j % array_lenght_D1) / array_lenght_D1 + 1;
        }
        for (int j = i+1; j < array_lenght_D1; j++)
        {
            array[i][j] = 0;
        }
        array[i][i] = 1;
    }
    printf("	--- Populate arrays.\n");
}

void populateCholesky2DIntArrayS(int array[POLY_CHOLESKY_ARRAY_001_LENGTH_D1][POLY_CHOLESKY_ARRAY_001_LENGTH_D2], long arrayLength, long arrayWidth)
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
