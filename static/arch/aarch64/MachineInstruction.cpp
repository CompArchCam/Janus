#include "MachineInstruction.h"
#include "Operand.h"
#include "BasicBlock.h"
#include "Function.h"
#include "IO.h"
#include "Utility.h"
#include "capstone/capstone.h"
#include "janus_aarch64.h"
#include <iostream>
#include <sstream>
#include <cstring>

// The bit that is 0 if it is a preindexed load/store with an immediate offset,
// rather than a load/store with a register offset
#define IMMED_OFFSET_ZERO_BIT 21

#define PREPOST_ZERO_BIT 24 // The bit that is 0 if using pre or post indexing
#define PREINDEX_BIT  11    // The bit that is 1 if preindexing specifically is used

#define PAIR_PREPOST_BIT  23 // The bit that is 1 if using pre or post indexing in STP and LDP 'pairwise' type instructions
#define PAIR_PREINDEX_BIT 24 // The bit that is 1 if preindexing specifically is used in STP and LDP 'pairwise' type instructions
#define LDR_NON_LITERAL_BIT 29 // The bit that distinguishes a literal ldr instruction from  normal ldr
                               // (a ldr with a literal address can look like a post-indexed ldr)

#define INSTR_64_BIT_BIT 30

using namespace janus;
using namespace std;

