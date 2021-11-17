/**
 * Computation intensive functionalities.
 * Patterns of high-computing applications.
 *
 */

#include <stdio.h>

//#define LOG_RESULTS
//#define LOG_RESULTS_BINARY
//#define LOG_RESULTS_JPAR

#ifdef LOG_RESULTS
FILE *logFile;
#endif /* MACRO */

// Polybench, cholesky
// - Arrays on stack.
void computationKernelPolybenchCholesky();
void computationKernelPolybenchCholeskyFloat();
// - Dynamic allocation of memory for the arrays.
void computationKernelPolybenchCholeskyD();
void computationKernelPolybenchCholeskyDFloat();

// Polybench, lu
void computationKernelPolybenchLu();
void computationKernelPolybenchLuFloat();

// Polybench, 2mm
void computationKernelPolybench2mm();
void computationKernelPolybench2mmFloat();

// Polybench, 3mm
void computationKernelPolybench3mm();
void computationKernelPolybench3mmFloat();

// Polybench, doitgen
void computationKernelPolybenchDoitgen();
void computationKernelPolybenchDoitgenFloat();

// Polybench, gramschmidt
// - Arrays on stack.
void computationKernelPolybenchGramschmidt();
void computationKernelPolybenchGramschmidtFloat();
// - Dynamically allocated arrays.
void computationKernelPolybenchGramschmidtD();
void computationKernelPolybenchGramschmidtDFloat();

// Polybench, tislov
void computationKernelPolybenchTislov();
void computationKernelPolybenchTislovFloat();

// Generic 1D loop, 2 arrays.
// - Arrays on stack.
void computationKernelGenericLoop1D2A();
void computationKernelGenericLoop1D2AFloat();
// - Dynamic allocation of arrays.
void computationKernelGenericLoop1D2AD();
void computationKernelGenericLoop1D2ADFloat();

// Generic 2D loop, 2 arrays.
// - Arrays on stack.
void computationKernelGenericLoop2D2A();
void computationKernelGenericLoop2D2AFloat();
// - Dynamic allocation of arrays.
void computationKernelGenericLoop2D2AD();
void computationKernelGenericLoop2D2ADFloat();

// Generic 3D loop, 2 arrays.
// - Arrays on stack.
void computationKernelGenericLoop3D2A();
void computationKernelGenericLoop3D2AFloat();
