#include "Variable.h"
#include "Loop.h"
#include "Function.h"
#include "IO.h"
#include "Analysis.h"
#include "Operand.h"
#include "Disassemble.h"
#include "Arch.h"
#include <iostream>
#include <sstream>
#include <set>
#include <stack>

using namespace janus;
using namespace std;

VarState::VarState() {
    value = 0;
    base = 0;
    index = 0;
    scale = 0;
    shift_type = 0;
    shift_value = 0;
    size = 0;
    type = JVAR_UNKOWN;

    lastModified = NULL;
    block = NULL;
    expr = NULL;
    isPHI = false;
    notUsed = true;
    constLoop = NULL;
    dependsOnMemory = false;
}

VarState::VarState(Variable var) {
    value = var.value;
    base = var.base;
    index = var.index;
    scale = var.scale;
    shift_type = var.shift_type;
    shift_value = var.shift_value;
    size = var.size;
    type = var.type;

    lastModified = NULL;
    block = NULL;
    expr = NULL;
    isPHI = false;
    notUsed = true;
    constLoop = NULL;
    dependsOnMemory = false;
}

VarState::VarState(Variable var, BasicBlock *block, Instruction* lastModified)
:block(block), lastModified(lastModified)
{
    value = var.value;
    base = var.base;
    index = var.index;
    scale = var.scale;
    shift_type = var.shift_type;
    shift_value = var.shift_value;
    size = var.size;
    type = var.type;

    expr = NULL;
    isPHI = false;
    notUsed = true;
    constLoop = NULL;
    dependsOnMemory = false;
}

VarState::VarState(Variable var, BasicBlock *block, bool isPHI)
:block(block), isPHI(isPHI), lastModified(NULL)
{
    value = var.value;
    base = var.base;
    index = var.index;
    scale = var.scale;
    shift_type = var.shift_type;
    shift_value = var.shift_value;
    size = var.size;
    type = var.type;

    expr = NULL;
    notUsed = true;
    constLoop = NULL;
    dependsOnMemory = false;
}

Variable::Variable(const JVar &var)
{
    value = var.value;
    base = var.base;
    index = var.index;
    scale = var.scale;
    shift_type = var.shift_type;
    shift_value = var.shift_value;
    size = var.size;
    type = var.type;
}

Variable::Variable() {
    type = JVAR_UNKOWN;
    value = 0;
    base = 0;
    index = 0;
    scale = 0;
    shift_type = 0;
    shift_value = 0;
    size = 0;
}

Variable::Variable(uint32_t reg)
{
    type = JVAR_REGISTER;
    value = (uint64_t)reg;
    base = 0;
    index = 0;
    scale = 0;
    shift_value = 0;
    shift_type = 0;
    size = 8;
}

Variable::Variable(uint64_t val)
{
    type = JVAR_CONSTANT;
    value = val;
    base = 0;
    index = 0;
    scale = 0;
    shift_value = 0;
    shift_type = 0;
    size = 8;
}

bool
janus::operator<(const Variable &a, const Variable &b)
{
    JVarPack v1, v2;
    v1.var = a;
    v2.var = b;
    //ignore size for non-stack variables (JAN-72)
    if (v1.var.type != JVAR_STACK) v1.var.size = 0;
    if (v2.var.type != JVAR_STACK) v2.var.size = 0;
    if (v1.data1 < v2.data1) return true;
    else if (v1.data1 == v2.data1) {
        if (v1.data2 < v2.data2) return true;
    }
    return false;
}

bool
janus::operator==(const Variable &a, const Variable &b)
{
    JVarPack v1, v2;
    v1.var = a;
    v2.var = b;
    //ignore size for non-stack variables (JAN-72)
    if (v1.var.type != JVAR_STACK) v1.var.size = 0;
    if (v2.var.type != JVAR_STACK) v2.var.size = 0;
    return (v1.data1 == v2.data1 && v1.data2 == v2.data2);
}

