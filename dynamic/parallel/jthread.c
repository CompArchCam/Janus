#include "jthread.h"
#include "control.h"
#include "loop.h"
#include "janus_atomic.h"

#ifdef JANUS_JITSTM
#include "jitstm.h"
#include "stm.h"
#endif

#include <linux/sched.h>/* for CLONE_ flags */
#include <sys/wait.h>  /* for wait */
#include <sys/types.h> /* for wait and mmap */
#include <sys/mman.h>  /* for mmap */
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

/* We avoid pthread because it has conflict uses of the TLS with DynamoRIO
 * Instead we implement our own threading library and put our TLS into
 * dcontext->client_data->tls */

/** \brief An array of janus thread pointers */
janus_thread_t **oracle;

/* ------------------------------------------------------
 * The following functions are called in *app* mode
 * They are subject to internal translation by DynamoRIO 
 * -----------------------------------------------------*/
/** \brief Prefix in entering the thread pool for the first time */
static int janus_thread_pool_app_init(void *args);

/* prototype of clone. */
extern int
clone(int (*fn) (void *arg), void *child_stack, int flags, void *arg, ...);

/** \brief internal thread creation using clone */
static pid_t create_thread(int (*fcn)(void *), void *arg);

static void* stack_alloc(size_t size);
static void stack_free(void *p, int size);
static void save_stack_pointer(janus_thread_t *tls);

static size_t janus_get_thread_stack_size();
/* ------------------------------------------------------
 * The following functions are called in DR mode
 * They are executed normally as client code
 * -----------------------------------------------------*/
/** \brief Per-thread initialisation */
static void janus_thread_init(janus_thread_t *local, uint64_t tid);

static volatile void* current_thread_arg = NULL;
static volatile long  next_expected_thread = 0;

/* Note that this call is performed as application */
void janus_create_threads()
{
    long i;
#ifdef JANUS_VERBOSE
    dr_printf("Main thread starts to create threads\n");
#endif
    int total_threads = rsched_info.number_of_threads;
    next_expected_thread = 1;

    for (i = 1; i<total_threads; i++) {
        //wait for the previous thread spawn handler to release the lock
        while (i > next_expected_thread){};
        current_thread_arg = (void *)i;
        create_thread(janus_thread_pool_app_init, (void *)i);
    }
#ifdef JANUS_VERBOSE
    dr_printf("Main thread finishes creating threads\n");
#endif
}

void
janus_delete_threads()
{
    /* Threads naturally exit and destroyed by DynamoRIO */
}

/** \brief Thread creation event
 *
 * It will be called whenever an application thread is created.
 * (Including the main thread) */
void janus_thread_spawn_handler(void *drcontext)
{
    //TODO: add safety check so that it can skip the original app threads

    //load the argument from shared location
    long tid = (long)current_thread_arg;
    //for the main thread
    //after obtaining the tid, let janus create the next thread
    //TODO: optimise the lock by reading the argument of clone syscall.
#ifdef JANUS_X86
    atomic_inc_64(&next_expected_thread);
#elif JANUS_AARCH64
    //David please implement atomic increment here 
    next_expected_thread++;
#endif

    //retrieve the TLS field
    janus_thread_t *tls = oracle[tid];

    //now assign the tls field to drcontext
    dr_set_tls_field(drcontext, tls);
    tls->drcontext = drcontext;

    //initialise the TLS
    janus_thread_init(tls, tid);

    //the main thread returns and resumes execution
    if (tid == 0) return;

    //the rest of threads continue to generate code
    janus_generate_loop_code(drcontext, tls);

#ifdef JANUS_VERBOSE
    dr_printf("A new janus thread %ld created in context %p\n", tid, drcontext);
#endif
}

