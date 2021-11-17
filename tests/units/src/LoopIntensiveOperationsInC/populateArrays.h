/**
 * Functions to allocate and populate arrays.
 * @author: Jahic
 * 1D - one dimensional array.
 * 2D - two dimensional array.
 * 3D - three dimensional array.
 *
 * D - Dynamically allocated memory for array
 * S - Stack array
 *
 */

#include "kernels/kernelConfiguration.h"
#include "globalConfiguration.h"

void allocate1DIntArrayD(int** array, long arrayLength);
void allocate1DFloatArrayD(float** array, long arrayLength);
void allocate2DIntArrayD(int*** array, long arrayLength, long arrayWidth);
void allocate2DFloatArrayD(float*** array, long arrayLength, long arrayWidth);


void populate1DIntArrayS(int array[ARRAY_LENGTH_D1], long d1);
void populate1DFloatArrayS(float array[ARRAY_LENGTH_D1], long d1);
void populate2DIntArrayS(int array[ARRAY_LENGTH_D1][ARRAY_LENGTH_D2], long arrayLength, long arrayWidth);
void populate2DFloatArrayS(float[ARRAY_LENGTH_D1][ARRAY_LENGTH_D2], long arrayLength, long arrayWidth);
void populate3DIntArrayS(int array[ARRAY_LENGTH_D1][ARRAY_LENGTH_D2][ARRAY_LENGTH_D3], long D1, long D2, long D3);
void populate3DFloatArrayS(float array[ARRAY_LENGTH_D1][ARRAY_LENGTH_D2][ARRAY_LENGTH_D3], long D1, long D2, long D3);
void populate1DIntArrayD(int** array, long arrayLength);
void populate1DFloatArrayD(float** array, long arrayLength);
void populate2DIntArrayD(int*** array, long arrayLength, long arrayWidth);
void populate2DFloatArrayD(float*** array, long arrayLength, long arrayWidth);

void print2DIntArrayD(int** array, long arrayLength, long arrayWidth);
void print2DFloatArrayS(float array[ARRAY_LENGTH_D1][ARRAY_LENGTH_D2], long arrayLength, long arrayWidth);

long sum1DIntArrayS(int array[ARRAY_LENGTH_D1], long arrayLength);
long sum1DIntArrayD(int* array, long arrayLength);
long sum2DIntArrayD(int** array, long arrayLength, long arrayWidth);
long sum2DIntArrayS(int array[ARRAY_LENGTH_D1][ARRAY_LENGTH_D2], long D1, long D2);
long sum3DIntArrayS(int array[ARRAY_LENGTH_D1][ARRAY_LENGTH_D2][ARRAY_LENGTH_D3], long D1, long D2, long D3);

double sum1DFloatArrayS(float array[ARRAY_LENGTH_D1], long arrayLength);
double sum1DFloatArrayD(float* array, long arrayLength);
double sum2DFloatArrayS(float array[ARRAY_LENGTH_D1][ARRAY_LENGTH_D2], long arrayLength, long arrayWidth);
double sum2DFloatArrayD(float** array, long arrayLength, long arrayWidth);
double sum3DFloatArrayS(float array[ARRAY_LENGTH_D1][ARRAY_LENGTH_D2][ARRAY_LENGTH_D3], long D1, long D2, long D3);

void write2DIntToFile(int array[ARRAY_LENGTH_D1][ARRAY_LENGTH_D2], long arrayLength, long arrayWidth);

void appendStringToFile(char* fileName, char* text);
