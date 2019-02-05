/* x86 specific analysis */

#include "Arch.h"
#include "BasicBlock.h"
#include "janus_arch.h"
#include <set>

using namespace janus;
using namespace std;

uint32_t csToJanus[] =
{
    JREG_NULL,
    JREG_AH, JREG_AL, JREG_AX, JREG_BH, JREG_BL,
    JREG_BP, JREG_BPL, JREG_BX, JREG_CH, JREG_CL,
    JREG_CS, JREG_CX, JREG_DH, JREG_DI, JREG_DIL,
    JREG_DL, JREG_DS, JREG_DX, JREG_EAX, JREG_EBP,
    JREG_EBX, JREG_ECX, JREG_EDI, JREG_EDX, JREG_EFLAGS,
    JREG_EIP, JREG_EIZ, JREG_ES, JREG_ESI, JREG_ESP,
    JREG_FPSW, JREG_FS, JREG_GS, JREG_IP, JREG_RAX,
    JREG_RBP, JREG_RBX, JREG_RCX, JREG_RDI, JREG_RDX,
    JREG_RIP, JREG_RIZ, JREG_RSI, JREG_RSP, JREG_SI,
    JREG_SIL, JREG_SP, JREG_SPL, JREG_SS, JREG_CR0,
    JREG_CR1, JREG_CR2, JREG_CR3, JREG_CR4, JREG_CR5,
    JREG_CR6, JREG_CR7, JREG_CR8, JREG_CR9, JREG_CR10,
    JREG_CR11, JREG_CR12, JREG_CR13, JREG_CR14, JREG_CR15,
    JREG_DR0, JREG_DR1, JREG_DR2, JREG_DR3, JREG_DR4,
    JREG_DR5, JREG_DR6, JREG_DR7, JREG_DR8, JREG_DR9,
    JREG_DR10, JREG_DR11, JREG_DR12, JREG_DR13, JREG_DR14,
    JREG_DR15, JREG_FP0, JREG_FP1, JREG_FP2, JREG_FP3,
    JREG_FP4, JREG_FP5, JREG_FP6, JREG_FP7,
    JREG_K0, JREG_K1, JREG_K2, JREG_K3, JREG_K4,
    JREG_K5, JREG_K6, JREG_K7, JREG_MM0, JREG_MM1,
    JREG_MM2, JREG_MM3, JREG_MM4, JREG_MM5, JREG_MM6,
    JREG_MM7, JREG_R8, JREG_R9, JREG_R10, JREG_R11,
    JREG_R12, JREG_R13, JREG_R14, JREG_R15,
    JREG_ST0, JREG_ST1, JREG_ST2, JREG_ST3,
    JREG_ST4, JREG_ST5, JREG_ST6, JREG_ST7,
    JREG_XMM0, JREG_XMM1, JREG_XMM2, JREG_XMM3, JREG_XMM4,
    JREG_XMM5, JREG_XMM6, JREG_XMM7, JREG_XMM8, JREG_XMM9,
    JREG_XMM10, JREG_XMM11, JREG_XMM12, JREG_XMM13, JREG_XMM14,
    JREG_XMM15, JREG_XMM16, JREG_XMM17, JREG_XMM18, JREG_XMM19,
    JREG_XMM20, JREG_XMM21, JREG_XMM22, JREG_XMM23, JREG_XMM24,
    JREG_XMM25, JREG_XMM26, JREG_XMM27, JREG_XMM28, JREG_XMM29,
    JREG_XMM30, JREG_XMM31, JREG_YMM0, JREG_YMM1, JREG_YMM2,
    JREG_YMM3, JREG_YMM4, JREG_YMM5, JREG_YMM6, JREG_YMM7,
    JREG_YMM8, JREG_YMM9, JREG_YMM10, JREG_YMM11, JREG_YMM12,
    JREG_YMM13, JREG_YMM14, JREG_YMM15, JREG_YMM16, JREG_YMM17,
    JREG_YMM18, JREG_YMM19, JREG_YMM20, JREG_YMM21, JREG_YMM22,
    JREG_YMM23, JREG_YMM24, JREG_YMM25, JREG_YMM26, JREG_YMM27,
    JREG_YMM28, JREG_YMM29, JREG_YMM30, JREG_YMM31, JREG_ZMM0,
    JREG_ZMM1, JREG_ZMM2, JREG_ZMM3, JREG_ZMM4, JREG_ZMM5,
    JREG_ZMM6, JREG_ZMM7, JREG_ZMM8, JREG_ZMM9, JREG_ZMM10,
    JREG_ZMM11, JREG_ZMM12, JREG_ZMM13, JREG_ZMM14, JREG_ZMM15,
    JREG_ZMM16, JREG_ZMM17, JREG_ZMM18, JREG_ZMM19, JREG_ZMM20,
    JREG_ZMM21, JREG_ZMM22, JREG_ZMM23, JREG_ZMM24, JREG_ZMM25,
    JREG_ZMM26, JREG_ZMM27, JREG_ZMM28, JREG_ZMM29, JREG_ZMM30,
    JREG_ZMM31, JREG_R8B, JREG_R9B, JREG_R10B, JREG_R11B,
    JREG_R12B, JREG_R13B, JREG_R14B, JREG_R15B, JREG_R8D,
    JREG_R9D, JREG_R10D, JREG_R11D, JREG_R12D, JREG_R13D,
    JREG_R14D, JREG_R15D, JREG_R8W, JREG_R9W, JREG_R10W,
    JREG_R11W, JREG_R12W, JREG_R13W, JREG_R14W, JREG_R15W,
    JREG_INVALID        // <-- mark the end of the list of registers
};

