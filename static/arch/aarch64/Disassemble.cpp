#include "Disassemble.h"
#include "Arch.h"
#include "Function.h"
#include "IO.h"
#include "JanusContext.h"
#include "Utility.h"
#include "capstone/capstone.h"
#include "janus_aarch64.h"

using namespace janus;
using namespace std;

#define ARM64_INS_LDP 0x9f
#define ARM64_INS_STP 0x144

static void linkRelocation(JanusContext *jc, Function *pltFunc);

static bool
propagateX29Instr(Instruction *instr, VarState *vs, int64_t offset,
                  map<BlockID, map<Variable, VarState *> *> globalDefs,
                  Function &function);

static bool propagateX29(VarState *vs, int64_t offset,
                         map<BlockID, map<Variable, VarState *> *> globalDefs,
                         Function &function);

void disassembleAll(JanusContext *jc)
{
    GSTEP("Disassembling instructions: ");
    cs_err err;
    // initialise capstone disassembly engine
    // TODO, recognise architecture automatically
    err = cs_open(CS_ARCH_ARM64, CS_MODE_ARM,
                  (csh *)(&jc->program.capstoneHandle));

    if (err) {
        printf("Failed on cs_open() in capstone with error returned: %u\n",
               err);
        exit(-1);
    }

    // we need the details for each instruction
    cs_option((csh)jc->program.capstoneHandle, CS_OPT_DETAIL, CS_OPT_ON);
    // skip the padding between the instructions
    cs_option((csh)jc->program.capstoneHandle, CS_OPT_SKIPDATA, CS_OPT_ON);

    // Disassemble each function
    for (auto &func : jc->functions) {
        // disassemble the function and register the function lookup table
        if (func.isExecutable) {
            disassemble(&func);
            jc->functionMap[func.startAddress] = &func;
        }
        // if the function is calling shared library call
        else {
            jc->externalFunctions[func.startAddress] = &func;
        }
    }
    GSTEPCONT(jc->functions.size() << " functions recognised" << endl);
    // NOTE: once all external calls are registered
    // We also need to link the PLT relocation address to external shared
    // library calls
    bool foundFortranMain = false;
    Function *fmain = NULL;

    // lift plt sections
    for (auto &func : jc->functions) {
        if (func.name == string(".plt") || func.name == string("_plt")) {
            func.isExecutable = false;
            linkRelocation(jc, &func);
            continue;
        }
        if (func.name == string("main") && !foundFortranMain) {
            jc->main = &func;
        }
        if (func.name == string("_start")) {
            // fmain = &func;
        }
        if (func.name == string("MAIN__")) {
            fmain = &func;
            foundFortranMain = true;
        }
    }

    if (foundFortranMain)
        jc->main = fmain;

    cs_close((csh *)(&jc->program.capstoneHandle));
}

static void linkRelocation(JanusContext *jc, Function *pltFunc)
{
    if (!pltFunc)
        return;

    uint32_t size = pltFunc->minstrs.size();

    if (!size)
        return;

    if (size % 4) {
        cout << "function " << pltFunc->name << " may not be a PLT section"
             << endl;
        return;
    }

    uint32_t nPltSym = size / 4;

    uint32_t i = 0;

    for (auto synthetic : jc->externalFunctions) {
        Function *synFunc = synthetic.second;
        synFunc->startAddress = pltFunc->minstrs[4 * i + 8].pc;
        synFunc->endAddress = pltFunc->minstrs[4 * i + 3 + 8].pc;
        synFunc->size = synFunc->endAddress - synFunc->startAddress;
        synFunc->name += "@plt";
        // update in the function map
        jc->functionMap[synFunc->startAddress] = synFunc;
        i++;
        if (i == nPltSym)
            break;
    }
}

