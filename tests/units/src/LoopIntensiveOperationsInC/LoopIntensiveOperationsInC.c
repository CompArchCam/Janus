//============================================================================
// Name        : LoopIntensiveOperationsInC.c
// Author      : Jasmin Jahic
// Version     :
// Copyright   : Feel free to use as you please, spread the word (STWO).
// Description : Loops with computing intensive operations.
//				 C.
//============================================================================

#include <stdio.h>      /* printf, scanf, puts, NULL */
#include <stdlib.h>     /* srand, rand, malloc */
#include "computationKernels.h"

int main(int argc, char **argv)
{
	printf("Start.\n");

	#ifdef LOG_RESULTS
	// Restart the file
	logFile = fopen("log.txt", "w+");
	fclose(logFile);
	#endif /* MACRO */

	// Polybench, cholesky
	computationKernelPolybenchCholesky();
	printf("	- computationKernelPolybenchCholesky...COMPLETE.\n");
	computationKernelPolybenchCholeskyD();
	printf("	- computationKernelPolybenchCholeskyD...COMPLETE.\n");
	computationKernelPolybenchCholeskyFloat();
	printf("	- computationKernelPolybenchCholeskyFloat...COMPLETE.\n");
	computationKernelPolybenchCholeskyDFloat();
	printf("	- computationKernelPolybenchCholeskyDFloat...COMPLETE.\n");

	// Polybench, 2mm
	computationKernelPolybench2mm();
	printf("	- computationKernelPolybench2mm...COMPLETE.\n");
	computationKernelPolybench2mmFloat();
	printf("	- computationKernelPolybench2mmFloat...COMPLETE.\n");

	// Polybench, 3mm
	computationKernelPolybench3mm();
	printf("	- computationKernelPolybench3mm...COMPLETE.\n");
	computationKernelPolybench3mmFloat();
	printf("	- computationKernelPolybench3mmFloat...COMPLETE.\n");

	// Polybench, tislov
	computationKernelPolybenchTislov();
	printf("	- computationKernelPolybenchTislov---COMPLETE.\n");
	computationKernelPolybenchTislovFloat();
	printf("	- computationKernelPolybenchTislovFloat---COMPLETE.\n");

	// Polybench, Gramschmidt
	computationKernelPolybenchGramschmidt();
	printf("	- computationKernelPolybenchGramschmidt...COMPLETE.\n");
	computationKernelPolybenchGramschmidtD();
	printf("	- computationKernelPolybenchGramschmidtD...COMPLETE.\n");
	computationKernelPolybenchGramschmidtFloat();
	printf("	- computationKernelPolybenchGramschmidtFloat...COMPLETE.\n");
	computationKernelPolybenchGramschmidtDFloat();
	printf("	- computationKernelPolybenchGramschmidtDFloat...COMPLETE.\n");


	// Generic, Loop1D2A
	computationKernelGenericLoop1D2A();
	printf("	- computationKernelGenericLoop1D2A...COMPLETE.\n");
	computationKernelGenericLoop1D2AD();
	printf("	- computationKernelGenericLoop1D2AD...COMPLETE.\n");
	computationKernelGenericLoop1D2AFloat();
	printf("	- computationKernelGenericLoop1D2AFloat...COMPLETE.\n");
	computationKernelGenericLoop1D2ADFloat();
	printf("	- computationKernelGenericLoop1D2ADFloat...COMPLETE.\n");

	// Generic, Loop2D2A
	computationKernelGenericLoop2D2A();
	printf("	- computationKernelGenericLoop2D2A...COMPLETE.\n");
	computationKernelGenericLoop2D2AD();
	printf("	- computationKernelGenericLoop2D2AD---COMPLETE.\n");
	computationKernelGenericLoop2D2AFloat();
	printf("	- computationKernelGenericLoop2D2AFloat...COMPLETE.\n");
	computationKernelGenericLoop2D2ADFloat();
	printf("	- computationKernelGenericLoop2D2ADFloat---COMPLETE.\n");
	// ------------------------------------------------------------------ //

	// ------------------------------------------------------------------ //
	// Unstable tests
	// ------------------------------------------------------------------ //
	// Polybench, lu
	//computationKernelPolybenchLu();
	//printf("	- computationKernelPolybenchLu...COMPLETE.\n");

	// Polybench, doitgen
	//computationKernelPolybenchDoitgen();
	//printf("	- computationKernelPolybenchDoitgen...COMPLETE.\n");

	// Generic 3D loop, 2 arrays.
	//computationKernelGenericLoop3D2A();
	//printf("	- computationKernelGenericLoop3D2A---COMPLETE.\n");

	printf("Exit.\n");

	return 0;
}