std::ostream& janus::operator<<(std::ostream& out, const Variable& v)
{
    switch (v.type) {
        case JVAR_REGISTER:
            out<<get_reg_name((int)v.value);
            break;
        case JVAR_STACK: {
            out<<"[RSP";
            int value = (int)v.value;
            if (value > 0) out <<"+0x"<<hex<<value<<"]";
            else if (value==0) out<<"]";
            else out <<"-0x"<<hex<<-value<<"]";
            break;
        }
        case JVAR_MEMORY:
        case JVAR_POLYNOMIAL: {
            out<<"[";
            int base = (int)v.base;
            int index = (int)v.index;
            int value = (int)v.value;
            int scale = (int)v.scale;
            if (base) out<<get_reg_name(base);
            if (index && base) out<<"+";
            if (index) {
                out<<get_reg_name(index);
                if (scale > 1) out <<"*"<<dec<<scale;
            }
            if (value > 0) out <<"+0x"<<hex<<value<<"]";
            else if (value==0) out<<"]";
            else out <<"-0x"<<hex<<-value<<"]";
            break;
        }
        case JVAR_STACKFRAME: {
            out<<"[";
            int base = (int)v.base;
            int index = (int)v.index;
            int value = (int)v.value;
            int scale = (int)v.scale;
            if (base) out<<get_reg_name(base);
            if (index && base) out<<"+";
            if (index) {
                out<<get_reg_name(index);
                if (scale > 1) out <<"*"<<dec<<scale;
            }
            if (value > 0) out <<"+0x"<<hex<<value<<"]";
            else if (value==0) out<<"]";
            else out <<"-0x"<<hex<<-value<<"]";
            break;
        }
        case JVAR_ABSOLUTE: {
            out<<"[";
            int64_t value = (int)v.value;
            if (value > 0) out <<"0x"<<hex<<value<<"]";
            else if (value==0) out<<"]";
            else out <<"-0x"<<hex<<-value<<"]";
            break;
        }
        case JVAR_CONSTANT: {
            int64_t value = (int)v.value;
            if (value>0) out <<"0x"<<hex<<value;
            else if (value<0) out<<"-0x"<<hex<<-value;
            else out<<"0";
            break;
        }
        case JVAR_CONTROLFLAG:
            out <<"CtrlFlag_"<<(int)v.value;
            break;
#ifdef JANUS_AARCH64
        case JVAR_SHIFTEDCONST:{
            int64_t value = (int)v.value;
            if (value>0) out <<"0x"<<hex<<value;
            else if (value<0) out<<"-0x"<<hex<<-value;
            else out<<"0";
            out << " " << get_shift_name(v.shift_type);
            out << " 0x" << (int)v.shift_value;
            break;
        }
        case JVAR_SHIFTEDREG: {
            out<<get_reg_name((int)v.value);
            out << " " << get_shift_name(v.shift_type);
            uint64_t shiftval = (int)v.shift_value;
            out << " 0x" << shiftval;
            break;
        }
#endif
        case JVAR_UNKOWN:
        default:
            out <<"UndefinedVar";
        break;
    }
    return out;
}

std::ostream& janus::operator<<(std::ostream& out, VarState* vs)
{
    Variable v = (Variable)(*vs);
    if (v.type != JVAR_CONSTANT && v.type != JVAR_SHIFTEDCONST)
        out<<v<<"_"<<dec<<vs->version;
    else
        out<<v;
    return out;
}

std::ostream& janus::operator<<(std::ostream& out, VarState& vs)
{
    //cast
    Variable v = vs;
    if (v.type != JVAR_CONSTANT && v.type != JVAR_SHIFTEDCONST)
        out<<v<<"_"<<dec<<vs.version;
    else
        out<<v;
    return out;
}

std::ostream& janus::operator<<(std::ostream& out, JVarProfile &vp)
{
    if (vp.type == INDUCTION_PROFILE) {
        out<<"Induction "<<(Variable)vp.var;
        if (vp.induction.op == UPDATE_ADD) out<<" add";
        else out <<" unknown";
        out <<" "<<(Variable)vp.induction.stride;
        if (vp.induction.check.type == JVAR_UNKOWN)
            out <<" no check";
        else
            out <<" check "<<(Variable)vp.induction.check;
    }
    return out;
}
