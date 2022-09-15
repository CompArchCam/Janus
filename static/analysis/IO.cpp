#include "IO.h"
#include "BasicBlock.h"
#include "Instruction.h"
#include "JanusContext.h"
#include "Loop.h"
#include "Variable.h"
#include <algorithm>
#include <map>
#include <set>
#include <string>

#define DUMP_FULL_LOOP_INFO
//#define PRINT_CONTROL_DEP_IN_SSA

using namespace std;
using namespace janus;

static string filename;
static ofstream fcfg;
static ofstream floopcfg;
ofstream loopLog;
ofstream loopLog2;

int stepIndex;

void GIO_Init(const char *name, JMode mode)
{
    stepIndex = 0;
    filename = string(name);
#ifdef DUMP_LOOP_LOGS
    loopLog.open(filename + ".loop.log", ios::out);
#endif
#ifdef DUMP_LOOP_LOGS_LEVEL_2
    loopLog2.open(filename + ".loop.alias.log", ios::out);
#endif
}

void GIO_Exit()
{
#ifdef DUMP_LOOP_LOGS
    loopLog.close();
#endif
#ifdef DUMP_LOOP_LOGS_LEVEL_2
    loopLog2.close();
#endif
}

void dumpCFG(JanusContext *context)
{
    if (!context)
        return;
    ofstream fcfg;
    fcfg.open(filename + ".proc.cfg", ios::out);
    for (auto &f : context->functions) {
        if (f.isExecutable) {
            f.visualize(&fcfg);
            fcfg << endl;
        }
    }
    fcfg.close();
}

void dumpLoopCFG(JanusContext *context)
{
    if (!context)
        return;
    ofstream fcfg;
    fcfg.open(filename + ".loop.cfg", ios::out);
    for (auto &loop : context->loops) {
        loop.printDot(&fcfg);
        fcfg << endl;
    }
    fcfg.close();
}

void dumpSSA(JanusContext *context)
{
    if (!context)
        return;
    ofstream fssa;
    fssa.open(filename + ".proc.ssa", ios::out);

    for (auto &f : context->functions)
        if (f.isExecutable) {
            printSSADot(f, &fssa);
            fssa.flush();
        }

    fssa.close();
}

void dumpLoopSSA(JanusContext *context)
{
    ofstream lssa;
    lssa.open(filename + ".loop.ssa", ios::out);

    for (auto &l : context->loops) {
        printSSADot(l, &lssa);
    }

    lssa.close();
}