MachineInstruction::MachineInstruction(InstID id, void *cs_handle, void *cs_instr)
:id(id)
{
    cs_insn *instr = (cs_insn *)cs_instr;
    cs_detail *detail = instr->detail;

    pc = instr->address;
    opcode = instr->id;

    aux = 0;

    /* Assign instruction names */
    int mnemonicLength = strlen(instr->mnemonic);
    int opLength = strlen(instr->op_str);
    name = (char *)malloc(mnemonicLength + opLength + 2);
    strncpy(name, instr->mnemonic, mnemonicLength);
    name[mnemonicLength] = ' ';
    strncpy(name + mnemonicLength + 1, instr->op_str, opLength + 1);
    name[mnemonicLength + opLength + 1] = '\0';

    size = instr->size;

    opndCount = detail->arm64.op_count;

    //allocate operand array
    operands = (Operand *)malloc(sizeof(Operand) * opndCount);
    //operands = new Operand[opndCount];

    for(int i=0; i<opndCount; i++)
    {
        operands[i] = Operand(&(detail->arm64.operands[i]));
    }

    Operand tmp; //Used for Operand swapping

    // Initialise instruction type
    coarseType = INSN_NORMAL;
    fineType = INSN_NORMAL2;

    switch(opcode){ // Some special cases
        case ARM64_INS_NOP:
            fineType = INSN_NOP;
            regReads = 0;
            regWrites = 0;
        break;
        case ARM64_INS_CBNZ:
        case ARM64_INS_CBZ:
            // Swap branch address to first operand (since rest of system is set up to believe the branch address comes first)
            tmp = operands[0];
            operands[0] = operands[1]; // Second operand is branch address here
            operands[1] = tmp;
        break;
        case ARM64_INS_TBNZ:
        case ARM64_INS_TBZ:
            // Swap branch address to first operand (since rest of system is set up to believe the branch address comes first)
            tmp = operands[0];
            operands[0] = operands[2]; // Third operand is branch address here
            operands[2] = tmp;
        break;
        case ARM64_INS_CMP:
            for(int i = 0; i < opndCount; i++)
            {
                operands[i].access = OPND_READ;
            }
        break;
        
        case ARM64_INS_LDR:
        case ARM64_INS_STR:
        case ARM64_INS_LDRB:
        case ARM64_INS_LDRH:
        case ARM64_INS_LDRSB:
        case ARM64_INS_LDRSH:
        case ARM64_INS_LDRSW:
        case ARM64_INS_LDTRB:
        case ARM64_INS_LDTRH:
        case ARM64_INS_LDTRSB:
        case ARM64_INS_LDTRSH:
        case ARM64_INS_LDTRSW:
        case ARM64_INS_LDTR:
        case ARM64_INS_LDURB:
        case ARM64_INS_LDUR:
        case ARM64_INS_LDURH:
        case ARM64_INS_LDURSB:
        case ARM64_INS_LDURSH:
        case ARM64_INS_LDURSW:

        case ARM64_INS_ST1:
        case ARM64_INS_ST2:
        case ARM64_INS_ST3:
        case ARM64_INS_ST4:
        case ARM64_INS_STRB:
        case ARM64_INS_STRH:
        case ARM64_INS_STTRB:
        case ARM64_INS_STTRH:
        case ARM64_INS_STTR:
        case ARM64_INS_STURB:
        case ARM64_INS_STUR:
        case ARM64_INS_STURH:
        {
            // Check load and store instructions for preindexing
            uint32_t raw_instr = 0;
            raw_instr = ((instr->bytes[3] << 24) | (instr->bytes[2] << 16) | (instr->bytes[1] << 8) | instr->bytes[0]);
            if( (!(raw_instr & (1 << IMMED_OFFSET_ZERO_BIT))) &&
                (!(raw_instr & (1 << PREPOST_ZERO_BIT))) &&
                (raw_instr & (1 << PREINDEX_BIT)) )
            {
                coarseType |= INSN_PREINDEX;
            }
            else if((!(raw_instr & (1 << IMMED_OFFSET_ZERO_BIT))) &&
                    (!(raw_instr & (1 << PREPOST_ZERO_BIT))) &&
                    (!(raw_instr & (1 << PREINDEX_BIT))) &&
                    (raw_instr & (1 << LDR_NON_LITERAL_BIT)))
            {
                coarseType |= INSN_POSTINDEX;
            }

        }
        break;

        case ARM64_INS_LDNP:
        case ARM64_INS_LDP:
        case ARM64_INS_LDPSW:
        case ARM64_INS_LDXP:

        case ARM64_INS_STNP:
        case ARM64_INS_STP:
        {
            // Check load-pair and store-pair instructions for preindexing
            uint32_t raw_instr = 0;
            raw_instr = ((instr->bytes[3] << 24) | (instr->bytes[2] << 16) | (instr->bytes[1] << 8) | instr->bytes[0]);
            if( (raw_instr & (1 << PAIR_PREPOST_BIT)) && (raw_instr & (1 << PAIR_PREINDEX_BIT)) )
            {
                coarseType |= INSN_PREINDEX;
            }
            else if( (raw_instr & (1 << PAIR_PREPOST_BIT)) && !(raw_instr & (1 << PAIR_PREINDEX_BIT)) )
            {
                coarseType |= INSN_POSTINDEX;
            }
        }
        break;

        case ARM64_INS_MOVN:
            // movn moves an inverted immediate so convert it to a movz of an immediate
            if(operands[0].size == 4){
                // For 32 bit values (i.e. going to a 32 bit reg), cast to be careful of signs
                int32_t imm = (int32_t)operands[1].intImm;
                imm = ~imm;
                operands[1].intImm = (int64_t)imm;
                opcode = ARM64_INS_MOVZ;
            }
            else if(operands[0].size == 8){
                operands[1].intImm = ~operands[1].intImm;
                opcode = ARM64_INS_MOVZ;
            }
        break;
    }

    //parse the instruction type

    int nGroup = detail->groups_count;
    for(int i=0; i<nGroup; i++)
    {
        switch(detail->groups[i]) {
            case CS_GRP_JUMP:
                coarseType |= INSN_CONTROL;

                switch(opcode){
                    case ARM64_INS_CBNZ:
                    case ARM64_INS_CBZ:
                    case ARM64_INS_TBNZ:
                    case ARM64_INS_TBZ:
                        fineType = INSN_CJUMP;
                    break;
                    case ARM64_INS_B:
                        /* B may or may not be conditional so need a check - if ARM64_CC_INVALID it has no condition,
                             ARM64_CC_AL and ARM64_CC_NV are condition codes that happen to always evaluate to 'true' */
                        if(detail->arm64.cc == ARM64_CC_INVALID || detail->arm64.cc == ARM64_CC_AL || detail->arm64.cc == ARM64_CC_NV)
                            fineType = INSN_JUMP;
                        else
                            fineType = INSN_CJUMP;
                    break;
                    case ARM64_INS_BL:
                    case ARM64_INS_BLR:
                        fineType = INSN_CALL;
                    break;
                    case ARM64_INS_RET:
                    case ARM64_INS_ERET:
                        fineType = INSN_RET;
                    default:
                        fineType = INSN_JUMP;
                    break;
                }
                break;
            case CS_GRP_CALL:
                coarseType |= INSN_CONTROL;
                fineType = INSN_CALL;
                break;
            case CS_GRP_RET: 
                coarseType |= INSN_CONTROL;
                fineType = INSN_RET;
                break;
            case CS_GRP_INT: 
                coarseType |= INSN_CONTROL;
                fineType = INSN_INT;
                break;
            case CS_GRP_IRET: 
                coarseType |= INSN_CONTROL;
                fineType = INSN_IRET;
                break;
            default:
                if(coarseType == INSN_NORMAL)
                    coarseType |= INSN_COMPLEX;
            break;
        }
    }


    int opsz = -1;
    for(int i=0; i<opndCount; i++) {
        if(operands[i].type == OPND_REG) {
            if(opsz < 0)
                opsz = operands[i].size; // Mem operands are probably reg size
            else {
                if(operands[i].size != opsz) { // If 2 different size reg.s, leave as-is
                    opsz = 0;
                }
            }
        }
    }

    /* get other coarse types */
    for(int i=0; i<opndCount; i++) {
        if(operands[i].isMemoryAccess()) {
            coarseType |= INSN_MEMORY;
            /* for the moment we can't determine whether it is a
             * stack access since base register is not analysed */

            //Set operand size based on that detected from reg.s,
            // but only if this enables a lower bound than the memory displacement
            if(opsz > 0 && operands[i].size > opsz) {
                operands[i].size = opsz;
            }
        }
        //TODO, may overlap with CMP type
        if(operands[i].isVectorRegister()) {
            coarseType |= INSN_FP;
        }
    }
}

