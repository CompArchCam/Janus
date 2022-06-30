#include "new_instr.h"
#include "Operand.h"
#include <cassert>
#include <sstream>

using namespace std;
using namespace janus;

void new_instr::push_var_opnd(int id)
{
    Operand op;
    op.type = OPND_REG;
    op.value = 1; // RAX -- arbitrary
    op.size = 8;
    op.structure = OPND_NONE;
    opnds.push_back({op, id});
}

std::string new_instr::print()
{
    stringstream ss;

    if (opcode == X86_INS_INVALID) {
        ss << "Definition: initial value of variable #" << def.begin()->first
           << " is " << def.begin()->second.print() << "." << endl;
    } else {
        ss << "Operation: " << opcode << "; Operands: ";
        for (auto p : opnds) {
            ss << "(" << p.first.name() << ", #" << p.second << "), ";
        }
        if (cloned)
            ss << "Clone of: " << cloned->id << " " << cloned->name;
        if (fixedRegisters)
            ss << " -> fixed registers";
        ss << endl;
    }

    return ss.str();
}

RegSet new_instr::originalRegsRead()
{
    RegSet ret;
    if ((!fixedRegisters) && (def.size())) {
        // definition ==> reads all registers it assigns values to
        for (auto &p : def) {
            if (p.second.field.type == VAR_REG)
                ret.insert(p.second.field.value);
        }
    }

    return ret;
}

new_instr_type new_instr::type()
{
    if (fixedRegisters) {
        assert(opcode != X86_INS_INVALID);
        return NEWINSTR_FIXED;
    }

    if (def.size()) {
        assert(opcode == X86_INS_INVALID);
        return NEWINSTR_DEF;
    }

    assert(opcode != X86_INS_INVALID);
    return NEWINSTR_OTHER;
}

bool new_instr::hasMemoryAccess()
{
    if (cloned)
        return cloned->hasMemoryAccess();
    if (type() == NEWINSTR_DEF)
        return false;
    if (opcode == X86_INS_LEA)
        return false;

    for (auto p : opnds) {
        if (p.second == -1)
            continue;
        if (p.first.isMemoryAccess())
            return true;
    }
    return false;
}
