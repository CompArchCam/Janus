#include "JanusContext.h"
#include "IO.h"
#include "janus_arch.h"
#include "LoopSelect.h"
#include <map>

using namespace std;
using namespace janus;

static bool
encodeLoopVariables(JanusContext *jc, Loop &loop);
static void rejectLoopFromRuntimeFeedback(JanusContext *jc, std::set<LoopID> &selected, std::string name);
//static bool selectLoopFromRuntimeFeedback(JanusContext *jc, std::set<LoopID> &selected);
static bool selectLoopFromRuntimeFeedback(std::set<LoopID> &selected, std::string name, std::vector<janus::Loop>& loops);

//currently we only checks the name only
static bool
checkSafeCall(Function &func)
{
    if (func.name.find("pow@plt") != std::string::npos) return true;
    if (func.name.find("sqrt@plt") != std::string::npos) return true;
    return false;
}

//static bool checkSafeSubCalls(Loop &loop)
static bool checkSafeSubCalls(Loop &loop, std::vector<janus::Function>& functions)
{
    //JanusContext *context = loop.parent->context;
    for (auto fid: loop.subCalls) {
        //Function &func = context->functions[fid];
    	Function &func = functions[fid];
        if (!checkSafeCall(func)) return false;
        IF_LOOPLOG(else loopLog<<"\tFunction call"<<func.name<<" indentified safe"<<endl);
    }
    return true;
}

bool loopHasFPUInstructions(Loop &loop){
    Function *function = loop.parent;
    //JanusContext *jc = function->context;
    for (auto bid: loop.body) {
        BasicBlock &bb = function->entry[bid];
        for (int i=0; i<bb.size; i++) {
            Instruction &instr = bb.instrs[i];
            if (instr.minstr->isFPU()){
                return true;
            }
        }
    }
    return false;
}

//bool checkSafetyForParallelisation(Loop &loop)
bool checkSafetyForParallelisation(janus::Loop &loop, std::vector<janus::Function>& functions, std::map<PCAddress, janus::Function *>& functionMap)
{
    Function *function = loop.parent;
    //JanusContext *jc = function->context;
    PCAddress target = 0;

#ifdef NEED_MORE_TEST
    if (loop.calls.size()) {
        //if (!checkSafeSubCalls(loop))
    	if (!checkSafeSubCalls(loop, functions))
            return false;
    }
#endif

    //currently we don't support loops with multiple exit
    if (loop.exit.size() > 1) {
        LOOPLOG("\tLoop is rejected currently we don't support loops with multiple exits"<<endl);
        return false;
    }

#ifdef NEED_MORE_TEST
    //check indirect calls in this loop
    for (auto bid: loop.body) {
        BasicBlock &bb = function->entry[bid];

        for (int i=0; i<bb.size; i++) {
            Instruction &instr = bb.instrs[i];
            //check instructions for unsafe operations
            if (instr.isNotSupported()) {
                IF_LOOPLOG(loopLog<<"\tFound unsafe instruction ";instr.print(&loopLog));
                return false;
            }
        }
        if (bb.lastInstr()->isIndirectCall()) {
            IF_LOOPLOG(loopLog<<"\tFound unsafe indirect call ";bb.lastInstr()->print(&loopLog));
            return false;
        }

        else if (bb.lastInstr()->fineType == INSN_CALL) {
            target = bb.lastInstr()->getTargetAddress();
            //if (jc->functionMap.find(target) == jc->functionMap.end()) {
            if (functionMap.find(target) == functionMap.end()) {
                LOOPLOG("\tFound unsafe external call: "<<hex<<bb.lastInstr()->getTargetAddress()<<endl);
                return false;
            }
            if (function->unRecognised.find(bb.lastInstr()->id) != function->unRecognised.end()) {
                LOOPLOG("\tFound unsafe call: ");
                IF_LOOPLOG(bb.lastInstr()->print(&loopLog));
                return false;
            }
        }
    }
#endif
    return true;
}

