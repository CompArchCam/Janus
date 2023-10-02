#include "offsetAnalysis.h"
using namespace std;
using namespace janus;
#define VERBOSE 0
std::set<int> currset;
std::set<int> curr_iset;
enum allocType{
    MALLOC=1,
    CALLOC=2
};

int 
isAllocCall(Instruction *instr){
    Function *targetFunc;
    if(instr->opcode == Instruction::Call){/*if it is a Call instruction*/
        //check if the call target is malloc@plt
        if (instr->block != NULL && instr->block->parentFunction!= NULL)
            targetFunc = instr->block->parentFunction->calls[instr->id];
        if(targetFunc == NULL) return 0;
        //cout<<"callee name: "<<targetFunc->name<<endl;
        if(!(targetFunc->name).compare("malloc@plt")){return MALLOC;}
        if(!(targetFunc->name).compare("calloc@plt")) return CALLOC;
    }
    return 0;
}

/*Check if the SSA node traces back to a malloc call*/

PCAddress
traceToAllocPC(VarState  *vs){

   PCAddress pc_malloc = 0;
   VarState *prevNode;
   //1. check if this node already trackable to malloc call
   //if(vs->trackable) return true;
   if(vs->mallocsite) return vs->mallocsite;
    
   //TODO: if vs alreay being tracked, stop here and return false to avoid loop. else add it to currently tracked
   //TODO: remove from currently tracked list before returning.
   if(currset.count(vs->id)) {currset.erase(vs->id); return 0;}
   currset.insert(vs->id);
   //2. If MEMORY NODE, check if this node has an immediate pred which is trackable to malloc, else traverse pred
   if(vs->type == JVAR_MEMORY || vs->type == JVAR_POLYNOMIAL){ //only MEMORY and POLYNOMIAL nodes will have pred
   //if(vs->type == JVAR_MEMORY){ //only MEMORY and POLYNOMIAL nodes will have pred
#if VERBOSE
       cout<<"vs: "<<vs<<endl;
#endif
       for(auto pred: vs->pred){
          if(pred->type== JVAR_CONSTANT) continue;
          //added new- start
          if(pred->isPHI){
              for(auto it: pred->pred){
                  if(it != pred){
                      pc_malloc = traceToAllocPC(it); //TODO: could a later false can overwrite an earlier true?
                      if(pc_malloc) {currset.erase(it->id); vs->mallocsite = pc_malloc; return pc_malloc;}
                  }
              }
          }//added new - end
          else{
              prevNode = pred;      //TODO: move the remaining if else inside this far loop. if found, break.
#if VERBOSE
              cout<<"pred:"<<pred<<endl;
#endif
              if(!prevNode) {currset.erase(vs->id); return 0;} //last node here. cannot go further. 
              //else if(prevNode->trackable)
              else if(prevNode->mallocsite)
                    {currset.erase(vs->id); vs->mallocsite= prevNode->mallocsite; return prevNode->mallocsite;}
              else{                       //track through the prev node up the SSA graph
                    pc_malloc= traceToAllocPC(prevNode);
                    if(pc_malloc) break; //added
               }
          }
       }
       /*if(!prevNode) return false; //last node here. cannot go further. 
       else if(prevNode->trackable)
            return true;
       else                       //track through the prev node up the SSA graph
            foundMalloc= traceToAlloc(prevNode);*/
       
   }

   //3. if not found through pred or if JVAR_REGISTER, check thru lasmodified instruction.
   if(pc_malloc == 0 || vs->type == JVAR_REGISTER){
      if(vs->lastModified == NULL) return 0;   //TODO: Also look into going to the next step
#if VERBOSE
      cout<<"last modified:"<<vs->lastModified->id<<endl;
#endif
      if(isAllocCall(vs->lastModified)){
#if VERBOSE
         cout<<"found MALLOC trace"<<endl;
#endif
         pc_malloc = vs->lastModified->pc;
         vs->trackable=true;
         vs->mallocsite = pc_malloc;
         currset.erase(vs->id);
         return pc_malloc;
      }
      else{//Go through the inputs of the lastModified instruction
          for(auto ip: vs->lastModified->inputs){
              if(ip->type== JVAR_CONSTANT) continue;
              //foundMalloc= traceToAlloc(ip); //added
#if VERBOSE
              cout<<"vs: "<<vs<<"  - last modified: "<<vs->lastModified->id<<endl;
              cout<<"ip: "<<ip<<endl;
#endif
              if(ip->isPHI){
                  for(auto pred: ip->pred){
                      if(pred != vs){
                          ip= pred;
                          pc_malloc= traceToAllocPC(ip); //TODO: could a later false can overwrite an earlier true?
                          if(pc_malloc) break;
                      }
                  }
              }
              else{
#if VERBOSE
                  cout<<"searching for ip: "<<ip<<endl;
#endif
                  pc_malloc= traceToAllocPC(ip); //TODO: could a later false can overwrite an earlier true?
                  if(pc_malloc) break;
              
              }
          }
      }
   }
   
   //4. if trace found to malloc and node is JVAR_MEMORY, mark all pred as trackable. speeds things up.
   //TODO: not sure if it's right to mark all pred as trackable. ANS: I think should be fine, extra overhead if not before malloc call but it will not find any corressponding values in reg or memory table. 
   /*if(foundMalloc && vs->type == JVAR_MEMORY){
       for(auto pred: vs->pred)
           pred->trackable = true;
   }*/
   currset.erase(vs->id);
   //return foundMalloc;
   vs->mallocsite = pc_malloc;
   return pc_malloc;
      
}
bool 
traceToAlloc(VarState  *vs){

   bool foundMalloc= false;
   VarState *prevNode;
   //1. check if this node already trackable to malloc call
   if(vs->trackable) return true;
    
   //TODO: if vs alreay being tracked, stop here and return false to avoid loop. else add it to currently tracked
   //TODO: remove from currently tracked list before returning.
   if(currset.count(vs->id)) {currset.erase(vs->id); return false;}
   currset.insert(vs->id);
   //2. If MEMORY NODE, check if this node has an immediate pred which is trackable to malloc, else traverse pred
   if(vs->type == JVAR_MEMORY || vs->type == JVAR_POLYNOMIAL){ //only MEMORY and POLYNOMIAL nodes will have pred
   //if(vs->type == JVAR_MEMORY){ //only MEMORY and POLYNOMIAL nodes will have pred
#if VERBOSE
       cout<<"vs: "<<vs<<endl;
#endif
       for(auto pred: vs->pred){
          if(pred->type== JVAR_CONSTANT) continue;
          //added new- start
          if(pred->isPHI){
              for(auto it: pred->pred){
                  if(it != pred){
                      foundMalloc= traceToAlloc(it); //TODO: could a later false can overwrite an earlier true?
                      if(foundMalloc) {currset.erase(it->id); return true;}
                  }
              }
          }//added new - end
          else{
              prevNode = pred;      //TODO: move the remaining if else inside this far loop. if found, break.
#if VERBOSE
              cout<<"pred:"<<pred<<endl;
#endif
              if(!prevNode) {currset.erase(vs->id); return false;} //last node here. cannot go further. 
              else if(prevNode->trackable)
                    {currset.erase(vs->id); return true;}
              else{                       //track through the prev node up the SSA graph
                    foundMalloc= traceToAlloc(prevNode);
                    if(foundMalloc) break; //added
               }
          }
       }
       /*if(!prevNode) return false; //last node here. cannot go further. 
       else if(prevNode->trackable)
            return true;
       else                       //track through the prev node up the SSA graph
            foundMalloc= traceToAlloc(prevNode);*/
       
   }

   //3. if not found through pred or if JVAR_REGISTER, check thru lasmodified instruction.
   if(!foundMalloc || vs->type == JVAR_REGISTER){
      if(vs->lastModified == NULL) return false;   //TODO: Also look into going to the next step
#if VERBOSE
      cout<<"last modified:"<<vs->lastModified->id<<endl;
#endif
      if(isAllocCall(vs->lastModified)){
#if VERBOSE
         cout<<"found MALLOC trace"<<endl;
#endif
         foundMalloc=true;
         vs->trackable=true;
         currset.erase(vs->id);
         return foundMalloc;
      }
      else{//Go through the inputs of the lastModified instruction
          for(auto ip: vs->lastModified->inputs){
              if(ip->type== JVAR_CONSTANT) continue;
              //foundMalloc= traceToAlloc(ip); //added
#if VERBOSE
              cout<<"vs: "<<vs<<"  - last modified: "<<vs->lastModified->id<<endl;
              cout<<"ip: "<<ip<<endl;
#endif
              if(ip->isPHI){
                  for(auto pred: ip->pred){
                      if(pred != vs){
                          ip= pred;
                          foundMalloc= traceToAlloc(ip); //TODO: could a later false can overwrite an earlier true?
                          if(foundMalloc) break;
                      }
                  }
              }
              else{
#if VERBOSE
                  cout<<"searching for ip: "<<ip<<endl;
#endif
                  foundMalloc= traceToAlloc(ip); //TODO: could a later false can overwrite an earlier true?
                  if(foundMalloc) break;
              
              }
          }
      }
   }
   
   //4. if trace found to malloc and node is JVAR_MEMORY, mark all pred as trackable. speeds things up.
   //TODO: not sure if it's right to mark all pred as trackable. ANS: I think should be fine, extra overhead if not before malloc call but it will not find any corressponding values in reg or memory table. 
   /*if(foundMalloc && vs->type == JVAR_MEMORY){
       for(auto pred: vs->pred)
           pred->trackable = true;
   }*/
   currset.erase(vs->id);
   return foundMalloc;
      
}
/*Check if the SSA node traces back to a certain instruction*/
bool 
traceToInstr(VarState  *vs, Instruction *instr){

    bool foundTrace= false;
    VarState *prevNode;
    //1. check if this node already trackable to malloc call
    if(vs->trackable) return true;
    if(curr_iset.count(vs->id)) {curr_iset.erase(vs->id); return false;}
        curr_iset.insert(vs->id);

   //2. If MEMORY NODE, check if this node has an immediate pred which is trackable to malloc, else traverse pred
    if(vs->type == JVAR_MEMORY){ //only MEMORY and POLYNOMIAL nodes will have pred
        for(auto pred: vs->pred){
            if(pred->type== JVAR_CONSTANT) continue;
            if(pred->isPHI){
                for(auto it: pred->pred){
                    if(it != pred){
                        foundTrace= traceToInstr(it, instr); 
                        if(foundTrace) {curr_iset.erase(it->id); return true;}
                    }
                }
            }//added new - end
            else{
                prevNode = pred;
                if(!prevNode) {curr_iset.erase(vs->id); return false;} //last node here. cannot go further. 
                else if(prevNode->trackable)
                    {curr_iset.erase(vs->id); return true;}
                else{                       //track through the prev node up the SSA graph
                    foundTrace= traceToInstr(prevNode, instr);
                    if(foundTrace) break; 
                }
            }
        }
   }

   //3. if not found through pred or if JVAR_REGISTER, check thru lasmodified instruction.
   if(!foundTrace || vs->type == JVAR_REGISTER){
      if(vs->lastModified == NULL) return false;   //TODO: Also look into going to the next step
      if(vs->lastModified == instr){
         foundTrace=true;
         vs->trackable=true;
         curr_iset.erase(vs->id);
         return foundTrace;
      }
      else{//Go through the inputs of the lastModified instruction
          for(auto ip: vs->lastModified->inputs){
              if(ip->type== JVAR_CONSTANT) continue;
              if(ip->isPHI){
                  for(auto pred: ip->pred){
                      if(pred != vs){
                          ip= pred;
                          foundTrace= traceToInstr(ip, instr); //TODO: could a later false can overwrite an earlier true?
                          if(foundTrace) break;
                      }
                  }
              }
              else{
                  foundTrace= traceToInstr(ip, instr); //TODO: could a later false can overwrite an earlier true?
                  if(foundTrace) break;
              
              }
          }
      }
   }
   curr_iset.erase(vs->id);
   return foundTrace;
      
}
void
get_range_expr(MemoryLocation *mloc, ExpandedExpr* range_expr, Iterator* loopIter){
    VarState *vs = mloc->vs;
    if(vs->index){
        if(loopIter->initKind == Iterator::NONE || loopIter->finalKind == Iterator::NONE) return;
        if(vs->index == loopIter->vs->value){
         //ExpandedExpr i_range(ExpandedExpr::SUM);
         //ExpandedExpr i_f(ExpandedExpr::SUM);
           range_expr->merge(loopIter->finalExprs);
           for(auto s: mloc->escev->strides){
              range_expr->multiply(s.second);
           }
           //range_expr->merge(&i_range);
           range_expr->merge(mloc->escev->start.ee);
           range_expr->merge(loopIter->initExprs);
        }
    }
    else if(vs->base){
        if(vs->base == loopIter->vs->value){
              //if(*it->finalExprs < *malloc_metadata[mvs->mallocsite].second->ee)
              range_expr->merge(loopIter->finalExprs);
              //if(miter->finalExprs && *miter->finalExprs < *malloc_metadata[mvs->mallocsite].second->ee)
              //    cout<<"within limit"<<endl;
         }
    }
    return;

}
