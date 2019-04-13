#ifndef _JANUS_PREFETCH_HANDLER_
#define _JANUS_PREFETCH_HANDLER_

#include "janus_api.h"

void memory_prefetch_handler(JANUS_CONTEXT);

void instr_clone_handler(JANUS_CONTEXT);

void instr_update_clone_handler(JANUS_CONTEXT);
#endif