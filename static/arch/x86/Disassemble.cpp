#include "Disassemble.h"
#include "JanusContext.h"
#include "Function.h"
#include "Utility.h"
#include "capstone/capstone.h"
#include "janus_arch.h"
#include "IO.h"

using namespace janus;
using namespace std;

/* Generate output states for a given instruction */
static void liftInstruction(Instruction &instr, Function *function);
static void linkRelocation(JanusContext *jc, Function *pltFunc);

void disassembleAll(JanusContext *jc)
{
    GSTEP("Disassembling instructions: ");
    cs_err      err;
    //initialise capstone disassembly engine
    //TODO, recognise architecture automatically
    err = cs_open(CS_ARCH_X86, CS_MODE_64, (csh *)(&jc->program.capstoneHandle));

    if (err) {
        printf("Failed on cs_open() in capstone with error returned: %u\n", err);
        exit(-1);
    }

    // we need the details for each instruction
    cs_option((csh)jc->program.capstoneHandle, CS_OPT_DETAIL, CS_OPT_ON);
    //skip the padding between the instructions
    cs_option((csh)jc->program.capstoneHandle, CS_OPT_SKIPDATA, CS_OPT_ON);

    //Disassemble each function
    for (auto &func: jc->functions) {
        //disassemble the function and register the function lookup table
        if (func.isExecutable) {
            disassemble(&func);
            jc->functionMap[func.startAddress] = &func;
        }
        //if the function is calling shared library call
        else {
            jc->externalFunctions[func.startAddress] = &func;
        }
    }
    GSTEPCONT(jc->functions.size()<<" functions recognised"<<endl);
    //NOTE: once all external calls are registered
    //We also need to link the PLT relocation address to external shared library calls
    bool foundFortranMain = false;
    Function *fmain = NULL;

    for (auto &func: jc->functions) {
        if (func.name == string(".plt") || func.name == string("_plt")) {
            func.isExecutable = false;
            linkRelocation(jc, &func);
            continue;
        }
        if (func.name == string("main") && !foundFortranMain) {
            jc->main = &func;
        }
        if (func.name == string("_start")) {
            //fmain = &func;
        }
        if (func.name == string("MAIN__")) {
            fmain = &func;
            foundFortranMain = true;
        }
    }

    if (foundFortranMain) jc->main = fmain;

    cs_close((csh *)(&jc->program.capstoneHandle));
}

static void linkRelocation(JanusContext *jc, Function *pltFunc)
{
    if (!pltFunc) return;

    uint32_t size = pltFunc->minstrs.size();

    if (!size) return;

    if (size % 3) { //TODO: this is not portable for binaries where .plt section may not be multiple of 3
        cout << "function "<<pltFunc->name<<" may not be a PLT section" << endl;
        return;
    }
    jc->pltsection = true;

    uint32_t nPltSym = size / 3;

    uint32_t i = 1;

    for (auto synthetic: jc->externalFunctions)
    {
        Function *synFunc = synthetic.second;
        synFunc->startAddress = pltFunc->minstrs[3*i].pc;
        synFunc->endAddress = pltFunc->minstrs[3*i+2].pc;
        synFunc->size = synFunc->endAddress - synFunc->startAddress;
        synFunc->name += "@plt";
        synFunc->isExternal = true;
        //update in the function map
        jc->functionMap[synFunc->startAddress] = synFunc;
        i++;
        //HACK: to build BB for plt stubs to be used for instrumentation 
        int offset = synFunc->startAddress - pltFunc->startAddress;
        synFunc->contents = pltFunc->contents + offset;
        disassemble(synFunc);
        
        if (i==nPltSym) break;
    }
}

///Disassemble for the given function
void disassemble(Function *function)
{
    //if already disassembled, return
    if(function->minstrs.size()) return;

    uint64_t handle = function->context->program.capstoneHandle;
    cs_insn             *instr;
    InstID              id = 0;

    uint8_t             *code = function->contents;
    size_t              codeSize = function->size;
    PCAddress           pc = function->startAddress;

    instr = cs_malloc((csh)handle);

    //reserve machine instructions
    function->minstrs.reserve(codeSize+1);
    while(cs_disasm_iter((csh)handle, (const uint8_t **)(&code), &codeSize, (uint64_t *)&pc, instr)) {
        function->minstrs.emplace_back(id,(void *)handle,(void *)instr);
        id++;
    }

    //second step, register the address map
    for (auto &instr:function->minstrs) {
        function->minstrTable[instr.pc] = instr.id;
    }
    cs_free(instr, 1);

    /* third step: create place holders for abstract Instructions */
    int instrCount = function->minstrs.size();
    //reserve space for abstract instructions
    function->instrs.reserve(instrCount);

    //get machine instruction data
    MachineInstruction *minstrs = function->minstrs.data();

    for (int i=0; i<instrCount; i++) {
        function->instrs.emplace_back(minstrs + i);
    }
}

