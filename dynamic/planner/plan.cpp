/* 
 * Janus Parallelism Planner *
 * 
 * This tool generates the DDG from profiling
 */
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include <set>
#include <utility>

#include "janus.h"
#include "janus_ddg.h"
#include "plan.h"
#include "plan_int.h"

using namespace std;
using namespace janus;

static void print_addr(bool read, addr_t addr, int id, pc_t pc);
static void print_command(plan_t *command);
unsigned char *print_reg(int reg);

static void planner_summarize(ostream &os);
static void planner_write_profile();
static void planner_write_feedback();

static pc_t current_block_pc = 0;
static int  current_block_id = -1;

static void plan_model_init(int number_of_cores);

/* Access events */
static void planner_read(addr_t addr, pc_t pc, int size);
static void planner_write(addr_t addr, pc_t pc, int size);
static void planner_delay(cycle_t cycle);

#ifdef INSTR_COUNTER
static inline void record_pc(pc_t pc);
static pc_t last_pc = 0;
#endif

/* Hardware */
Core                    *cores;
Core                    *current_core;
SharedInfo              shared;
/* Data dependences */
DDG                     crossDDG;

static vector<Core>     multicore;

/*dependency set*/
typedef pair<pc_t, pc_t> DepPair;
typedef set<DepPair> DepSet;
DepSet current_DepSet;

/*debug symbol information*/
std::unordered_map<pc_t, PCAddress> addr_map;
# define MAX_SYM_RESULT 256
char name[MAX_SYM_RESULT];
char file[MAX_SYM_RESULT];

void plan_model_init(int number_of_cores)
{
    /* allocate cores */
    for(int i=0;i<number_of_cores; i++)
    {
        multicore.emplace_back(i);
    }

    cores = multicore.data();

    /* Link them in a circle double linked list style */
    for(int i=0;i<number_of_cores; i++)
    {
        if (i>0)
            cores[i].prev = cores + i - 1;
        if (i<number_of_cores-1)
            cores[i].next = cores + i + 1;
    }

    cores[0].prev = cores + number_of_cores - 1;
    cores[number_of_cores - 1].next = cores;
}

/* planner events */
void planner_start(int loop_id, int num_threads)
{
    plan_model_init(num_threads);

    /* Reset shared information */
    memset(&shared, 0, sizeof(shared));
    shared.loop_id = loop_id;
    shared.num_of_threads = num_threads;

#ifdef PLANNER_VERBOSE
    printf("Planner started\n");
#endif
}

void planner_stop()
{
#ifdef PLANNER_VERBOSE
    printf("Execution model finishes\n");
#endif
    if (shared.loop_on) {
        planner_finish_loop();
    }
    /* screen output */
    planner_summarize(cout);
    /* file output */
#ifdef FIX
    ofstream llog;
    stringstream ss;
    ss << "Loop_"<<dec<<shared.loop_id<<".log";
    llog.open(ss.str(),ios::out);

    if (shared.total_invocation) {
        planner_summarize(llog);
        //planner_write_profile();
        planner_write_feedback();
    }
#endif
}

/* Loop events */
void planner_begin_loop()
{
    shared.loop_on = true;
    shared.dep_found =false;	
    current_core = NULL;
    shared.current_iteration = 0;

    for(int i=0; i<shared.num_of_threads; i++)
        cores[i].clear();

#ifdef PLANNER_VERBOSE
    cout <<endl<<"Loop begins"<<endl;
#endif
    instrumentation_switch = 1;
}