/// Disassemble for the given function
void disassemble(Function *function)
{
    // if already disassembled, return
    if (function->minstrs.size())
        return;

    uint64_t handle = function->context->program.capstoneHandle;
    cs_insn *instr;
    InstID id = 0;

    uint8_t *code = function->contents;
    size_t codeSize = function->size;
    PCAddress pc = function->startAddress;

    instr = cs_malloc((csh)handle);

    // reserve machine instructions
    function->minstrs.reserve(codeSize + 1);
    while (cs_disasm_iter((csh)handle, (const uint8_t **)(&code), &codeSize,
                          (uint64_t *)&pc, instr)) {
        function->minstrs.emplace_back(id, (void *)handle, (void *)instr);
        id++;
    }

    // second step, register the address map
    for (auto &instr : function->minstrs) {
        function->minstrTable[instr.pc] = instr.id;
    }
    cs_free(instr, 1);

    /* third step: create place holders for abstract Instructions */
    int instrCount = function->minstrs.size();
    // reserve space for abstract instructions
    function->instrs.reserve(instrCount);

    // get machine instruction data
    MachineInstruction *minstrs = function->minstrs.data();

    for (int i = 0; i < instrCount; i++) {
        function->instrs.emplace_back(minstrs + i);
    }
}

Instruction::Opcode liftOpcode(MachineInstruction *minstr)
{
    if (!minstr)
        return Instruction::Undefined;
    if (minstr->isType(INSN_CONTROL)) {
        switch (minstr->fineType) {
        case INSN_JUMP:
            return Instruction::DirectBranch;
        case INSN_CJUMP:
            return Instruction::ConditionalBranch;
        case INSN_CALL:
            return Instruction::Call;
        case INSN_RET:
            return Instruction::Return;
        case INSN_INT:
            return Instruction::Interupt;
        default:
            return Instruction::Undefined;
        }
    } else {
        switch (minstr->opcode) {
        case ARM64_INS_NOP:
            return Instruction::Nop;

        // Basic arithmetic operations
        case ARM64_INS_ADC:
        case ARM64_INS_ADDHN:
        case ARM64_INS_ADDHN2:
        case ARM64_INS_ADDP:
        case ARM64_INS_ADD:
        case ARM64_INS_ADDV:
            return Instruction::Add;
        case ARM64_INS_SUBHN:
        case ARM64_INS_SUBHN2:
        case ARM64_INS_SUB:
            return Instruction::Sub;
        case ARM64_INS_MADD:
        case ARM64_INS_SMADDL:
        case ARM64_INS_UMADDL:
        case ARM64_INS_FMADD:
        case ARM64_INS_MSUB:
        case ARM64_INS_SMSUBL:
        case ARM64_INS_UMSUBL:
        case ARM64_INS_FMSUB:
        case ARM64_INS_MNEG:
        case ARM64_INS_UMNEGL:
        case ARM64_INS_SMNEGL:
        case ARM64_INS_FMUL:
        case ARM64_INS_FMULX:
        case ARM64_INS_FNMUL:
        case ARM64_INS_PMULL2:
        case ARM64_INS_PMULL:
        case ARM64_INS_PMUL:
        case ARM64_INS_SMULH:
        case ARM64_INS_SMULL2:
        case ARM64_INS_SMULL:
        case ARM64_INS_SQDMULH:
        case ARM64_INS_SQDMULL:
        case ARM64_INS_SQDMULL2:
        case ARM64_INS_SQRDMULH:
        case ARM64_INS_UMULH:
        case ARM64_INS_UMULL2:
        case ARM64_INS_UMULL:
            return Instruction::Mul;
        case ARM64_INS_UDIV:
        case ARM64_INS_SDIV:
        case ARM64_INS_FDIV:
            return Instruction::Div;

        // Shifts
        case ARM64_INS_LSL:
        case ARM64_INS_SHLL2:
        case ARM64_INS_SHLL:
        case ARM64_INS_SHL:
        case ARM64_INS_SQRSHL:
        case ARM64_INS_SQSHLU:
        case ARM64_INS_SQSHL:
        case ARM64_INS_SRSHL:
        case ARM64_INS_SSHLL2:
        case ARM64_INS_SSHLL:
        case ARM64_INS_SSHL:
        case ARM64_INS_UQRSHL:
        case ARM64_INS_UQSHL:
        case ARM64_INS_URSHL:
        case ARM64_INS_USHLL2:
        case ARM64_INS_USHLL:
        case ARM64_INS_USHL:
            return Instruction::Shl;
        case ARM64_INS_LSR:
        case ARM64_INS_RSHRN2:
        case ARM64_INS_RSHRN:
        case ARM64_INS_SHRN2:
        case ARM64_INS_SHRN:
        case ARM64_INS_UQRSHRN:
        case ARM64_INS_UQRSHRN2:
        case ARM64_INS_UQSHRN:
        case ARM64_INS_UQSHRN2:
        case ARM64_INS_URSHR:
        case ARM64_INS_USHR:
            return Instruction::LShr;
        case ARM64_INS_ASR:
        case ARM64_INS_SQRSHRN:
        case ARM64_INS_SQRSHRN2:
        case ARM64_INS_SQRSHRUN:
        case ARM64_INS_SQRSHRUN2:
        case ARM64_INS_SQSHRN:
        case ARM64_INS_SQSHRN2:
        case ARM64_INS_SQSHRUN:
        case ARM64_INS_SQSHRUN2:
        case ARM64_INS_SRSHR:
        case ARM64_INS_SSHR:
            return Instruction::AShr;

        // Bitwise logic
        case ARM64_INS_AND:
            return Instruction::And;
        case ARM64_INS_ORR:
        case ARM64_INS_ORN:
            return Instruction::Or;
        case ARM64_INS_EON:
        case ARM64_INS_EOR:
            return Instruction::Xor;
        case ARM64_INS_NOT:
        case ARM64_INS_MVNI:
            return Instruction::Not;

        case ARM64_INS_NEG:
            return Instruction::Neg;

        case ARM64_INS_MOV:
        case ARM64_INS_FMOV:
        case ARM64_INS_SMOV:
        case ARM64_INS_UMOV:
        case ARM64_INS_MOVI:
        case ARM64_INS_MOVK:
        case ARM64_INS_MOVZ:
        case ARM64_INS_SXTB:
        case ARM64_INS_SXTH:
        case ARM64_INS_SXTW:
        case ARM64_INS_UXTB:
        case ARM64_INS_UXTH:
        case ARM64_INS_UXTW:
            return Instruction::Mov;

        case ARM64_INS_LD1:
        case ARM64_INS_LD1R:
        case ARM64_INS_LD2R:
        case ARM64_INS_LD2:
        case ARM64_INS_LD3R:
        case ARM64_INS_LD3:
        case ARM64_INS_LD4:
        case ARM64_INS_LD4R:
        case ARM64_INS_LDARB:
        case ARM64_INS_LDARH:
        case ARM64_INS_LDAR:
        case ARM64_INS_LDAXP:
        case ARM64_INS_LDAXRB:
        case ARM64_INS_LDAXRH:
        case ARM64_INS_LDAXR:
        case ARM64_INS_LDNP:
        case ARM64_INS_LDP:
        case ARM64_INS_LDPSW:
        case ARM64_INS_LDRB:
        case ARM64_INS_LDR:
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
        case ARM64_INS_LDXP:
        case ARM64_INS_LDXRB:
        case ARM64_INS_LDXRH:
        case ARM64_INS_LDXR:
            return Instruction::Load;

        case ARM64_INS_ST1:
        case ARM64_INS_ST2:
        case ARM64_INS_ST3:
        case ARM64_INS_ST4:
        case ARM64_INS_STLRB:
        case ARM64_INS_STLRH:
        case ARM64_INS_STLR:
        case ARM64_INS_STLXP:
        case ARM64_INS_STLXRB:
        case ARM64_INS_STLXRH:
        case ARM64_INS_STLXR:
        case ARM64_INS_STNP:
        case ARM64_INS_STP:
        case ARM64_INS_STRB:
        case ARM64_INS_STR:
        case ARM64_INS_STRH:
        case ARM64_INS_STTRB:
        case ARM64_INS_STTRH:
        case ARM64_INS_STTR:
        case ARM64_INS_STURB:
        case ARM64_INS_STUR:
        case ARM64_INS_STURH:
        case ARM64_INS_STXP:
        case ARM64_INS_STXRB:
        case ARM64_INS_STXRH:
        case ARM64_INS_STXR:
            return Instruction::Store;

        /* Fence-type instructions
           NOTE: These are a bit different to x86. DMB is equivalent to MFENCE,
            DSB ensures all instructions preceding it are finished before
            those after it are executed, and ISB flushes the pipeline so
            that no instructions after the ISB are even fetched or decoded
            until those preceding it have finished.
            Maybe this means they shouldn't be classed as Fence instructions? */
        case ARM64_INS_ISB:
        case ARM64_INS_DSB:
        case ARM64_INS_DMB:
            return Instruction::Fence;

        /* Some things that don't seem to exist on arm

        case SOME_LEA_LIKE_INSTRUCTION:
            return Instruction::GetPointer;
        */
        case ARM64_INS_CMP:
        case ARM64_INS_CMN:
        case ARM64_INS_FCMP:
        case ARM64_INS_FCMPE:

        /* Conditinal Compares (should these be here?) */
        case ARM64_INS_CCMP:
        case ARM64_INS_CCMN:
        case ARM64_INS_FCCMP:
        case ARM64_INS_FCCMPE:
            return Instruction::Compare;

        default:
            return Instruction::Undefined;
        }
    }
}

