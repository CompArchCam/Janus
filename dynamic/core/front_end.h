#ifndef _JANUS_FRONT_END_
#define _JANUS_FRONT_END_

/* Frontend library for JANUS framework */
#include "janus.h"
#include "dr_api.h"

#ifdef __cplusplus
extern "C" {
#endif
#define MAX_MODULES 128                  //max modules loaded
extern module_data_t* loaded_modules[MAX_MODULES];
extern int nmodules;
//Initialise JANUS system in the dynamoRIO client
//Returns the rule type read from the rule file
//Pass in the pointer to number of threads to be updated by the front end
void janus_init(client_id_t id);
void janus_init_asan(client_id_t id);

//Find the corresponding static rule from specified address
RRule *get_static_rule(PCAddress addr);
RRule *get_static_rule_security(PCAddress addr);

void   load_static_rules(char *rule_path, uint64_t base);
void set_client_mode(int mode);
int get_client_mode();
void  load_static_rules_security(char *rule_path, const module_data_t *info);
#ifdef __cplusplus
}
#endif
#endif