Instruction::Opcode
liftOpcode(MachineInstruction *minstr)
{
    if (!minstr) return Instruction::Undefined;
    if (minstr->isType(INSN_CONTROL)) {
        switch (minstr->fineType) {
            case INSN_JUMP: return Instruction::DirectBranch;
            case INSN_CJUMP: return Instruction::ConditionalBranch;
            case INSN_CALL: return Instruction::Call;
            case INSN_RET: return Instruction::Return;
            case INSN_INT: return Instruction::Interupt;
            default: return Instruction::Undefined;
        }
    } else {
        switch (minstr->opcode) {
            case X86_INS_NOP:
                return Instruction::Nop;
            //add
            case X86_INS_ADD:
            case X86_INS_INC:
            case X86_INS_ADDPD:
            case X86_INS_ADDPS:
            case X86_INS_ADDSD:
            case X86_INS_ADDSS:
            case X86_INS_FADD:
            case X86_INS_FIADD:
            case X86_INS_FADDP:
            case X86_INS_PADDB:
            case X86_INS_PADDD:
            case X86_INS_PADDQ:
            case X86_INS_PADDSB:
            case X86_INS_PADDSW:
            case X86_INS_PADDUSB:
            case X86_INS_PADDUSW:
            case X86_INS_PADDW:
            case X86_INS_VADDPD:
            case X86_INS_VADDPS:
            case X86_INS_VADDSD:
            case X86_INS_VADDSS:
                return Instruction::Add;
            //sub
            case X86_INS_SUB:
            case X86_INS_DEC:
            case X86_INS_PSUBB:
            case X86_INS_PSUBD:
            case X86_INS_PSUBQ:
            case X86_INS_PSUBSB:
            case X86_INS_PSUBSW:
            case X86_INS_PSUBUSB:
            case X86_INS_PSUBUSW:
            case X86_INS_PSUBW:
            case X86_INS_PFSUBR:
            case X86_INS_PFSUB:
            case X86_INS_SUBPD:
            case X86_INS_SUBPS:
            case X86_INS_FSUBR:
            case X86_INS_FISUBR:
            case X86_INS_FSUBRP:
            case X86_INS_SUBSD:
            case X86_INS_SUBSS:
            case X86_INS_FSUB:
            case X86_INS_FISUB:
            case X86_INS_FSUBP:
                return Instruction::Sub;
            //mul
            case X86_INS_MUL:
            case X86_INS_IMUL:
            case X86_INS_MULSD:
            case X86_INS_MULPD:
            case X86_INS_MULSS:
            case X86_INS_VMULSD:
            case X86_INS_VMULSS:
                return Instruction::Mul;
            //div
            case X86_INS_DIV:
            case X86_INS_IDIV:
            case X86_INS_DIVSD:
            case X86_INS_DIVSS:
            case X86_INS_DIVPD:
            case X86_INS_VDIVSD:
            case X86_INS_VDIVSS:
                return Instruction::Div;
            //rem

            //shl
            case X86_INS_SHL:
            case X86_INS_SAL:
                return Instruction::Shl;
            //lshr
            case X86_INS_SHR:
                return Instruction::LShr;
            //ashr
            case X86_INS_SAR:
                return Instruction::AShr;
            //and
            case X86_INS_AND:
            case X86_INS_ANDPD:
            case X86_INS_ANDPS:
                return Instruction::And;
            //or
            case X86_INS_OR:
                return Instruction::Or;
            //xor
            case X86_INS_XOR:
            case X86_INS_PXOR:
            case X86_INS_XORPD:
            case X86_INS_VXORPS:
            case X86_INS_VXORPD:
            case X86_INS_XORPS:
                return Instruction::Xor;
            //mov
            case X86_INS_MOV:
            case X86_INS_MOVD:
            case X86_INS_MOVQ:
            case X86_INS_MOVAPS:
            case X86_INS_MOVAPD:
            case X86_INS_MOVSD:
            case X86_INS_MOVSS:
            case X86_INS_MOVSX:
            case X86_INS_MOVSXD:
            case X86_INS_VMOVSS:
            case X86_INS_VMOVAPS:
            case X86_INS_CVTSI2SD:
            case X86_INS_CVTSI2SS:
            case X86_INS_CVTTSS2SI:
            case X86_INS_CVTDQ2PD:
            case X86_INS_CVTDQ2PS:
            case X86_INS_CVTPD2DQ:
            case X86_INS_CVTPD2PS:
            case X86_INS_CVTPS2DQ:
            case X86_INS_CVTPS2PD:
            case X86_INS_CVTSD2SI:
            case X86_INS_CVTSD2SS:
            case X86_INS_CVTSS2SD:
            case X86_INS_CVTSS2SI:
            case X86_INS_CVTTPD2DQ:
            case X86_INS_CVTTPS2DQ:
            case X86_INS_CVTTSD2SI:
            case X86_INS_MOVZX:
            case X86_INS_MOVDQA:
            case X86_INS_MOVDQU:
            case X86_INS_MOVUPD:
            case X86_INS_MOVUPS:
            case X86_INS_MOVLPS:
            case X86_INS_MOVABS:
            case X86_INS_MOVHLPS:
            case X86_INS_MOVHPD:
            case X86_INS_MOVHPS:
            case X86_INS_UNPCKLPD:
            case X86_INS_CDQE:
            case X86_INS_CDQ:
            //conditional move
            case X86_INS_CMOVA:
            case X86_INS_CMOVAE:
            case X86_INS_CMOVB:
            case X86_INS_CMOVBE:
            case X86_INS_FCMOVBE:
            case X86_INS_FCMOVB:
            case X86_INS_CMOVE:
            case X86_INS_FCMOVE:
            case X86_INS_CMOVG:
            case X86_INS_CMOVGE:
            case X86_INS_CMOVL:
            case X86_INS_CMOVLE:
            case X86_INS_FCMOVNBE:
            case X86_INS_FCMOVNB:
            case X86_INS_CMOVNE:
            case X86_INS_FCMOVNE:
            case X86_INS_CMOVNO:
            case X86_INS_CMOVNP:
            case X86_INS_FCMOVNU:
            case X86_INS_CMOVNS:
            case X86_INS_CMOVO:
            case X86_INS_CMOVP:
            case X86_INS_FCMOVU:
            case X86_INS_CMOVS:
                return Instruction::Mov;
            //neg
            case X86_INS_NEG:
                return Instruction::Neg;
            //not
            case X86_INS_NOT:
                return Instruction::Not;
            //load effective address
            case X86_INS_LEA:
                return Instruction::GetPointer;
            case X86_INS_CMP:
            case X86_INS_TEST:
            case X86_INS_UCOMISD:
            case X86_INS_UCOMISS:
            case X86_INS_VCOMISD:
            case X86_INS_VCOMISS:
            case X86_INS_VUCOMISD:
            case X86_INS_VUCOMISS:
                return Instruction::Compare;
            default: return Instruction::Undefined;
        }
    }
    return Instruction::Undefined;
}