/** \brief Allocates a second stack to use during parallelization for thread 0 */
void janus_allocate_main_thread_stack()
{

    janus_thread_t *local = oracle[0];
    local->stack = stack_alloc(janus_get_thread_stack_size());
    uint64_t stack_ptr = (uint64_t)local->stack;
    //make stack 64 bytes aligned
    stack_ptr -= stack_ptr & 64;
    local->stack_ptr = (void *)stack_ptr;
}

/** \brief AGets the required parallelization stack size, based on the stack size requirement of parallelized loops */
static size_t janus_get_thread_stack_size()
{
    //We calculate the stack size only on the first call, save the result in the static variable
    //and on further calls return the static variable
    static size_t requiredStackSize = 0; 
    if (requiredStackSize != 0)
    {
        return requiredStackSize;
    } 
    loop_t *loop_array = shared->loops;
    RSchedHeader *header = rsched_info.header;
    int numLoops = header->numLoops;
    for (int i = 0; i < numLoops; i++)
    {
        if (loop_array[i].header->useStack && loop_array[i].header->stackFrameSize > requiredStackSize)
        {
            requiredStackSize = loop_array[i].header->stackFrameSize;
        }
    }
    requiredStackSize += 8 * 1024; //We add a little extra for safety
    if (requiredStackSize < 32 * 1024)
        requiredStackSize = 32 * 1024;
#ifdef JANUS_VERBOSE
    dr_printf("Janus thread stack size: %lu bytes!\n", requiredStackSize);
#endif
    return requiredStackSize;
}

/** \brief Thread exit event 
 *
 * It will be called whenever an application thread is deleted. */
void janus_thread_exit_handler(void *drcontext)
{
#ifdef JANUS_VERBOSE
    dr_printf("A janus thread deleted %p\n",drcontext);
#endif
#ifdef JANUS_STATS
#ifdef JANUS_X86
    janus_thread_t *local = dr_get_tls_field(drcontext);
    if (local->id != 0) return;
#endif
    int i;

    dr_printf("No. iterations\n");
    for (i=0; i<rsched_info.number_of_threads; i++) {
        dr_printf("%ld ",oracle[i]->stats.num_of_iterations);
    }
    dr_printf("\n");
    dr_printf("Thread counter 0\n");
    for (i=0; i<rsched_info.number_of_threads; i++) {
        dr_printf("%ld ",oracle[i]->stats.counter0);
    }
    dr_printf("\n");
    dr_printf("No. of invocation %ld\n",shared->loop_invocation);
#ifdef JANUS_LOOP_TIMER
    dr_printf("Loop elapsed time with loop init/merge %ld\n",loop_outer_counter);
    dr_printf("Loop elapsed time without loop init/merge %ld\n",loop_inner_counter);
#endif
#endif
}

/** \brief Per-thread initialisation */
static void janus_thread_init(janus_thread_t *local, uint64_t tid)
{
    local->id = tid;

    //allocate thread private loop code
    local->gen_code = (loop_code_t *)malloc(sizeof(loop_code_t) * rsched_info.header->numLoops);

#ifdef JANUS_JITSTM
    //allocate data structures for the just-in-time STM
    janus_thread_init_jitstm(local);
#endif

#ifdef JANUS_VERBOSE
    dr_printf("Thread %ld initialised\n", local->id);
#endif
}

int janus_thread_pool_app_init(void *args)
{
    long tid = (long)args;

    janus_thread_t *tls = oracle[tid];

    save_stack_pointer(tls);

    //enter thread pool
    janus_thread_pool_app(tls, 0);
}

