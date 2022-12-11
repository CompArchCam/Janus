#ifndef _JANUS_GUARD_
#define _JANUS_GUARD_
#include <vector>
#include <iostream>
#include <map>
#include <set>
#include <stack>
#include <string>
#include <assert.h>
#include "janus_api.h"

#define MAX_BB_SIZE 4096
using namespace std;

typedef uintptr_t   addr_t;
typedef uintptr_t   pc_t;


typedef struct _access {
    addr_t          LEAddr;
    addr_t          val_atLEA;
    pc_t            pc;
    addr_t          base;
    addr_t          bound;
    reg_id_t        src_reg;
    reg_id_t        dest_reg;
    reg_id_t        base_reg;
    int16_t         opcode;
    int8_t          size;
    int8_t          mode;
    uint64_t             id;
    PCAddress       v_pc; /*application PC to get debug information*/
} guard_t;


bool inRegSet(uint64_t bits, uint32_t reg)
{
  if((bits >> (reg-1)) & 1)
      return true;
  if(bits == 0 || bits == 1)
      return true;
  return false;
}

int get_64bit(int id){
    if(1<=id && id<=16){ //64 bit version
        return id;
    }
    else if(16<id && id<=32){ //32 bit version
        return id-16;
    }
    else if(32<id && id<=48){
        return id-32;
    }
    return id;
}
int get_32bit(int id){
    if(1<=id && id<=16) //64 bit version
        return id+16;
    else if(16<id && id<=32) //32 bit version
        return id;
    else if(32<id && id<=48) //16 bit version
        return id-16;
    return id;
}
int get_16bit(int id){
    if(1<=id && id<=16) //64 bit version
        return id+32;
    else if(16<id && id<=32) //32 bit version
        return id+16;
    else if(32<id && id<=48) //16 bit version
        return id;
    return id;
}
bool is_64bit(int id){
    if(1<=id && id<=16)
        return true;
    return false;
}
bool is_32bit(int id){
    if(16<id && id<=32)
        return true;
    return false;
}
bool is_16bit(reg_id_t id){
    if(32<id && id<=48)
        return true;
    return false;
}

#endif
