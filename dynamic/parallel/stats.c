#include "stats.h"
#include "control.h"
#include "jthread.h"

#ifdef JANUS_LOOP_TIMER
static uint64_t counter1;
static uint64_t counter2;
uint64_t loop_inner_counter;
uint64_t loop_outer_counter;
#endif

void stats_insert_counter_handler(JANUS_CONTEXT)
{
#ifdef JANUS_STATS
    instr_t *trigger = get_trigger_instruction(bb,rule);
    loop_t *loop = &(shared->loops[rule->reg0]);
    reg_id_t s1 = loop->header->scratchReg1;

    //PRE_INSERT(bb,trigger,INSTR_CREATE_int1(drcontext));
    PRE_INSERT(bb, trigger,
        INSTR_CREATE_inc(drcontext,
                         OPND_CREATE_MEM64(s1, LOCAL_COUNTER_OFFSET)));
#endif
}

#ifdef JANUS_LOOP_TIMER

void loop_inner_timer_resume(long down, long up)
{
    counter1 = (up<<32) + down;
}

void loop_inner_timer_pause(long down, long up)
{
    loop_inner_counter += ((up<<32) + down - counter1);
}

void loop_outer_timer_resume(long down, long up)
{
    counter2 = (up<<32) + down;
}

void loop_outer_timer_pause(long down, long up)
{
    loop_outer_counter += ((up<<32) + down - counter2);
}

#endif //JANUS_LOOP_TIMER
