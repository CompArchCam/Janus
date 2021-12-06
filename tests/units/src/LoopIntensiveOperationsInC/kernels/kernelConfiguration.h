/**
 * Kernel configuration
 */
// - Polybench, cholesky
// - POLY_CHOLESKY_ARRAY_001_LENGTH_D2 must be higher than POLY_CHOLESKY_ARRAY_001_LENGTH_D1
// - The dimensions should be equal.
#define POLY_CHOLESKY_ARRAY_001_LENGTH_D1 2000
#define POLY_CHOLESKY_ARRAY_001_LENGTH_D2 2000

// - Results:
struct POLY_CHOLESKY_RESULT
{
	long initArraySum;
	long modifiedArraySum;
} poly_cholesky_result;

struct POLY_CHOLESKY_FLOAT_RESULT
{
	double initArraySum;
	double modifiedArraySum;
} poly_cholesky_float_result;
// - Results:
struct POLY_CHOLESKY_D_RESULT
{
	long initArraySum;
	long modifiedArraySum;
} poly_cholesky_d_result;

struct POLY_CHOLESKY_D_FLOAT_RESULT
{
	double initArraySum;
	double modifiedArraySum;
} poly_cholesky_d_float_result;


// - Polybench, lu
// Stack arrays.
#define POLY_LU_ARRAY_001_LENGTH_D1 2000
#define POLY_LU_ARRAY_001_LENGTH_D2 2000
// - Results:
struct POLY_LU_RESULT
{
	long initArraySum;
	long modifiedArraySum;
} poly_lu_result;

struct POLY_LU_FLOAT_RESULT
{
	double initArraySum;
	double modifiedArraySum;
} poly_lu_float_result;


// - Polybench, 2mm
#define POLY_2MM_ARRAY_001_LENGTH_D1 1000
#define POLY_2MM_ARRAY_001_LENGTH_D2 1100

#define POLY_2MM_ARRAY_002_LENGTH_D1 1000
#define POLY_2MM_ARRAY_002_LENGTH_D2 1200

#define POLY_2MM_ARRAY_003_LENGTH_D1 1200
#define POLY_2MM_ARRAY_003_LENGTH_D2 1000

#define POLY_2MM_ARRAY_004_LENGTH_D1 1000
#define POLY_2MM_ARRAY_004_LENGTH_D2 1300

#define POLY_2MM_ARRAY_005_LENGTH_D1 1000
#define POLY_2MM_ARRAY_005_LENGTH_D2 1300
// - Results:
struct POLY_2MM_RESULT
{
	long initArray1Sum;
	long initArray2Sum;
	long initArray3Sum;
	long initArray4Sum;
	long initArray5Sum;

	long modifiedArray1Sum;
	long modifiedArray2Sum;
	long modifiedArray3Sum;
	long modifiedArray4Sum;
	long modifiedArray5Sum;
} poly_2mm_result;

struct POLY_2MM_FLOAT_RESULT
{
	double initArray1Sum;
	double initArray2Sum;
	double initArray3Sum;
	double initArray4Sum;
	double initArray5Sum;

	double modifiedArray1Sum;
	double modifiedArray2Sum;
	double modifiedArray3Sum;
	double modifiedArray4Sum;
	double modifiedArray5Sum;
} poly_2mm_float_result;

// - Polybench, 3mm
#define POLY_3MM_ARRAY_001_LENGTH_D1 1100
#define POLY_3MM_ARRAY_001_LENGTH_D2 1200

#define POLY_3MM_ARRAY_002_LENGTH_D1 1400
#define POLY_3MM_ARRAY_002_LENGTH_D2 1200

#define POLY_3MM_ARRAY_003_LENGTH_D1 1300
#define POLY_3MM_ARRAY_003_LENGTH_D2 1500

#define POLY_3MM_ARRAY_004_LENGTH_D1 1500
#define POLY_3MM_ARRAY_004_LENGTH_D2 1400

