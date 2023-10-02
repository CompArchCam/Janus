#include "MachineInstruction.h"
#include "Operand.h"
#include "BasicBlock.h"
#include "Function.h"
#include "IO.h"
#include "Utility.h"
#include "capstone/capstone.h"
#include "janus_arch.h"
#include <iostream>
#include <sstream>
#include <cstring>

using namespace janus;
using namespace std;

static char *get_eflag_name(uint64_t flag);

MachineInstruction::MachineInstruction(InstID id, void *cs_handle, void *cs_instr)
:id(id)
{
    cs_insn *instr = (cs_insn *)cs_instr;
    cs_detail *detail = instr->detail;

    pc = instr->address;
    opcode = instr->id;

    cs_x86 *x86;
    x86 = &(detail->x86);
    eflags = x86->eflags;
   // cout<<"elfags: "<<hex<<eflags<<endl;
    /* Assign instruction names */
    int mnemonicLength = strlen(instr->mnemonic);
    int opLength = strlen(instr->op_str);
    name = (char *)malloc(mnemonicLength + opLength + 2);
    strncpy(name, instr->mnemonic, mnemonicLength);
    name[mnemonicLength] = ' ';
    strncpy(name + mnemonicLength + 1, instr->op_str, opLength + 1);
    name[mnemonicLength + opLength + 1] = '\0';

    size = instr->size;

    opndCount = detail->x86.op_count;

    //allocate operand array
    operands = (Operand *)malloc(sizeof(Operand) * opndCount);
    //operands = new Operand[opndCount];

    for(int i=0; i<opndCount; i++)
    {
        operands[i] = Operand(&(detail->x86.operands[i]));
    }

    //parse the instruction type
    coarseType = INSN_NORMAL;
    fineType = INSN_NORMAL2;

    if(opcode == X86_INS_NOP) {
        fineType = INSN_NOP;
        regReads = 0;
        regWrites = 0;
       return;
    } else if (opcode == X86_INS_MOVQ ||
               opcode == X86_INS_MOVD) {
        //a capstone bug
        operands[0].access = OPND_WRITE;
    } else if(opcode == X86_INS_CMP ||
              opcode == X86_INS_TEST ||
              opcode == X86_INS_UCOMISS) {
        fineType = INSN_CMP;
        //another capstone bug
        operands[0].access = OPND_READ;
        operands[1].access = OPND_READ;
    }

    int nGroup = detail->groups_count;
    for(int i=0; i<nGroup; i++)
    {
        switch(detail->groups[i]) {
            case CS_GRP_JUMP:
                coarseType |= INSN_CONTROL;

                if((opcode >= X86_INS_JAE && opcode <= X86_INS_JS)&&(opcode != X86_INS_JMP))
                    fineType = INSN_CJUMP;
                else if(opcode == X86_INS_JMP)
                    fineType = INSN_JUMP;
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

    /* get other coarse types */
    for(int i=0; i<opndCount; i++) {
        if(operands[i].isMemoryAccess()) {
            coarseType |= INSN_MEMORY;
            /* for the moment we can't determine whether it is a
             * stack access since base register is not analysed */
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
        if (opcode != X86_INS_LEA)
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
    switch (opcode) {
        case X86_INS_SUB:
        case X86_INS_CMP:
        case X86_INS_TEST:
        case X86_INS_UCOMISD:
            return true;
        default:
            return false;
    }
    return false;
}

bool
MachineInstruction::readControlFlag()
{
    //add conditional move later
    return (isType(INSN_CONTROL) && fineType == INSN_CJUMP);
}

uint32_t
MachineInstruction::updatesOFlag(){ //overflow flag
   return ((eflags & (OF_FLAG_MODIFIED | OF_FLAG_SET | OF_FLAG_RESET)) ? 1 : 0);
}
uint32_t
MachineInstruction::readsOFlag(){
   return ((eflags & OF_FLAG_READ) ? 1 : 0);
}
uint32_t
MachineInstruction::readsArithFlags(){ // AF,SF, ZF, CF, PF,OF
   if(opcode == X86_INS_SBB || opcode == X86_INS_ADC) return 1;
   return ((eflags & ARITH_FLAGS_READ) ? 1 : 0);
}
uint32_t
MachineInstruction::updatesArithFlags(){
   cout<<hex<<"eflags: "<<eflags<<endl;
   return ((eflags & (ARITH_FLAGS_MODIFIED | ARITH_FLAGS_SET | ARITH_FLAGS_RESET)) ? 1 : 0);
}
uint32_t
MachineInstruction::readsintArithFlags(){ // AF,SF, ZF, CF, PF,OF
   if(isFPU87()) return 0;
   if(opcode == X86_INS_SBB || opcode == X86_INS_ADC) return 1;
   return ((eflags & ARITH_FLAGS_READ) ? 1 : 0);
}
uint32_t
MachineInstruction::updatesintArithFlags(){ // AF,SF, ZF, CF, PF,OF
   if(isFPU87()) return 0;
   return ((eflags & (ARITH_FLAGS_MODIFIED | ARITH_FLAGS_SET | ARITH_FLAGS_RESET)) ? 1 : 0);
}
uint32_t
MachineInstruction::readsStatusFlags(){ //AF,SF, ZF, CF, PF
   if(opcode == X86_INS_SBB) return 1;
   return ((eflags & STATUS_FLAG_READ) ? 1 : 0);

}
uint32_t MachineInstruction::updatesStatusFlags(){
   return ((eflags & (STATUS_FLAG_MODIFIED | STATUS_FLAG_SET | STATUS_FLAG_RESET)) ? 1 : 0);
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
    return RegSet(regReads).contains(reg);
}

bool
MachineInstruction::writeReg(uint32_t reg)
{
    return RegSet(regWrites).contains(reg);
}

int
MachineInstruction::isMoveStackPointer()
{
    /* For x86, it is sub rsp, #offset */
    if (opcode != X86_INS_SUB) return 0;
    if (operands[0].type != OPND_REG && operands[0].reg != JREG_RSP) return 0;
    if (operands[1].type != OPND_IMM) return 0;
    return operands[1].intImm;
}

int
MachineInstruction::isAddStackPointer()
{
    /* For x86, it is add rsp, #offset */
    if (opcode != X86_INS_ADD) return 0;
    if (operands[0].type != OPND_REG || operands[0].reg != JREG_RSP) return 0;
    if (operands[1].type != OPND_IMM) return 0;
    return operands[1].intImm;
}

bool
MachineInstruction::isMakingStackBase()
{
    if(operands[0].type != OPND_REG || operands[1].type != OPND_REG) return false;
    if(operands[0].reg == JREG_RBP && operands[1].reg == JREG_RSP) return true;
    return false;
}

bool
MachineInstruction::isRecoveringStackBase()
{
    if (writeReg(JREG_RBP) && writeReg(JREG_RSP)) return true;
    return false;
}

bool
MachineInstruction::isIndirectCall()
{
    if (opcode == X86_INS_CALL) {
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

bool
MachineInstruction::isCall()
{
    return (opcode == X86_INS_CALL);
}

bool
MachineInstruction::isReturn()
{
    return (opcode == X86_INS_RET);
}
bool
MachineInstruction::isLeave(){

    return (opcode == X86_INS_LEAVE);
}

bool MachineInstruction::checksStackCanary(){
  if(opcode == X86_INS_XOR){ //for g++
       if(operands[1].structure == OPND_SEGMEM &&
                operands[1].segMem.segment == JREG_FS &&
                        operands[1].segMem.disp == 0x28
                                && operands[0].type == OPND_REG)
          return true;

   }
   return false;
}
bool MachineInstruction::readsStackCanary(){
   if(isMOV()){
       if(operands[1].structure == OPND_SEGMEM &&
                operands[1].segMem.segment == JREG_FS
                        && operands[1].segMem.disp == 40
                                && operands[0].type == OPND_REG){
           return true;
       }

   }
   return false;
}

//TODO explore all opcodes that do this or implement otherwise
bool
MachineInstruction::isMOV()
{
    return (opcode == X86_INS_MOV       ||
            opcode == X86_INS_MOVD      ||
            opcode == X86_INS_MOVQ      ||
            opcode == X86_INS_MOVAPS    ||
            opcode == X86_INS_MOVAPD    ||
            opcode == X86_INS_MOVUPS    || 
            opcode == X86_INS_MOVSD     ||
            opcode == X86_INS_MOVSS     ||
            opcode == X86_INS_MOVSXD    ||
            opcode == X86_INS_VMOVSS    ||
            opcode == X86_INS_VMOVAPS   ||
            opcode == X86_INS_VUCOMISS  ||
            opcode == X86_INS_CVTSI2SD  ||
            opcode == X86_INS_CVTSI2SS  ||
            opcode == X86_INS_CVTTSS2SI ||
            opcode == X86_INS_CVTDQ2PD  ||
            opcode == X86_INS_CVTDQ2PS  ||
            opcode == X86_INS_CVTPD2DQ  ||
            opcode == X86_INS_CVTPD2PS  ||
            opcode == X86_INS_CVTPS2DQ  ||
            opcode == X86_INS_CVTPS2PD  ||
            opcode == X86_INS_CVTSD2SI  ||
            opcode == X86_INS_CVTSD2SS  ||
            opcode == X86_INS_CVTSS2SD  ||
            opcode == X86_INS_CVTSS2SI   ||
            opcode == X86_INS_CVTTPD2DQ ||
            opcode == X86_INS_CVTTPS2DQ ||
            opcode == X86_INS_CVTTSD2SI ||
            opcode == X86_INS_MOVSXD    ||
            opcode == X86_INS_MOVDQA    ||
            opcode == X86_INS_MOVDQU    ||
            opcode == X86_INS_MOVZX     ||
            opcode == X86_INS_MOVSX     ||
            opcode == X86_INS_MOVSW     ||
            opcode == X86_INS_MOVABS    ||
            opcode == X86_INS_CDQ       ||
            opcode == X86_INS_CDQE      ||
                      //conditional move
            opcode == X86_INS_CMOVA     ||
            opcode == X86_INS_CMOVAE    ||
            opcode == X86_INS_CMOVB     ||
            opcode == X86_INS_CMOVBE    ||
            opcode == X86_INS_FCMOVBE   ||
            opcode == X86_INS_FCMOVB    ||
            opcode == X86_INS_CMOVE     ||
            opcode == X86_INS_FCMOVE    ||
            opcode == X86_INS_CMOVG     ||
            opcode == X86_INS_CMOVGE    ||
            opcode == X86_INS_CMOVL     ||
            opcode == X86_INS_CMOVLE    ||
            opcode == X86_INS_FCMOVNBE  ||
            opcode == X86_INS_FCMOVNB   ||
            opcode == X86_INS_CMOVNE    ||
            opcode == X86_INS_FCMOVNE   ||
            opcode == X86_INS_CMOVNO    ||
            opcode == X86_INS_CMOVNP    ||
            opcode == X86_INS_FCMOVNU   ||
            opcode == X86_INS_CMOVNS    ||
            opcode == X86_INS_CMOVO     ||
            opcode == X86_INS_CMOVP     ||
            opcode == X86_INS_FCMOVU    ||
            opcode == X86_INS_CMOVS);
}

bool
MachineInstruction::isMOV_imm(int *output) {
    if (opcode == X86_INS_MOV) {
        if (operands[1].type == OPND_IMM) {
            *output = operands[1].intImm;
            return true;
        }
    }
    return false;
}

bool
MachineInstruction::isMOV_partial() {
    return (opcode == X86_INS_VCVTSI2SS ||
            opcode == X86_INS_VCVTSS2SD ||
            opcode == X86_INS_VCVTSI2SD);
}

bool
MachineInstruction::isLEA()
{
    return (opcode == X86_INS_LEA);
}
//TODO: missing opcodes here. match with opcodes in Disassemble.cpp
bool
MachineInstruction::isADD()
{
    return (opcode == X86_INS_ADD    ||
            opcode == X86_INS_ADDSS  ||
            opcode == X86_INS_ADDSD  ||
            opcode == X86_INS_ADDPD  ||
            opcode == X86_INS_ADDPS  ||
            opcode == X86_INS_FADD  ||
            opcode == X86_INS_FIADD  ||
            opcode == X86_INS_FADDP  ||
            opcode == X86_INS_PADDB  ||
            opcode == X86_INS_PADDSB  ||
            opcode == X86_INS_PADDSW  ||
            opcode == X86_INS_PADDW  ||
            opcode == X86_INS_PADDUSB  ||
            opcode == X86_INS_PADDUSW  ||
            opcode == X86_INS_VADDSS ||
            opcode == X86_INS_VADDSD ||
            opcode == X86_INS_VADDPS ||
            opcode == X86_INS_VADDPD ||
            opcode == X86_INS_PADDD  ||
            opcode == X86_INS_PADDQ);
}

bool
MachineInstruction::isSUB() {
    return (opcode == X86_INS_SUB    ||
            opcode == X86_INS_SUBSS  ||
            opcode == X86_INS_SUBSD  ||
            opcode == X86_INS_VSUBSS ||
            opcode == X86_INS_VSUBSD);
}

bool
MachineInstruction::isINC()
{
    return (opcode == X86_INS_INC);
}

bool
MachineInstruction::isDEC()
{
    return (opcode == X86_INS_DEC);
}

bool
MachineInstruction::isNEG()
{
    return (opcode == X86_INS_NEG || opcode == X86_INS_NOT);
}


bool
MachineInstruction::isPUSH() {
    return (opcode == X86_INS_PUSH);
}

bool
MachineInstruction::isPOP() {
    return (opcode == X86_INS_POP);
}

bool
MachineInstruction::isXORself() {
    if (opcode == X86_INS_VXORPS ||
        opcode == X86_INS_VXORPD)
        if (opndCount == 3 && operands[0].reg == operands[1].reg && operands[1].reg == operands[2].reg)
        return true;
    return (opcode == X86_INS_XOR   ||
            opcode == X86_INS_PXOR  ||
            opcode == X86_INS_XORPS || 
            opcode == X86_INS_XORPD) && opndCount == 2 && operands[0].reg == operands[1].reg;
}

bool
MachineInstruction::isTypeChange() {
    return (opcode == X86_INS_CDQE || opcode == X86_INS_CDQ);
}

int
MachineInstruction::isShiftRight() {
    if (opcode == X86_INS_SHR || opcode == X86_INS_SAR) {
        if (operands[1].type == OPND_IMM)
            return (int)operands[1].intImm;
    }
    return -1;
}

int
MachineInstruction::isShiftLeft() {
    if (opcode == X86_INS_SHL || opcode == X86_INS_SAL) {
        if (operands[1].type == OPND_IMM)
            return (int)operands[1].intImm;
    }
    return -1;
}

bool
MachineInstruction::isVectorInstruction() {
    for (int i=0; i<opndCount; i++) {
        if (operands[i].isVectorRegister()) {
            return true;
        }
    }
    return false;
}

bool
MachineInstruction::isXMMInstruction() {
    for (int i=0; i<opndCount; i++) {
        if (operands[i].isXMMRegister()) {
            return true;
        }
    }
    return false;
}

bool
MachineInstruction::isCMP() {
    return (opcode == X86_INS_CMP ||
            opcode == X86_INS_TEST ||
            opcode == X86_INS_UCOMISD);
}

bool
MachineInstruction::isMUL() {
    return (opcode == X86_INS_IMUL  ||
            opcode == X86_INS_MULSD ||
            opcode == X86_INS_MULPD ||
            opcode == X86_INS_MULSS ||
            opcode == X86_INS_VMULSD||
            opcode == X86_INS_VMULSS);
}

bool
MachineInstruction::isDIV() {
    return (opcode == X86_INS_IDIV  ||
            opcode == X86_INS_DIVSD ||
            opcode == X86_INS_DIVSS ||
            opcode == X86_INS_DIVPD ||
            opcode == X86_INS_VDIVSD||
            opcode == X86_INS_VDIVSS);
}

bool
MachineInstruction::isNotSupported() {
    if (opcode == X86_INS_SBB) return true;
    if (opcode == X86_INS_ADC) return true;
    return false;
}

//TODO recognize (some) opcodes, which do NOT have implicit operands
bool
MachineInstruction::hasImplicitOperands() {
    if ((opcode == X86_INS_LEA) || (opcode == X86_INS_MOV) || (opcode == X86_INS_MOVSXD)) return false;
    return true;
}

bool
MachineInstruction::isFPinstruction(){
    return (isFPU87() || isFP_SSE() || isFP_AVX());
}

//TODO: SSE1, SSE2 , SSE3, SSE4
bool
MachineInstruction::isFP_SSE(){
    return   (opcode == X86_INS_ADDPD ||
            opcode ==  X86_INS_ADDPS ||
            opcode == X86_INS_ADDSD ||
            opcode == X86_INS_ADDSS ||
            opcode == X86_INS_SUBSD ||
            opcode == X86_INS_SUBSS ||
            opcode == X86_INS_SUBPD ||
            opcode == X86_INS_SUBPS ||
            opcode == X86_INS_MULSD ||
            opcode == X86_INS_MULPD ||
            opcode == X86_INS_MULPS ||
            opcode == X86_INS_MULSS ||
            opcode == X86_INS_DIVSD ||
            opcode == X86_INS_DIVSS ||
            opcode == X86_INS_DIVPD || 
            opcode == X86_INS_DIVPS ||
            opcode == X86_INS_ANDPD ||
            opcode == X86_INS_ANDPS ||
            opcode ==  X86_INS_ANDNPD || 
            opcode == X86_INS_ANDNPS );
}
//TODO: AVX, AVX2, AVX512
bool
MachineInstruction::isFP_AVX(){
    return  (opcode ==  X86_INS_VADDPD ||
            opcode == X86_INS_VADDPS ||
            opcode == X86_INS_VADDSD ||
            opcode == X86_INS_VADDSS ||
            opcode == X86_INS_VMULSD ||
            opcode == X86_INS_VMULSS ||
            opcode == X86_INS_VMULPD ||
            opcode == X86_INS_VMULPS ||
            opcode == X86_INS_VDIVSD ||
            opcode == X86_INS_VDIVSS ||
            opcode == X86_INS_VDIVPS ||
            opcode == X86_INS_VDIVPD ||
            opcode == X86_INS_VANDNPD || 
            opcode == X86_INS_VANDNPS ||
            opcode == X86_INS_VANDPD ||
            opcode == X86_INS_VANDPS);
}

bool
MachineInstruction::isFPU87(){
    return
        (opcode == X86_INS_FABS ||
        opcode == X86_INS_FADD ||
        opcode == X86_INS_FADDP ||
        opcode == X86_INS_FBLD ||
        opcode == X86_INS_FBSTP ||
        opcode == X86_INS_FCHS ||
        opcode == X86_INS_FCOM ||
        opcode == X86_INS_FCOMP ||
        opcode == X86_INS_FCOMPP ||
        opcode == X86_INS_FDECSTP ||
        opcode == X86_INS_FDIV ||
        opcode == X86_INS_FDIVP ||
        opcode == X86_INS_FDIVR ||
        opcode == X86_INS_FDIVRP ||
        opcode == X86_INS_FFREE ||
        opcode == X86_INS_FIADD ||
        opcode == X86_INS_FICOM ||
        opcode == X86_INS_FICOMP ||
        opcode == X86_INS_FIDIV ||
        opcode == X86_INS_FIDIVR ||
        opcode == X86_INS_FILD ||
        opcode == X86_INS_FIMUL ||
        opcode == X86_INS_FINCSTP ||
        opcode == X86_INS_FIST ||
        opcode == X86_INS_FISTP ||
        opcode == X86_INS_FISUB ||
        opcode == X86_INS_FISUBR ||
        opcode == X86_INS_FLD ||
        opcode == X86_INS_FLD1 ||
        opcode == X86_INS_FLDCW ||
        opcode == X86_INS_FLDENV ||
        opcode == X86_INS_FLDL2E ||
        opcode == X86_INS_FLDL2T ||
        opcode == X86_INS_FLDLG2 ||
        opcode == X86_INS_FLDLN2 ||
        opcode == X86_INS_FLDPI ||
        opcode == X86_INS_FLDZ ||
        opcode == X86_INS_FMUL ||
        opcode == X86_INS_FMULP ||
        opcode == X86_INS_FNCLEX ||
        opcode == X86_INS_FNINIT ||
        opcode == X86_INS_FNOP ||
        opcode == X86_INS_FNSAVE ||
        opcode == X86_INS_FNSTCW ||
        opcode == X86_INS_FNSTENV ||
        opcode == X86_INS_FNSTSW ||
        opcode == X86_INS_FPATAN ||
        opcode == X86_INS_FPREM ||
        opcode == X86_INS_FPTAN ||
        opcode == X86_INS_FRNDINT ||
        opcode == X86_INS_FRSTOR ||
        opcode == X86_INS_FSCALE ||
        opcode == X86_INS_FSQRT ||
        opcode == X86_INS_FST ||
        opcode == X86_INS_FSTP ||
        opcode == X86_INS_FSUB ||
        opcode == X86_INS_FSUBP ||
        opcode == X86_INS_FSUBR ||
        opcode == X86_INS_FSUBRP ||
        opcode == X86_INS_FTST ||
        opcode == X86_INS_FXAM ||
        opcode == X86_INS_FXCH ||
        opcode == X86_INS_FXTRACT ||
        opcode == X86_INS_FYL2X);
      
}
static char *get_eflag_name(uint64_t flag)
{
        switch(flag) {
                default:
                        return NULL;
                case X86_EFLAGS_UNDEFINED_OF:
                        return "UNDEF_OF";
                case X86_EFLAGS_UNDEFINED_SF:
                        return "UNDEF_SF";
                case X86_EFLAGS_UNDEFINED_ZF:
                        return "UNDEF_ZF";
                case X86_EFLAGS_MODIFY_AF:
                        return "MOD_AF";
                case X86_EFLAGS_UNDEFINED_PF:
                        return "UNDEF_PF";
                case X86_EFLAGS_MODIFY_CF:
                        return "MOD_CF";
                case X86_EFLAGS_MODIFY_SF:
                        return "MOD_SF";
                case X86_EFLAGS_MODIFY_ZF:
                        return "MOD_ZF";
                 case X86_EFLAGS_UNDEFINED_AF:
                        return "UNDEF_AF";
               case X86_EFLAGS_MODIFY_PF:
                        return "MOD_PF";
                case X86_EFLAGS_UNDEFINED_CF:
                        return "UNDEF_CF";
                case X86_EFLAGS_MODIFY_OF:
                        return "MOD_OF";
                case X86_EFLAGS_RESET_OF:
                        return "RESET_OF";
                case X86_EFLAGS_RESET_CF:
                        return "RESET_CF";
                case X86_EFLAGS_RESET_DF:
                        return "RESET_DF";
                case X86_EFLAGS_RESET_IF:
                        return "RESET_IF";
                case X86_EFLAGS_TEST_OF:
                        return "TEST_OF";
                case X86_EFLAGS_TEST_SF:
                        return "TEST_SF";
                case X86_EFLAGS_TEST_ZF:
                        return "TEST_ZF";
                case X86_EFLAGS_TEST_PF:
                        return "TEST_PF";
                case X86_EFLAGS_TEST_CF:
                        return "TEST_CF";
                case X86_EFLAGS_RESET_SF:
                        return "RESET_SF";
                case X86_EFLAGS_RESET_AF:
                        return "RESET_AF";
                case X86_EFLAGS_RESET_TF:
                        return "RESET_TF";
                case X86_EFLAGS_RESET_NT:
                        return "RESET_NT";
                case X86_EFLAGS_PRIOR_OF:
                        return "PRIOR_OF";
                case X86_EFLAGS_PRIOR_SF:
                        return "PRIOR_SF";
                case X86_EFLAGS_PRIOR_ZF:
                        return "PRIOR_ZF";
                case X86_EFLAGS_PRIOR_AF:
                        return "PRIOR_AF";
                case X86_EFLAGS_PRIOR_PF:
                        return "PRIOR_PF";
                case X86_EFLAGS_PRIOR_CF:
                        return "PRIOR_CF";
                case X86_EFLAGS_PRIOR_TF:
                        return "PRIOR_TF";
                case X86_EFLAGS_PRIOR_IF:
                        return "PRIOR_IF";
                case X86_EFLAGS_PRIOR_DF:
                        return "PRIOR_DF";
                case X86_EFLAGS_TEST_NT:
                        return "TEST_NT";
                case X86_EFLAGS_TEST_DF:
                        return "TEST_DF";
                case X86_EFLAGS_RESET_PF:
                        return "RESET_PF";
                case X86_EFLAGS_PRIOR_NT:
                        return "PRIOR_NT";
                case X86_EFLAGS_MODIFY_TF:
                        return "MOD_TF";
                case X86_EFLAGS_MODIFY_IF:
                        return "MOD_IF";
                case X86_EFLAGS_MODIFY_DF:
                        return "MOD_DF";
                case X86_EFLAGS_MODIFY_NT:
                        return "MOD_NT";
                case X86_EFLAGS_MODIFY_RF:
                        return "MOD_RF";
                case X86_EFLAGS_SET_CF:
                        return "SET_CF";
                case X86_EFLAGS_SET_DF:
                        return "SET_DF";
                case X86_EFLAGS_SET_IF:
                        return "SET_IF";
        }
}
void
MachineInstruction::printEFlags(){
     if(eflags){
        printf("\tEFLAGS:");
        for(int i = 0; i <= 45; i++)
        if (eflags & ((uint64_t)1 << i)) {
                printf(" %s", get_eflag_name((uint64_t)1 << i));
        }
        printf("\n");
     }
}
