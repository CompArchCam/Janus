#include <cassert>
#include <iostream>
#include "func.h"
#include "util.h"
using namespace std;
/*--- Global Var Decl Start ---*/
#include <fstream>
#include <iostream>
#include <stdint.h>

/*--- Global Var Decl End ---*/


/*--- DSL Function Start ---*/

/*--- DSL Function Finish ---*/

void exit_routine(){
    /*--- Termination Start ---*/

/*--- Termination End ---*/
}
void init_routine(){
    /*--- Init Start ---*/

/*--- Init End ---*/
}

bool inRegSet(uint64_t bits, uint32_t reg)
{
    if((bits >> (reg-1)) & 1)
        return true;
    if(bits == 0 || bits == 1)
        return true;
    return false;
}