/* Caller must check this is a control flow instruction */
PCAddress
MachineInstruction::getTargetAddress()
{
        if (opndCount &&
           (operands[0].type == OPND_IMM))
        {
            return operands[0].value;
        }

    return 0;
}

ostream& janus::operator<<(std::ostream& out, const MachineInstruction& instr)
{
    out <<instr.name;
    return out;
}

bool
MachineInstruction::hasMemoryReference()
{
    return isType(INSN_MEMORY);
}

bool
MachineInstruction::hasMemoryAccess()
{
    if (hasMemoryReference()) {
        if(opcode != ARM64_INS_ADR && opcode != ARM64_INS_ADRP)
            return true;
    }
    return false;
}

bool
MachineInstruction::isType(MachineInstCoarseType type)
{
    return (coarseType & (uint32_t)type);
}

bool
MachineInstruction::writeControlFlag()
{
    return (
                opcode == ARM64_INS_CMP    ||
                opcode == ARM64_INS_CMN    ||
                opcode == ARM64_INS_CCMP   ||
                opcode == ARM64_INS_CCMN   ||
                opcode == ARM64_INS_FCMP   ||
                opcode == ARM64_INS_FCMPE  ||
                opcode == ARM64_INS_FCCMP  ||
                opcode == ARM64_INS_FCCMPE
            );
}

bool
MachineInstruction::readControlFlag() /* Will have to add cond. instr. support to this */
{
    return (isType(INSN_CONTROL) && fineType == INSN_CJUMP &&
            (
                // CJUMP instructions that do not read condition flags (e.g. CBZ Compare and Branch if Zero - does its own comparing)
                opcode != ARM64_INS_CBZ  &&
                opcode != ARM64_INS_CBNZ &&
                opcode != ARM64_INS_TBZ  &&
                opcode != ARM64_INS_TBNZ
            )
            ||
            (
                // Other conditional instructions (that hence read the control flags)
                opcode == ARM64_INS_CCMP   ||
                opcode == ARM64_INS_CCMN   ||
                opcode == ARM64_INS_CSINC  ||
                opcode == ARM64_INS_CSINV  ||
                opcode == ARM64_INS_CSNEG  ||
                opcode == ARM64_INS_CSEL   ||
                opcode == ARM64_INS_CSET   ||
                opcode == ARM64_INS_CSETM  ||
                opcode == ARM64_INS_CINC   ||
                opcode == ARM64_INS_CINV   ||
                opcode == ARM64_INS_CNEG   ||
                opcode == ARM64_INS_FCCMP  ||
                opcode == ARM64_INS_FCCMPE ||
                opcode == ARM64_INS_FCSEL
            ));
}

bool
MachineInstruction::useReg(uint32_t reg)
{
    return (RegSet(regReads).contains(reg) ||
            RegSet(regWrites).contains(reg));
}

bool
MachineInstruction::readReg(uint32_t reg)
{
    for(int i = 0; i < opndCount; i++)
    {
        if(operands[i].type == OPND_REG && (operands[i].access & OPND_READ) && operands[i].reg == reg)
        {
            return true;
        }
    }
    return false;
    //return RegSet(regReads).contains(reg);
}

bool
MachineInstruction::writeReg(uint32_t reg)
{
    for(int i = 0; i < opndCount; i++)
    {
        if(operands[i].type == OPND_REG && (operands[i].access & OPND_WRITE) && operands[i].reg == reg)
        {
            return true;
        }
    }
    return false;
    //return RegSet(regWrites).contains(reg);
}

bool
MachineInstruction::isAllocatingStackFrame()
{
    bool isStackPtrDecr = false;
    bool readX29 = false;
    bool readX30 = false;
    if(isType(INSN_PREINDEX))
    {
        for(int i = 0; i < opndCount; i++)
        {
            if(operands[i].type == OPND_MEM &&
                (operands[i].mem.base == JREG_SP || operands[i].mem.base == JREG_WSP) &&
                 operands[i].mem.disp < 0)
            {
                isStackPtrDecr = true;
            }
            if(operands[i].type == OPND_REG && (operands[i].access & OPND_READ))
            {
                if(operands[i].reg == JREG_X29)
                {
                    readX29 = true;
                }
                if(operands[i].reg == JREG_X30)
                {
                    readX30 = true;
                }
            }

        }
    }
    return (isStackPtrDecr && readX29 && readX30);
}

