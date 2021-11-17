/* AArch64 specific routines */
#include "janus_arch.h"

static const char *regNameMaps[] =
{
    "NULL", /**< Sentinel value indicating no register", for address modes. */
    "INVALID", /**< Sentinel value indicating an invalid register */

    /* 64-bit general purpose */
    "X0", /**< The "X0" register */
    "X1", /**< The "X1" register */
    "X2", /**< The "X2" register */
    "X3", /**< The "X3" register */
    "X4", /**< The "X4" register */
    "X5", /**< The "X5" register */
    "X6", /**< The "X6" register */
    "X7", /**< The "X7" register */
    "X8", /**< The "X8" register */
    "X9", /**< The "X9" register */
    "X10", /**< The "X10" register */
    "X11", /**< The "X11" register */
    "X12", /**< The "X12" register */
    "X13", /**< The "X13" register */
    "X14", /**< The "X14" register */
    "X15", /**< The "X15" register */
    "X16", /**< The "X16" register */
    "X17", /**< The "X17" register */
    "X18", /**< The "X18" register */
    "X19", /**< The "X19" register */
    "X20", /**< The "X20" register */
    "X21", /**< The "X21" register */
    "X22", /**< The "X22" register */
    "X23", /**< The "X23" register */
    "X24", /**< The "X24" register */
    "X25", /**< The "X25" register */
    "X26", /**< The "X26" register */
    "X27", /**< The "X27" register */
    "X28", /**< The "X28" register */
    "X29", /**< The "X29" register */
    "X30", /**< The "X30" register */

    "SP", /**< The "SP" register */
    "XZR", /**< The "XZR" register */

    /* 32-bit general purpose */
    "W0", /**< The "W0" register */
    "W1", /**< The "W1" register */
    "W2", /**< The "W2" register */
    "W3", /**< The "W3" register */
    "W4", /**< The "W4" register */
    "W5", /**< The "W5" register */
    "W6", /**< The "W6" register */
    "W7", /**< The "W7" register */
    "W8", /**< The "W8" register */
    "W9", /**< The "W9" register */
    "W10", /**< The "W10" register */
    "W11", /**< The "W11" register */
    "W12", /**< The "W12" register */
    "W13", /**< The "W13" register */
    "W14", /**< The "W14" register */
    "W15", /**< The "W15" register */
    "W16", /**< The "W16" register */
    "W17", /**< The "W17" register */
    "W18", /**< The "W18" register */
    "W19", /**< The "W19" register */
    "W20", /**< The "W20" register */
    "W21", /**< The "W21" register */
    "W22", /**< The "W22" register */
    "W23", /**< The "W23" register */
    "W24", /**< The "W24" register */
    "W25", /**< The "W25" register */
    "W26", /**< The "W26" register */
    "W27", /**< The "W27" register */
    "W28", /**< The "W28" register */
    "W29", /**< The "W29" register */
    "W30", /**< The "W30" register */

    "WSP", /**< The "WSP" register */
    "WZR", /**< The "WZR" register */

    /* SIMD registers - note that these are the same 32 registers accessed as different sizes etc */

    /* Quad-word (128 bit) SIMD */
    "Q0", /**< The "Q0" register */
    "Q1", /**< The "Q1" register */
    "Q2", /**< The "Q2" register */
    "Q3", /**< The "Q3" register */
    "Q4", /**< The "Q4" register */
    "Q5", /**< The "Q5" register */
    "Q6", /**< The "Q6" register */
    "Q7", /**< The "Q7" register */
    "Q8", /**< The "Q8" register */
    "Q9", /**< The "Q9" register */
    "Q10", /**< The "Q10" register */
    "Q11", /**< The "Q11" register */
    "Q12", /**< The "Q12" register */
    "Q13", /**< The "Q13" register */
    "Q14", /**< The "Q14" register */
    "Q15", /**< The "Q15" register */
    "Q16", /**< The "Q16" register */
    "Q17", /**< The "Q17" register */
    "Q18", /**< The "Q18" register */
    "Q19", /**< The "Q19" register */
    "Q20", /**< The "Q20" register */
    "Q21", /**< The "Q21" register */
    "Q22", /**< The "Q22" register */
    "Q23", /**< The "Q23" register */
    "Q24", /**< The "Q24" register */
    "Q25", /**< The "Q25" register */
    "Q26", /**< The "Q26" register */
    "Q27", /**< The "Q27" register */
    "Q28", /**< The "Q28" register */
    "Q29", /**< The "Q29" register */
    "Q30", /**< The "Q30" register */
    "Q31", /**< The "Q31" register */

    /* Double-word (64-bit) SIMD */
    "D0", /**< The "D0" register */
    "D1", /**< The "D1" register */
    "D2", /**< The "D2" register */
    "D3", /**< The "D3" register */
    "D4", /**< The "D4" register */
    "D5", /**< The "D5" register */
    "D6", /**< The "D6" register */
    "D7", /**< The "D7" register */
    "D8", /**< The "D8" register */
    "D9", /**< The "D9" register */
    "D10", /**< The "D10" register */
    "D11", /**< The "D11" register */
    "D12", /**< The "D12" register */
    "D13", /**< The "D13" register */
    "D14", /**< The "D14" register */
    "D15", /**< The "D15" register */
    "D16", /**< The "D16" register */
    "D17", /**< The "D17" register */
    "D18", /**< The "D18" register */
    "D19", /**< The "D19" register */
    "D20", /**< The "D20" register */
    "D21", /**< The "D21" register */
    "D22", /**< The "D22" register */
    "D23", /**< The "D23" register */
    "D24", /**< The "D24" register */
    "D25", /**< The "D25" register */
    "D26", /**< The "D26" register */
    "D27", /**< The "D27" register */
    "D28", /**< The "D28" register */
    "D29", /**< The "D29" register */
    "D30", /**< The "D30" register */
    "D31", /**< The "D31" register */

    /* Single-word (32-bit) SIMD */
    "S0", /**< The "S0" register */
    "S1", /**< The "S1" register */
    "S2", /**< The "S2" register */
    "S3", /**< The "S3" register */
    "S4", /**< The "S4" register */
    "S5", /**< The "S5" register */
    "S6", /**< The "S6" register */
    "S7", /**< The "S7" register */
    "S8", /**< The "S8" register */
    "S9", /**< The "S9" register */
    "S10", /**< The "S10" register */
    "S11", /**< The "S11" register */
    "S12", /**< The "S12" register */
    "S13", /**< The "S13" register */
    "S14", /**< The "S14" register */
    "S15", /**< The "S15" register */
    "S16", /**< The "S16" register */
    "S17", /**< The "S17" register */
    "S18", /**< The "S18" register */
    "S19", /**< The "S19" register */
    "S20", /**< The "S20" register */
    "S21", /**< The "S21" register */
    "S22", /**< The "S22" register */
    "S23", /**< The "S23" register */
    "S24", /**< The "S24" register */
    "S25", /**< The "S25" register */
    "S26", /**< The "S26" register */
    "S27", /**< The "S27" register */
    "S28", /**< The "S28" register */
    "S29", /**< The "S29" register */
    "S30", /**< The "S30" register */
    "S31", /**< The "S31" register */

    /* Half-word (16-bit) SIMD */
    "H0", /**< The "H0" register */
    "H1", /**< The "H1" register */
    "H2", /**< The "H2" register */
    "H3", /**< The "H3" register */
    "H4", /**< The "H4" register */
    "H5", /**< The "H5" register */
    "H6", /**< The "H6" register */
    "H7", /**< The "H7" register */
    "H8", /**< The "H8" register */
    "H9", /**< The "H9" register */
    "H10", /**< The "H10" register */
    "H11", /**< The "H11" register */
    "H12", /**< The "H12" register */
    "H13", /**< The "H13" register */
    "H14", /**< The "H14" register */
    "H15", /**< The "H15" register */
    "H16", /**< The "H16" register */
    "H17", /**< The "H17" register */
    "H18", /**< The "H18" register */
    "H19", /**< The "H19" register */
    "H20", /**< The "H20" register */
    "H21", /**< The "H21" register */
    "H22", /**< The "H22" register */
    "H23", /**< The "H23" register */
    "H24", /**< The "H24" register */
    "H25", /**< The "H25" register */
    "H26", /**< The "H26" register */
    "H27", /**< The "H27" register */
    "H28", /**< The "H28" register */
    "H29", /**< The "H29" register */
    "H30", /**< The "H30" register */
    "H31", /**< The "H31" register */

    /* Byte (8-bit) SIMD */
    "B0", /**< The "B0" register */
    "B1", /**< The "B1" register */
    "B2", /**< The "B2" register */
    "B3", /**< The "B3" register */
    "B4", /**< The "B4" register */
    "B5", /**< The "B5" register */
    "B6", /**< The "B6" register */
    "B7", /**< The "B7" register */
    "B8", /**< The "B8" register */
    "B9", /**< The "B9" register */
    "B10", /**< The "B10" register */
    "B11", /**< The "B11" register */
    "B12", /**< The "B12" register */
    "B13", /**< The "B13" register */
    "B14", /**< The "B14" register */
    "B15", /**< The "B15" register */
    "B16", /**< The "B16" register */
    "B17", /**< The "B17" register */
    "B18", /**< The "B18" register */
    "B19", /**< The "B19" register */
    "B20", /**< The "B20" register */
    "B21", /**< The "B21" register */
    "B22", /**< The "B22" register */
    "B23", /**< The "B23" register */
    "B24", /**< The "B24" register */
    "B25", /**< The "B25" register */
    "B26", /**< The "B26" register */
    "B27", /**< The "B27" register */
    "B28", /**< The "B28" register */
    "B29", /**< The "B29" register */
    "B30", /**< The "B30" register */
    "B31", /**< The "B31" register */

    /* PSTATE condition flags */
    "NZCV", /**< The "NZCV" register */
    "FPCR", /**< The "FPCR" register */
    "FPSR", /**< The "FPSR" register */

    /* SIMD vector registers */
    "V0", /**< The "V0" register */
    "V1", /**< The "V1" register */
    "V2", /**< The "V2" register */
    "V3", /**< The "V3" register */
    "V4", /**< The "V4" register */
    "V5", /**< The "V5" register */
    "V6", /**< The "V6" register */
    "V7", /**< The "V7" register */
    "V8", /**< The "V8" register */
    "V9", /**< The "V9" register */
    "V10", /**< The "V10" register */
    "V11", /**< The "V11" register */
    "V12", /**< The "V12" register */
    "V13", /**< The "V13" register */
    "V14", /**< The "V14" register */
    "V15", /**< The "V15" register */
    "V16", /**< The "V16" register */
    "V17", /**< The "V17" register */
    "V18", /**< The "V18" register */
    "V19", /**< The "V19" register */
    "V20", /**< The "V20" register */
    "V21", /**< The "V21" register */
    "V22", /**< The "V22" register */
    "V23", /**< The "V23" register */
    "V24", /**< The "V24" register */
    "V25", /**< The "V25" register */
    "V26", /**< The "V26" register */
    "V27", /**< The "V27" register */
    "V28", /**< The "V28" register */
    "V29", /**< The "V29" register */
    "V30", /**< The "V30" register */
    "V31", /**< The "V31" register */
};