int liftInstructions(janus::Function *function)
{
    //lift individual instruction
    int count = 0;
    for (auto &instr: function->instrs) {
        count++;
        liftInstruction(instr, function);
    }
    return count;
}

/* Generate output states for a given instruction */
static void liftInstruction(Instruction &instr, Function *function)
{
    MachineInstruction *minstr = instr.minstr;
    BasicBlock *block = instr.block;
    if (!minstr || !block) return;

    if (minstr->fineType == INSN_NOP) return;

    for(int i=0; i<minstr->opndCount; i++)
    {
        Variable var = minstr->operands[i].lift(instr.pc + instr.minstr->size);
        //record this variable
        function->allVars.insert(var);
        //add outputs to the instruction
        if (minstr->operands[i].access == OPND_WRITE ||
            minstr->operands[i].access == OPND_READ_WRITE) {
            VarState *vs = new VarState(var, block, &instr);
            function->allStates.insert(vs);
            instr.outputs.push_back(vs);
        }
    }

    //if the instruction modifies eflag, add to the output too
    if (minstr->writeControlFlag()) {
        Variable var((uint32_t)0);
        var.type = JVAR_CONTROLFLAG;
        function->allVars.insert(var);
        VarState *vs = new VarState(var, block, &instr);
        function->allStates.insert(vs);
        instr.outputs.push_back(vs);
    }

    //imul
    if ((minstr->opcode == X86_INS_IMUL ||
         minstr->opcode == X86_INS_IDIV ||
         minstr->opcode == X86_INS_MUL  ||
         minstr->opcode == X86_INS_DIV) && minstr->opndCount == 1) {
        //implicit EAX:EDX output
        Variable var((uint32_t)JREG_RAX);
        VarState *vs = new VarState(var, block, &instr);
        function->allStates.insert(vs);
        instr.outputs.push_back(vs);
        Variable var2((uint32_t)JREG_RDX);
        vs = new VarState(var2, block, &instr);
        function->allStates.insert(vs);
        instr.outputs.push_back(vs);
    }

    //call
    else if (minstr->opcode == X86_INS_CALL) {
        //force the calling function to generate RAX output
        Variable var((uint32_t)JREG_RAX);
        VarState *vs = new VarState(var, block, &instr);
        function->allStates.insert(vs);
        instr.outputs.push_back(vs);
        Variable var2((uint32_t)JREG_XMM0);
        vs = new VarState(var2, block, &instr);
        function->allStates.insert(vs);
        instr.outputs.push_back(vs);
    }

    //push or pop
    else if (minstr->opcode == X86_INS_PUSH || minstr->opcode == X86_INS_POP) {
        Variable var((uint32_t)JREG_RSP);
        VarState *vs = new VarState(var, block, &instr);
        function->allStates.insert(vs);
        instr.outputs.push_back(vs);
        if (minstr->opcode == X86_INS_POP) {
            for(int i=0; i<minstr->opndCount; i++)
            {
                Variable var = minstr->operands[i].lift(instr.pc + instr.minstr->pc);
                vs = new VarState(var, block, &instr);
                function->allStates.insert(vs);
                instr.outputs.push_back(vs);
            }
        }
    }

    else if (minstr->opcode == X86_INS_CDQ ||
             minstr->opcode == X86_INS_CDQE) {
        Variable var((uint32_t)JREG_RAX);
        VarState *vs = new VarState(var, block, &instr);
        function->allStates.insert(vs);
        instr.outputs.push_back(vs);
    }

    else if (minstr->opcode == X86_INS_MOVDQU ||
             minstr->opcode == X86_INS_MOVDQA) {
        Variable var = minstr->operands[0].lift(instr.pc + instr.minstr->pc);
        minstr->operands[0].access = OPND_WRITE;
        minstr->operands[1].access = OPND_READ;
        VarState *vs = new VarState(var, block, &instr);
        function->allStates.insert(vs);
        instr.outputs.push_back(vs);
    }
}