#define POLY_3MM_ARRAY_005_LENGTH_D1 1100
#define POLY_3MM_ARRAY_005_LENGTH_D2 1200

#define POLY_3MM_ARRAY_006_LENGTH_D1 1200
#define POLY_3MM_ARRAY_006_LENGTH_D2 1300
// - Results:
struct POLY_3MM_RESULT
{
	long initArray1Sum;
	long initArray2Sum;
	long initArray3Sum;
	long initArray4Sum;
	long initArray5Sum;
	long initArray6Sum;

	long modifiedArray1Sum;
	long modifiedArray2Sum;
	long modifiedArray3Sum;
	long modifiedArray4Sum;
	long modifiedArray5Sum;
	long modifiedArray6Sum;
} poly_3mm_result;

struct POLY_3MM_FLOAT_RESULT
{
	double initArray1Sum;
	double initArray2Sum;
	double initArray3Sum;
	double initArray4Sum;
	double initArray5Sum;
	double initArray6Sum;

	double modifiedArray1Sum;
	double modifiedArray2Sum;
	double modifiedArray3Sum;
	double modifiedArray4Sum;
	double modifiedArray5Sum;
	double modifiedArray6Sum;
} poly_3mm_float_result;

// - Polybench, doitgen
#define POLY_DOITGEN_ARRAY_001_LENGTH_D1 200

#define POLY_DOITGEN_ARRAY_002_LENGTH_D1 220
#define POLY_DOITGEN_ARRAY_002_LENGTH_D2 220

#define POLY_DOITGEN_ARRAY_003_LENGTH_D1 200
#define POLY_DOITGEN_ARRAY_003_LENGTH_D2 180
#define POLY_DOITGEN_ARRAY_003_LENGTH_D3 220
// - Results:
struct POLY_DOITGEN_RESULT
{
	long initSumSum;
	long initC4Sum;
	long initASum;

	long modifiedSumSum;
	long modifiedC4Sum;
	long modifiedASum;
} poly_doitgen_result;

struct POLY_DOITGEN_FLOAT_RESULT
{
	double initSumSum;
	double initC4Sum;
	double initASum;

	double modifiedSumSum;
	double modifiedC4Sum;
	double modifiedASum;
} poly_doitgen_float_result;

// - Polybench, gramschmidt
#define POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D1 1000
#define POLY_GRAMSCHMIDT_ARRAY_001_LENGTH_D2 1200

#define POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D1 1200
#define POLY_GRAMSCHMIDT_ARRAY_002_LENGTH_D2 1200

#define POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D1 1000
#define POLY_GRAMSCHMIDT_ARRAY_003_LENGTH_D2 1200
// - Results:
struct POLY_GRAMSCHMIDT_RESULT
{
	long initArray1Sum;
	long initArray2Sum;
	long initArray3Sum;

	long modifiedArray1Sum;
	long modifiedArray2Sum;
	long modifiedArray3Sum;
} poly_gramschmidt_result;
// - Results:
struct POLY_GRAMSCHMIDT_FLOAT_RESULT
{
	double initArray1Sum;
	double initArray2Sum;
	double initArray3Sum;

	double modifiedArray1Sum;
	double modifiedArray2Sum;
	double modifiedArray3Sum;
} poly_gramschmidt_float_result;
// - Results:
struct POLY_GRAMSCHMIDT_D_RESULT
{
	long initArray1Sum;
	long initArray2Sum;
	long initArray3Sum;

	long modifiedArray1Sum;
	long modifiedArray2Sum;
	long modifiedArray3Sum;
} poly_gramschmidt_d_result;
// - Results:
struct POLY_GRAMSCHMIDT_D_FLOAT_RESULT
{
	double initArray1Sum;
	double initArray2Sum;
	double initArray3Sum;

	double modifiedArray1Sum;
	double modifiedArray2Sum;
	double modifiedArray3Sum;
} poly_gramschmidt_d_float_result;