int
MachineInstruction::getStackFrameAllocSize()
{// User of this function must check that a stack frame is being allocated using isAllocatingStackFrame()
    int frameSize = 0;
    
    for(int i = 0; i < opndCount; i++)
    {
        if(operands[i].type = OPND_MEM)
        {
            frameSize = (-1 * operands[i].mem.disp); // Mem displacement will be negative since stack is upside down
        }

    }
    return frameSize;
}

bool
MachineInstruction::isSubSP()
{
    return ((operands[0].type == OPND_REG && operands[1].type == OPND_REG)
            && (operands[0].reg == JREG_SP || operands[0].reg == JREG_WSP)
            && (operands[1].reg == JREG_SP || operands[1].reg == JREG_WSP)
            && (operands[2].type == OPND_IMM));
}

int MachineInstruction::getSPSubSize()
{// User of this function must check that the instruction is a { sub sp, sp, #immediate } instruction using isSubSP()
    return operands[2].intImm;
}

bool
MachineInstruction::isMoveSPToX29()
{
    bool writeX29 = false, readSP = false;
    // Note - mov reg, sp is an alias of add reg, sp, #0
    for(int i = 0; i < opndCount; i++)
    {
        if(operands[i].type == OPND_REG)
        {
            if(operands[i].reg == JREG_X29 && (operands[i].access & OPND_WRITE))
            {
                writeX29 = true;
            }
            if((operands[i].reg == JREG_SP || operands[i].reg == JREG_WSP) && (operands[i].access & OPND_READ))
            {
                readSP = true;
            }
        }
    }
    return (opcode == ARM64_INS_MOV && readSP && writeX29);
}


bool
MachineInstruction::isDeallocatingStackFrame()
{
    bool isStackPtrIncr = false;
    bool writeX29 = false;
    bool writeX30 = false;
    if(isType(INSN_POSTINDEX))
   {
        for(int i = 0; i < opndCount; i++)
        {
            if(operands[i].type == OPND_MEM &&
                (operands[i].mem.base == JREG_SP || operands[i].mem.base == JREG_WSP))
            {
                isStackPtrIncr = true;
            }
            if(operands[i].type == OPND_REG && (operands[i].access & OPND_WRITE))
            {
                if(operands[i].reg == JREG_X29)
                {
                    writeX29 = true;
                }
                if(operands[i].reg == JREG_X30)
                {
                    writeX30 = true;
                }
            }

        }
    }
    return (isStackPtrIncr && writeX29 && writeX30);
}

int
MachineInstruction::getStackFrameDeallocSize()
{// User of this function must check that a stack frame is being deallocated using isDeallocatingStackFrame()
    int frameSize = 0;
    
    for(int i = 0; i < opndCount; i++)
    {
        if(operands[i].type == OPND_IMM) // Assuming the only immediate will be the offset
        {
            frameSize = operands[i].intImm;
        }

    }
    return frameSize;
}


bool
MachineInstruction::isX29MemAccess()
{
    for(int i = 0; i < opndCount; i++)
    {
        if(operands[i].type == OPND_MEM && operands[i].mem.base == JREG_X29)
        {
            return true;
        }
    }
    return false;
}

bool
MachineInstruction::usesPropagatedStackFrame()
{
    return (aux == 1);
}

void
MachineInstruction::setUsesPropagatedStackFrame(bool value) /* Use aux location to store 1 if uses
                                                                 propagated x29 stack frame reference */
{
    if(value) aux = 1;
    else aux = 0;
}
/*

int
MachineInstruction::isMoveStackPCS_GRPointer()
{
    // For x86, it is sub rsp, #offset
    if (opcode != X86_INS_SUB) return 0;
    if (!(readReg(JREG_RSP) && writeReg(JREG_RSP))) return 0;
    if (operands[1].type != OPND_IMM) return 0;
    return operands[1].intImm;
}

bool
MachineInstruction::isMakingStackBase()
{
    if (readReg(JREG_RSP) && writeReg(JREG_RBP)) return true;
    return false;
}
,
bool
MachineInstruction::isRecoveringStackBase()
{
    if (writeReg(JREG_RBP) && writeReg(JREG_RSP)) return true;
    return false;
}



bool
MachineInstruction::isIndirectCall()
{
    if (opcode == ARM64_INS_BLR) {
        if (operands[0].type != OPND_IMM) return true;
    }
    return false;
}



bool
MachineInstruction::isConditionalMove()
{
    if (opcode >= X86_INS_CMOVA && opcode <=X86_INS_CMOVS) return true;
    return false;
}
*/
