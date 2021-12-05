/* Header file to implement a JANUS client */
#include <iostream>
#include "janus_api.h"
#include "dsl_core.h"

#include "func.h"

/*--- Dynamic Handlers Start ---*/
void handler_1(JANUS_CONTEXT){
    instr_t * trigger = get_trigger_instruction(bb,rule);
    dr_insert_clean_call(drcontext,bb,trigger,(void*)func_1, false,1,OPND_CREATE_INT32(rule->reg0));
}

void create_handler_table(){
    htable[0] = (void*)&handler_1;
}

/*--- Dynamic Handlers Finish ---*/
