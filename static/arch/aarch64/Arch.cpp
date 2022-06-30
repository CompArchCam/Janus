/* AArch64 specific analysis */

#include "Arch.h"
#include "BasicBlock.h"
#include "janus_aarch64.h"

using namespace janus;
using namespace std;

uint32_t csToJanus[] = {
    JREG_NULL,

    JREG_X29,     JREG_X30, JREG_NZCV, JREG_SP,  JREG_WSP, JREG_WZR, JREG_XZR,
    JREG_B0,      JREG_B1,  JREG_B2,   JREG_B3,  JREG_B4,  JREG_B5,  JREG_B6,
    JREG_B7,      JREG_B8,  JREG_B9,   JREG_B10, JREG_B11, JREG_B12, JREG_B13,
    JREG_B14,     JREG_B15, JREG_B16,  JREG_B17, JREG_B18, JREG_B19, JREG_B20,
    JREG_B21,     JREG_B22, JREG_B23,  JREG_B24, JREG_B25, JREG_B26, JREG_B27,
    JREG_B28,     JREG_B29, JREG_B30,  JREG_B31, JREG_D0,  JREG_D1,  JREG_D2,
    JREG_D3,      JREG_D4,  JREG_D5,   JREG_D6,  JREG_D7,  JREG_D8,  JREG_D9,
    JREG_D10,     JREG_D11, JREG_D12,  JREG_D13, JREG_D14, JREG_D15, JREG_D16,
    JREG_D17,     JREG_D18, JREG_D19,  JREG_D20, JREG_D21, JREG_D22, JREG_D23,
    JREG_D24,     JREG_D25, JREG_D26,  JREG_D27, JREG_D28, JREG_D29, JREG_D30,
    JREG_D31,     JREG_H0,  JREG_H1,   JREG_H2,  JREG_H3,  JREG_H4,  JREG_H5,
    JREG_H6,      JREG_H7,  JREG_H8,   JREG_H9,  JREG_H10, JREG_H11, JREG_H12,
    JREG_H13,     JREG_H14, JREG_H15,  JREG_H16, JREG_H17, JREG_H18, JREG_H19,
    JREG_H20,     JREG_H21, JREG_H22,  JREG_H23, JREG_H24, JREG_H25, JREG_H26,
    JREG_H27,     JREG_H28, JREG_H29,  JREG_H30, JREG_H31, JREG_Q0,  JREG_Q1,
    JREG_Q2,      JREG_Q3,  JREG_Q4,   JREG_Q5,  JREG_Q6,  JREG_Q7,  JREG_Q8,
    JREG_Q9,      JREG_Q10, JREG_Q11,  JREG_Q12, JREG_Q13, JREG_Q14, JREG_Q15,
    JREG_Q16,     JREG_Q17, JREG_Q18,  JREG_Q19, JREG_Q20, JREG_Q21, JREG_Q22,
    JREG_Q23,     JREG_Q24, JREG_Q25,  JREG_Q26, JREG_Q27, JREG_Q28, JREG_Q29,
    JREG_Q30,     JREG_Q31, JREG_S0,   JREG_S1,  JREG_S2,  JREG_S3,  JREG_S4,
    JREG_S5,      JREG_S6,  JREG_S7,   JREG_S8,  JREG_S9,  JREG_S10, JREG_S11,
    JREG_S12,     JREG_S13, JREG_S14,  JREG_S15, JREG_S16, JREG_S17, JREG_S18,
    JREG_S19,     JREG_S20, JREG_S21,  JREG_S22, JREG_S23, JREG_S24, JREG_S25,
    JREG_S26,     JREG_S27, JREG_S28,  JREG_S29, JREG_S30, JREG_S31, JREG_W0,
    JREG_W1,      JREG_W2,  JREG_W3,   JREG_W4,  JREG_W5,  JREG_W6,  JREG_W7,
    JREG_W8,      JREG_W9,  JREG_W10,  JREG_W11, JREG_W12, JREG_W13, JREG_W14,
    JREG_W15,     JREG_W16, JREG_W17,  JREG_W18, JREG_W19, JREG_W20, JREG_W21,
    JREG_W22,     JREG_W23, JREG_W24,  JREG_W25, JREG_W26, JREG_W27, JREG_W28,
    JREG_W29,     JREG_W30, JREG_X0,   JREG_X1,  JREG_X2,  JREG_X3,  JREG_X4,
    JREG_X5,      JREG_X6,  JREG_X7,   JREG_X8,  JREG_X9,  JREG_X10, JREG_X11,
    JREG_X12,     JREG_X13, JREG_X14,  JREG_X15, JREG_X16, JREG_X17, JREG_X18,
    JREG_X19,     JREG_X20, JREG_X21,  JREG_X22, JREG_X23, JREG_X24, JREG_X25,
    JREG_X26,     JREG_X27, JREG_X28,

    JREG_V0,      JREG_V1,  JREG_V2,   JREG_V3,  JREG_V4,  JREG_V5,  JREG_V6,
    JREG_V7,      JREG_V8,  JREG_V9,   JREG_V10, JREG_V11, JREG_V12, JREG_V13,
    JREG_V14,     JREG_V15, JREG_V16,  JREG_V17, JREG_V18, JREG_V19, JREG_V20,
    JREG_V21,     JREG_V22, JREG_V23,  JREG_V24, JREG_V25, JREG_V26, JREG_V27,
    JREG_V28,     JREG_V29, JREG_V30,  JREG_V31,

    JREG_INVALID, // <-- mark the end of the list of registers
};