void getInstructionInputs(janus::MachineInstruction *minstr,
                          std::vector<janus::Variable> &v)
{
    if (minstr->fineType == INSN_NOP)
        return;
    switch (minstr->opcode) {
    // Store instructions have read/write to everything for some reason
    case ARM64_INS_ST1:
    case ARM64_INS_ST2:
    case ARM64_INS_ST3:
    case ARM64_INS_ST4:
    case ARM64_INS_STLRB:
    case ARM64_INS_STLRH:
    case ARM64_INS_STLR:
    case ARM64_INS_STLXP:
    case ARM64_INS_STLXRB:
    case ARM64_INS_STLXRH:
    case ARM64_INS_STLXR:
    case ARM64_INS_STNP:
    case ARM64_INS_STP:
    case ARM64_INS_STRB:
    case ARM64_INS_STR:
    case ARM64_INS_STRH:
    case ARM64_INS_STTRB:
    case ARM64_INS_STTRH:
    case ARM64_INS_STTR:
    case ARM64_INS_STURB:
    case ARM64_INS_STUR:
    case ARM64_INS_STURH:
    case ARM64_INS_STXP:
    case ARM64_INS_STXRB:
    case ARM64_INS_STXRH:
    case ARM64_INS_STXR:
        // Get inputs (non-memory only)
        for (int i = 0; i < minstr->opndCount; i++) {
            if (minstr->operands[i].access == OPND_READ ||
                (minstr->operands[i].access == OPND_READ_WRITE &&
                 minstr->operands[i].type != OPND_MEM)) {
                Variable var = minstr->operands[i].lift(minstr->pc);
                v.push_back(var);
            }
// Pre- and post-indexing
#if 0
                if((minstr->isType(INSN_PREINDEX) || minstr->isType(INSN_POSTINDEX)) &&
                            minstr->operands[i].type == OPND_MEM) {
                    // For pre/post indexed addresses, base register is read from
                    Variable var((uint32_t)(minstr->operands[i].mem.base));
                    v.push_back(var);
                }
#endif
        }

        break;

    // Load instructions have read/write to everything for some reason
    case ARM64_INS_LD1:
    case ARM64_INS_LD1R:
    case ARM64_INS_LD2R:
    case ARM64_INS_LD2:
    case ARM64_INS_LD3R:
    case ARM64_INS_LD3:
    case ARM64_INS_LD4:
    case ARM64_INS_LD4R:
    case ARM64_INS_LDARB:
    case ARM64_INS_LDARH:
    case ARM64_INS_LDAR:
    case ARM64_INS_LDAXP:
    case ARM64_INS_LDAXRB:
    case ARM64_INS_LDAXRH:
    case ARM64_INS_LDAXR:
    case ARM64_INS_LDNP:
    case ARM64_INS_LDP:
    case ARM64_INS_LDPSW:
    case ARM64_INS_LDRB:
    case ARM64_INS_LDR:
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
    case ARM64_INS_LDXP:
    case ARM64_INS_LDXRB:
    case ARM64_INS_LDXRH:
    case ARM64_INS_LDXR:
        // Get inputs (memory only)
        for (int i = 0; i < minstr->opndCount; i++) {
            if (minstr->operands[i].access == OPND_READ ||
                (minstr->operands[i].access == OPND_READ_WRITE &&
                 minstr->operands[i].type == OPND_MEM)) {
                Variable var = minstr->operands[i].lift(minstr->pc);
                v.push_back(var);

                if (minstr->operands[i].type == OPND_MEM &&
                    minstr->opcode ==
                        ARM64_INS_LDP) { // Add an additional implicit memory
                                         // operand for load pair instructions
                    var.value += minstr->operands[i].size;
                    v.push_back(var);
                }
            }
// Pre- and post-indexing
#if 0
                if((minstr->isType(INSN_PREINDEX) || minstr->isType(INSN_POSTINDEX)) &&
                            minstr->operands[i].type == OPND_MEM) {
                    // For pre/post indexed addresses, base register is read from
                    Variable var((uint32_t)(minstr->operands[i].mem.base));
                    v.push_back(var);
                }
#endif
        }
        break;

    // Instructions for which we ignore READ_WRITE operands because they're
    // basically just WRITE operands
    case ARM64_INS_MOVZ:
    case ARM64_INS_MOVI:
    case ARM64_INS_MOVN:
    case ARM64_INS_SXTB:
    case ARM64_INS_SXTH:
    case ARM64_INS_SXTW:
    case ARM64_INS_UXTB:
    case ARM64_INS_UXTH:
    case ARM64_INS_UXTW:

    case ARM64_INS_LSL:
        for (int i = 0; i < minstr->opndCount; i++) {
            if (minstr->operands[i].access == OPND_READ) {
                Variable var = minstr->operands[i].lift(minstr->pc);
                v.push_back(var);
            }
        }
        break;
    default:
        for (int i = 0; i < minstr->opndCount; i++) {
            if (minstr->operands[i].access == OPND_READ ||
                minstr->operands[i].access == OPND_READ_WRITE) {
                Variable var = minstr->operands[i].lift(minstr->pc);
                v.push_back(var);
            }
        }
        break;
    }

    // Any post-processing

    switch (minstr->opcode) {
    case ARM64_INS_RET: {
        Variable var((uint32_t)JREG_X0);
        v.push_back(var);
    } break;
    default:
        break;
    }

    /*
        if (minstr->isXORself()) {
            v.clear();
            Variable var;
            var.type = JVAR_CONSTANT;
            var.value = 0;
            v.push_back(var);
        }
    */

    if (minstr->readControlFlag()) {
        Variable var((uint32_t)0);
        var.type = JVAR_CONTROLFLAG;
        v.push_back(var);
    }

    /*  Turns out you can use any R0-R7 for return values
        or V0-V7 for returns from vector/SIMD calculations
        also R8 for 'indirect result address'

        else if (minstr->isReturn()) {
            Variable var((uint32_t)JREG_X0);
            v.push_back(var);
        }

    */
}

