#ifndef _Janus_AARCH64_
#define _Janus_AARCH64_

#include "janus.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Register mappings used by static analyser */
enum {

    JREG_NULL=0, /**< Sentinel value indicating no register, for address modes. */
    JREG_INVALID, /**< Sentinel value indicating an invalid register */

    /* 64-bit general purpose */
    JREG_X0, /**< The "X0" register */
    JREG_X1, /**< The "X1" register */
    JREG_X2, /**< The "X2" register */
    JREG_X3, /**< The "X3" register */
    JREG_X4, /**< The "X4" register */
    JREG_X5, /**< The "X5" register */
    JREG_X6, /**< The "X6" register */
    JREG_X7, /**< The "X7" register */
    JREG_X8, /**< The "X8" register */
    JREG_X9, /**< The "X9" register */
    JREG_X10, /**< The "X10" register */
    JREG_X11, /**< The "X11" register */
    JREG_X12, /**< The "X12" register */
    JREG_X13, /**< The "X13" register */
    JREG_X14, /**< The "X14" register */
    JREG_X15, /**< The "X15" register */
    JREG_X16, /**< The "X16" register */
    JREG_X17, /**< The "X17" register */
    JREG_X18, /**< The "X18" register */
    JREG_X19, /**< The "X19" register */
    JREG_X20, /**< The "X20" register */
    JREG_X21, /**< The "X21" register */
    JREG_X22, /**< The "X22" register */
    JREG_X23, /**< The "X23" register */
    JREG_X24, /**< The "X24" register */
    JREG_X25, /**< The "X25" register */
    JREG_X26, /**< The "X26" register */
    JREG_X27, /**< The "X27" register */
    JREG_X28, /**< The "X28" register */
    JREG_X29, /**< The "X29" register */
    JREG_X30, /**< The "X30" register */

    JREG_SP, /**< The "SP" register */
    JREG_XZR, /**< The "XZR" register */

    /* 32-bit general purpose */
    JREG_W0, /**< The "W0" register */
    JREG_W1, /**< The "W1" register */
    JREG_W2, /**< The "W2" register */
    JREG_W3, /**< The "W3" register */
    JREG_W4, /**< The "W4" register */
    JREG_W5, /**< The "W5" register */
    JREG_W6, /**< The "W6" register */
    JREG_W7, /**< The "W7" register */
    JREG_W8, /**< The "W8" register */
    JREG_W9, /**< The "W9" register */
    JREG_W10, /**< The "W10" register */
    JREG_W11, /**< The "W11" register */
    JREG_W12, /**< The "W12" register */
    JREG_W13, /**< The "W13" register */
    JREG_W14, /**< The "W14" register */
    JREG_W15, /**< The "W15" register */
    JREG_W16, /**< The "W16" register */
    JREG_W17, /**< The "W17" register */
    JREG_W18, /**< The "W18" register */
    JREG_W19, /**< The "W19" register */
    JREG_W20, /**< The "W20" register */
    JREG_W21, /**< The "W21" register */
    JREG_W22, /**< The "W22" register */
    JREG_W23, /**< The "W23" register */
    JREG_W24, /**< The "W24" register */
    JREG_W25, /**< The "W25" register */
    JREG_W26, /**< The "W26" register */
    JREG_W27, /**< The "W27" register */
    JREG_W28, /**< The "W28" register */
    JREG_W29, /**< The "W29" register */
    JREG_W30, /**< The "W30" register */

    JREG_WSP, /**< The "WSP" register */
    JREG_WZR, /**< The "WZR" register */


    /* SIMD registers - note that these are the same 32 registers accessed as different sizes etc */

    /* Quad-word (128 bit) SIMD */
    JREG_Q0, /**< The "Q0" register */
    JREG_Q1, /**< The "Q1" register */
    JREG_Q2, /**< The "Q2" register */
    JREG_Q3, /**< The "Q3" register */
    JREG_Q4, /**< The "Q4" register */
    JREG_Q5, /**< The "Q5" register */
    JREG_Q6, /**< The "Q6" register */
    JREG_Q7, /**< The "Q7" register */
    JREG_Q8, /**< The "Q8" register */
    JREG_Q9, /**< The "Q9" register */
    JREG_Q10, /**< The "Q10" register */
    JREG_Q11, /**< The "Q11" register */
    JREG_Q12, /**< The "Q12" register */
    JREG_Q13, /**< The "Q13" register */
    JREG_Q14, /**< The "Q14" register */
    JREG_Q15, /**< The "Q15" register */
    JREG_Q16, /**< The "Q16" register */
    JREG_Q17, /**< The "Q17" register */
    JREG_Q18, /**< The "Q18" register */
    JREG_Q19, /**< The "Q19" register */
    JREG_Q20, /**< The "Q20" register */
    JREG_Q21, /**< The "Q21" register */
    JREG_Q22, /**< The "Q22" register */
    JREG_Q23, /**< The "Q23" register */
    JREG_Q24, /**< The "Q24" register */
    JREG_Q25, /**< The "Q25" register */
    JREG_Q26, /**< The "Q26" register */
    JREG_Q27, /**< The "Q27" register */
    JREG_Q28, /**< The "Q28" register */
    JREG_Q29, /**< The "Q29" register */
    JREG_Q30, /**< The "Q30" register */
    JREG_Q31, /**< The "Q31" register */

