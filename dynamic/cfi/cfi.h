#ifndef _JANUS_CFI_
#define _JANUS_CFI_
#include <vector>
#include <iostream>
#include <unordered_map>
#include <map>
#include <set>
#include <stack>
#include <string>
#include <assert.h>
#include "janus_api.h"
using namespace std;

typedef uintptr_t   addr_t;
typedef uintptr_t   pc_t;
enum {MEM_REF_STORE = 0, ABS_MEM_STORE, ARITH_MEM_STORE, CONST_MEM_STORE, CONST_ABS_MEM_STORE};

extern std::stack<std::pair<uint64_t, uint64_t>>       shadow_stack;


#endif