/* Conditions for current DOALL selection 
 * 1. The Phi node of the loop's start block is either constant or induction variables
 * 2. All its memory accesses are either LOOP_MEM_CONSTANT or LOOP_MEM_INDEPENDENT_ARRAY with no memory alias.
 * 3. No undecided memory accesses (LOOP_MEM_UNDECIDED)
 * 4. No cross-iteration dependences (LOOP_MEM_MAY_ALIAS_ARRAY)
 * 5. Safety checks, see checkSafetyForParallelisation()
 * 6. Remove redudant loops in the same loop nest (last step)
 */
//void selectDOALLLoops(JanusContext *jc, std::set<LoopID> &selected)
void selectDOALLLoops(std::set<LoopID> &selected, std::vector<janus::Function>& functions, std::map<PCAddress, janus::Function *>& functionMap, std::vector<janus::Loop>& loops,
		LoopAnalysisReport& loopAnalysisReport, janus::Function* fmain, std::string name)
{
    LOOPLOG("Performing automatic DOALL loop selection:"<<endl);

    //for (auto &loop : jc->loops) {
    for (auto &loop : loops) {
        //if (jc->manualLoopSelection && !loop.pass) continue;
    	if (loopAnalysisReport.getManualLoopSelection() && !loop.pass) continue;
        LOOPLOG("Loop "<<dec<<loop.id<<":");
        if (loop.unsafe) {
            LOOPLOG("is unsafe, therefore rejected"<<endl);
            continue;
        } else {
            LOOPLOG(endl);
        }
        bool passed = true;

        //Condition 0: Loop doesn't have init in same BB as start of main
        //BasicBlock *mainEntry = jc->main->entry;
        BasicBlock *mainEntry = fmain->entry;
        Function *parent = loop.parent;
        BasicBlock *parentEntry = parent->entry;
        for (auto bid: loop.init) {
            BasicBlock *bb = parentEntry + bid;
            if (bb == mainEntry){
                LOOPLOG("Loop will have THREAD_CREATE in same BB as PARA_LOOP_INIT, so PARA_LOOP_INIT will create an infinite loop when run, therefore loop is rejected");
                passed = false;
            }
            for (auto bb2: bb->pred){
                if (bb2->fake && bb2 == mainEntry){
                    LOOPLOG("Loop will have THREAD_CREATE in same BB as PARA_LOOP_INIT, so PARA_LOOP_INIT will create an infinite loop when run, therefore loop is rejected");
                    passed = false;
                }
            }
        }

        //condition 1: no undecided cross-iteration dependencies
        if (loop.undecidedPhiVariables.size()) {
            for (auto phi: loop.undecidedPhiVariables) {
                LOOPLOG("\tphi node "<<phi<<"undecided"<<endl);
            }
            passed = false;
            LOOPLOG("\tFound dependencies that could not handled by Janus"<<endl);
        }

        //condition 2: the loop must have a main iterator
        if (!loop.mainIterator) {
            LOOPLOG("\tFound no main iterator for this loop"<<endl);
            passed = false;
        }

        //condition 3: no undecided memory accesses
        //if (!jc->manualLoopSelection) {
        if (!loopAnalysisReport.getManualLoopSelection()) {
            if (loop.undecidedMemAccesses.size()) {
                LOOPLOG("\tFound undecided memory accesses"<<endl);
                passed = false;
            }

        //condition 4: no memory dependencies
        if (loop.memoryDependences.size()) {
            LOOPLOG("\tFound depending memory accesses"<<endl);
            passed = false;
        }

        }

        //if (!checkSafetyForParallelisation(loop)) {
        if (!checkSafetyForParallelisation(loop, functions, functionMap)) {
            LOOPLOG("\tThis loop is not safe for DOALL parallelisation"<<endl);
            passed = false;
        }

        //condition 6 removed in the future
        //no simd register in the induction variable
        for (auto iter: loop.iterators) {
            VarState *vs = iter.first;
            if (vs->type == JVAR_REGISTER && jreg_is_simd(vs->value)) {
                LOOPLOG("\tThis loop contains SIMD iterators which is not yet implemented"<<endl);
                passed = false;
                break;
            }
        }
        
        //condition 7: no FPU instructions
        //We currently don't really analyze them, which causes issues
        if (loopHasFPUInstructions(loop)){
            LOOPLOG("\tThis loop has FPU instructions, which are currently not analyzed by Janus, so loop might be unsafe!");
            passed = false;
        }

        if (passed) {
            LOOPLOG("\tDOALL Check Phase 1 Passed"<<endl);
            selected.insert(loop.id);
        } else loop.unsafe = true;
        LOOPLOG(""<<endl);
    }

    /* unset all the loops for manual selected loops */
    //if (jc->manualLoopSelection) {
	if (loopAnalysisReport.getManualLoopSelection()) {
        //for (auto &loop : jc->loops)
		for (auto &loop : loops)
            loop.pass = false;
    }

    LOOPLOG("-- Phase 1 Finished -- Recognised safe DOALL loops: "<<endl);
    IF_LOOPLOG(
        for (auto id: selected)
            loopLog <<dec<<id<<" ";
        loopLog <<endl<<endl
    );
 
    /* Step 7: select loops from manual specification */
    //selectLoopFromRuntimeFeedback(jc, selected);
    selectLoopFromRuntimeFeedback(selected, name, loops);

    /* Step 8: Filter loop ness for manual selection- Added by MA*/
    //filterLoopNests(jc, selected);
    filterLoopNests(selected, loops);

    printf("		selectDOALLLoops::Number of selected loops for parallelization = %lu. \n", selected.size());

    LOOPLOG("-- Final selected DOALL loops: "<<endl);
    IF_LOOPLOG(
    for (auto id: selected)
        loopLog <<dec<<id<<" ";
    loopLog <<endl<<endl
    );

    cout <<"\tFinal selected DOALL loops: ";
    for (auto id: selected)
        cout <<dec<<id<<" ";
    cout <<endl;
    printf("	selectDOALLLoops::DONE. \n");
}