    /* Double-word (64-bit) SIMD */
    JREG_D0, /**< The "D0" register */
    JREG_D1, /**< The "D1" register */
    JREG_D2, /**< The "D2" register */
    JREG_D3, /**< The "D3" register */
    JREG_D4, /**< The "D4" register */
    JREG_D5, /**< The "D5" register */
    JREG_D6, /**< The "D6" register */
    JREG_D7, /**< The "D7" register */
    JREG_D8, /**< The "D8" register */
    JREG_D9, /**< The "D9" register */
    JREG_D10, /**< The "D10" register */
    JREG_D11, /**< The "D11" register */
    JREG_D12, /**< The "D12" register */
    JREG_D13, /**< The "D13" register */
    JREG_D14, /**< The "D14" register */
    JREG_D15, /**< The "D15" register */
    JREG_D16, /**< The "D16" register */
    JREG_D17, /**< The "D17" register */
    JREG_D18, /**< The "D18" register */
    JREG_D19, /**< The "D19" register */
    JREG_D20, /**< The "D20" register */
    JREG_D21, /**< The "D21" register */
    JREG_D22, /**< The "D22" register */
    JREG_D23, /**< The "D23" register */
    JREG_D24, /**< The "D24" register */
    JREG_D25, /**< The "D25" register */
    JREG_D26, /**< The "D26" register */
    JREG_D27, /**< The "D27" register */
    JREG_D28, /**< The "D28" register */
    JREG_D29, /**< The "D29" register */
    JREG_D30, /**< The "D30" register */
    JREG_D31, /**< The "D31" register */

    /* Single-word (32-bit) SIMD */
    JREG_S0, /**< The "S0" register */
    JREG_S1, /**< The "S1" register */
    JREG_S2, /**< The "S2" register */
    JREG_S3, /**< The "S3" register */
    JREG_S4, /**< The "S4" register */
    JREG_S5, /**< The "S5" register */
    JREG_S6, /**< The "S6" register */
    JREG_S7, /**< The "S7" register */
    JREG_S8, /**< The "S8" register */
    JREG_S9, /**< The "S9" register */
    JREG_S10, /**< The "S10" register */
    JREG_S11, /**< The "S11" register */
    JREG_S12, /**< The "S12" register */
    JREG_S13, /**< The "S13" register */
    JREG_S14, /**< The "S14" register */
    JREG_S15, /**< The "S15" register */
    JREG_S16, /**< The "S16" register */
    JREG_S17, /**< The "S17" register */
    JREG_S18, /**< The "S18" register */
    JREG_S19, /**< The "S19" register */
    JREG_S20, /**< The "S20" register */
    JREG_S21, /**< The "S21" register */
    JREG_S22, /**< The "S22" register */
    JREG_S23, /**< The "S23" register */
    JREG_S24, /**< The "S24" register */
    JREG_S25, /**< The "S25" register */
    JREG_S26, /**< The "S26" register */
    JREG_S27, /**< The "S27" register */
    JREG_S28, /**< The "S28" register */
    JREG_S29, /**< The "S29" register */
    JREG_S30, /**< The "S30" register */
    JREG_S31, /**< The "S31" register */