void decodeRegSet(uint64_t bits, std::set<uint32_t> &regSet)
{
    uint64_t mask = 1;
    for (int i=0; i<63; i++) {
        if (bits & mask) {
            if (i < 16)
                regSet.insert(JREG_RAX + i);
            else if (i < 32)
                regSet.insert(JREG_XMM0 + i - 16);
        }
        mask = mask << 1;
    }
    if (bits & mask) {
        regSet.insert(JREG_EFLAGS);
    }
}

int decodeNextReg(uint64_t bits, int reg)
{
    if (reg >= JREG_XMM0) {
        reg += 16 - JREG_XMM0;
    }
    else {
        reg += -JREG_RAX;
    }
    uint64_t mask = 1;
    mask = mask << reg;
    for (int i=reg; i<63; i++) {
        if (bits & mask) {
            if (i < 16)
                return JREG_RAX + i;
            else if (i < 32)
                return JREG_XMM0 + i - 16;
        }
        mask = mask << 1;
    }
    return JREG_NULL;
}

void getAllGPRegisters(std::set<uint32_t> &regSet)
{
    regSet.insert((uint32_t)JREG_RAX);
    regSet.insert((uint32_t)JREG_RCX);
    regSet.insert((uint32_t)JREG_RDX);
    regSet.insert((uint32_t)JREG_RBX);
    //regSet.insert((uint32_t)JREG_RSP);
    regSet.insert((uint32_t)JREG_RBP);
    regSet.insert((uint32_t)JREG_RSI);
    regSet.insert((uint32_t)JREG_RDI);
    regSet.insert((uint32_t)JREG_R8);
    regSet.insert((uint32_t)JREG_R9);
    regSet.insert((uint32_t)JREG_R10);
    regSet.insert((uint32_t)JREG_R11);
    regSet.insert((uint32_t)JREG_R12);
    regSet.insert((uint32_t)JREG_R13);
    regSet.insert((uint32_t)JREG_R14);
    regSet.insert((uint32_t)JREG_R15);
}