static const char *shiftNameMaps[] =
{
    "INVALID",
    "LSL",
    "MSL",
    "LSR",
    "ASR",
    "ROR",
    "INVALID",
};

static const uint64_t regBitMaps[] =
{
    0, /**< Sentinel value indicating no register", for address modes. */
    0, /**< Sentinel value indicating an invalid register */

    /* 64-bit general purpose */
    1L, /**< The "X0" register */
    1L<<1, /**< The "X1" register */
    1L<<2, /**< The "X2" register */
    1L<<3, /**< The "X3" register */
    1L<<4, /**< The "X4" register */
    1L<<5, /**< The "X5" register */
    1L<<6, /**< The "X6" register */
    1L<<7, /**< The "X7" register */
    1L<<8, /**< The "X8" register */
    1L<<9, /**< The "X9" register */
    1L<<10, /**< The "X10" register */
    1L<<11, /**< The "X11" register */
    1L<<12, /**< The "X12" register */
    1L<<13, /**< The "X13" register */
    1L<<14, /**< The "X14" register */
    1L<<15, /**< The "X15" register */
    1L<<16, /**< The "X16" register */
    1L<<17, /**< The "X17" register */
    1L<<18, /**< The "X18" register */
    1L<<19, /**< The "X19" register */
    1L<<20, /**< The "X20" register */
    1L<<21, /**< The "X21" register */
    1L<<22, /**< The "X22" register */
    1L<<23, /**< The "X23" register */
    1L<<24, /**< The "X24" register */
    1L<<25, /**< The "X25" register */
    1L<<26, /**< The "X26" register */
    1L<<27, /**< The "X27" register */
    1L<<28, /**< The "X28" register */
    1L<<29, /**< The "X29" register */
    1L<<30, /**< The "X30" register */

    1L<<31, /**< The "SP" register */
    0, /**< The "XZR" register */

    /* 32-bit general purpose */
    1L, /**< The "W0" register */
    1L<<1, /**< The "W1" register */
    1L<<2, /**< The "W2" register */
    1L<<3, /**< The "W3" register */
    1L<<4, /**< The "W4" register */
    1L<<5, /**< The "W5" register */
    1L<<6, /**< The "W6" register */
    1L<<7, /**< The "W7" register */
    1L<<8, /**< The "W8" register */
    1L<<9, /**< The "W9" register */
    1L<<10, /**< The "W10" register */
    1L<<11, /**< The "W11" register */
    1L<<12, /**< The "W12" register */
    1L<<13, /**< The "W13" register */
    1L<<14, /**< The "W14" register */
    1L<<15, /**< The "W15" register */
    1L<<16, /**< The "W16" register */
    1L<<17, /**< The "W17" register */
    1L<<18, /**< The "W18" register */
    1L<<19, /**< The "W19" register */
    1L<<20, /**< The "W20" register */
    1L<<21, /**< The "W21" register */
    1L<<22, /**< The "W22" register */
    1L<<23, /**< The "W23" register */
    1L<<24, /**< The "W24" register */
    1L<<25, /**< The "W25" register */
    1L<<26, /**< The "W26" register */
    1L<<27, /**< The "W27" register */
    1L<<28, /**< The "W28" register */
    1L<<29, /**< The "W29" register */
    1L<<30, /**< The "W30" register */

    1L<<31, /**< The "WSP" register */
    0, /**< The WZR register */

    /* SIMD registers - note that these are the same 32 registers accessed as different sizes etc */

    /* Quad-word (128 bit) SIMD */
    1L<<32, /**< The "Q0" register */
    1L<<33, /**< The "Q1" register */
    1L<<34, /**< The "Q2" register */
    1L<<35, /**< The "Q3" register */
    1L<<36, /**< The "Q4" register */
    1L<<37, /**< The "Q5" register */
    1L<<38, /**< The "Q6" register */
    1L<<39, /**< The "Q7" register */
    1L<<40, /**< The "Q8" register */
    1L<<41, /**< The "Q9" register */
    1L<<42, /**< The "Q10" register */
    1L<<43, /**< The "Q11" register */
    1L<<44, /**< The "Q12" register */
    1L<<45, /**< The "Q13" register */
    1L<<46, /**< The "Q14" register */
    1L<<47, /**< The "Q15" register */
    1L<<48, /**< The "Q16" register */
    1L<<49, /**< The "Q17" register */
    1L<<50, /**< The "Q18" register */
    1L<<51, /**< The "Q19" register */
    1L<<52, /**< The "Q20" register */
    1L<<53, /**< The "Q21" register */
    1L<<54, /**< The "Q22" register */
    1L<<55, /**< The "Q23" register */
    1L<<56, /**< The "Q24" register */
    1L<<57, /**< The "Q25" register */
    1L<<58, /**< The "Q26" register */
    1L<<59, /**< The "Q27" register */
    1L<<60, /**< The "Q28" register */
    1L<<61, /**< The "Q29" register */
    1L<<62, /**< The "Q30" register */
    1L<<63, /**< The "Q31" register */

    /* Double-word (64-bit) SIMD */
    1L<<32, /**< The "D0" register */
    1L<<33, /**< The "D1" register */
    1L<<34, /**< The "D2" register */
    1L<<35, /**< The "D3" register */
    1L<<36, /**< The "D4" register */
    1L<<37, /**< The "D5" register */
    1L<<38, /**< The "D6" register */
    1L<<39, /**< The "D7" register */
    1L<<40, /**< The "D8" register */
    1L<<41, /**< The "D9" register */
    1L<<42, /**< The "D10" register */
    1L<<43, /**< The "D11" register */
    1L<<44, /**< The "D12" register */
    1L<<45, /**< The "D13" register */
    1L<<46, /**< The "D14" register */
    1L<<47, /**< The "D15" register */
    1L<<48, /**< The "D16" register */
    1L<<49, /**< The "D17" register */
    1L<<50, /**< The "D18" register */
    1L<<51, /**< The "D19" register */
    1L<<52, /**< The "D20" register */
    1L<<53, /**< The "D21" register */
    1L<<54, /**< The "D22" register */
    1L<<55, /**< The "D23" register */
    1L<<56, /**< The "D24" register */
    1L<<57, /**< The "D25" register */
    1L<<58, /**< The "D26" register */
    1L<<59, /**< The "D27" register */
    1L<<60, /**< The "D28" register */
    1L<<61, /**< The "D29" register */
    1L<<62, /**< The "D30" register */
    1L<<63, /**< The "D31" register */

    /* Single-word (32-bit) SIMD */
    1L<<32, /**< The "S0" register */
    1L<<33, /**< The "S1" register */
    1L<<34, /**< The "S2" register */
    1L<<35, /**< The "S3" register */
    1L<<36, /**< The "S4" register */
    1L<<37, /**< The "S5" register */
    1L<<38, /**< The "S6" register */
    1L<<39, /**< The "S7" register */
    1L<<40, /**< The "S8" register */
    1L<<41, /**< The "S9" register */
    1L<<42, /**< The "S10" register */
    1L<<43, /**< The "S11" register */
    1L<<44, /**< The "S12" register */
    1L<<45, /**< The "S13" register */
    1L<<46, /**< The "S14" register */
    1L<<47, /**< The "S15" register */
    1L<<48, /**< The "S16" register */
    1L<<49, /**< The "S17" register */
    1L<<50, /**< The "S18" register */
    1L<<51, /**< The "S19" register */
    1L<<52, /**< The "S20" register */
    1L<<53, /**< The "S21" register */
    1L<<54, /**< The "S22" register */
    1L<<55, /**< The "S23" register */
    1L<<56, /**< The "S24" register */
    1L<<57, /**< The "S25" register */
    1L<<58, /**< The "S26" register */
    1L<<59, /**< The "S27" register */
    1L<<60, /**< The "S28" register */
    1L<<61, /**< The "S29" register */
    1L<<62, /**< The "S30" register */
    1L<<63, /**< The "S31" register */

    /* Half-word (16-bit) SIMD */
    1L<<32, /**< The "H0" register */
    1L<<33, /**< The "H1" register */
    1L<<34, /**< The "H2" register */
    1L<<35, /**< The "H3" register */
    1L<<36, /**< The "H4" register */
    1L<<37, /**< The "H5" register */
    1L<<38, /**< The "H6" register */
    1L<<39, /**< The "H7" register */
    1L<<40, /**< The "H8" register */
    1L<<41, /**< The "H9" register */
    1L<<42, /**< The "H10" register */
    1L<<43, /**< The "H11" register */
    1L<<44, /**< The "H12" register */
    1L<<45, /**< The "H13" register */
    1L<<46, /**< The "H14" register */
    1L<<47, /**< The "H15" register */
    1L<<48, /**< The "H16" register */
    1L<<49, /**< The "H17" register */
    1L<<50, /**< The "H18" register */
    1L<<51, /**< The "H19" register */
    1L<<52, /**< The "H20" register */
    1L<<53, /**< The "H21" register */
    1L<<54, /**< The "H22" register */
    1L<<55, /**< The "H23" register */
    1L<<56, /**< The "H24" register */
    1L<<57, /**< The "H25" register */
    1L<<58, /**< The "H26" register */
    1L<<59, /**< The "H27" register */
    1L<<60, /**< The "H28" register */
    1L<<61, /**< The "H29" register */
    1L<<62, /**< The "H30" register */
    1L<<63, /**< The "H31" register */

    /* Byte (8-bit) SIMD */
    1L<<32, /**< The "B0" register */
    1L<<33, /**< The "B1" register */
    1L<<34, /**< The "B2" register */
    1L<<35, /**< The "B3" register */
    1L<<36, /**< The "B4" register */
    1L<<37, /**< The "B5" register */
    1L<<38, /**< The "B6" register */
    1L<<39, /**< The "B7" register */
    1L<<40, /**< The "B8" register */
    1L<<41, /**< The "B9" register */
    1L<<42, /**< The "B10" register */
    1L<<43, /**< The "B11" register */
    1L<<44, /**< The "B12" register */
    1L<<45, /**< The "B13" register */
    1L<<46, /**< The "B14" register */
    1L<<47, /**< The "B15" register */
    1L<<48, /**< The "B16" register */
    1L<<49, /**< The "B17" register */
    1L<<50, /**< The "B18" register */
    1L<<51, /**< The "B19" register */
    1L<<52, /**< The "B20" register */
    1L<<53, /**< The "B21" register */
    1L<<54, /**< The "B22" register */
    1L<<55, /**< The "B23" register */
    1L<<56, /**< The "B24" register */
    1L<<57, /**< The "B25" register */
    1L<<58, /**< The "B26" register */
    1L<<59, /**< The "B27" register */
    1L<<60, /**< The "B28" register */
    1L<<61, /**< The "B29" register */
    1L<<62, /**< The "B30" register */
    1L<<63, /**< The "B31" register */

    /* PSTATE condition flags */
    0, /**< The "NZCV" register */
    0, /**< The "FPCR" register */
    0, /**< The "FPSR" register */

    /* SIMD vector registers */
    1L<<32, /**< The "V0" register */
    1L<<33, /**< The "V1" register */
    1L<<34, /**< The "V2" register */
    1L<<35, /**< The "V3" register */
    1L<<36, /**< The "V4" register */
    1L<<37, /**< The "V5" register */
    1L<<38, /**< The "V6" register */
    1L<<39, /**< The "V7" register */
    1L<<40, /**< The "V8" register */
    1L<<41, /**< The "V9" register */
    1L<<42, /**< The "V10" register */
    1L<<43, /**< The "V11" register */
    1L<<44, /**< The "V12" register */
    1L<<45, /**< The "V13" register */
    1L<<46, /**< The "V14" register */
    1L<<47, /**< The "V15" register */
    1L<<48, /**< The "V16" register */
    1L<<49, /**< The "V17" register */
    1L<<50, /**< The "V18" register */
    1L<<51, /**< The "V19" register */
    1L<<52, /**< The "V20" register */
    1L<<53, /**< The "V21" register */
    1L<<54, /**< The "V22" register */
    1L<<55, /**< The "V23" register */
    1L<<56, /**< The "V24" register */
    1L<<57, /**< The "V25" register */
    1L<<58, /**< The "V26" register */
    1L<<59, /**< The "V27" register */
    1L<<60, /**< The "V28" register */
    1L<<61, /**< The "V29" register */
    1L<<62, /**< The "V30" register */
    1L<<63, /**< The "V31" register */
};