    /* Half-word (16-bit) SIMD */
    JREG_H0, /**< The "H0" register */
    JREG_H1, /**< The "H1" register */
    JREG_H2, /**< The "H2" register */
    JREG_H3, /**< The "H3" register */
    JREG_H4, /**< The "H4" register */
    JREG_H5, /**< The "H5" register */
    JREG_H6, /**< The "H6" register */
    JREG_H7, /**< The "H7" register */
    JREG_H8, /**< The "H8" register */
    JREG_H9, /**< The "H9" register */
    JREG_H10, /**< The "H10" register */
    JREG_H11, /**< The "H11" register */
    JREG_H12, /**< The "H12" register */
    JREG_H13, /**< The "H13" register */
    JREG_H14, /**< The "H14" register */
    JREG_H15, /**< The "H15" register */
    JREG_H16, /**< The "H16" register */
    JREG_H17, /**< The "H17" register */
    JREG_H18, /**< The "H18" register */
    JREG_H19, /**< The "H19" register */
    JREG_H20, /**< The "H20" register */
    JREG_H21, /**< The "H21" register */
    JREG_H22, /**< The "H22" register */
    JREG_H23, /**< The "H23" register */
    JREG_H24, /**< The "H24" register */
    JREG_H25, /**< The "H25" register */
    JREG_H26, /**< The "H26" register */
    JREG_H27, /**< The "H27" register */
    JREG_H28, /**< The "H28" register */
    JREG_H29, /**< The "H29" register */
    JREG_H30, /**< The "H30" register */
    JREG_H31, /**< The "H31" register */

    /* Byte (8-bit) SIMD */
    JREG_B0, /**< The "B0" register */
    JREG_B1, /**< The "B1" register */
    JREG_B2, /**< The "B2" register */
    JREG_B3, /**< The "B3" register */
    JREG_B4, /**< The "B4" register */
    JREG_B5, /**< The "B5" register */
    JREG_B6, /**< The "B6" register */
    JREG_B7, /**< The "B7" register */
    JREG_B8, /**< The "B8" register */
    JREG_B9, /**< The "B9" register */
    JREG_B10, /**< The "B10" register */
    JREG_B11, /**< The "B11" register */
    JREG_B12, /**< The "B12" register */
    JREG_B13, /**< The "B13" register */
    JREG_B14, /**< The "B14" register */
    JREG_B15, /**< The "B15" register */
    JREG_B16, /**< The "B16" register */
    JREG_B17, /**< The "B17" register */
    JREG_B18, /**< The "B18" register */
    JREG_B19, /**< The "B19" register */
    JREG_B20, /**< The "B20" register */
    JREG_B21, /**< The "B21" register */
    JREG_B22, /**< The "B22" register */
    JREG_B23, /**< The "B23" register */
    JREG_B24, /**< The "B24" register */
    JREG_B25, /**< The "B25" register */
    JREG_B26, /**< The "B26" register */
    JREG_B27, /**< The "B27" register */
    JREG_B28, /**< The "B28" register */
    JREG_B29, /**< The "B29" register */
    JREG_B30, /**< The "B30" register */
    JREG_B31, /**< The "B31" register */

    /* PSTATE condition flags */
    JREG_NZCV, /**< The "NZCV" register */
    JREG_FPCR, /**< The "FPCR" register */
    JREG_FPSR, /**< The "FPSR" register */

    /* SIMD vector registers */
    JREG_V0, /**< The "V0" register */
    JREG_V1, /**< The "V1" register */
    JREG_V2, /**< The "V2" register */
    JREG_V3, /**< The "V3" register */
    JREG_V4, /**< The "V4" register */
    JREG_V5, /**< The "V5" register */
    JREG_V6, /**< The "V6" register */
    JREG_V7, /**< The "V7" register */
    JREG_V8, /**< The "V8" register */
    JREG_V9, /**< The "V9" register */
    JREG_V10, /**< The "V10" register */
    JREG_V11, /**< The "V11" register */
    JREG_V12, /**< The "V12" register */
    JREG_V13, /**< The "V13" register */
    JREG_V14, /**< The "V14" register */
    JREG_V15, /**< The "V15" register */
    JREG_V16, /**< The "V16" register */
    JREG_V17, /**< The "V17" register */
    JREG_V18, /**< The "V18" register */
    JREG_V19, /**< The "V19" register */
    JREG_V20, /**< The "V20" register */
    JREG_V21, /**< The "V21" register */
    JREG_V22, /**< The "V22" register */
    JREG_V23, /**< The "V23" register */
    JREG_V24, /**< The "V24" register */
    JREG_V25, /**< The "V25" register */
    JREG_V26, /**< The "V26" register */
    JREG_V27, /**< The "V27" register */
    JREG_V28, /**< The "V28" register */
    JREG_V29, /**< The "V29" register */
    JREG_V30, /**< The "V30" register */
    JREG_V31, /**< The "V31" register */

};


#ifdef __cplusplus
}
#endif

#endif
