/* Note that this file is automatic generated */

#include "DSLGen.h"
#include "DSLGenUtil.h"

/*--- Global Var Decl Start ---*/
/*--- Global Var Decl End ---*/

using namespace std;
using namespace janus;

void ruleGenerationTemplate(JanusContext &jc) {

/*--- Static RuleGen Start ---*/
for (auto &func: jc.functions){
    for (auto &B: func.blocks){
        uint64_t local_inst_count = 0;
        Instruction *End = B.instrs+ B.size;
        for(auto *I=B.instrs; I < End;I++){
            if( get_opcode(I) == Instruction::Load){
                local_inst_count = local_inst_count + 1;
            }
        }
        if(local_inst_count > 0){
            insertCustomRule<BasicBlock>(1,B,4, true,local_inst_count);
        }
    }
}
/*--- Static RuleGen Finish ---*/

}