///Return the string of the register name
const char *get_reg_name(int reg)
{
    if (reg > JREG_V31) return "NULL";
    return regNameMaps[reg];
}

const char *get_shift_name(uint8_t shift)
{
    if(shift >= JSHFT_END) return shiftNameMaps[JSHFT_END];
    else return shiftNameMaps[shift];
}

///Return the size of given register
int get_reg_size(int reg)
{
    int size = 0;
    switch(reg)
    {
        case JREG_NULL:
            size = 0;
        break;
        case JREG_SP:
            size = 8;
        break;
        case JREG_WSP:
            size = 4;
        break;
        case JREG_NZCV:
            size = 8; /* Warning - this is just a guess - couldn't find any info anywhere */
        break;
        case JREG_XZR:
            size = 8;
        break;
        case JREG_WZR:
            size = 4;
        break;
        case JREG_INVALID:
            size = 0;
        break;

        default:
            if(reg >= JREG_X0 && reg <= JREG_X30)       size = 8;
            else if(reg >= JREG_W0 && reg <= JREG_W30)  size = 4;
            else if(reg >= JREG_Q0 && reg <= JREG_Q31)  size = 16;
            else if(reg >= JREG_D0 && reg <= JREG_D31)  size = 8;
            else if(reg >= JREG_S0 && reg <= JREG_S31)  size = 4;
            else if(reg >= JREG_H0 && reg <= JREG_H31)  size = 2;
            else if(reg >= JREG_B0 && reg <= JREG_B31)  size = 1;
            else size = 8; // That's a sensible default, right?

            /* See enum in janus-aarch64.h for details of registers and sizes */

        break;
    }
}

