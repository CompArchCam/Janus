#include "UpdateSequence.h"
#include "BasicBlock.h"
#include "OptRule.h"

using namespace std;

namespace janus
{

// helper defining m[k]==0 as default
template <typename T> static inline int mapget(T k, map<T, int> &m)
{
    if (m.find(k) == m.end())
        return 0;
    return m[k];
}

void UpdateSequence::addUpdate(VariableState *vs, long long mult)
{
    if ((vs->getVar().field.type == VAR_IMM) ||
        (vs->getVar().field.type == VAR_RIP)) {
        constupd += mult * vs->getVar().field.value;
    } else {
        long long mul = mapget(vs->rep(), multiplier) + mult;
        if (mul) {
            multiplier[vs->rep()] = mul;
        } else
            multiplier.erase(vs->rep());
    }
}

void UpdateSequence::addConstUpdate(long long value) { constupd += value; }

int UpdateSequence::compute(BasicBlock *bb, int index, int in,
                            vector<new_instr *> &gen, int &varid)
{
    for (auto p : multiplier) {
        VariableState *vs = p.first;
        int mul = p.second;

        // For now we add mul times. This is not too bad since it results only
        // in at most as many
        //     instructions as the original update had (i += 100*j is translated
        //     into i += k where k is a constant (k==100j), hence this is only 1
        //     update)
        // Still, there might be some possible IMPROVEMENT here (add more
        // complex computations)

        pair<Operand, int> op;
        if (vs->type() == VARSTATE_MEMORY_OPND) {
            int baseid =
                evaluate(bb, index, vs->phi[0], bb->parentLoop, 0, gen, varid);
            op = {Operand(vs->getVar()), baseid};
        } else {
            new_instr def(bb->instrs[index].pc);
            int defvarid = varid++;
            def.def[defvarid] = bb->findEqualConstant(vs, index)->getVar();
            op = {Operand((uint32_t)1), defvarid};
        }

        // ADD updates
        for (int i = 0; i < mul; i++) {
            new_instr *ni = new new_instr(X86_INS_ADD, bb->instrs[index].pc);
            ni->push_var_opnd(in);
            ni->push_opnd(op.second, op.first);
            ni->outputs.push_back(in);
            gen.push_back(ni);
        }

        // SUB updates
        for (int i = 0; i > mul; i--) {
            new_instr *ni = new new_instr(X86_INS_SUB, bb->instrs[index].pc);
            ni->push_var_opnd(in);
            ni->push_opnd(op.second, op.first);
            ni->outputs.push_back(in);
            gen.push_back(ni);
        }
    }

    if (constupd) {
        new_instr *ni = new new_instr(
            (constupd > 0) ? X86_INS_ADD : X86_INS_SUB, bb->instrs[index].pc);
        ni->push_var_opnd(in);
        ni->push_opnd(-1, *getImmediateOperand((int64_t)abs(constupd)));

        ni->outputs.push_back(in);
        gen.push_back(ni);
    }

    return in;
}

pair<Operand, int> UpdateSequence::computeNtimes(BasicBlock *bb, int index,
                                                 int64_t N,
                                                 vector<new_instr *> &gen,
                                                 int &varid)
{
    if (multiplier.empty()) {
        // constant update -> return just an immediate operand
        Operand opnd;
        opnd.type = OPND_IMM;
        opnd.structure = OPND_NONE;
        opnd.size = 8;
        opnd.access = OPND_READ;

        opnd.intImm = N * constupd;
        return {opnd, -1};
    }

    // Otherwise start by loading the constant (immediate) update into a
    // register
    new_instr *ni = new new_instr(X86_INS_MOV, bb->instrs[index].pc);
    ni->push_var_opnd(varid);
    ni->push_opnd(-1, *getImmediateOperand(constupd));
    ni->outputs.push_back(varid);
    int in = varid;
    varid++;
    gen.push_back(ni);

    long long cu_save = constupd;
    constupd = 0ll;

    // compute non-immediate update
    in = compute(bb, index, in, gen, varid);

    constupd = cu_save;

    int out = varid;
    ni = new new_instr(X86_INS_IMUL, bb->instrs[index].pc);
    ni->push_var_opnd(varid);
    ni->push_opnd(-1, *getImmediateOperand(N));
    ni->outputs.push_back(varid);
    varid++;
    gen.push_back(ni);

    return {Operand(Variable((uint32_t)1)), out}; // Register operand
}

UpdateSequence::UpdateSequence() {}

UpdateSequence::UpdateSequence(const UpdateSequence &c)
{
    constupd = c.constupd;
    multiplier = c.multiplier;
}

bool UpdateSequence::operator==(const UpdateSequence &oth) const
{
    // the sum of constants has to agree
    if (constupd != oth.constupd)
        return false;

    // other updates have to agree as well
    UpdateSequence diff(oth);
    for (auto p : multiplier)
        diff.addUpdate(p.first, -p.second);
    return (!diff.multiplier.size());
}

bool UpdateSequence::operator!=(const UpdateSequence &oth) const
{
    return !(operator==(oth));
}

} // namespace janus
