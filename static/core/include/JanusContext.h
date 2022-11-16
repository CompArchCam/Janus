#ifndef _Janus_CONTEXTS_
#define _Janus_CONTEXTS_

#include "janus.h"

//#include "Executable.h"
#include "ExecutableBinaryStructure.h"
#include "Function.h"
#include "Loop.h"
#include "IO.h"

#include <vector>
#include <map>
#include <set>
#include <string>

class JanusContext
{
private:
    ///Analysis mode
    JMode                                       mode;
    ///The name of the executable
    std::string                                 name;
    ///the raw data parsed from the executable
    //janus::Executable  program;

    // Analyzed binary structure, after lifting
    BinaryStructure binaryStrucure;

public:


    ///A vector of recognised loops in the executable
    std::vector<janus::Loop>                    loops;
    ///A vector of recognised loop nests - inter-procedural
    std::vector<std::set<LoopID>>               loopNests;

    //call graphs
    std::map<FuncID, std::set<FuncID>>          callGraph;
    //data structure for look up
    std::map<PCAddress, janus::Function *>      functionMap;
    //shared library calls or external functions
    std::map<PCAddress, janus::Function *>      externalFunctions;

    ///Shared library profiling, enabled by default. Disable with -noshared switch
    bool					sharedOn;
    
    int                                         passedLoop;
    //flag to turn on profiling information
    bool                                        useProfiles;
    bool                                        manualLoopSelection;

    JanusContext(const char* name, JMode mode);

    ///Construct the CFG and SSA graph for the executable
    void                            buildProgramDependenceGraph();
    ///Recognise and analyse loops from the program dependence graph
    void                            analyseLoop();
    ///Recognise loop nests for loops and functions
    void                            analyseLoopAndFunctionRelations();
    ///Only recognise loops from the program dependence graph
    void                            analyseLoopLite();
};

#endif
