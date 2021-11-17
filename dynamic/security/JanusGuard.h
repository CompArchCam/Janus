#ifndef _JANUS_GUARD_
#define _JANUS_GUARD_
#include <vector>
#include <unordered_map>
#include <map>
#include <set>
#include <string>
#include <assert.h>
#include "janus_api.h"

#define MAX_ALLOWED_COMMAND 4096

typedef uintptr_t   addr_t;
typedef uintptr_t   pc_t;

/* pointer metadata */
typedef struct p_metadata{
    addr_t                      base;
    uint64_t                    bound;
    p_metadata() {clear();}
    void                        clear() {base=0x0; bound=0;};
} metadata;

/* Intermediate operation for beep event generation */
typedef enum _opcode {
    CM_UNKNOWN=0,
    CM_BND_CHECK,
    CM_RECORD_BASE,
    CM_RECORD_BOUND,
    CM_REG_REG_COPY,
    CM_MEM_REG_LOAD,
    CM_REG_MEM_STORE,
    CM_VALUE_MEM,
    CM_VALUE_REG
} guard_opcode_t;

/* A plan represents an event to be generated at runtime */
typedef struct _access {
    addr_t          addr;
    addr_t          ptr_addr;
    pc_t            pc;
    addr_t          base;
    addr_t          bound;
    reg_id_t        src_reg;
    reg_id_t        dest_reg;
    int16_t         opcode;
    int8_t          size;
    int8_t          mode;
    uint64_t             id;
    PCAddress       v_pc; /*application PC to get debug information*/
} guard_t;

std::unordered_map<reg_id_t, metadata>      reg_table;  //use unordered_map for faster lookup. 
std::unordered_map<addr_t, metadata>        memory_table;
int32_t mlc_base_counter;
int32_t mlc_size_counter;
#endif
