#include "janus.h"
#include "Operand.h"
#include "Arch.h"

#include "capstone/capstone.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cassert>
#include <cstring>

using namespace std;
using namespace janus;

Operand::Operand()
{
    type = OPND_INVALID;
}

/* Parse capstone operand */
Operand::Operand(void *capstone_opnd)
{
    //TODO, this construction is x86 specific
    cs_x86_op *op = (cs_x86_op *)capstone_opnd;
    type = (OperandType)op->type;
    access = op->access;
    size = op->size;

    switch(type) {
        case OPND_REG:
            reg = csToJanus[op->reg];
        break;
        case OPND_IMM:
            intImm = op->imm;
            //Immediate value always read only
            access = 1;
        break;
        case OPND_MEM:
        //for memory, we need to dig it a little bit more
        if(op->mem.segment) {
            structure = OPND_SEGMEM;
            segMem.segment = csToJanus[op->mem.segment];
            segMem.base = csToJanus[op->mem.base];
            segMem.index = csToJanus[op->mem.index];
            segMem.disp = op->mem.disp;
        }
        else if(op->mem.index) {
            int base = csToJanus[op->mem.base];
            if(base == JREG_RSP || base == JREG_ESP)
                structure = STACK_PTR_IND;
            else if(base == JREG_RBP || base == JREG_EBP)
                structure = STACK_BASE_IND;
            else 
                structure = OPND_FULLMEM;
            fullMem.base = base;
            fullMem.index = csToJanus[op->mem.index];
            fullMem.scale = op->mem.scale;
            fullMem.disp = op->mem.disp;
        }
        else {
            int base = csToJanus[op->mem.base];
            if(base == JREG_RSP || base == JREG_ESP)
                structure = STACK_PTR_IMM;
            else if(base == JREG_RBP || base == JREG_EBP)
                structure = STACK_BASE_IMM;
            else 
                structure = OPND_DISPMEM;
            
            dispMem.base = base;
            dispMem.disp = op->mem.disp;
        }
        break;
        default:
        break;
    }
}

bool Operand::isVectorRegister()
{
    if(type == OPND_REG) {
        if(reg >=JREG_MM0 && reg <GSEG_ES) return true;
        if(reg >=JREG_DR0 && reg <JREG_R8B) return true;
        if(reg >=JREG_FP0 && reg <JREG_INVALID2) return true;
    }
    return false;
}

bool Operand::isXMMRegister()
{
    if(type == OPND_REG) {
        if(reg >=JREG_XMM0 && reg <=JREG_XMM15) return true;
        if(reg >=JREG_YMM0 && reg <=JREG_ZMM31) return true;
        if(reg >=JREG_XMM16 && reg <=JREG_XMM31) return true;
    }
    return false;
}

bool Operand::isMemoryAccess()
{
    return type == OPND_MEM;
}

std::ostream& janus::operator<<(std::ostream& out, const Operand& v)
{
    switch(v.type) {
        case OPND_REG:
            out <<get_reg_name(v.reg);
        break;
        case OPND_IMM:
            out <<hex<<"0x"<<v.intImm;
        break;
        case OPND_MEM:
        //for memory, we need to dig it a little bit more
        if(v.structure == OPND_SEGMEM) {
            out << "["<<get_reg_name(v.segMem.segment)<<":";
            if(v.segMem.base)
                out<<get_reg_name(v.segMem.base);
            if(v.segMem.index)
                out<<"+"<<get_reg_name(v.segMem.index);
            if(v.segMem.disp) {
                if(v.segMem.disp<0)
                    out<<"-0x"<<(-(v.segMem.disp));
                else
                    out<<"+0x"<<(v.segMem.disp);
            }
            out <<"]";
        }
        else if(v.structure == OPND_FULLMEM ||
                v.structure == STACK_BASE_IND ||
                v.structure == STACK_PTR_IND) {
            out <<"[";
            if(v.fullMem.base)
                out<<get_reg_name(v.fullMem.base);
            if(v.fullMem.scale) {
                if(v.fullMem.base)
                    out<<"+";
                if(v.fullMem.scale != 1) {
                    out <<v.fullMem.scale<<"*";
                }
            }
            out<<get_reg_name(v.fullMem.index);
            if(v.fullMem.disp) {
                if(v.fullMem.disp<0)
                    out<<"-0x"<<hex<<(-(v.fullMem.disp));
                else
                    out<<"+0x"<<hex<<(v.fullMem.disp);
            }
            out <<"]";
        }
        else {
            out <<"["<<get_reg_name(v.fullMem.base);
            if(v.dispMem.disp) {
                if(v.dispMem.disp<0)
                    out<<"-0x"<<hex<<(-(v.dispMem.disp));
                else
                    out<<"+0x"<<hex<<(v.dispMem.disp);
            }
            out <<"]";
        }
        break;
        case OPND_FP:
            out <<v.fpImm;
        break;
        default:
        break;
    }
    return out;
}

Variable Operand::lift(PCAddress nextInstrPc)

{
    Variable var;
    memset(&var, 0, sizeof(Variable));
    var.size = size;
    switch (type) {
    case OPND_REG:
        {
            var.type = JVAR_REGISTER;
            var.value = get_full_reg_id(reg);
            const char *c = get_reg_name(var.value);
            if ((c[1] == 'M') && (c[2] == 'M')) {
                if (c[0] == 'X') var.size = 16;
                if (c[0] == 'Y') var.size = 32;
                if (c[0] == 'Z') var.size = 64;
            }
        }
        break;
    case OPND_MEM:

        if (structure == STACK_PTR_IMM) {
            var.type = JVAR_STACK;
            var.value = dispMem.disp;
        }
        else if (structure == STACK_BASE_IMM) {
            var.type = JVAR_MEMORY;
            var.base = JREG_RBP;
            var.value = dispMem.disp;
        }
        else if (dispMem.base == JREG_RIP) {
            var.type = JVAR_ABSOLUTE;
            var.value = dispMem.disp + nextInstrPc; //in RIP relative addresses, we need the pc of the next instruction
        } else if (structure == OPND_DISPMEM) {
            var.value = dispMem.disp;
            var.base = dispMem.base;
            var.type = JVAR_MEMORY;
        } else if (structure == OPND_FULLMEM) {
            var.value = fullMem.disp;
            var.index = fullMem.index;
            var.base = fullMem.base;
            var.scale = fullMem.scale;
            var.type = JVAR_MEMORY;
        } else if (structure == STACK_BASE_IND) {
            var.value = fullMem.disp;
            var.index = fullMem.index;
            var.base = fullMem.base;
            var.scale = fullMem.scale;
            var.type = JVAR_MEMORY;
        } else if (structure == STACK_PTR_IND) {
            var.value = fullMem.disp;
            var.index = fullMem.index;
            var.base = fullMem.base;
            var.scale = fullMem.scale;
            var.type = JVAR_MEMORY;
        } else var.type = JVAR_UNKOWN;
        break;
    case OPND_IMM:
        var.type = JVAR_CONSTANT;
        var.value = value;
        break;
    default:
        var.type = JVAR_UNKOWN;
    }
    return var;
}