void planner_new_loop_iteration()
{
    if (!shared.loop_on) return;

    if (current_core) {
        /* Switch to the next thread
         * For every new iteration, switch the core.
         * Round Robin of iterations assignment to cores.
         * Core represents a thread.
         * */
        current_core = current_core->next;
        shared.current_iteration++;
        current_DepSet.clear();
    } else {
        current_core = cores;
        shared.current_iteration = 0;
    }
    /* Clear the current core to begin new iteration */
    current_core->clear();
    
    if(profmode==DOALL_LIGHT){
        if((shared.total_invocation >= nThreshold && 
            shared.total_iteration+shared.current_iteration >= iThreshold) ||
            stopDependencyChecks){ //Check if we've either run the loop for the required number of iterations of invocations
                                   //or the StopDependecyChecks flag is on (set when allowed time has run out)
                if (shared.dep_found == false){
                    // screen output 
                    shared.loop_on = false;
                    shared.total_iteration += (shared.current_iteration);
                    shared.total_invocation++;
                    planner_summarize(cout);
                    exit(0);
            }
        }
    }
#ifdef PLANNER_VERBOSE
    cout <<endl<<"Loop iteration "<<shared.current_iteration<<" begins"<<endl;
#endif
}

void planner_finish_loop()
{
    if (!shared.loop_on) return;
    if (!current_core) return;
    shared.loop_on = false;
    shared.dep_found = false;
    shared.total_invocation++;
    shared.total_iteration += (shared.current_iteration+1);

#ifdef PLANNER_VERBOSE
    cout <<endl<<"Loop finishes, total invocations so far: " << shared.total_invocation <<endl;
#endif
    instrumentation_switch = 0;
    if(profmode==DOALL_LIGHT){
        //in DOALL_LIGHT, we break after at least nThreshold invocations AND iThreshold total iterations
        //or when stopDependencyChecks is true (when we've run out of time)
        if((shared.total_invocation >= nThreshold && shared.total_iteration >= iThreshold) || stopDependencyChecks){  
            /* screen output */
            planner_summarize(cout);
            exit(0);
        }
    }
}
/* Main execution loop for planner */
void planner_execute(int command_size)
{
    plan_opcode_t op;

    for (int i=0; i<command_size; i++)
    {
        plan_t &command = commands[i];
        op = (plan_opcode_t)command.opcode;

        switch (op) {
            case CM_MEM:
                if ((int)command.mode == 1)
                    planner_read(command.addr, command.pc, command.size);
                else if ((int)command.mode == 2)
                    planner_write(command.addr, command.pc, command.size);
                else if ((int)command.mode == 3) {
                    planner_read(command.addr, command.pc, command.size);
                    planner_write(command.addr, command.pc, command.size);
                }
                addr_map[command.pc] = command.v_pc; /*application PC to be used for symbol debug information*/
                shared.total_memory_access++;
            break;
            case CM_LOOP_START:
                planner_begin_loop();
            break;
            case CM_LOOP_ITER:
                planner_new_loop_iteration();
            break;
            case CM_LOOP_FINISH:
                planner_finish_loop();
                emulate_pc = 0;
                return;
            break;
            case CM_DELAY:
                planner_delay(commands[i].id);
            break;
            default:
                printf("Dynamic operation not recognised %d\n", op);
            break;
        }
        if(profmode == DOALL || profmode == DOALL_LIGHT){ 
            if(shared.dep_found){
                cout<< "Loop "<<shared.loop_id << ": cross memory dependences found - not a dynamic DOALL"<<endl; 
                exit(0);
            }
        }
    }
    /* Reset pc */
    emulate_pc = 0;
}

/* Delay events */
void planner_delay(cycle_t cycle)
{
    total_program_cycles += cycle;
    if (shared.loop_on && current_core) {
        shared.current_loop_cycles += cycle;
    }
}

/* summary */
void planner_stat()
{
    planner_summarize(cout);
}

void planner_read(addr_t addr, pc_t pc, int size)
{
    if (!shared.loop_on || !current_core) return;
#ifdef PLANNER_VERBOSE
    print_addr(1, addr, 0, pc);
#endif
    current_core->read(addr, pc, size);
}

void planner_write(addr_t addr, pc_t pc, int size)
{
    if (!shared.loop_on || !current_core) return;
#ifdef PLANNER_VERBOSE
    print_addr(0, addr, 0, pc);
#endif
    current_core->write(addr, pc, size);
}

