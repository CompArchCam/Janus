#include "DSLGen.h"
#include "JanusContext.h"
#include <type_traits>

using namespace janus;
using namespace std;

enum { BEFORE = 1, AFTER, AT, ENTRY, EXIT, ITER };
void generateCustomRules(JanusContext *jc)
{
    // call templates generated by DSL code generator
    ruleGenerationTemplate(*jc);
}

template <>
void insertCustomRule<Instruction>(int ruleID, Instruction &comp, int trigger,
                                   bool attach_data, int data)
{
    Instruction &instr = comp;
    RewriteRule rule;
    switch (trigger) {
    case BEFORE:
        if (instr.block) {
            rule = RewriteRule((RuleOp)ruleID, instr.block->instrs->pc,
                               instr.pc, instr.id);
            if (attach_data)
                rule.reg0 = data;
            else
                rule.reg0 = 0;
            insertRule(0, rule, instr.block);
        }
        break;
    case AFTER:
        if (instr.opcode ==
            Instruction::
                Call /*|| instr.opcode == Instruction::DirectBranch*/) {
            if (instr.block && instr.block->succ1) {

                rule = RewriteRule((RuleOp)ruleID, instr.block->succ1,
                                   POST_INSERT);
                if (attach_data)
                    rule.reg0 = data;
                else
                    rule.reg0 = 0;
                insertRule(0, rule, instr.block);
            }
        }
        break;
    default:
        cout << "ERROR: Invalid location for rule attachment" << endl;
        exit(1);
        break;
    }
}
template <>
void insertCustomRule<Function>(int ruleID, Function &comp, int trigger,
                                bool attach_data, int data)
{
    Function &func = comp;
    RewriteRule rule;
    if (!func.entry || !func.minstrs.size())
        return;
    switch (trigger) {
    case ENTRY:
        rule = RewriteRule((RuleOp)ruleID, func.entry, PRE_INSERT);
        if (attach_data)
            rule.reg0 = data;
        else
            rule.reg0 = 0;
        insertRule(0, rule, func.entry);
        break;
    case EXIT:
        for (auto retID : func.terminations) {
            BasicBlock &bb = func.blocks[retID];
            if (bb.lastInstr()->opcode == Instruction::Return) {
                rule = RewriteRule((RuleOp)ruleID, &bb, PRE_INSERT);
                if (attach_data)
                    rule.reg0 = data;
                else
                    rule.reg0 = 0;
                insertRule(0, rule, &bb);
            }
        }
        break;
    default:
        cout << "ERROR: Invalid location for rule attachment" << endl;
        exit(1);
        break;
    }
}
template <>
void insertCustomRule<BasicBlock>(int ruleID, BasicBlock &comp, int trigger,
                                  bool attach_data, int data)
{
    BasicBlock &bb = comp;
    RewriteRule rule;
    int instrID;
    switch (trigger) {
    case ENTRY:
        /*get first instruction of basic block*/
        instrID = bb.startInstID;
        rule = RewriteRule((RuleOp)ruleID, &bb, PRE_INSERT);
        if (attach_data)
            rule.reg0 = data;
        else
            rule.reg0 = 0;
        insertRule(0, rule, &bb);
        break;
    case EXIT:
        instrID = bb.endInstID;

        rule = RewriteRule(PROF_CALL_START, &bb, POST_INSERT);

        if (attach_data)
            rule.reg0 = data;
        else
            rule.reg0 = 0;

        insertRule(0, rule, &bb);
        break;
    default:
        break;
    }
}
template <>
void insertCustomRule<Loop>(int ruleID, Loop &comp, int trigger,
                            bool attach_data, int data)
{
    Loop &loop = comp;
    RewriteRule rule;
    switch (trigger) {
    case ENTRY:
        for (auto bb : loop.init) {
            rule = RewriteRule((RuleOp)ruleID, loop.parent->entry + bb,
                               POST_INSERT);
            if (attach_data) {
                rule.reg0 = data;
            } else
                rule.reg0 = 0;
            insertRule(loop.id, rule, loop.parent->entry + bb);
        }
        break;
    case EXIT:
        for (auto bb : loop.exit) {
            rule = RewriteRule((RuleOp)ruleID, loop.parent->entry + bb,
                               POST_INSERT);
            if (attach_data) {
                rule.reg0 = data;
            } else
                rule.reg0 = 0;
            insertRule(loop.id, rule, loop.parent->entry + bb);
        }
        break;
    case ITER:
        rule = RewriteRule((RuleOp)ruleID, loop.start, PRE_INSERT);
        if (attach_data)
            rule.reg0 = data;
        else
            rule.reg0 = 0;
        insertRule(loop.id, rule, loop.start);

        break;
    case BEFORE:
        break;
    case AFTER:
        break;
    default:
        cout << "ERROR: Invalid location for rule attachment" << endl;
        exit(1);
        break;
    }
}
