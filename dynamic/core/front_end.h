#ifndef _JANUS_FRONT_END_
#define _JANUS_FRONT_END_

/* Frontend library for JANUS framework */
#include "janus.h"
#include "dr_api.h"

#ifdef __cplusplus
extern "C" {
#endif
//Initialise JANUS system in the dynamoRIO client
//Returns the rule type read from the rule file
//Pass in the pointer to number of threads to be updated by the front end
void janus_init(client_id_t id);

void         load_static_rules(char *rule_path, uint64_t base);
//Find the corresponding static rule from specified address
RRule *get_static_rule(PCAddress addr);

#ifdef __cplusplus
}
#endif
#endif