void getInstructionInputs(janus::MachineInstruction *minstr, vector<Variable> &v)
{
    if (minstr->fineType == INSN_NOP) return;

    for(int i=0; i<minstr->opndCount; i++)
    {
        if (minstr->operands[i].access == OPND_READ ||
            minstr->operands[i].access == OPND_READ_WRITE) {
            Variable var = minstr->operands[i].lift(minstr->pc + minstr->size);
            v.push_back(var);
        }
    }

    if (minstr->isXORself()) {
        v.clear();
        Variable var;
        var.type = JVAR_CONSTANT;
        var.value = 0;
        v.push_back(var);
    }

    else if (minstr->readControlFlag()) {
        Variable var((uint32_t)0);
        var.type = JVAR_CONTROLFLAG;
        v.push_back(var);
    }

    else if (minstr->isLEA()) {
        if (v[0].type == JVAR_STACK) {
            v[0].base = JREG_RSP;
        }
        v[0].type = JVAR_POLYNOMIAL;
    }

    else if (minstr->isReturn()) {
        Variable var((uint32_t)JREG_RAX);
        v.push_back(var);
        Variable var2((uint32_t)JREG_XMM0);
        v.push_back(var2);
    }

    else if (minstr->opcode == X86_INS_CDQ ||
             minstr->opcode == X86_INS_CDQE) {
        Variable var((uint32_t)JREG_RAX);
        v.push_back(var);
    }

    else if (minstr->opcode == X86_INS_INC ||
             minstr->opcode == X86_INS_DEC) {
        Variable var;
        var.type = JVAR_CONSTANT;
        var.value = 1;
        v.push_back(var);
    }
    else if (minstr->opcode == X86_INS_MOVDQU ||
             minstr->opcode == X86_INS_MOVDQA) {
        Variable var = minstr->operands[1].lift(minstr->pc + minstr->size);
        v.push_back(var);
    }
}

void linkArchSpecSSANodes(Function &function, map<BlockID, map<Variable, VarState*> *> &globalDefs)
{
    
}