void getAllSIMDRegisters(std::set<uint32_t> &regSet)
{
    regSet.insert((uint32_t)JREG_XMM0);
    regSet.insert((uint32_t)JREG_XMM1);
    regSet.insert((uint32_t)JREG_XMM2);
    regSet.insert((uint32_t)JREG_XMM3);
    regSet.insert((uint32_t)JREG_XMM4);
    regSet.insert((uint32_t)JREG_XMM5);
    regSet.insert((uint32_t)JREG_XMM6);
    regSet.insert((uint32_t)JREG_XMM7);
    regSet.insert((uint32_t)JREG_XMM8);
    regSet.insert((uint32_t)JREG_XMM9);
    regSet.insert((uint32_t)JREG_XMM10);
    regSet.insert((uint32_t)JREG_XMM11);
    regSet.insert((uint32_t)JREG_XMM12);
    regSet.insert((uint32_t)JREG_XMM13);
    regSet.insert((uint32_t)JREG_XMM14);
    regSet.insert((uint32_t)JREG_XMM15);
}


///get a vector of standard calling convention input arguments
void getInputCallConvention(std::vector<janus::Variable> &v)
{
    //currently we only support Linux x86-64 calling convention
    v.emplace_back((uint32_t)JREG_RDI);
    v.emplace_back((uint32_t)JREG_RSI);
    v.emplace_back((uint32_t)JREG_RDX);
    v.emplace_back((uint32_t)JREG_RCX);
    v.emplace_back((uint32_t)JREG_R8);
    v.emplace_back((uint32_t)JREG_R9);
    /*
    v.emplace_back((uint32_t)JREG_XMM0);
    v.emplace_back((uint32_t)JREG_XMM1);
    v.emplace_back((uint32_t)JREG_XMM2);
    v.emplace_back((uint32_t)JREG_XMM3);
    v.emplace_back((uint32_t)JREG_XMM4);
    v.emplace_back((uint32_t)JREG_XMM5);
    v.emplace_back((uint32_t)JREG_XMM6);
    v.emplace_back((uint32_t)JREG_XMM7);
    */
    //v.emplace_back((uint32_t)JREG_RAX);
}
///get a vector of standard calling convention output arguments
void getOutputCallConvention(std::vector<janus::Variable> &v)
{
    v.emplace_back((uint32_t)JREG_RAX);
}

void analyseStack(Function *function)
{
    BasicBlock *entry = function->entry;
    function->hasBasePointer = false;
    function->hasConstantStackPointer = true;
    uint64_t totalFrameSize;
    /* XXX we use a simple solution by looking at the first block
     * of the function to scan any explicit stack pointer movement */
    int stackSize = 0;
    bool found = false;
    int size = 0;

    for (int i=0; i<entry->size; i++) {
        MachineInstruction &instr = *entry->instrs[i].minstr;

        /* check whether rbp is a base pointer */
        if (instr.isMakingStackBase()) {
            function->hasBasePointer = true;
            size = 0;
            continue;
        }
        if (instr.isPUSH()) {
            size += instr.operands[0].size;
        }
        
        if (!found) {
            stackSize = instr.isMoveStackPointer();
            if (!stackSize) continue;
            function->stackFrameSize = stackSize;
            found = true;
        }
    }
    function->totalFrameSize = function->stackFrameSize + size;

    for (int i=1; i<function->numBlocks; i++) {
        if (function->terminations.find(i) != function->terminations.end()) continue;

        auto &bb = function->entry[i];
        for (int j=0; j<bb.size; j++) {
            auto &mi = *bb.instrs[j].minstr;

            int sizeChange = 0;
            sizeChange += mi.isMoveStackPointer();
            if (mi.isPUSH())
                sizeChange += mi.operands[0].size;
            if (mi.isPOP())
                sizeChange -= mi.operands[0].size;

            if (sizeChange) {
                function->hasConstantStackPointer = false;
            }
        }
    }

    if (function->totalFrameSize == 0) {
        int max_offset = 0;
        int min_offset = 0;
        /* Work out the implicit stack usages*/
        for (auto var: function->allVars) {
            if (var.type == JVAR_STACK) {
                int offset = var.value;
                if (max_offset < offset) max_offset = offset;
                if (min_offset > offset) min_offset = offset;
            }
        }
        //currently we only allow implicit stack with negative offset
        if (min_offset<0) {
            function->implicitStack = true;
            function->totalFrameSize = max_offset-min_offset;
        }
    }
}