void linkArchSpecSSANodes(janus::Function &function,
                          map<BlockID, map<Variable, VarState *> *> &globalDefs)
{
    // case 1: link normal memory accesses
#ifdef FIX
    // Propagate X29 to any registers that use it's value with an offset
    int noOfX29VarStates = 0;
    for (auto vs : function.allStates) {
        if (vs->type == JVAR_REGISTER && vs->value == JREG_X29) {
            noOfX29VarStates++;
            // TODO check return value for propagation failure
            propagateX29(vs, 0, globalDefs, function);
        }
    }
#endif
}

#ifdef FIX
static bool propagateX29(VarState *vs, int64_t offset,
                         map<BlockID, map<Variable, VarState *> *> globalDefs,
                         Function &function)
{
    bool propagationValid = true;
    if (vs->type == JVAR_MEMORY) {
        propagationValid = true;
    } else if (vs->type == JVAR_REGISTER) {
        if (vs->value == JREG_SP || vs->value == JREG_WSP) {
            propagationValid =
                true; /* Perform no transformations on SP related
                      references but the propagation isn't invalidated */
        } else {
            for (auto &bb : function.blocks) {
                for (int i = 0; i < bb.size; i++) {
                    int inputNo = 0, outputNo = 0;
                    for (auto vi : bb.instrs[i].inputs) {
                        if (vi == vs) { // Deliberate ptr equality test
                            propagationValid &=
                                propagateX29Instr(&bb.instrs[i], vs, offset,
                                                  globalDefs, function);
                        } else if (vi->type == JVAR_MEMORY &&
                                   vi->base == vs->value) {
                            if (vi->index != JREG_NULL &&
                                vi->index != JREG_INVALID) {
                                propagationValid =
                                    false; // Can't currently deal with
                                           // base-and-index addressing
                            } else {
                                propagationValid = true;
                                // Indicate that instruction uses propagated X29
                                bb.instrs[i]
                                    .minstr->setUsesPropagatedStackFrame(true);

                                // Replace VarState with stackframe equivalent
                                map<Variable, VarState *> blockDefs =
                                    *globalDefs[vi->block->bid];
                                Variable frameVar = Variable();
                                frameVar.type = JVAR_STACKFRAME;
                                frameVar.base = JREG_X29;
                                frameVar.index = JREG_NULL;
                                frameVar.value = vi->value + offset;
                                frameVar.size = vi->size;
                                VarState *frameVS = getOrInitVarState(
                                    frameVar, blockDefs, function);
                                for (auto vp : vi->pred) {
                                    bb.instrs[i].inputs.push_back(vp);
                                    vp->dependants.insert(&bb.instrs[i]);
                                }
                                bb.instrs[i].inputs.push_back(frameVS);

                                if (bb.instrs[i].minstr->opcode ==
                                    ARM64_INS_LDP) {
                                    // In case of a ldp, add an extra stack
                                    // frame VarState for the second of the
                                    // 'pair'
                                    frameVar.value += frameVar.size;
                                    frameVS = getOrInitVarState(
                                        frameVar, blockDefs, function);
                                    bb.instrs[i].inputs.push_back(frameVS);
                                }

                                bb.instrs[i].inputs.erase(
                                    bb.instrs[i].inputs.begin() + inputNo);
                                break; /* Must break out of loop as vector
                                          indices will get messed up after we
                                          remove one element */
                            }
                        }
                        inputNo++;
                    }
                    for (auto vo : bb.instrs[i].outputs) {
                        if (vo->type == JVAR_MEMORY && vo->base == vs->value) {
                            if (vo->index != JREG_NULL &&
                                vo->index != JREG_INVALID) {
                                propagationValid =
                                    false; // Can't currently deal with
                                           // base-and-index addressing
                            } else {
                                propagationValid = true;

                                // Replace VarState with stackframe equivalent
                                map<Variable, VarState *> blockDefs;
                                if (globalDefs[vo->block->bid] != NULL)
                                    blockDefs = *globalDefs[vo->block->bid];
                                else // If no definition map has been created
                                     // for this basic block
                                    blockDefs = map<Variable, VarState *>();
                                Variable frameVar = Variable();
                                frameVar.type = JVAR_STACKFRAME;
                                frameVar.base = JREG_X29;
                                frameVar.index = JREG_NULL;
                                frameVar.value = vo->value + offset;
                                frameVar.size = vo->size;
                                VarState *frameVS = getOrInitVarState(
                                    frameVar, blockDefs, function);
                                for (auto vp : vo->pred) {
                                    bb.instrs[i].inputs.push_back(
                                        vp); // Mem base and offset are still
                                             // inputs
                                    vp->dependants.insert(&bb.instrs[i]);
                                }
                                bb.instrs[i].outputs.push_back(frameVS);

                                if (bb.instrs[i].minstr->opcode ==
                                    ARM64_INS_STP) {
                                    // In case of a stp, add an extra stack
                                    // frame VarState for the second of the
                                    // 'pair'
                                    frameVar.value += frameVar.size;
                                    frameVS = getOrInitVarState(
                                        frameVar, blockDefs, function);
                                    bb.instrs[i].outputs.push_back(frameVS);
                                }

                                bb.instrs[i].outputs.erase(
                                    bb.instrs[i].outputs.begin() + outputNo);
                                break; /* Must break out of loop as vector
                                          indices will get messed up after we
                                          remove one element */
                            }
                        }
                        outputNo++;
                    }
                }
            }
        }
    } else {
        propagationValid = false;
    }
    return propagationValid;
}