void printSSADot(janus::Function &function, void *outputStream)
{

    std::ostream &os = *(std::ostream *)outputStream;
    os << "digraph " << function.name << "_SSA {" << endl;
    os << "label=\"" << function.name << " SSA graph\";" << endl;

    // set generic style
    os << "node [style=\"rounded,filled\"]" << endl;

    // print nodes
    for (auto &vs : function.getCFG().ssaVariables) {
        if (vs->notUsed)
            continue;
        if (vs->isPHI) {
            os << "V" << dec << vs << " [label=\"" << vs << " in BB" << dec
               << vs->block->bid << "\"";
            if (vs->type == JVAR_STACK || vs->type == JVAR_STACKFRAME)
                os << ",fillcolor=dodgerblue";
            else
                os << ",fillcolor=cyan";
            os << ",shape=\"diamond\"];" << endl;
        } else if (vs->type == JVAR_MEMORY) {
            os << "V" << dec << vs << " [label=\"" << vs
               << "\",fillcolor=indigo,fontcolor=white,shape=box];" << endl;
        } else if (vs->type == JVAR_POLYNOMIAL) {
            os << "V" << dec << vs << " [label=\"" << vs
               << "\",fillcolor=pink,shape=box];" << endl;
        } else {
            os << "V" << dec << vs << " [label=\"" << vs << "\"";
            if (vs->type == JVAR_STACK || vs->type == JVAR_STACKFRAME)
                os << ",fillcolor=dodgerblue";
            os << ",shape=\"box\"];" << endl;
        }
    }

    for (auto &instr : function.instrs) {
        if (instr.opcode == Instruction::Nop)
            continue;
        else if (instr.opcode == Instruction::Call) {
            os << "I" << dec << instr.id << " [label=\"" << instr
               << "\",shape=\"box\",fillcolor=crimson, fontcolor=white];"
               << endl;
        } else
            os << "I" << dec << instr.id << " [label=\"" << instr
               << "\",shape=\"box\",fillcolor=gold];" << endl;
    }

    // print edges
    for (auto &instr : function.instrs) {
        for (auto vs : function.getCFG().getSSAVarRead(instr)) {
            os << "V" << dec << vs << " -> I" << instr.id << ";" << endl;
        }
        for (auto vs : function.getCFG().getSSAVarWrite(instr)) {
            if (vs->notUsed)
                continue;
            os << "I" << dec << instr.id << " -> V" << vs << ";" << endl;
        }
    }

    // print phi node and memory edges
    for (auto &vs : function.getCFG().ssaVariables) {
        if (vs->notUsed)
            continue;
        if (vs->isPHI) {
            for (auto phi : vs->pred) {
                os << "V" << dec << phi << " -> V" << vs << ";" << endl;
            }
        }
        if (vs->type == JVAR_MEMORY || vs->type == JVAR_POLYNOMIAL) {
            for (auto vi : vs->pred) {
                os << "V" << dec << vi << " -> V" << vs << ";" << endl;
            }
        }
        if (vs->type == JVAR_SHIFTEDCONST || vs->type == JVAR_SHIFTEDREG) {
            for (auto vi : vs->pred) {
                os << "V" << dec << vi << " -> V" << vs << ";" << endl;
            }
        }
    }
    os << "}" << endl << endl;
}

static void addPredRecurisve(VarState *vs, set<VarState *> &loopNodeExpanded)
{
    loopNodeExpanded.insert(vs);
    for (auto pred : vs->pred) {
        if (loopNodeExpanded.find(pred) == loopNodeExpanded.end())
            addPredRecurisve(pred, loopNodeExpanded);
        else {
            // check a little bit further
            for (auto pred2 : pred->pred) {
                if (loopNodeExpanded.find(pred2) == loopNodeExpanded.end())
                    addPredRecurisve(pred2, loopNodeExpanded);
            }
        }
    }
}

