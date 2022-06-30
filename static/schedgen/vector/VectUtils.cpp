#include "VectUtils.h"

int currentLoopId;

static void printExpr(const Expr *expr, map<const Expr *, int> &seen)
{
    if (seen.find(expr) == seen.end()) {
        seen[expr] = seen.size();
        cout << expr->kind << endl;
        switch (expr->kind) {
        case (Expr::ExprKind::INTEGER):
            cout << expr->i;
            break;
        case (Expr::ExprKind::MEM):
            cout << " <" << seen[expr] << ",mem>(";
            printExpr(expr->m, seen);
            cout << ")";
        case (Expr::ExprKind::VAR):
            cout << " <" << seen[expr] << ",VAR," << expr->v << dec << ">(";
            printExpr(expr->v->expr, seen);
            cout << ")";
            break;
        case (Expr::ExprKind::UNARY):
            cout << " <" << seen[expr] << "," << expr->u.op << ">(";
            printExpr(expr->u.e, seen);
            cout << ")";
        case (Expr::ExprKind::BINARY):
            cout << "(";
            printExpr(expr->b.e1, seen);
            cout << " <" << seen[expr] << "," << expr->b.op << "> ";
            printExpr(expr->b.e2, seen);
            cout << ")";
        case (Expr::ExprKind::PHI):
            cout << "(";
            printExpr(expr->p.e1, seen);
            cout << " <" << seen[expr] << ",OR> ";
            printExpr(expr->p.e2, seen);
            cout << ")";
        default:
            cout << " UNKOWN ";
        }
    } else {
        cout << "expr(" << seen[expr] << ")";
    }
}

void printExpr(const Expr *expr)
{
    map<const Expr *, int> seen;
    printExpr(expr, seen);
}

void deleteMapValues(map<uint32_t, inductRegInfo *> &map)
{
    for (auto it = map.begin(); it != map.end(); it++) {
        delete it->second;
    }
}

void insertRegCheckRule(Loop *loop, uint8_t reg, uint32_t value,
                        set<VECT_RULE *> &rules)
{
    // Assume single basic block in body.
    for (auto bid : loop->body) {
        BasicBlock *bb = &(loop->parent->entry[bid]);
        uint64_t startPC = bb->instrs[0].pc;
        for (auto initBid : loop->init) {
            bb = &(loop->parent->entry[initBid]);
            MachineInstruction *instr = bb->instrs[bb->size - 1].minstr;
            PCAddress triggerPC = instr->pc;
            rules.insert(new VECT_REG_CHECK_rule(
                bb, triggerPC, instr->id, startPC - triggerPC, reg, value));
        }
    }
}

void insertUnalignedRule(Loop *loop, RegSet &unusedRegs, RegSet &freeRegs,
                         set<VECT_RULE *> &rules)
{
    // Assume single basic block in body.
    for (auto bid : loop->body) {
        BasicBlock *bb = &(loop->parent->entry[bid]);
        // Target start of basic block.
        uint64_t startPC = bb->instrs[0].pc;
        for (auto initBid : loop->init) {
            bb = &(loop->parent->entry[initBid]);
            MachineInstruction *instr = &(bb->instrs[bb->size - 1]);
            uint64_t triggerPC = instr->pc;
            rules.insert(new VECT_LOOP_PEEL_rule(
                bb, triggerPC, instr->id, startPC - triggerPC,
                unusedRegs.getNextLowest(JREG_RAX), freeRegs.getNextLowest(),
                loop->staticIterCount));
        }
    }
}

void insertForceSSERule(Loop *loop, set<VECT_RULE *> &rules)
{
    for (auto initBid : loop->init) {
        BasicBlock *bb = &(loop->parent->entry[initBid]);
        MachineInstruction *instr = &(bb->instrs[0]);
        rules.insert(new VECT_RULE(bb, instr->pc, instr->id, VECT_FORCE_SSE));
    }
}

void removeLiveRegs(Loop *loop, RegSet &freeRegs)
{
    for (Loop *parentLoop : loop->ancestors) {
        for (auto const &value : parentLoop->iterators) {
            if (value.second.kind ==
                janus::Iterator::IteratorKind::INDUCTION_IMM) {
                auto induct = value.first;
                freeRegs.remove(induct->getVar().getRegister());
            }
        }
    }
    RegSet liveRegSet = loop->start->liveRegSet(0);
    freeRegs.subtract(liveRegSet);
}

bool checkDependingRegs(Loop *loop, map<uint32_t, inductRegInfo *> &inductRegs,
                        set<uint32_t> &reductRegs)
{
    set<uint32_t> dependingRegs;
    loop->dependingRegs.toSTLSet(dependingRegs);
    for (auto reg : dependingRegs) {
        cout << "depends on " << getRegNameX86(reg) << endl;
        if (reductRegs.find(reg) == reductRegs.end() &&
            inductRegs.find(reg) == inductRegs.end()) {
            LOOPLOGLINE("loop " << dec << currentLoopId
                                << " has non-reduct/induct dependent reg.");
            return false;
        }
    }
    return true;
}

void updateRulesWithVectorWordSize(set<VECT_RULE *> &rules,
                                   uint32_t vectorWordSize)
{
    for (auto rule : rules) {
        rule->updateVectorWordSize(vectorWordSize);
    }
}