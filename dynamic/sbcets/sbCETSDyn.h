#ifndef _JANUS_GUARD_
#define _JANUS_GUARD_
#include <vector>
#include <iostream>
#include <unordered_map>
#include <map>
#include <set>
#include <stack>
#include <string>
#include <assert.h>
#include "janus_api.h"
#include "instrumentation.h"
//#define SB_VERBOSE
//#define SB_VERBOSE_DETAIL
#define MAX_ALLOWED_COMMAND 4096
using namespace std;

typedef uintptr_t   addr_t;
typedef uintptr_t   pc_t;
enum {MEM_REF = 0, ABS_MEM, ARITH_MEM, GLOBALVAL_TO_REG, ABS_GLOBALMEM_TO_REG};
enum {MEM_REF_STORE = 0, ABS_MEM_STORE, ARITH_MEM_STORE, CONST_MEM_STORE, CONST_ABS_MEM_STORE};
/* Intermediate operation for beep event generation */
typedef enum _opcode {
    CM_UNKNOWN=0,
    CM_BND_CHECK,
    CM_RECORD_BASE,
    CM_RECORD_BOUND,
    CM_REG_REG_COPY,
    CM_MEM_REG_LOAD,
    CM_REG_MEM_STORE,
    CM_VALUE_MEM,
    CM_VALUE_REG
} guard_opcode_t;

typedef pair<uint64_t, uint64_t> pairdata;
//typedef pair<uintptr_t, uintptr_t> pairdata;
//extern std::unordered_map<int, metadata>      reg_table;  //use unordered_map for faster lookup. 
extern std::unordered_map<int, pairdata>      reg_table;  //use unordered_map for faster lookup. 
extern std::unordered_map<uint64_t, pairdata>        memory_table;
extern std::unordered_map<int, pairdata>      split_reg_table;  //use unordered_map for faster lookup. 
extern std::unordered_map<uint64_t, pairdata>        split_memory_table;
extern std::unordered_map<int, pairdata>      reg_bounds;  //use unordered_map for faster lookup. 
extern std::unordered_map<uint64_t, pairdata>        mem_bounds;
extern std::unordered_map<addr_t, int>                 freed;
extern std::stack<std::pair<uint64_t, uint64_t>>       shadow_stack;
extern int32_t mlc_base_counter;
extern int32_t mlc_size_counter;


#endif