// - Polybench, tislov
#define POLY_TISLOV_ARRAY_001_LENGTH_D1 5000

#define POLY_TISLOV_ARRAY_002_LENGTH_D1 5000

#define POLY_TISLOV_ARRAY_003_LENGTH_D1 5000
#define POLY_TISLOV_ARRAY_003_LENGTH_D2 5000

// - Results:
struct POLY_TISLOV_RESULT
{
	long initBSum;
	long initXSum;
	long initLSum;

	long modifiedBSum;
	long modifiedXSum;
	long modifiedLSum;
} poly_tislov_result;
// - Results:
struct POLY_TISLOV_FLOAT_RESULT
{
	double initBSum;
	double initXSum;
	double initLSum;

	double modifiedBSum;
	double modifiedXSum;
	double modifiedLSum;
} poly_tislov_float_result;

// - Generic arrays
#define GENERIC_ARRAY_001_D1 100
#define GENERIC_ARRAY_001_D2 100
#define GENERIC_ARRAY_001_D3 100

#define GENERIC_ARRAY_002_D1 100
#define GENERIC_ARRAY_002_D2 100
#define GENERIC_ARRAY_002_D3 100

// -- Arrays on heap
#define GENERIC_ARRAY_D_001_D1 1000
#define GENERIC_ARRAY_D_001_D2 1000
#define GENERIC_ARRAY_D_001_D3 100

#define GENERIC_ARRAY_D_002_D1 100
#define GENERIC_ARRAY_D_002_D2 100
#define GENERIC_ARRAY_D_002_D3 100
// - Results:
struct GENERIC_1D2A_RESULT
{
	long initArray1Sum;
	long initArray2Sum;

	long modifiedArray1Sum;
	long modifiedArray2Sum;
} genericLoop1D2A_result;
// - Results:
struct GENERIC_1D2A_FLOAT_RESULT
{
	double initArray1Sum;
	double initArray2Sum;

	double modifiedArray1Sum;
	double modifiedArray2Sum;
} genericLoop1D2A_float_result;
// - Results:
struct GENERIC_1D2AD_RESULT
{
	long initArray1Sum;
	long initArray2Sum;

	long modifiedArray1Sum;
	long modifiedArray2Sum;
} genericLoop1D2AD_result;
// - Results:
struct GENERIC_1D2AD_FLOAT_RESULT
{
	double initArray1Sum;
	double initArray2Sum;

	double modifiedArray1Sum;
	double modifiedArray2Sum;
} genericLoop1D2AD_float_result;

struct GENERIC_2D2A_RESULT
{
	long initArray1Sum;
	long initArray2Sum;

	long modifiedArray1Sum;
	long modifiedArray2Sum;
} genericLoop2D2A_result;

struct GENERIC_2D2A_FLOAT_RESULT
{
	double initArray1Sum;
	double initArray2Sum;

	double modifiedArray1Sum;
	double modifiedArray2Sum;
} genericLoop2D2A_float_result;

struct GENERIC_2D2AD_RESULT
{
	long initArray1Sum;
	long initArray2Sum;

	long modifiedArray1Sum;
	long modifiedArray2Sum;
} genericLoop2D2AD_result;

struct GENERIC_2D2AD_FLOAT_RESULT
{
	double initArray1Sum;
	double initArray2Sum;

	double modifiedArray1Sum;
	double modifiedArray2Sum;
} genericLoop2D2AD_float_result;

struct GENERIC_3D2A_RESULT
{
	long initArray1Sum;
	long initArray2Sum;

	long modifiedArray1Sum;
	long modifiedArray2Sum;
} genericLoop3D2A_result;

struct GENERIC_3D2A_FLOAT_RESULT
{
	double initArray1Sum;
	double initArray2Sum;

	double modifiedArray1Sum;
	double modifiedArray2Sum;
} genericLoop3D2A_float_result;
