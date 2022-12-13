#ifndef _ASAN_RULE_GEN_
#define _ASAN_RULE_GEN_
#include <string.h>
#include "SchedGenInt.h"
#include "Analysis.h"
void
generateASANRule(JanusContext *jc);

static std::set<std::string> gcc_clang_func = {
    "__lib_csu_fini",
    "__libc_csu_init",
    "__libc_csu_fini",
    "_init",
    "__libc_init_first",
    "_fini",
    "_rtld_fini",
    "_exit",
    "__get_pc_think_bx",
    "__do_global_dtors_aux",
    "__gmon_start",
    "_start",
    "frame_dummy",
    "__do_global_ctors_aux",
    "__do_global_dtors_aux",
    "__register_frame_info",
    "deregister_tm_clones",
    "register_tm_clones",
    "__do_global_dtors_aux",
    "__frame_dummy_init_array_entry",
    "__init_array_start",
    "__do_global_dtors_aux_fini_array_entry",
    "__init_array_end",
    "__stack_chk_fail",
    "__cxa_atexit",
    "__cxa_finalize"
};
#endif