void decodeRegSet(uint64_t bits, std::set<uint32_t> &regSet)
{
    uint64_t mask = 1;
    for (int i = 0; i < 63; i++) {
        if (bits & mask) {
            if (i <= 30)
                regSet.insert(JREG_X0 + i);
            else if (i == 31)
                regSet.insert(JREG_SP);
            else if (i >= 32)
                regSet.insert(JREG_Q0 +
                              (i - 32)); /* Default assume quad-word use */
        }
        mask = mask << 1;
    }
}

int decodeNextReg(uint64_t bits, int reg)
{
    if (reg >= JREG_X0 && reg <= JREG_X30)
        reg -= JREG_X0;
    else if (reg >= JREG_W0 && reg <= JREG_W30)
        reg -= JREG_W0;
    else if (reg >= JREG_Q0 && reg <= JREG_Q30)
        reg = reg - JREG_Q0 + 32;
    else if (reg >= JREG_D0 && reg <= JREG_D30)
        reg = reg - JREG_D0 + 32;
    else if (reg >= JREG_S0 && reg <= JREG_S30)
        reg = reg - JREG_S0 + 32;
    else if (reg >= JREG_H0 && reg <= JREG_H30)
        reg = reg - JREG_H0 + 32;
    else if (reg >= JREG_B0 && reg <= JREG_B30)
        reg = reg - JREG_B0 + 32;
    else if (reg == JREG_SP || reg == JREG_WSP)
        reg = 31;
    /* You call with a register that doesn't exist, you get a weird value. */

    uint64_t mask = 1;
    mask = mask << reg;
    for (int i = reg; i < 63; i++) {
        if (bits & mask) {
            if (i <= 30)
                return JREG_X0 + i;
            else if (i == 31)
                return JREG_SP;
            else if (i >= 32)
                return JREG_Q0 + (i - 32); /* Default assume quad-word use */
        }
        mask = mask << 1;
    }
    return JREG_NULL;
}

void getAllGPRegisters(std::set<uint32_t> &regSet)
{
    regSet.insert((uint32_t)JREG_X0);
    regSet.insert((uint32_t)JREG_X1);
    regSet.insert((uint32_t)JREG_X2);
    regSet.insert((uint32_t)JREG_X3);
    regSet.insert((uint32_t)JREG_X4);
    regSet.insert((uint32_t)JREG_X5);
    regSet.insert((uint32_t)JREG_X6);
    regSet.insert((uint32_t)JREG_X7);
    regSet.insert((uint32_t)JREG_X8);
    regSet.insert((uint32_t)JREG_X9);
    regSet.insert((uint32_t)JREG_X10);
    regSet.insert((uint32_t)JREG_X11);
    regSet.insert((uint32_t)JREG_X12);
    regSet.insert((uint32_t)JREG_X13);
    regSet.insert((uint32_t)JREG_X14);
    regSet.insert((uint32_t)JREG_X15);
}

void getAllSIMDRegisters(std::set<uint32_t> &regSet)
{
    // TODO add simd registers for aarch64
}

/// get a vector of standard calling convention input arguments
void getInputCallConvention(std::vector<janus::Variable> &v)
{
    // I think this is right?
    v.emplace_back((uint32_t)JREG_X0);
    v.emplace_back((uint32_t)JREG_X1);
    v.emplace_back((uint32_t)JREG_X2);
    v.emplace_back((uint32_t)JREG_X3);
    v.emplace_back((uint32_t)JREG_X4);
    v.emplace_back((uint32_t)JREG_X5);
    v.emplace_back((uint32_t)JREG_X6);
    v.emplace_back((uint32_t)JREG_X7);
}

