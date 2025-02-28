#include <assert.h>
#include <inttypes.h>
#include <sys/types.h>
#include <unwind.h>
#include <stddef.h>

#include "dr_api.h"
#include "drsyms.h"
#include "drmgr.h"
#include "drwrap.h"
#include "drvector.h"

//#define DEBUG

#ifdef DEBUG
        #include "debug.h"
#else
        #define printf(...)
        #define indent
        #define unindent
        #define tdebug(...)
#endif

int tls_key = -1;

/*********************** stack ***********************/
void push(void *addr)
{   
    drvector_t *vec = (drvector_t *)drmgr_get_tls_field(dr_get_current_drcontext(), tls_key);
    drvector_append(vec, addr);
}

void *pop()
{   
    drvector_t *vec = (drvector_t *)drmgr_get_tls_field(dr_get_current_drcontext(), tls_key);
    assert(vec->entries > 0);
    vec->entries--;
    return vec->array[vec->entries];
}

void *peek()
{   
    drvector_t *vec = (drvector_t *)drmgr_get_tls_field(dr_get_current_drcontext(), tls_key);
    assert(vec->entries > 0);
    return vec->array[vec->entries-1];
}
/******************* instrumentation *****************/
void on_call(void *call_ins, void *target_addr)
{
    tdebug("%p: call %p <%s>\n", call_ins, target_addr, get_sym(target_addr));
    push(call_ins);
    indent;
}

void on_ret(void *ret_ins, void *target_addr)
{
    unindent;
    ptrdiff_t diff;
    while (diff = target_addr - pop(), !(0 <= diff && diff <= 8))
    {
        tdebug("skipping a frame\n");
        unindent;
    }
    tdebug("returning to %p\n", target_addr);
}
/******************** analysis ********************/
dr_emit_flags_t new_bb(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst, bool for_trace, bool translating, void *user_data)
{
    if (inst == instrlist_last(bb)) {
        if (instr_is_call_direct(inst))
            dr_insert_call_instrumentation(drcontext, bb, inst, on_call);
        else if (instr_is_call_indirect(inst))
            dr_insert_mbr_instrumentation(drcontext, bb, inst, on_call, SPILL_SLOT_1);
        else if (instr_is_return(inst))
            dr_insert_mbr_instrumentation(drcontext, bb, inst, on_ret, SPILL_SLOT_1);
    }

    return DR_EMIT_DEFAULT;
}

/******************* threads **********************/
void on_thread(void *drcontext)
{
    drvector_t *vec = (drvector_t*)dr_thread_alloc(drcontext, sizeof *vec);
    assert(vec != NULL);
    drvector_init(vec, 10, false, NULL);
    drmgr_set_tls_field(drcontext, tls_key, vec);
}

void on_thread_exit(void *drcontext)
{
    drvector_t *vec = (drvector_t*)drmgr_get_tls_field(drcontext, tls_key);
    drvector_delete(vec);
    dr_thread_free(drcontext, vec, sizeof *vec);
}
/***************** c++ exceptions ****************/
void on_call_phase2(void *wrapctx, void **user_data)
{
    *user_data = drwrap_get_arg(wrapctx, 1);
    printf("_Unwind_RaiseException_Phase2 called; catch @ %p\n", *user_data);
}

void on_ret_phase2(void *wrapctx, void *user_data)
{
    printf("_Unwind_RaiseException_Phase2 returned\n");
    push((void*)_Unwind_GetIP(user_data));
}

void on_module_load(void *drcontext, const module_data_t *info, bool loaded)
{
    printf("Looking for _Unwind_RaiseException_Phase2 in %s\n", info->full_path);
    size_t offset;
    drsym_error_t error = drsym_lookup_symbol(info->full_path,
                                              "_Unwind_RaiseException_Phase2",
                                              &offset,
                                              DRSYM_DEFAULT_FLAGS);
    if (error == DRSYM_SUCCESS || error == DRSYM_ERROR_LINE_NOT_AVAILABLE)
    {
        printf("Found it at %p\n", info->start + offset);
        drwrap_wrap(info->start + offset, &on_call_phase2, &on_ret_phase2);
        drmgr_unregister_module_load_event(&on_module_load);
    }
}
/******************* main ********************/
void on_exit()
{
    printf("Stopping shadow stack\n");
    drmgr_exit();
    drwrap_exit();
    drsym_exit();
}

DR_EXPORT void dr_init(client_id_t id)
{
    printf("Starting shadow stack\n");
    drmgr_init();
    drwrap_init();
    drsym_init(0);

    tls_key = drmgr_register_tls_field();

    dr_register_exit_event(&on_exit);
    drmgr_register_bb_instrumentation_event(NULL, &new_bb, NULL);
    drmgr_register_module_load_event(&on_module_load);
    drmgr_register_thread_init_event(&on_thread);
    drmgr_register_thread_exit_event(&on_thread_exit);
}