void printSSADot(janus::Loop &loop, void *outputStream)
{
    Function *parent = loop.parent;

    std::ostream &os = *(std::ostream *)outputStream;
    os << "digraph Loop_" << loop.id << "_SSA {" << endl;
    os << "label=\"Loop " << loop.id
       << " SSA graph in function:" << loop.parent->name << "\";" << endl;

    // set generic style
    os << "node [style=\"rounded,filled\"]" << endl;

    BasicBlock *entry = loop.parent->getCFG().entry;
    set<VarState *> loopNodes;
    set<VarState *> loopNodeExpanded;
    for (auto bid : loop.body) {
        BasicBlock &bb = entry[bid];
        for (int i = 0; i < bb.size; i++) {
            Instruction &instr = bb.instrs[i];
            for (auto vs : parent->getCFG().getSSAVarRead(instr)) {
                loopNodes.insert(vs);
            }
            for (auto vs : parent->getCFG().getSSAVarWrite(instr)) {
                loopNodes.insert(vs);
            }
        }
    }
    // for phi nodes, prints a little bit further than loop nodes
    for (auto vs : loopNodes) {
        loopNodeExpanded.insert(vs);
        // recursively add new predecessors
        for (auto pred : vs->pred) {
            addPredRecurisve(pred, loopNodeExpanded);
        }
    }

    // print nodes
    for (auto vs : loopNodeExpanded) {
        if (vs->notUsed)
            continue;
        if (vs->isPHI) {
            os << "V" << dec << vs->id << " [label=\"" << vs << " in BB" << dec
               << vs->block->bid;
            if (loop.mainIterator && vs == loop.mainIterator->vs)
                os << " main iterator";
            os << "\"";
            if (loop.iterators.find(vs) != loop.iterators.end())
                os << ",fillcolor=lawngreen";
            else if (vs->type == JVAR_STACK || vs->type == JVAR_STACKFRAME)
                os << ",fillcolor=dodgerblue";
            else
                os << ",fillcolor=cyan";
            os << ",shape=\"diamond\"];" << endl;
        } else if (vs->type == JVAR_MEMORY) {
            os << "V" << dec << vs->id << " [label=\"" << vs
               << "\",fillcolor=indigo,fontcolor=white,shape=box];" << endl;
        } else if (vs->type == JVAR_POLYNOMIAL) {
            os << "V" << dec << vs->id << " [label=\"" << vs
               << "\",fillcolor=pink,shape=box];" << endl;
        } else {
            os << "V" << dec << vs->id << " [label=\"" << vs << "\"";
            if (vs->type == JVAR_STACK || vs->type == JVAR_STACKFRAME)
                os << ",fillcolor=dodgerblue";
            else if (loop.isConstant(vs))
                os << ",fillcolor=white";
            os << ",shape=\"box\"];" << endl;
        }
    }

    for (auto bid : loop.body) {
        BasicBlock &bb = entry[bid];
        for (int i = 0; i < bb.size; i++) {
            Instruction &instr = bb.instrs[i];
            if (instr.opcode == Instruction::Nop)
                continue;
            else if (instr.opcode == Instruction::Call) {
                os << "I" << dec << instr.id << " [label=\"" << instr
                   << "\",shape=\"box\",fillcolor=crimson, fontcolor=white];"
                   << endl;
            } else
                os << "I" << dec << instr.id << " [label=\"" << instr
                   << "\",shape=\"box\",fillcolor=gold];" << endl;
        }
    }

    // print edges
    for (auto bid : loop.body) {
        BasicBlock &bb = entry[bid];
        for (int i = 0; i < bb.size; i++) {
            Instruction &instr = bb.instrs[i];
            for (auto vs : parent->getCFG().getSSAVarRead(instr)) {
                os << "V" << dec << vs->id << " -> I" << instr.id << ";"
                   << endl;
            }
            for (auto vs : parent->getCFG().getSSAVarWrite(instr)) {
                if (vs->notUsed)
                    continue;
                os << "I" << dec << instr.id << " -> V" << vs->id << ";"
                   << endl;
            }
        }
    }

    // print control dependent edges
#ifdef PRINT_CONTROL_DEP_IN_SSA
    for (auto bid : loop.body) {
        BasicBlock &bb = entry[bid];
        for (int i = 0; i < bb.size; i++) {
            Instruction &instr = bb.instrs[i];
            for (auto cd : instr.controlDep) {
                // the control dependent must be in the loop
                if (loop.contains(cd->block->bid))
                    os << "I" << dec << cd->id << " -> I" << instr.id
                       << " [style=\"dashed\", color=crimson];" << endl;
            }
        }
    }
#endif
    // print phi node and memory edges
    for (auto vs : loopNodeExpanded) {
        if (vs->notUsed)
            continue;
        if (vs->isPHI) {
            for (auto phi : vs->pred) {
                os << "V" << dec << phi->id << " -> V" << vs->id << ";" << endl;
            }
        }
        if (vs->type == JVAR_MEMORY || vs->type == JVAR_POLYNOMIAL) {
            for (auto vi : vs->pred) {
                os << "V" << dec << vi->id << " -> V" << vs->id << ";" << endl;
            }
        }
        if (vs->type == JVAR_SHIFTEDCONST || vs->type == JVAR_SHIFTEDREG) {
            for (auto vi : vs->pred) {
                os << "V" << dec << vi->id << " -> V" << vs->id << ";" << endl;
            }
        }
    }
    os << "}" << endl << endl;
}

void generateExeReport(JanusContext *context)
{
    ofstream report;
    report.open(filename + ".report.txt", ios::out);
    generateExeReport(context, &report);
    report.close();
}

