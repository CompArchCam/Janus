/* Janus Abstract Instructions */

#include "Instruction.h"
#include "MachineInstruction.h"
#include "Disassemble.h"
#include "Function.h"
#include "JanusContext.h"

using namespace janus;
using namespace std;

Instruction::Instruction()
{
    minstr=NULL;
}

Instruction::Instruction(MachineInstruction *minstr)
:minstr(minstr)
{
    if (minstr) {
        id = minstr->id;
        pc = minstr->pc;
        opcode = liftOpcode(minstr);
    }
    block=NULL;
}

ostream& janus::operator<<(ostream& out, const Instruction& instr)
{
    out<<dec<<instr.id<<" ";
    switch (instr.opcode) {
        case Instruction::Undefined: out<<"Undefined"; break;
        case Instruction::DirectBranch: out<<"DirectBranch"; break;
        case Instruction::ConditionalBranch: out<<"ConditionalBranch"; break;
        case Instruction::Call: {
            out<<"Call ";
            auto targetFunc = instr.block->parentFunction->calls[instr.id];
            if (targetFunc) {
                out<<targetFunc->name;
                return out;
            }
        }
        break;
        case Instruction::Return: out<<"Return"; break;
        case Instruction::Interupt: out<<"Interupt"; break;
        case Instruction::Add: out<<"Add"; break;
        case Instruction::Sub: out<<"Sub"; break;
        case Instruction::Mul: out<<"Mul"; break;
        case Instruction::Div: out<<"Div"; break;
        case Instruction::Rem: out<<"Rem"; break;
        case Instruction::Shl: out<<"Shl"; break;
        case Instruction::LShr: out<<"LShr"; break;
        case Instruction::AShr: out<<"AShr"; break;
        case Instruction::And: out<<"And"; break;
        case Instruction::Or: out<<"Or"; break;
        case Instruction::Xor: out<<"Xor"; break;
        case Instruction::Mov: out<<"Mov"; break;
        case Instruction::Neg: out<<"Neg"; break;
        case Instruction::Not: out<<"Not"; break;
        case Instruction::Load: out<<"Load"; break;
        case Instruction::Store: out<<"Store"; break;
        case Instruction::Fence: out<<"Fence"; break;
        case Instruction::GetPointer: out<<"GetPointer"; break;
        case Instruction::Compare: out<<"Compare"; break;
        case Instruction::Phi: out<<"Phi"; break;
        case Instruction::Nop: out<<"Nop"; break;
        default: out<<"Unknown"; break;
    }
    if (instr.minstr) {
        out <<" "<<*instr.minstr;
    }
    else
        out << "NULL";
    return out;
}

///Return true if the instruction is a control flow instruction including jumps, returns, interupts etc
bool
Instruction::isControlFlow() {
    return (opcode == Instruction::DirectBranch ||
            opcode == Instruction::ConditionalBranch ||
            opcode == Instruction::Call ||
            opcode == Instruction::Return ||
            opcode == Instruction::Interupt);
}

bool
Instruction::isConditionalJump(){
    return (opcode == Instruction::ConditionalBranch);
}
int
Instruction::getTargetInstID()
{
    if (!minstr) return -1;
    PCAddress targetAddr = minstr->getTargetAddress();
    if (targetAddr == 0) return -1;
    /* Lookup in the instr table */
    auto instrTable = block->parentFunction->minstrTable;
    if (instrTable.find(targetAddr) != instrTable.end()) {
        return instrTable[targetAddr];
    }
    return -1;
}

//Function* Instruction::getTargetFunction()
Function* Instruction::getTargetFunction(std::map<PCAddress, janus::Function *> functionMap)
{
    if (!minstr) return NULL;
    if (opcode != Instruction::Call) return NULL;
    PCAddress targetAddr = minstr->getTargetAddress();
    if (targetAddr == 0) return NULL;
    /* Lookup in the instr table */
    //auto functionMap = block->parentFunction->context->functionMap;
    if (functionMap.find(targetAddr) != functionMap.end()) {
        return functionMap[targetAddr];
    } else return NULL;
}

