/**
 * Adapted from http://web.cse.ohio-state.edu/~pouchet.2/software/polybench/
 */

#include <stdio.h>

#include "doitgen.h"


// Polybench, doitgen
void computationKernelPolybenchDoitgen()
{
    // _PB_NR = 150.
	// _PB_NQ = 140.
	// _PB_NP = 1600.748019
	// Stack arrays:
	long array_lenght_D1 = POLY_DOITGEN_ARRAY_001_LENGTH_D1;
	static float sum[POLY_DOITGEN_ARRAY_001_LENGTH_D1];
	populateDoitgen1DFloatArray1S(sum, array_lenght_D1);
	//printf("		--- populate1DFloatArrayS populated.\n");

	array_lenght_D1 = POLY_DOITGEN_ARRAY_002_LENGTH_D1;
	long array_lenght_D2 = POLY_DOITGEN_ARRAY_002_LENGTH_D2;
	//printf("		--- ARRAY_LENGTH_D1=%lu, ARRAY_LENGTH_D2=%lu.\n", array_lenght_D1, array_lenght_D2);
	static float C4[POLY_DOITGEN_ARRAY_002_LENGTH_D1][POLY_DOITGEN_ARRAY_002_LENGTH_D2];
	//printf("		--- Try to populate2DFloatArrayS populated.\n");
	populateDoitgen2DFloatArray2S(C4, array_lenght_D1, array_lenght_D2);
	//printf("		--- populate2DFloatArrayS populated.\n");

	array_lenght_D1 = POLY_DOITGEN_ARRAY_003_LENGTH_D1;
	array_lenght_D2 = POLY_DOITGEN_ARRAY_003_LENGTH_D2;
	long array_lenght_D3 = POLY_DOITGEN_ARRAY_003_LENGTH_D3;
	static float A[POLY_DOITGEN_ARRAY_003_LENGTH_D1][POLY_DOITGEN_ARRAY_003_LENGTH_D2][POLY_DOITGEN_ARRAY_003_LENGTH_D3];
	populateDoitgen3DFloatArray3S(A, array_lenght_D1, array_lenght_D2, array_lenght_D3);

	//printf("		--- populate3DFloatArrayS populated.\n");

    for (int r = 0; r < array_lenght_D1; r++)
        for (int q = 0; q < array_lenght_D2; q++)
        {
            for (int p = 0; p < array_lenght_D3; p++)
            {
                sum[p] = 0.0;
                for (int s = 0; s < array_lenght_D3; s++)
                    sum[p] += A[r][q][s] * C4[s][p];
            }
            for (int p = 0; p < array_lenght_D3; p++)
                A[r][q][p] = sum[p];
        }
}

void populateDoitgen1DFloatArray1S(float array[POLY_DOITGEN_ARRAY_001_LENGTH_D1], long d1)
{
	for(long i=0; i< d1;i++)
	{
		if(i>10)
			array[i]=i%10;
		else
			array[i]=i;
	}
}

void populateDoitgen2DFloatArray2S(float array[POLY_DOITGEN_ARRAY_002_LENGTH_D1][POLY_DOITGEN_ARRAY_002_LENGTH_D2], long arrayLength, long arrayWidth)
{
	//printf("		--- populateDoitgen2DFloatArray2S.\n");
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

void populateDoitgen3DFloatArray3S(float array[POLY_DOITGEN_ARRAY_003_LENGTH_D1][POLY_DOITGEN_ARRAY_003_LENGTH_D2][POLY_DOITGEN_ARRAY_003_LENGTH_D3], long d1, long d2, long d3)
{
	for(long i=0; i< d1;i++)
	{
		for(long j=0; j<d2; j++)
		{
			for(long k=0; k<d3; k++)
			{
				if(i>10)
					array[i][j][k]=i%10;
				else
					array[i][j][k]=i;
			}
		}
	}
}