//void filterLoopNests(JanusContext *jc, std::set<LoopID> &selected)
void filterLoopNests(std::set<LoopID> &selected, std::vector<janus::Loop>& allLoops)
{
    //Loop *loops = jc->loops.data();
	Loop *loops = allLoops.data();
    map<FuncID,set<Loop*>> buckets;
    set<Loop*> toRemove;

    //sort loops into function buckets
    for (auto loopID : selected) {
        Loop *loop = loops + loopID - 1;
        FuncID fid = loop->parent->fid;
        buckets[fid].insert(loop);
    }

    //remove children loop in each bucket
    for (auto bucket: buckets) {
        for (auto loop: bucket.second) {
            for (auto loop2: bucket.second) {
                if (loop != loop2) {
                    if (loop2->isAncestorOf(loop)) {
                        //remove this loop
                        toRemove.insert(loop);
                        LOOPLOG("Loop "<<dec<<loop->id<<" is removed because it is the inner loop in the outter loop "<<loop2->id<<endl);
                    }
                }
            }
        }
    }

    //clear the selected
    selected.clear();
    //reload the new version
    for (auto bucket: buckets) {
        for (auto loop: bucket.second) {
            if (toRemove.find(loop) == toRemove.end())
                selected.insert(loop->id);
        }
    }
}

//void filterLoopHeuristicBased(JanusContext *jc, std::set<LoopID> &selected)
// TODO: Is this a dead function? No one seems to call it.
/*void filterLoopHeuristicBased(JanusContext *jc, std::set<LoopID> &selected, std::vector<janus::Loop>& allLoops)
{
    //auto &loopPool = jc->loops;
	auto &loopPool = allLoops;
    set<LoopID> toRemove;

    //sort loops into buckets
    for (auto loopID : selected) {
        Loop &loop = loopPool[loopID-1];

        //condition 1: basic block size is 1 and instruction size is less than 10
        if (loop.body.size() == 1 && loop.start->size < 10)
            toRemove.insert(loopID);

        //condition 2: low iteration count
        /
        //if (loop.staticIterCount != -1) {
            //if (loop.staticIterCount < 16) {
                //LOOPLOG("Loop "<<dec<<loop.id<<" is removed because it has an iteration count of "<<loop.staticIterCount<<endl);
                //toRemove.insert(loopID);
            //}
        //}
    }

    for (auto id: toRemove)
        selected.erase(id);
}*/