void Core::read(addr_t addr, pc_t pc, int size)
{
    int check_round = ((int)shared.current_iteration > (int)shared.num_of_threads) ? shared.num_of_threads : shared.current_iteration;
    //trim the address
    addr = addr - addr % GRANULARITY;
    auto query = memory.find(addr);
    /* Cross iteration dependences */
    if (query == memory.end()) {
        /* Not found in current core, search for previous core */
        Core *check = prev;
        for (int i=0; i<check_round; i++) {
            auto query2 = check->memory.find(addr);
            if (query2 != check->memory.end()) {
                Unit &entry = (*query2).second;
                if (entry.write_pc) {
                    /* Record this dependence */
                    shared.dep_found= true;
                    if(profmode == DOALL || profmode == DOALL_LIGHT) return; //no further analysis needed
                    if(!current_DepSet.count(make_pair(pc, entry.write_pc))){ //skip adding the pair if already there in the same iteration
                        (crossDDG[pc])[entry.write_pc]++;
                        current_DepSet.insert(make_pair(pc, entry.write_pc));
                    }
                }
                break;
            }
            check = check->prev;
        }
    }
    memory[addr].read_pc = pc;
}

void Core::write(addr_t addr, pc_t pc, int size)
{
    if (!shared.loop_on || !current_core) return; //Not sure what this is about
    addr = addr - addr % GRANULARITY;
    int check_round = ((int)shared.current_iteration > (int)shared.num_of_threads) ? shared.num_of_threads : shared.current_iteration;
    auto query = memory.find(addr);
    /* Cross iteration dependences */
    if (query == memory.end()) {
        /* Not found in current core, search for previous core */
        Core *check = prev;
        for (int i=0; i<check_round; i++) {
            auto query2 = check->memory.find(addr);
            if (query2 != check->memory.end()) {
                Unit &entry = (*query2).second;
                if (entry.write_pc || entry.read_pc) { //Check for WAW and WAR
                    /* Record this dependence */
                    shared.dep_found= true;
                    if(profmode == DOALL || profmode == DOALL_LIGHT) return; //no further analysis needed
                    if(!current_DepSet.count(make_pair(pc, entry.write_pc))){ //skip adding the pair if already there in the same iteration
                        (crossDDG[pc])[entry.write_pc]++;
                        current_DepSet.insert(make_pair(pc, entry.write_pc));
                    }
                }
                break;
            }
            check = check->prev;
        }
    }
    memory[addr].write_pc = pc;
}

void print_command(plan_t *command)
{
    if (!command) return;

    plan_opcode_t op = (plan_opcode_t)command->opcode;

    switch (op) {
        case CM_BLOCK:
            printf("Block pc %lx\n", command->pc);
        break;
        case CM_REG_READ:
            printf("Reg read %s id %d\n", print_reg_name(command->addr), command->id);
        break;
        case CM_REG_WRITE:
            printf("Reg write %s id %d\n", print_reg_name(command->addr), command->id);
        break;
        case CM_MEM_READ:
            printf("Mem read %lx id %d\n", command->addr, command->id);
        break;
        case CM_MEM_WRITE:
            printf("Mem write %lx id %d\n", command->addr, command->id);
        break;
        case CM_LOOP_START:
            printf("loop start id %d\n", command->id);
        break;
        case CM_LOOP_ITER:
            printf("loop iter start id %d\n", command->id);
        break;
        case CM_LOOP_FINISH:
            printf("loop finish id %d\n", command->id);
        break;
        case CM_DELAY:
            printf("Delay %d\n", command->id);
        break;
        default:
        break;
    }
}


