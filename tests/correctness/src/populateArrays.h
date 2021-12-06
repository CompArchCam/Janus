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

#include "kernelConfiguration.h"
#include "globalConfiguration.h"

void allocate1DIntArrayD(int** array, long arrayLength);
void allocate1DFloatArrayD(float** array, long arrayLength);
void allocate2DIntArrayD(int*** array, long arrayLength, long arrayWidth);
void allocate2DFloatArrayD(float*** array, long arrayLength, long arrayWidth);


void populate1DIntArrayS(long D1, int array[D1]);
void populate2DIntArrayS(long D1, long D2, int array[D1][D2]);
void populate3DIntArrayS(long D1, long D2, long D3, int array[D1][D2][D3]);
void populate1DIntArrayD(int** array, long arrayLength);
void populate2DIntArrayD(int*** array, long arrayLength, long arrayWidth);

void populate1DFloatArrayS(long D1, float array[D1]);
void populate2DFloatArrayS(long D1, long D2, float[D1][D2]);
void populate3DFloatArrayS(long D1, long D2, long D3, float array[D1][D2][D3]);
void populate1DFloatArrayD(float** array, long arrayLength);
void populate2DFloatArrayD(float*** array, long arrayLength, long arrayWidth);

void print2DIntArrayD(int** array, long arrayLength, long arrayWidth);
void print2DIntArrayS(long D1, long D2, int array[D1][D2]);
void print2DFloatArrayS(long D1, long D2, float array[D1][D2]);
void print3DFloatArrayS(long D1, long D2, long D3, float array[D1][D2][D3]);

long sum1DIntArrayS(long D1, int array[D1]);
long sum2DIntArrayS(long D1, long D2, int array[D1][D2]);
long sum3DIntArrayS(long D1, long D2, long D3, int array[D1][D2][D3]);
long sum1DIntArrayD(int* array, long arrayLength);
long sum2DIntArrayD(int** array, long arrayLength, long arrayWidth);

double sum1DFloatArrayS(long D1, float array[D1]);
double sum2DFloatArrayS(long D1, long D2, float array[D1][D2]);
double sum3DFloatArrayS(long D1, long D2, long D3, float array[D1][D2][D3]);
double sum2DFloatArrayD(float** array, long D1, long D2);
double sum1DFloatArrayD(float* array, long D1);

void write2DIntToFile(int array[ARRAY_LENGTH_D1][ARRAY_LENGTH_D2], long arrayLength, long arrayWidth);

void appendStringToFile(char* fileName, char* text);
