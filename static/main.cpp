#include "JanusContext.h"
#include "SchedGen.h"
#include "ProgramDependenceAnalysis.h"
#include "SourceCodeStructure.h"
#include "LoopAnalysisReport.h"

#include "Profile.h"

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

    printf("Default analysis --- START --- \n");

    GIO_Init(argv[argNo], mode);
   
    //Load executables
    JanusContext *jc = new JanusContext(argv[argNo], mode);

    //janus::Executable  program;
    // program.disassemble(this);
    char *executableName = argv[argNo];

    janus::ExecutableBinaryStructure executableBinaryStructure = janus::ExecutableBinaryStructure(string(executableName));
    //SourceCodeStructure sourceCodeStructure;

    if(executableBinaryStructure.isExecutable)
    	printf("isExecutable\n");

    printf("Executable name: %s\n", executableBinaryStructure.getExecutableName().c_str());
    printf("WordSize = %d\n", executableBinaryStructure.wordSize);
    printf("sections.size() = %lu\n", executableBinaryStructure.sections.size());
    printf("symbols.size() = %lu\n", executableBinaryStructure.symbols.size());
    printf("crossSectionRef.size() = %lu\n", executableBinaryStructure.crossSectionRef.size());

    // The main function of the program (not entry)
    janus::Function *fmain;
    //std::map<PCAddress, janus::Function *>* functionMap = &(jc->functionMap);
    //std::map<PCAddress, janus::Function *> functionMap = sourceCodeStructure.getFunctionMap();
    std::map<PCAddress, janus::Function *> functionMap;
    std::map<PCAddress, janus::Function *> externalFunctions;
    // TODO: Program object to contain fmain, functionMap, externalFunctions
    // Updates fmain, &functionMap, externalFunctions, and returns functions
    // TODO: Hmm, it seems that it does not update fmain and that should be fine as the original logic does the same.
    // Returns a list of functions.
    // Updates main function and function map.
    // Updates external functions.
    printf("	executableBinaryStructure.disassemble --- START --- \n");

    std::vector<janus::Function> functions;
    // Function class contains several pointers.
    // Hence, default copy constructor won't work (many of those are destroyed in the destructor).
    printf("	disassemble --- START --- \n");
    executableBinaryStructure.disassemble(functions, &fmain, functionMap, externalFunctions);
    printf("	disassemble --- DONE --- \n");

    // Update the code structure with analysis results.
    //sourceCodeStructure.updateBinaryStructure(fmain, functions, functionMap);
    //
    jc->sharedOn= sharedOn;

    //build CFG
    //jc->buildProgramDependenceGraph();
    janus::ProgramDependenceAnalysis programDependenceAnalysis;

    //programDependenceAnalysis.buildCFGForEachFunction(functions);
    printf("	buildCFGForEachFunction --- START --- \n");
    programDependenceAnalysis.buildCFGForEachFunction(functions, functionMap);
    printf("	buildCFGForEachFunction --- DONE --- \n");

    printf("	liftDisassemblyToIR --- START --- \n");
    programDependenceAnalysis.liftDisassemblyToIR(functions);
    printf("	liftDisassemblyToIR --- DONE --- \n");

    printf("	constructSSA --- START --- \n");
    programDependenceAnalysis.constructSSA(functions);
    printf("	constructSSA --- DONE --- \n");

    printf("	constructControlDependenceGraph --- START --- \n");
    programDependenceAnalysis.constructControlDependenceGraph(functions);
    printf("		fmain->entry->bid = %d\n", fmain->entry->bid);
    printf("	constructControlDependenceGraph --- DONE --- \n");



	//std::vector<janus::Loop> loops = sourceCodeStructure.getAllLoops();
    std::vector<janus::Loop> loops;
    // TODO: Is it necessary to keep loops anywhere as part of any structure or we can use them as a local variable,
    // to be updated by different analyses?
	LoopAnalysisReport loopAnalysisReport;

	std::vector<std::set<LoopID>> loopNests;

	printf("Default analysis --- DONE --- \n");

    switch(mode)
    {
    	case JNONE:
    		;
    		break;
    	case JLCOV:
    		printf("JLCOV --- START --- \n");
    		IF_VERBOSE(cout<<"Loop coverage profiling mode enabled"<<endl);
    		// Get the loops from CFG
    		programDependenceAnalysis.identifyLoopsFromCFG(functions, loops);
    		printf("	JLCOV --- numLoops = %lu \n", loops.size());
    		programDependenceAnalysis.performBasicLoopAnalysis(loops, loopAnalysisReport, functions);
    		//programDependenceAnalysis.performBasicPassWithBasicFunctionTranslate(loops, loopAnalysisReport);
    		programDependenceAnalysis.performBasicPassWithBasicFunctionTranslate(loops, functions);
    		printf("JLCOV --- DONE --- \n");
    		break;
    	case JFCOV:
    		// Because no identification of loops is applied on this one,
    		// it does not make any sense to perform any analysis.
    		// I hope that I am not missing anything?
    		//programDependenceAnalysis.performBasicLoopAnalysis(loops, loopAnalysisReport, functions);
    		break;
    	case JPROF:
    		printf("JPROF --- START --- \n");
    		// Get the loops from CFG
    		// Identify loops from functions and update loops.
    		programDependenceAnalysis.identifyLoopsFromCFG(functions, loops);
    		printf("	identifyLoopsFromCFG --- DONE --- \n");
    		programDependenceAnalysis.analyseLoopRelationsWithinProcedure(functions, loops);
    		printf("	analyseLoopRelationsWithinProcedure --- DONE --- \n");
    		programDependenceAnalysis.analyseLoopAndFunctionRelations(loops, loopNests);
    		printf("	analyseLoopAndFunctionRelations --- DONE --- \n");
    		loadLoopCoverageProfiles(executableBinaryStructure.getExecutableName(), loops);
    		printf("	loadLoopCoverageProfiles --- DONE --- \n");
    		programDependenceAnalysis.performAdvanceLoopAnalysisWithReduceLoopsAliasAnalysis(loops, loopAnalysisReport, functions);
    		printf("	performAdvanceLoopAnalysis --- DONE --- \n");
    		//programDependenceAnalysis.reduceLoopsAliasAnalysis(loops, loopAnalysisReport, functions);
    		programDependenceAnalysis.performLoopAnalysisPasses(loops, loopAnalysisReport, loopNests);
    		printf("JPROF --- DONE --- \n");
    		break;
    	case JPARALLEL:
    		printf("JPARALLEL --- START --- \n");
    		// Get the loops from CFG
    		programDependenceAnalysis.identifyLoopsFromCFG(functions, loops);
    		printf("	identifyLoopsFromCFG --- DONE --- \n");
    		programDependenceAnalysis.analyseLoopRelationsWithinProcedure(functions, loops);
    		printf("	analyseLoopRelationsWithinProcedure --- DONE --- \n");
    		programDependenceAnalysis.analyseLoopAndFunctionRelations(loops, loopNests);
    		printf("	analyseLoopAndFunctionRelations --- DONE --- \n");
    		loopAnalysisReport = programDependenceAnalysis.loadLoopSelectionReport(loops, executableBinaryStructure.getExecutableName());
    		printf("	loadLoopSelectionReport --- DONE --- \n");
    		programDependenceAnalysis.performAdvanceLoopAnalysis(loops, loopAnalysisReport, functions);
    		printf("	performAdvanceLoopAnalysis --- DONE --- \n");
    		//programDependenceAnalysis.performBasicPassWithAdvanceFunctionTranslate(loops, loopAnalysisReport);
    		programDependenceAnalysis.performBasicPassWithAdvanceFunctionTranslate(loops, functions);
    		printf("	performBasicPassWithAdvanceFunctionTranslate --- DONE --- \n");
    		programDependenceAnalysis.performLoopAnalysisPasses(loops, loopAnalysisReport, loopNests);
    		printf("JPARALLEL --- DONE --- \n");
    		break;
    	case JVECTOR:
    		printf("JVECTOR --- START --- \n");
    		programDependenceAnalysis.identifyLoopsFromCFG(functions, loops);
    		programDependenceAnalysis.analyseLoopRelationsWithinProcedure(functions, loops);
    		programDependenceAnalysis.analyseLoopAndFunctionRelations(loops, loopNests);
    		programDependenceAnalysis.performAdvanceLoopAnalysis(loops, loopAnalysisReport, functions);
    		//programDependenceAnalysis.performBasicPassWithAdvanceFunctionTranslate(loops, loopAnalysisReport);
    		programDependenceAnalysis.performBasicPassWithAdvanceFunctionTranslate(loops, functions);
    		programDependenceAnalysis.performLoopAnalysisPasses(loops, loopAnalysisReport, loopNests);
    		break;
    	case JFETCH:
    		printf("JFETCH --- START --- \n");
    		programDependenceAnalysis.identifyLoopsFromCFG(functions, loops);
    		programDependenceAnalysis.analyseLoopRelationsWithinProcedure(functions, loops);
    		programDependenceAnalysis.analyseLoopAndFunctionRelations(loops, loopNests);
    		programDependenceAnalysis.performAdvanceLoopAnalysis(loops, loopAnalysisReport, functions);
    		//programDependenceAnalysis.performBasicPassWithAdvanceFunctionTranslate(loops, loopAnalysisReport);
    		programDependenceAnalysis.performBasicPassWithAdvanceFunctionTranslate(loops, functions);
    		programDependenceAnalysis.performLoopAnalysisPasses(loops, loopAnalysisReport, loopNests);
    		break;
    	case JANALYSIS:
    		printf("JANALYSIS --- START --- \n");
    		programDependenceAnalysis.identifyLoopsFromCFG(functions, loops);
    		programDependenceAnalysis.analyseLoopRelationsWithinProcedure(functions, loops);
    		programDependenceAnalysis.analyseLoopAndFunctionRelations(loops, loopNests);
    		loopAnalysisReport = programDependenceAnalysis.loadLoopSelectionReport(loops, executableBinaryStructure.getExecutableName());
    		programDependenceAnalysis.performAdvanceLoopAnalysis(loops, loopAnalysisReport, functions);
    		//programDependenceAnalysis.performBasicPassWithAdvanceFunctionTranslate(loops, loopAnalysisReport);
    		programDependenceAnalysis.performBasicPassWithAdvanceFunctionTranslate(loops, functions);
    		programDependenceAnalysis.performLoopAnalysisPasses(loops, loopAnalysisReport, loopNests);
    		break;
    	case JGRAPH:
    		printf("JGRAPH --- START --- \n");
    		programDependenceAnalysis.identifyLoopsFromCFG(functions, loops);
    		programDependenceAnalysis.performBasicLoopAnalysis(loops, loopAnalysisReport, functions);
    		//programDependenceAnalysis.performBasicPassWithBasicFunctionTranslate(loops, loopAnalysisReport);
    		programDependenceAnalysis.performBasicPassWithBasicFunctionTranslate(loops, functions);
    		break;
    	case JOPT:
    		printf("JOPT --- START --- \n");
    		programDependenceAnalysis.identifyLoopsFromCFG(functions, loops);
    		programDependenceAnalysis.analyseLoopRelationsWithinProcedure(functions, loops);
    		programDependenceAnalysis.analyseLoopAndFunctionRelations(loops, loopNests);
    		programDependenceAnalysis.performAdvanceLoopAnalysis(loops, loopAnalysisReport, functions);
    		//programDependenceAnalysis.performBasicPassWithAdvanceFunctionTranslate(loops, loopAnalysisReport);
    		programDependenceAnalysis.performBasicPassWithAdvanceFunctionTranslate(loops, functions);
    		break;
    	case JSECURE:
    		printf("JSECURE --- START --- \n");
    		programDependenceAnalysis.identifyLoopsFromCFG(functions, loops);
    		programDependenceAnalysis.analyseLoopRelationsWithinProcedure(functions, loops);
    		programDependenceAnalysis.analyseLoopAndFunctionRelations(loops, loopNests);
    		programDependenceAnalysis.performAdvanceLoopAnalysis(loops, loopAnalysisReport, functions);
    		//programDependenceAnalysis.performBasicPassWithAdvanceFunctionTranslate(loops, loopAnalysisReport);
    		programDependenceAnalysis.performBasicPassWithAdvanceFunctionTranslate(loops, functions);
    		programDependenceAnalysis.performLoopAnalysisPasses(loops, loopAnalysisReport, loopNests);
    		break;
    	case JCUSTOM:
    		printf("JCUSTOM --- START --- \n");
    		programDependenceAnalysis.identifyLoopsFromCFG(functions, loops);
    		programDependenceAnalysis.analyseLoopRelationsWithinProcedure(functions, loops);
    		programDependenceAnalysis.analyseLoopAndFunctionRelations(loops, loopNests);
    		programDependenceAnalysis.performBasicLoopAnalysis(loops, loopAnalysisReport, functions);
    		//programDependenceAnalysis.performBasicPassWithBasicFunctionTranslate(loops, loopAnalysisReport);
    		programDependenceAnalysis.performBasicPassWithBasicFunctionTranslate(loops, functions);
    		programDependenceAnalysis.performLoopAnalysisPasses(loops, loopAnalysisReport, loopNests);
    		break;
    	case JDLL:
    		printf("JDLL --- START --- \n");
    		programDependenceAnalysis.identifyLoopsFromCFG(functions, loops);
    		programDependenceAnalysis.analyseLoopRelationsWithinProcedure(functions, loops);
    		programDependenceAnalysis.analyseLoopAndFunctionRelations(loops, loopNests);
    		programDependenceAnalysis.performBasicLoopAnalysis(loops, loopAnalysisReport, functions);
    		//programDependenceAnalysis.performBasicPassWithBasicFunctionTranslate(loops, loopAnalysisReport);
    		programDependenceAnalysis.performBasicPassWithBasicFunctionTranslate(loops, functions);
    		programDependenceAnalysis.performLoopAnalysisPasses(loops, loopAnalysisReport, loopNests);
    		break;
        default:
        {
            usage();
            return 1;
        }
    }

    //analyse
    //jc->analyseLoop();


    /*
	// Identify loops from the control flow graph
	if (mode != JFCOV)
		loops = programDependenceAnalysis.identifyLoopsFromCFG(functions);

	// Analyze loop relations within one procedure
    // Update loops
	if (mode != JFCOV || mode != JLCOV || mode != JGRAPH)
		programDependenceAnalysis.analyseLoopRelationsWithinProcedure(functions, &loops);

    // Analyze loop relations across procedures
	if (mode != JFCOV || mode != JLCOV || mode != JGRAPH)
		std::vector<std::set<LoopID>> loopNests = programDependenceAnalysis.analyseLoopAndFunctionRelations(loops);

	// Load profiling information
    if (mode == JPROF) {
        //For automatic profiler mode, load loop coverage before analysis
    	loadLoopCoverageProfiles(executableBinaryStructure.getExecutableName(), loops);
    }

    LoopAnalysisReport loopAnalysisReport;
    //load loop selection from previous run
    if (mode == JPARALLEL || mode == JANALYSIS)
    	loopAnalysisReport = programDependenceAnalysis.loadLoopSelectionReport(loops, sourceCodeStructure.getExecutableName());

	if (mode != JPARALLEL &&
	        mode != JPROF &&
	        mode != JANALYSIS &&
	        mode != JVECTOR &&
	        mode != JOPT &&
		    mode != JSECURE &&
	        mode != JFETCH)
	{
		programDependenceAnalysis.performBasicLoopAnalysis(loops, loopAnalysisReport, functions);
	}
	else
	{
		programDependenceAnalysis.performAdvanceLoopAnalysis(loops, loopAnalysisReport, functions);
	}

	// Only valid for JPROF.
	// If a loop is removed, then nothing.
	// therwise, this analysis will call
	if (mode == JPROF)
		programDependenceAnalysis.reduceLoopsAliasAnalysis(loops, loopAnalysisReport);
	else
	{

		if (mode != JPARALLEL &&
			        mode != JANALYSIS &&
			        mode != JVECTOR &&
			        mode != JOPT &&
				    mode != JSECURE &&
			        mode != JFETCH)
			programDependenceAnalysis.performBasicPassWithBasicFunctionTranslate(loops, loopAnalysisReport);
		else
			programDependenceAnalysis.performBasicPassWithAdvanceFunctionTranslate(loops, loopAnalysisReport);
	}


	programDependenceAnalysis.performLoopAnalysisPasses(loops, loopAnalysisReport);
	*/
	// TODO: Eliminate JanusContext completely from the below.

    // TODO: It seems that some rules generation and dumping functions are calling analysis again. Check this.

    if(mode == JANALYSIS || mode == JGRAPH) {
        //dumpCFG(jc);
    	dumpCFG(functions);
        //dumpSSA(jc);
        dumpSSA(functions);
        //dumpLoopCFG(jc);
        dumpLoopCFG(loops);
        //dumpLoopSSA(jc);
        dumpLoopSSA(loops);
        //generateExeReport(jc);
        generateExeReport(executableBinaryStructure.getExecutableName(), functions, loops);
        generateExeReport(&cout, executableBinaryStructure.getExecutableName(), functions, loops);
    } else {
    	//generateRules(jc);
    	// TODO: Generation of rules still has encoded inside the dependency on the mode (mode stored in jc). Fix this.
    	printf("generateRules --- START --- \n");
        generateRules(jc, functionMap, functions, loops, loopAnalysisReport, fmain, executableBinaryStructure.getExecutableName());
        printf("generateRules --- DONE --- \n");
    }

    delete jc;

    GIO_Exit();
}