static void
print_debug_info(ostream &os, app_pc addr)
{
    drsym_error_t symres;
    drsym_info_t sym;
    module_data_t *data;
    data = dr_lookup_module(addr);
    if (data == NULL) {
        os<<"NO MODULE FOUND"<<endl;
        return;
    }
    sym.struct_size = sizeof(sym);
    sym.name = name;
    sym.name_size = MAX_SYM_RESULT;
    sym.file = file;
    sym.file_size = MAXIMUM_PATH;
    symres = drsym_lookup_address(data->full_path, addr - data->start, &sym,
				      DRSYM_DEFAULT_FLAGS);
    if (symres == DRSYM_SUCCESS || symres == DRSYM_ERROR_LINE_NOT_AVAILABLE) {
      /*const char *modname = dr_module_preferred_name(data);
      if (modname == NULL)
	  modname = "<noname>";
      os<<"module: "modname<<" symbol: "<<sym.name<<endl;*/
      if (symres == DRSYM_ERROR_LINE_NOT_AVAILABLE) {
	os<<"?:? + ?";
      } else {
	os<<sym.file<<":"<<sym.line<<" + "<<sym.line_offs;
      }
    } else
	os<<"?:? + ?";
    dr_free_module_data(data);
}

static void planner_summarize(ostream &os)
{
    os << "total invocation: "<<dec<<shared.total_invocation<<endl;
    os << "total iteration: "<<shared.total_iteration<<endl;
    if(shared.total_iteration)
        os << "Average memory instructions per iteration: "<<shared.total_memory_access / (double)shared.total_iteration<<endl;

    if(shared.total_iteration) {
    }
    else
        os <<"Loop "<<shared.loop_id<<" was not executed during this run." <<endl;
    os << "Cross Iteration Memory Dependences: "<<crossDDG.size()<<endl;
    if(profmode ==DOALL || profmode == DOALL_LIGHT){
        if(!crossDDG.size()){
            ofstream selectFile;
            std::string str(app_full_name);
            selectFile.open(str + ".loop.select", ios::app);
            selectFile << shared.loop_id << " 1" <<endl;
            selectFile.close();
        }
    }
    for (auto ddgmap:crossDDG) {
        for (auto pair:ddgmap.second) {
            os <<dec<<ddgmap.first<<" -> "<<pair.first<<" ratio: "<<(float)pair.second/(float)shared.total_iteration<<endl;
            os<<"Dependency pair: <";
            print_debug_info(os, (app_pc)addr_map[ddgmap.first]);
            os<<" , ";
            print_debug_info(os, (app_pc)addr_map[pair.first]);
            os<<" >"<<endl; 
        }
    }
}

void planner_write_feedback()
{
    stringstream ss;
    ss << "Loop_"<<dec<<shared.loop_id<<".fdb";
    FILE *feedback_file = fopen(ss.str().c_str(),"w");

    LoopInfo info;
    info.id = shared.loop_id;
    //info.coverage = (double)(shared.total_loop_cycles)/(double)shared.total_program_cycles; //commented by MA
    info.total_iteration = shared.total_iteration;
    info.total_invocation = shared.total_invocation;
    info.total_memory_access = shared.total_memory_access;
    int dependence_count = 0;
    int freq_dependence_count = 0;
    for (auto dep: crossDDG) {
        for(auto s: dep.second) {
            uint occurence = s.second;
            dependence_count++;
            if ((double)occurence/(double)info.total_iteration > 0.5)
                freq_dependence_count++;
        }
    }
    info.dependence_count = dependence_count;
    info.freq_dependence_count = freq_dependence_count;

    fwrite(&info,sizeof(LoopInfo),1,feedback_file);
    fclose(feedback_file);
}

static void print_addr(bool read, addr_t addr, int id, pc_t pc)
{
    cout<<"pc "<<dec<<pc<<" ";
    if (read)
        cout <<"Read ";
    else
        cout <<"Write ";

    if (addr < NUM_OF_REGS)
        cout <<print_reg_name(addr)<<dec<<" ";
    else
        cout <<hex<<"0x"<<addr<<" " <<dec;
    cout <<endl;
}

#ifdef INSTR_COUNTER
static inline void record_pc(pc_t pc)
{
    if (pc != last_pc)
        instr_counter[pc]++;
    last_pc = pc;
}
#endif

Core::Core(int id)
:id(id)
{

}

void Core::clear()
{
    memory.clear();
    size = 0;
}