/// get a vector of standard calling convention output arguments
void getOutputCallConvention(std::vector<janus::Variable> &v)
{
    v.emplace_back((uint32_t)JREG_X0);
    // ARM don't seem to specify any of X0-X7 specifically for return value,
    // assuming X0
}

enum StackFrameState {
    LOOK_FOR_FRAME_ALLOC,   // Looking for an instruction that allocates a stack
                            // frame using preindexed addressing
    LOOK_FOR_MOV_SP_TO_X29, // Looking for a {mov x29, sp} instruction
    CHECK_FOR_X29_WRITE,    // Checking that no instructions write to x29

    FAILED, // Analysis has failed and x29 is not used as a frame pointer
};

void relift_x29_mem(Function *function)
{
    if (function->hasBasePointer) {
        for (auto &instr : function->instrs) {
            for (auto &vs : instr.inputs) {
                if (vs->type == JVAR_MEMORY && vs->base == JREG_X29 &&
                    vs->index == JREG_NULL)
                    vs->type = JVAR_STACKFRAME;
            }
            for (auto &vs : instr.outputs) {
                if (vs->type == JVAR_MEMORY && vs->base == JREG_X29 &&
                    vs->index == JREG_NULL)
                    vs->type = JVAR_STACKFRAME;
            }
        }
    }
}

void analyseStack(Function *function)
{

    BasicBlock *entry = function->entry;
    function->hasBasePointer = true;
    function->hasConstantStackPointer = true;

    uint64_t frameSize = 0;

    StackFrameState state = LOOK_FOR_FRAME_ALLOC;

    for (int i = 0; i < entry->size; i++) {
        MachineInstruction &instr = *entry->instrs[i].minstr;

        switch (state) {
        case LOOK_FOR_FRAME_ALLOC:
            if (instr.isAllocatingStackFrame()) {
                state = LOOK_FOR_MOV_SP_TO_X29;
                frameSize += instr.getStackFrameAllocSize();
            } else if (instr.isSubSP()) {
                frameSize += instr.getSPSubSize();
            } else if (instr.isX29MemAccess()) {
                state = FAILED;
            }
            break;
        case LOOK_FOR_MOV_SP_TO_X29:
            if (instr.isMoveSPToX29()) {
                state = CHECK_FOR_X29_WRITE;
            }
            break;
        case CHECK_FOR_X29_WRITE:
            if (instr.writeReg(JREG_X29)) {
                state = FAILED;
            }
            break;
        }
    }

    if (state == CHECK_FOR_X29_WRITE) // No writes to X29 since stack was set up
    {
        function->totalFrameSize = frameSize;
        function->hasBasePointer = true;
    } else {
        function->hasBasePointer = false;
    }

    if (function->hasBasePointer) { // Iterate through non-terminating basic
                                    // blocks to check that none of them write
                                    // to X29, and through the terminating
                                    // blocks to make sure they all deallocate a
                                    // correct-sized stack frame
        for (auto &bb : function->blocks) {
            if (&bb != entry) // Already checked the start block
            {
                if (function->terminations.find(bb.bid) ==
                    function->terminations.end()) // Non-terminating blocks
                {
                    for (int i = 0; i < bb.size; i++) {
                        MachineInstruction &instr = *bb.instrs[i].minstr;

                        if (instr.writeReg(JREG_X29)) {
                            function->hasBasePointer = false;
                        }
                    }
                }

                else // Terminating blocks
                {
                    bool dealloc_found =
                        false; // Ensures we only take into account the first
                               // dealloc instr we find
                    bool matching_dealloc_found =
                        false; // True if the first found dealloc instr was the
                               // same size as the original alloc
                    for (int i = 0; i < bb.size; i++) {
                        MachineInstruction &instr = *bb.instrs[i].minstr;

                        if ((!dealloc_found) &&
                            instr.isDeallocatingStackFrame()) {
                            dealloc_found = true;
                            if (instr.getStackFrameDeallocSize() ==
                                function->stackFrameSize) {
                                matching_dealloc_found = true;
                            }
                        } else if (dealloc_found && instr.isX29MemAccess()) {
                            function->hasBasePointer = false;
                        }
                    }
                    if (dealloc_found &&
                        !matching_dealloc_found) // If a dealloc is found it
                                                 // must match
                    {
                        function->hasBasePointer = false;
                    }
                }
            }
        }
    }

    return;
}
