///this file defines the runtime check handlers in Janus
#ifndef _JANUS_RUNTIME_CHECK_CONTROL_
#define _JANUS_RUNTIME_CHECK_CONTROL_

#include "janus_api.h"
#include "state.h"
#include "loop.h"

//#define SLOW_RUNTIME_CHECK

void
loop_array_bound_check_handler(JANUS_CONTEXT);

#endif