/* Note that this file is automatic generated */
#include "DSLGen.h"
#include "DSLGenUtil.h"
#include "Analysis.h"
/*--- Global Var Decl Start ---*/
#include <fstream>
#include <iostream>
#include <stdint.h>

/*--- Global Var Decl End ---*/

using namespace std;
using namespace janus;

uint64_t bitmask;

void ruleGenerationTemplate(JanusContext &jc) {
/*--- Static RuleGen Start ---*/
for (auto &func: jc.functions){
    livenessAnalysis(&func);
    for (auto &B: func.blocks){
        uint64_t local_inst_count = 0;
        Instruction *End = B.instrs+ B.size;
        for(auto *I=B.instrs; I < End;I++){
            if( get_opcode(I) == Instruction::Inc){
                local_inst_count = local_inst_count + 1;
            }
        }
        if(local_inst_count > 0){
            bitmask = func.liveRegIn[B.bid].bits;
            insertCustomRule<BasicBlock>(1,B,4, true, 0, bitmask);
        }
    }
}

/*--- Static RuleGen Finish ---*/

}