void generateExeReport(JanusContext *context, void *outputStream)
{
    ofstream &report = *(ofstream *)outputStream;

    report << "----------------------------------------------------------------"
              "-----------"
           << endl;
    report << "Janus Static Analysis Report for Executable : " << context->name
           << endl;
    report << "----------------------------------------------------------------"
              "-----------"
           << endl;

    report << "\tAnalysis Summary:" << endl;

    report << "\tNo. of recognised functions: " << context->functions.size()
           << endl;
    int num_func_with_loops = 0;
    int num_leaf_func = 0;
    for (auto &func : context->functions) {
        if (func.loops.size()) {
            num_func_with_loops++;
            if (func.subCalls.size() == 0)
                num_leaf_func++;
        }
    }
    report << "\tNo. of functions that contain loops: " << dec
           << num_func_with_loops << endl;
    report << "\tNo. of leaf functions that contain loops: " << num_leaf_func
           << endl;

    report << "\n\tNo. of recognised loops: " << context->loops.size() << endl;
    int num_loops_nests = 0;
    int num_loops_with_iter = 0;
    int num_unsafe_loops = 0;
    int num_undecided_loops = 0;
    int num_loops_with_subcall = 0;
    int num_loops_with_static_dep = 0;
    int num_doall_loops = 0;
    int num_profile_loops = 0;
    bool shared_call_found = false;
    int num_loops_with_shared_call = 0;
    int num_doall_loops_with_runtime_check = 0;
    set<FuncID> shared_call_ids;
    for (auto &loop : context->loops) {
        if (loop.ancestors.size() == 0)
            num_loops_nests++;
        if (loop.mainIterator)
            num_loops_with_iter++;
        if (loop.unsafe)
            num_unsafe_loops++;
        if (loop.undecidedMemAccesses.size())
            num_undecided_loops++;
        if (loop.subCalls.size())
            num_loops_with_subcall++;
        if (loop.memoryDependences.size() &&
            loop.undecidedMemAccesses.size() == 0)
            num_loops_with_static_dep++;
        if (!loop.unsafe && loop.mainIterator &&
            loop.memoryDependences.size() == 0 &&
            loop.undecidedMemAccesses.size() == 0) {
            if (loop.memoryAccesses.size() > 1)
                num_doall_loops_with_runtime_check++;
            else
                num_doall_loops++;
        }
        if (!loop.unsafe && loop.mainIterator &&
            loop.memoryDependences.size() == 0 &&
            loop.undecidedMemAccesses.size() != 0)
            num_profile_loops++;

        shared_call_found = false;
        for (auto sub : loop.subCalls) {
            if (context->functions[sub].isExternal) {
                shared_call_found = true;
                shared_call_ids.insert(sub);
            }
        }
        if (shared_call_found)
            num_loops_with_shared_call++;
    }
    report << "\tNo. of recognised loop nests: " << num_loops_nests << endl;
    report << "\tNo. of loops that could not be parallelised by Janus "
           << num_unsafe_loops << endl;
    report << "\tNo. of loops with recognised loop iterators "
           << num_loops_with_iter << endl;
    report << "\tNo. of loops with ambiguous memory accesses "
           << num_undecided_loops << endl;
    report << "\tNo. of loops with clear static dependencies "
           << num_loops_with_static_dep << endl;
    report << "\tNo. of loops with sub function calls "
           << num_loops_with_subcall << endl;
    report << "\tNo. of loops with sub shared library calls "
           << num_loops_with_shared_call << endl;
    for (auto sb : shared_call_ids)
        report << "\t\t" << context->functions[sb].name << endl;

    report << "\n\tNo. of static DOALL loops " << num_doall_loops << endl;
    report << "\tNo. of DOALL loops that requires runtime array base check "
           << num_doall_loops_with_runtime_check << endl;
    report << "\tNo. of loops that requires profiling " << num_profile_loops
           << endl;
}
