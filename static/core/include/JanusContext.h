#ifndef _Janus_CONTEXTS_
#define _Janus_CONTEXTS_

#include "janus.h"

#include "Executable.h"
#include "Function.h"
#include "IO.h"
#include "Loop.h"

#include <map>
#include <set>
#include <string>
#include <vector>

class JanusContext
{
  public:
    /// Analysis mode
    JMode mode;
    /// The name of the executable
    std::string name;
    /// the raw data parsed from the executable
    janus::Executable program;
    /// A vector of recognised functions in the executable
    std::vector<janus::Function> functions;
    /// A vector of recognised loops in the executable
    std::vector<janus::Loop> loops;
    /// A vector of recognised loop nests - inter-procedural
    std::vector<std::set<LoopID>> loopNests;
    // the main function of the program (not entry)
    janus::Function *main;
    // call graphs
    std::map<FuncID, std::set<FuncID>> callGraph;
    // data structure for look up
    std::map<PCAddress, janus::Function *> functionMap;
    // shared library calls or external functions
    std::map<PCAddress, janus::Function *> externalFunctions;

    /// Shared library profiling, enabled by default. Disable with -noshared
    /// switch
    bool sharedOn;

    int passedLoop;
    // flag to turn on profiling information
    bool useProfiles;
    bool manualLoopSelection;

    JanusContext(const char *name, JMode mode);

    /// Construct the CFG and SSA graph for the executable
    void buildProgramDependenceGraph();
    /// Recognise and analyse loops from the program dependence graph
    void analyseLoop();
    /// Recognise loop nests for loops and functions
    void analyseLoopAndFunctionRelations();
    /// Only recognise loops from the program dependence graph
    void analyseLoopLite();
};

#endif
