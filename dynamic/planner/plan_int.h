#ifndef _PLAN_MODEL_
#define _PLAN_MODEL_

#include <vector>
#include <unordered_map>
#include <map>
#include <set>
#include <string>
#include "plan.h"

/* Base class for the BEEP execution model
 * Please add your virtual function and implementation here */
namespace janus {

/* Abstraction of the hardware */
struct Unit {
    pc_t                        write_pc;
    pc_t                        read_pc;
    Unit() {clear();}
    void                        clear() {write_pc=0; read_pc=0;};
};

class Core {
public:
    int                         id;
    /* Registers */
    Unit                        regs[NUM_OF_REGS+1];
    /* Memory */
    std::unordered_map<addr_t, Unit>      memory;  //use unordered_map for faster lookup. 
    /* Total size */
    uint64_t                    size;
    /* double linked list */
    Core                        *prev;
    Core                        *next;

    Core(int id);
    void                        read(addr_t addr, pc_t pc, int size);
    void                        write(addr_t addr, pc_t pc, int size);
    void                        clear();
};

}/* End namespace gabp */

struct SharedInfo {
    int                         loop_id;
    int                         num_of_threads;

    /* Timing info */
    cycle_t                     total_program_cycles;
    cycle_t                     total_loop_cycles;
    cycle_t                     current_loop_cycles;

    /* Loop info */
    iter_t                      total_iteration;
    iter_t                      total_invocation;
    iter_t                      total_memory_access;
    iter_t                      total_function_call;
    iter_t                      current_iteration;

    /* Runtime flag */
    bool                        loop_on;
    bool			dep_found; //for DOALL loops
};

typedef std::unordered_map<pc_t, std::unordered_map<pc_t, uint64_t>> DDG;

extern SharedInfo               shared;
extern DDG                      crossDDG;
#endif