void
Instruction::printDot(void *outputStream) {
    ostream &os = *(ostream *)outputStream;
    os<<dec<<id<<" ";
    if (isControlFlow()) {
        if (opcode == Call) {
            auto targetFunc = block->parentFunction->calls[id];
            if (targetFunc)
                os<<" call "<<targetFunc->name;
            else
                os<<" "<<minstr->name;
        } else {
            int tid = getTargetInstID();
            if (tid > 0)
                os<<" "<<minstr->name<<" -> "<<dec<<tid;
            else
                os<<" "<<minstr->name;
        }
    } else os<<" "<<minstr->name;
}

void
Instruction::getInputs(std::vector<Variable> &v)
{
    if (minstr) {
        getInstructionInputs(minstr, v);
    }

    if (opcode == Xor && v[0].type == JVAR_CONSTANT && v[0].value == 0)
        opcode = Mov;
}

bool
Instruction::isMemoryAccess()
{
    if (opcode == Nop) return false;
    for (auto vs: inputs) {
        if (vs->type == JVAR_STACK ||
            vs->type == JVAR_STACKFRAME ||
            vs->type == JVAR_MEMORY ||
            vs->type == JVAR_ABSOLUTE) {
            return true;
        }
    }

    for (auto vs: outputs) {
        if (vs->type == JVAR_STACK ||
            vs->type == JVAR_STACKFRAME ||
            vs->type == JVAR_MEMORY ||
            vs->type == JVAR_ABSOLUTE) {
            return true;
        }
    }
    return false;
}

bool
Instruction::isVectorInstruction()
{
#ifdef JANUS_X86
    return minstr->isXMMInstruction();
#else
    return false;
#endif
}

MemoryInstruction::MemoryInstruction(Instruction *instr)
:instr(instr), mem(NULL), nonMem(NULL), type(Unknown)
{
    //corner case
    if (instr->opcode == Instruction::Compare) {
        type = ReadAndRead;
        for (auto vs: instr->inputs) {
            if (vs->type == JVAR_STACK ||
                vs->type == JVAR_STACKFRAME ||
                vs->type == JVAR_MEMORY ||
                vs->type == JVAR_ABSOLUTE) {
                mem = vs;
            } else nonMem = vs;
        }
        return;
    }

    for (auto vs: instr->inputs) {
        if (vs->type == JVAR_STACK ||
            vs->type == JVAR_STACKFRAME ||
            vs->type == JVAR_MEMORY ||
            vs->type == JVAR_ABSOLUTE) {
            type = Read;
            mem = vs;
            break;
        }
    }

    for (auto vs: instr->outputs) {
        if (vs->type == JVAR_STACK ||
            vs->type == JVAR_STACKFRAME ||
            vs->type == JVAR_MEMORY ||
            vs->type == JVAR_ABSOLUTE) {
            if (type == Read) {
                //the read and write term must be the same
                Variable read = (Variable)*mem;
                Variable write = (Variable)*vs;
                if (!(read == write)) type = Unknown;
                else type = ReadAndWrite;
            }
            else {
                type = Write;
                mem = vs;
                break;
            }
        }
    }

    //work out the non-mem part
    if (type == Read) {
        for (auto vs: instr->outputs) {
            if (vs != mem) {
                nonMem = vs;
                break;
            }
        }
    } else if (type == Write) {
        for (auto vs: instr->inputs) {
            if (vs != mem) {
                nonMem = vs;
                break;
            }
        }
    } else if (type == ReadAndWrite) {
        for (auto vs: instr->inputs) {
            Variable read = (Variable)*mem;
            Variable write = (Variable)*vs;
            if (vs != mem) {
                nonMem = vs;
                break;
            }
        }
    }
}

std::ostream& janus::operator<<(std::ostream& out, const MemoryInstruction& minstr)
{
    out <<dec<<minstr.instr->id<<" ";
    if (minstr.nonMem == NULL || minstr.mem == NULL) {
        out<<"NULL";
        return out;
    }
    if (minstr.type == MemoryInstruction::Read) {
        out << (Variable)*minstr.nonMem<<" <-- "<<(Variable)*minstr.mem;
    } else if (minstr.type == MemoryInstruction::Write) {
        out << (Variable)*minstr.mem<<" <-- "<<(Variable)*minstr.nonMem;
    } else if (minstr.type == MemoryInstruction::ReadAndWrite) {
        out << (Variable)*minstr.mem<<" <-- "<<(Variable)*minstr.nonMem<<","<<(Variable)*minstr.mem;
    }
    return out;
}
