#include "JanusContext.h"
#include "SchedGen.h"
#include <string.h>

using namespace std;

static void usage()
{
    cout<<"Usage: analyze + <option> + <executable> + [profile_info]"<<endl;
    cout<<"Option:"<<endl;
    cout<<"  -a: static analysis without generating rules"<<endl;
    cout<<"  -c: generate custom analysis and rules from Cinnamon DSL"<<endl;
    cout<<"  -cfg: generate CFG from the binary"<<endl;
    cout<<"  -s: generate rules for secure execution"<<endl;
    cout<<"  -p: generate rules for automatic parallelisation"<<endl;
    cout<<"  -f: generate rules for automatic prefetch"<<endl;
    cout<<"  -lc: generate rules for loop coverage profiling"<<endl;
    cout<<"  -fc: generate rules for function coverage profiling"<<endl;
    cout<<"  -pr: generate rules for automatic loop profiling"<<endl;
    cout<<"  -pr -noshared: generate rules for automatic loop profiling & disable shared library profiling"<<endl;
    cout<<"  -o: generate rules for single thread optimisation"<<endl;
    cout<<"  -v: generate rules for automatic vectorisation"<<endl;
    cout<<"  -d: generate rules for testing dll instrumentation"<<endl;
}

int main(int argc, char **argv) {

    IF_VERBOSE(cout<<"---------------------------------------------------------------"<<endl);
    IF_VERBOSE(cout<<"\t\tJanus Static Binary Analyser"<<endl);
    IF_VERBOSE(cout<<"---------------------------------------------------------------"<<endl<<endl);

    if(argc != 3 && argc != 4) {
        usage();
        return 1;
    }

    JMode mode = JNONE;

    if(argv[1][0]=='-')
    {
        char option = argv[1][1];
        switch(option) {
            case 'a': mode = JANALYSIS; IF_VERBOSE(cout<<"Static analysis mode enabled"<<endl); break;
            case 'c':
                if (argv[1][2] == 'f' && argv[1][3] == 'g') {
                    mode = JGRAPH;
                    IF_VERBOSE(cout<<"Control flow graph mode enabled"<<endl);
                } else {
                    mode = JCUSTOM;
                    IF_VERBOSE(cout<<"Custom Cinnamon DSL mode enabled"<<endl);
                }
                break;
            case 'p':
                if (argv[1][2] == 'r') {
                    mode = JPROF;
                    IF_VERBOSE(cout<<"Automatic profiling mode enabled"<<endl);
                } else {
                    mode = JPARALLEL;
                    IF_VERBOSE(cout<<"Parallelisation mode enabled"<<endl);
                }
                break;
            case 'l': 
                if (argv[1][2] == 'c') {
                    mode = JLCOV;
                    IF_VERBOSE(cout<<"Loop coverage profiling mode enabled"<<endl); break;
                }
                else {
                    usage();
                    return 1;
                }
                break;
            case 'f': 
                if (argv[1][2] == 'c') {
                    mode = JFCOV;
                    IF_VERBOSE(cout<<"Function coverage profiling mode enabled"<<endl);
                }
                else {
                    mode = JFETCH;
                    IF_VERBOSE(cout<<"Software prefetch mode enabled"<<endl);
                }
                break;
            case 'v': mode = JVECTOR; IF_VERBOSE(cout<<"Loop vectorisation enabled"<<endl); break;
            case 's': mode = JSECURE; IF_VERBOSE(cout<<"Secure execution enabled"<<endl); break;
            case 'o': mode = JOPT; IF_VERBOSE(cout<<"Binary optimiser mode enabled"<<endl); break;
            case 'd': mode = JDLL; IF_VERBOSE(cout<<"Testing for dll instrumentation enabled"<<endl); break;
            default: {
                usage();
                return 1;
            }
        }
    }
    else {
        usage();
        return 1;
    }
    int argNo=2;
    bool sharedOn= true;
    if(argc ==4){
	if(strcmp(argv[2], "-noshared")==0 && mode == JPROF){
	    sharedOn = false;
	    argNo =3;
	}
	else{
	    usage();
	    return 1;
	}
    }
   
    GIO_Init(argv[argNo], mode);
   
    //Load executables
    JanusContext *jc = new JanusContext(argv[argNo], mode);

    //janus::Executable  program;
    // program.disassemble(this);
    char *filename = argv[argNo];
    ExecutableBinaryStructure executableBinaryStructure = ExecutableBinaryStructure(filename);
    // Returns a list of functions.
    // Updates main function and function map.
    // Updates external functions.
    Function *fmain;
    std::map<PCAddress, janus::Function *>*  functionMap = &(jc->functionMap);
    std::map<PCAddress, janus::Function *>* externalFunctions;
    std::vector<janus::Function> functions = executableBinaryStructure.disassemble(fmain, functionMap, externalFunctions);

    //
    jc->sharedOn= sharedOn;
    
    //build CFG
    jc->buildProgramDependenceGraph();

    //analyse
    jc->analyseLoop();

    if(mode == JANALYSIS || mode == JGRAPH) {
        dumpCFG(jc);
        dumpSSA(jc);
        dumpLoopCFG(jc);
        dumpLoopSSA(jc);
        generateExeReport(jc);
        generateExeReport(jc, &cout);
    } else {
        generateRules(jc);
    }

    delete jc;

    GIO_Exit();
}