static bool
encodeLoopVariables(JanusContext *jc, Loop &loop)
{
    //XXX: TODO currently we reject induction variables with negative stride
    /*
    for (auto &ind: loop.encodedVariables) {
        if (ind.type == INDUCTION_PROFILE) {
            if (ind.induction.op == UPDATE_ADD) {
                if (ind.induction.stride.type == JVAR_CONSTANT) {
                    if ((int64_t)(ind.induction.stride.value) < 0)
                        return false;
                }
            }
        }
    }
    */
    return !loop.unsafe;
}

//void checkRuntimeQuestion(JanusContext *jc, std::set<LoopID> &selected)
// TODO: Seems like dead code.
//{
//    auto &loopPool = jc->loops;

    //sort loops into buckets
    /*
    for (auto loopID : selected) {
        Loop &loop = loopPool[loopID-1];
        //if there is only one array, it is fine, no need for dynamic alias analysis
        bool singleIndex = true;
        int num_int = 0;
        if (loop.arrayBases.size()<2) continue;
        for (auto arrayBase: loop.arrayBases) {
            if (!arrayBase.second.isConstant()) {
                singleIndex = false;
                continue;
            }
            if (arrayBase.second.size() > 1) singleIndex = false;
            else {
                for (auto e:arrayBase.second.exprs) {
                    if (e.first == (VariableState *)1)
                        num_int++;
                    else if (e.first->getVar().field.type == VAR_IMM)
                        num_int++;
                }
            }
        }
        num_int = loop.arrayBases.size() - num_int;
        if (singleIndex && num_int > 1) loop.needRuntimeCheck = true;
    }*/
//}

//static void rejectLoopFromRuntimeFeedback(JanusContext *jc, std::set<LoopID> &selected)
static void rejectLoopFromRuntimeFeedback(JanusContext *jc, std::set<LoopID> &selected, std::string name)
{
    /* Read ddg files from manual specification */
    ifstream iselect;
    //iselect.open(jc->name+".loop.reject", ios::in);
    iselect.open(name+".loop.reject", ios::in);
    set<LoopID> toRemove;

    if (!iselect.good()) return;
    //LOOPLOG("\tReading "<<jc->name<<".loop.reject for loop reject"<<endl);
    LOOPLOG("\tReading "<<name<<".loop.reject for loop reject"<<endl);

    int loopID,type;
    while (iselect >> loopID >> type) {
        if (type == 0) {
            if (selected.find(loopID) != selected.end()) {
                LOOPLOG("Loop "<<dec<<loopID<<" is rejected because it is beneficial from previous runtime "<<endl);
                toRemove.insert(loopID);
            }
        } else if (type == 1) {
            selected.insert(loopID);
        }
    }

    for (auto id: toRemove) {
        selected.erase(id);
    }
}

//static bool selectLoopFromRuntimeFeedback(JanusContext *jc, std::set<LoopID> &selected)
static bool selectLoopFromRuntimeFeedback(std::set<LoopID> &selected, std::string name, std::vector<janus::Loop>& loops)
{
	printf("selectLoopFromRuntimeFeedback::loops.size() = %lu\n", loops.size());
	printf("selectLoopFromRuntimeFeedback:name = %s\n", name.c_str());
    ifstream iselect;
    //iselect.open(jc->name+".loop.select", ios::in);
    iselect.open(name+".loop.select", ios::in);

    if (!iselect.good()) return false;
    //LOOPLOG("\tReading "<<jc->name<<".loop.select for manual loop selection"<<endl);
    LOOPLOG("\tReading "<<name<<".loop.select for manual loop selection"<<endl);

    selected.clear();
    int loopID,type;
    IF_LOOPLOG(loopLog<<"\t");
    while (iselect >> loopID >> type) {
        selected.insert(loopID);
        IF_LOOPLOG(loopLog<<loopID<<" ");
    }
    LOOPLOG(endl);

    //remove unsafe loops
    for (auto lidIter = selected.begin(); lidIter != selected.end() ; ){
        //if (jc->loops[(*lidIter)-1].unsafe)
    	if (loops[(*lidIter)-1].unsafe)
            selected.erase(lidIter++);
        else
            ++lidIter;
    }
    return true;
}