///Return the bit representation of a given register
uint64_t get_reg_bit_array(int reg)
{
    if (reg > JREG_V31) return JREG_INVALID;
    return regBitMaps[reg];
}

///Return the register id of the full size of the given register
uint32_t get_full_reg_id(uint32_t id)
{
    if (id >= JREG_W0 && id <= JREG_W30)
        id = id - JREG_W0 + JREG_X0;
    else if (id >= JREG_D0 && id <= JREG_D30)
        id = id - JREG_D0 + JREG_Q0;
    else if (id >= JREG_S0 && id <= JREG_S30)
        id = id - JREG_S0 + JREG_Q0;
    else if (id >= JREG_H0 && id <= JREG_H30)
        id = id - JREG_H0 + JREG_Q0;
    else if (id >= JREG_B0 && id <= JREG_B30)
        id = id - JREG_B0 + JREG_Q0;
    else if (id == JREG_WSP)
        id = JREG_SP;
    else if (id == JREG_WZR)
        id = JREG_XZR;

    return id;
}

int jreg_is_gpr(int reg)
{
    if (reg == JREG_X29) return 0;
    if (reg >= JREG_X0 && reg <= JREG_X30) return 1;
    else return 0;
}

int jreg_is_simd(int reg)
{
    if (reg >= JREG_Q0 && reg <= JREG_V31) return 1;
    else return 0;
}