/* Thread pool : (application code) */
int janus_thread_pool_app(janus_thread_t *tls, int status)
{
    void (*parallel)(void *, uint64_t);

#ifdef JANUS_VERBOSE
    if (status == 0)
        dr_printf("thread %d enters thread pool\n",tls->id);
    else
        dr_printf("thread %d enters thread pool again\n",tls->id);
#endif

    /* We must place the in_pool flag before finished
     * to prevent race conditions */
    tls->flag_space.finished = 0;
    tls->flag_space.in_pool = 1;

    /* thread loop */
    while(1)
    {
        if (shared->start_run) {
            /* Wait for the code to be ready */
            while(!(shared->code_ready));
            /* When code is ready, assign the parallel function */
            parallel = (void (*)(void *, uint64_t))tls->gen_code[shared->current_loop->dynamic_id].thread_loop_init;
            /* Unset the flags */
            tls->flag_space.loop_on = 1;
            tls->flag_space.in_pool = 0;
            /* Jump to the loop code */
#ifdef JANUS_VERBOSE
            dr_printf("thread %d starts to run the loop %d\n",tls->id, shared->current_loop->static_id);
#endif
            /* parallelise the loop */
            parallel(tls, shared->current_loop->start_addr);

            /* Never reached here */
        }
    }

    /* Never reach here */
#ifdef JANUS_X86
    asm("int3");
#endif
    return 0;
}

int janus_reenter_thread_pool_app(janus_thread_t *tls)
{
    tls->flag_space.loop_on = 0;

#ifdef JANUS_VERBOSE
    dr_printf("Thread %d reenters thread pool because iteration count < thread count!\n", tls->id);
#endif
    /* set finished */
    tls->flag_space.finished = 1;
    /* wait for the main thread */
    while (!shared->need_yield);

    janus_thread_pool_app(tls, 1);
}

static pid_t
create_thread(int (*fcn)(void *), void *arg)
{
    pid_t newpid;
    int flags;
    void *my_stack;

    my_stack = stack_alloc(janus_get_thread_stack_size());

    flags = (CLONE_THREAD | CLONE_VM | CLONE_PARENT |
             CLONE_FS | CLONE_FILES  | CLONE_IO | CLONE_SIGHAND);
    newpid = clone(fcn, my_stack, flags, arg, NULL, NULL, NULL);

    if (newpid == -1) {
        dr_printf("Error in calling syscall clone\n");
        stack_free(my_stack, janus_get_thread_stack_size());
        return -1;
    }
    return newpid;
}

/* allocate stack storage on the app's heap */
void *
stack_alloc(size_t size)
{
    size_t sp;
    void *q = NULL;
    void *p;

#if STACK_OVERFLOW_PROTECT
    /* allocate an extra page and mark it non-accessible to trap stack overflow */
    q = mmap(0, PAGE_SIZE, PROT_NONE, MAP_ANON|MAP_PRIVATE, -1, 0);
    assert(q);
    stack_redzone_start = (size_t) q;
#endif

    p = mmap(q, size, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
    assert(p);
#ifdef DEBUG
    memset(p, 0xab, size);
#endif

    /* stack grows from high to low addresses, so return a ptr to the top of the
       allocated region */
    sp = (size_t)p + size;

    return (void*) sp;
}

/* free memory-mapped stack storage */
void
stack_free(void *p, int size)
{
    size_t sp = (size_t)p - size;

#ifdef DEBUG
    memset((void *)sp, 0xcd, size);
#endif
    munmap((void *)sp, size);

#if STACK_OVERFLOW_PROTECT
    sp = sp - PAGE_SIZE;
    munmap((void*) sp, PAGE_SIZE);
#endif
}


static void save_stack_pointer(janus_thread_t *tls)
{
    uint64_t stack_ptr, stack_base;
#ifdef JANUS_X86
    //save the parent's stack pointer
    asm ("lea (%%rsp), %0\n\t"
         "movq %%rbp, %1\n\t"
        : "=r"(stack_ptr), "=r"(stack_base)
        :
        :);
#elif JANUS_AARCH64
    asm("mov %0, sp\n\t"
        "mov %1, x29\n\t"
        : "=r"(stack_ptr), "=r"(stack_base)
        :
        :);
#endif
    tls->stack = (void *)stack_base;

    //make the stack pointer 64 bytes aligned
    stack_ptr -= stack_ptr & 64;
    tls->stack_ptr = (void *)stack_ptr;
}