static bool
propagateX29Instr(Instruction *instr, VarState *vs, int64_t offset,
                  map<BlockID, map<Variable, VarState *> *> globalDefs,
                  Function &function)
{
    bool instPropagateValid = true;
    if (instr->opcode == Instruction::Add) {
        bool isAddImmed = true;
        int64_t immediate = 0;

        for (auto vi : instr->inputs) {
            if (vi->type == JVAR_CONSTANT) {
                if (immediate == 0) {
                    // Assign the immediate value of the add
                    immediate = vi->value;
                } else {
                    // Multiple immediate inputs
                    isAddImmed = false;
                }
            } else if (vi->type == JVAR_REGISTER) {
                if (vi->value != vs->value) {
                    // A register input other than the preceding vs that we came
                    // from
                    isAddImmed = false;
                }
            } else {
                // An operand of type other than register or immediate
                isAddImmed = false;
            }
        }
        if (isAddImmed) {
            VarState *addDst = instr->outputs.front();
            instPropagateValid =
                propagateX29(addDst, offset + immediate, globalDefs, function);
        } else {
            // Invalid add immediate so propagation fails
            instPropagateValid = false;
        }
    } else if (instr->opcode == Instruction::Mov) {
        VarState *movDst = instr->outputs.front();
        instPropagateValid = propagateX29(movDst, offset, globalDefs, function);
    } else if (instr->opcode == Instruction::Load) {
        for (auto vi : instr->inputs) {
            // If the load has the parent VarState as its base register
            if (vi->type == JVAR_MEMORY) {
                if (vs->type == JVAR_REGISTER && vi->base == vs->value) {
                    instPropagateValid =
                        propagateX29(vi, offset, globalDefs, function);
                } else {
                    instPropagateValid = false;
                }
            }
        }
    } else if (instr->opcode == Instruction::Store) {
        for (auto vo : instr->outputs) {
            // If the store has the parent VarState as its base register
            if (vo->type == JVAR_MEMORY) {
                if (vs->type == JVAR_REGISTER && vo->base == vs->value) {
                    instPropagateValid =
                        propagateX29(vo, offset, globalDefs, function);
                } else {
                    instPropagateValid = false;
                }
            }
        }
    } else if (instr->opcode == Instruction::Call) {
        instPropagateValid =
            true; /* Assume everything is fine if a call depends on a register
                   - it's probably a 'fake' dependency because all calls
                       *technically* depend on x0-x8 anyway */
    } else {
        instPropagateValid = false;
    }
    return instPropagateValid;
}
#endif
