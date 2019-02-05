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
    cs_arm64_op *op = (cs_arm64_op *)capstone_opnd;
    type = (OperandType)op->type;
    access = op->access;
    shift.type = (uint8_t)op->shift.type;
    shift.value = op->shift.value;

    switch(type) {
        case OPND_REG:
            reg = csToJanus[op->reg];
            //Set size of reg based on register name
            if(reg >=JREG_X0 && reg <=JREG_X30) size = 8;
            if(reg >=JREG_W0 && reg <=JREG_W30) size = 4;

            if(reg >=JREG_Q0 && reg <=JREG_Q31) size = 16;
            if(reg >=JREG_D0 && reg <=JREG_D31) size = 8;
            if(reg >=JREG_S0 && reg <=JREG_S31) size = 4;
            if(reg >=JREG_H0 && reg <=JREG_H31) size = 2;
            if(reg >=JREG_B0 && reg <=JREG_B31) size = 1;

        break;
        case OPND_IMM:
            intImm = op->imm;
            //Immediate value always read only
            access = 1;

            switch(shift.type)
            {
                case JSHFT_LSL:
                    intImm <<= shift.value;
                    break;
                case JSHFT_ASR:
                    intImm >>= shift.value;
                    break;
            }
            shift.type = JSHFT_INVALID;
        break;
        case OPND_MEM:
            mem.base = csToJanus[op->mem.base];
            mem.index = csToJanus[op->mem.index];
            mem.disp = op->mem.disp;

            //set size of mem based on displacement alignment (preferring larger sizes)
            if(mem.disp % 8 == 0){
                size = 8;
            }
            else if (mem.disp % 4 == 0){
                size = 4;
            }
            else if (mem.disp % 2 == 0){
                size = 2;
            }
            else{
                size = 1;
            }
        break;
        /* These 4 are shoved in the same uint64_t as they're not a priority */
        case OPND_PSTATE:
            other = (uint64_t)op->pstate;
        break;
        case OPND_SYS:
            other = (uint64_t)op->sys;
        break;
        case OPND_PREFETCH:
            other = (uint64_t)op->prefetch;
        break;
        case OPND_BARRIER:
            other = (uint64_t)op->barrier;
        break;
        default:
        break;
    }
}

bool Operand::isVectorRegister()
{
    /* Check for any of the ARM NEON SIMD vector registers */
    if(type == OPND_REG) {
        if(reg >=JREG_V0 && reg <=JREG_V31) return true;
        if(reg >=JREG_Q0 && reg <=JREG_Q31) return true;
        if(reg >=JREG_D0 && reg <=JREG_D31) return true;
        if(reg >=JREG_S0 && reg <=JREG_S31) return true;
        if(reg >=JREG_H0 && reg <=JREG_H31) return true;
        if(reg >=JREG_B0 && reg <=JREG_B31) return true;
    }
    return false;
}

bool Operand::isXMMRegister()
{
    return false; /* Since there is no XMM on ARM (could do NEON maybe?) */
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
            if(v.shift.type != JSHFT_INVALID && v.shift.type <= JSHFT_END)
                out << " " << get_shift_name(v.shift.type) << " #" << v.shift.value;
        break;
        case OPND_IMM:
            out <<hex<<"0x"<<v.intImm;
            if(v.shift.type != JSHFT_INVALID && v.shift.type <= JSHFT_END)
                out << " " << get_shift_name(v.shift.type) << " #" << v.shift.value;
        break;
        case OPND_MEM:
            out <<"[";
            if(v.mem.base)
                out <<get_reg_name(v.mem.base);
            if(v.mem.index){
                out <<"+";
                out <<get_reg_name(v.mem.index);
            }
            if(v.mem.disp){
                if(v.mem.disp < 0)
                    out<<"-0x"<<hex<<(-(v.mem.disp));
                else
                    out<<"+0x"<<hex<<(v.mem.disp);
            }
            if(v.shift.type != JSHFT_INVALID && v.shift.type <= JSHFT_END)
                out << " " << get_shift_name(v.shift.type) << " #" << v.shift.value;
            out <<"]";
        break;
        case OPND_FP:
            out <<v.fpImm;
            if(v.shift.type != JSHFT_INVALID && v.shift.type <= JSHFT_END)
                out << " " << get_shift_name(v.shift.type) << " #" << v.shift.value;
        break;
        default:
        break;
    }
    return out;
}

Variable Operand::lift(PCAddress pc)
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
            }
            break;
        case OPND_MEM:
            var.base = mem.base;
            var.index = mem.index;
            var.value = mem.disp;
            if(var.base == JREG_X29){
                var.type = JVAR_STACKFRAME;
            }
            else
                var.type = JVAR_MEMORY;
            break;
        case OPND_IMM:
            var.type = JVAR_CONSTANT;
            var.value = value;
            break;
        default:
            var.type = JVAR_UNKOWN;
    }

    if(shift.value != 0 && shift.type != JSHFT_INVALID)
    {
        var.shift_type = shift.type;
        var.shift_value = shift.value;
        if(var.type == JVAR_CONSTANT)
            var.type = JVAR_SHIFTEDCONST;
        else if(var.type == JVAR_REGISTER)
            var.type = JVAR_SHIFTEDREG;
    }
    else
    {
        var.shift_type = JSHFT_INVALID;
    }
    return var;
}
