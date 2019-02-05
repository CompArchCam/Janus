#ifndef _JANUS_PRINTER_
#define _JANUS_PRINTER_

#include "janus.h"

/* Prints internal data structures for debugging purposes */
const char * print_janus_mode(JMode mode);

void print_rule(RRule *rule);

const char *print_rule_opcode(RuleOp op);

const char *print_register_name(uint32_t reg);

void print_janus_variable(JVar var);

#endif
