/* JANUS Client for loop coverage profiler */

/* Header file to implement a JANUS client */
#include "janus_api.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <sstream>
#include <vector>
#include <fstream>
#include <sys/time.h>
#include <signal.h>

/*-----Debug Symbol info----*/
#include "dr_api.h"
# include "drsyms.h"

using namespace std;

int numLoops;
uint64_t global_counter;
uint64_t loop_counter;
uint64_t invocations;
uint32_t loop_on;
uint64_t global_interrupt_count; //to count total interrupt events

struct loop_info {
    uint64_t cycles;
    uint64_t iterations;
    uint64_t invocations;
    uint64_t suspicious;
    uint64_t interrupt_count; //number of timer interrupts
};
loop_info *infos;
//set<uint32_t> living_loops;

const char *app_name;
const char *app_full_name;

/*debug symbol information*/
PCAddress *addrMap;
# define MAX_SYM_RESULT 256
char name[MAX_SYM_RESULT];
char file[MAX_SYM_RESULT];

typedef struct Node{
    struct Node* next;
    int data;
} Node;

static Node* head_livingloops= NULL;
static Node* tail_livingloops= NULL;
static int livingloopsCount=0;
int isLivingLoop(int data);

void addtoLivingLoops(int data) {  /* add at at the end of list*/
    if(isLivingLoop(data))
    	return;
    Node *temp; 
    temp = (Node *)malloc(sizeof(Node));
    temp->next= NULL;
    temp->data= data;
    if(head_livingloops==NULL){
        tail_livingloops=temp;
        head_livingloops=temp;
    }
    else{
        tail_livingloops->next = temp;
        tail_livingloops=temp;
    }
    livingloopsCount++;
}
void removeFromLivingLoops(int data) {
   // Store head node
   Node* temp = head_livingloops;
   Node* prev;
    
    // If head node itself holds the key to be deleted
    if (temp != NULL && temp->data == data)
    {
        head_livingloops = temp->next;   // Changed head
        if(head_livingloops == NULL) // was only one node
            tail_livingloops = NULL;
        free(temp);               // free old head
        return;
    }
     
    // Search for the key to be deleted, keep track of the
    // previous node as we need to change 'prev->next'
    while (temp != NULL && temp->data != data)
    {
	 prev = temp;
	 temp = temp->next;
    }
      
    // If key was not present in linked list
    if (temp == NULL) return;
      
    if(temp->next ==NULL) //if found in tail
        tail_livingloops = prev;

    // Unlink the node from linked list
    prev->next = temp->next;
    livingloopsCount--;
    free(temp);  // Free memory
}


int isLivingLoop(int data){
   Node *temp = head_livingloops;
   while(temp != NULL){
   	if(temp->data == data)
	    return 1;
	else 
	   temp = temp->next;
   }
   return 0;
}
/*---------------------------------*/

static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating);

/*static dr_signal_action_t
event_timer_handler(void *drcontext, dr_siginfo_t *siginfo);*/

static void
timer_handler(void *drcontext, dr_mcontext_t *mcontext); //timer interrupt handling routine

static void
coverage_thread_init(void *drcontext) {
    global_counter = 0;
    loop_counter = 0;
    invocations = 0;
    loop_on = 0;
    global_interrupt_count=0; //init total interrupt events
}


static void
print_debug_info(app_pc addr)
{
    drsym_error_t symres;
    drsym_info_t sym;
    module_data_t *data;
    data = dr_lookup_module(addr);
    if (data == NULL) {
	printf("NO MODULE FOUND\n");
	return;
    }
    sym.struct_size = sizeof(sym);
    sym.name = name;
    sym.name_size = MAX_SYM_RESULT;
    sym.file = file;
    sym.file_size = MAXIMUM_PATH;
    symres = drsym_lookup_address(data->full_path, addr - data->start, &sym,
				      DRSYM_DEFAULT_FLAGS);
    if (symres == DRSYM_SUCCESS || symres == DRSYM_ERROR_LINE_NOT_AVAILABLE) {
      /*const char *modname = dr_module_preferred_name(data);
      if (modname == NULL)
	  modname = "<noname>";
      os<<"module: "modname<<" symbol: "<<sym.name<<endl;*/
      if (symres == DRSYM_ERROR_LINE_NOT_AVAILABLE) {
	printf("?:? + ?");
      } else {
        printf("%s:%ld + %ld", sym.file, sym.line, sym.line_offs);
      }
    } else
	printf("?:? + ?");
    dr_free_module_data(data);
}

static void
coverage_thread_exit(void *drcontext) {
    /* read LoopID to function name mapping information from map.out file*/
    ifstream in(string(string(app_full_name)+"_map.out").c_str());
    if(!in){
         std::cerr << "Cannot open the File" << endl;
    }
    string str;
    std::vector<std::string> fnames;
    while(std::getline(in, str))
	fnames.push_back(str);
    in.close();

    //STDOUT output
    printf("--------------------------------------------------\n");
    printf("Janus Dynamic Loop Coverage- Sample-based Profiling - Results:\n");
    printf("Total Loops found: %d\n", numLoops);
    printf("%8s %12s %12s %12s %12s %16s:\n", "id", "fraction(%)", "invocation", "iteration", "inaccurate", "function");
    for (int i=1; i<=numLoops; i++) {
        if (infos[i].interrupt_count){
            printf("%8d %12.2f %12ld %12ld %12ld %20s: ", i, (float)infos[i].interrupt_count/global_interrupt_count*100, infos[i].invocations, infos[i].iterations, infos[i].suspicious, fnames[i-1].c_str());
	    print_debug_info((app_pc)addrMap[i]);
	    printf("\n");
	}
    }
    //FILE output
    stringstream ss;
    ss << app_full_name<<".lcov";
    FILE *output = fopen(ss.str().c_str(), "w");
    printf("--------------------------------------------------\n");
    fprintf(output, "--------------------------------------------------\n");
    fprintf(output, "Janus Dynamic Loop Coverage- Sample-based Profiling - Results:\n");
    fprintf(output,"Total Loops found: %d\n", numLoops);
    fprintf(output, "%8s %12s %12s %12s %12s %16s:\n", "id", "fraction(%)", "invocation", "iteration", "inaccurate", "function");
    for (int i=1; i<=numLoops; i++) {
        if (infos[i].interrupt_count)
            fprintf(output, "%8d %12.2f %12ld %12ld %12ld %20s\n", i, (float)infos[i].interrupt_count/global_interrupt_count*100, infos[i].invocations, infos[i].iterations, infos[i].suspicious, fnames[i-1].c_str());
    }
    fprintf(output, "--------------------------------------------------\n");
    fclose(output);
   
   /* ---- Close symbol access library*/
   if (drsym_exit() != DRSYM_SUCCESS) {
	printf("WARNING: error cleaning up symbol library\n");
    }
}

static void loop_start(int id, PCAddress addr)
{
    addtoLivingLoops(id);
    addrMap[id] = addr;
}

static void loop_iter(int id)
{
    infos[id].iterations++;
}


static void loop_end(int id)
{
    if(isLivingLoop(id)){
        removeFromLivingLoops(id);
        infos[id].invocations++;
    } else infos[id].suspicious = 1;
}
static void
prof_loop_start_handler(JANUS_CONTEXT, PCAddress bbAddr){
    instr_t *trigger = get_trigger_instruction(bb, rule);
    if (instr_is_cbr(trigger)){
        //Here we have the case where we have a conditional jump that either jumps to the loop (rule->reg1 == 1 then)
        //or execution falls through to the loop (rule->reg1 == 2)
        //We assemble something like:
        //A: jcc C
        //B: *loop_start*
        //C: jcc loop
        //The opcode of A will be inverted if jumping goes to loop (reg1 == 1)
        DR_ASSERT_MSG(rule->reg1 == 1 || rule->reg1 == 2, 
            "PROF_LOOP_START attached to conditional jump, but rule->reg1 doesn't have right information!\n");
        
        if (instr_get_note(trigger) != (void*)1){
            //We first remove and replace the original trigger jump instruction
            //Because then it's located in the DynamoRIO instruction locations with a high address
            //And is accessible by a 32-bit jump distance from our new jcc instruction (A)
            //(we also check to make sure we replace the trigger only once)
            instr_t *triggerClone = INSTR_CREATE_jcc(drcontext, instr_get_opcode(trigger), instr_get_target(trigger));
            instr_set_translation(triggerClone, instr_get_app_pc(trigger));
            instrlist_replace(bb, trigger, triggerClone);
            trigger = triggerClone;
            instr_set_note(trigger, (void*)1);
        }

        instr_t *jumpOver = instr_clone(drcontext, trigger);
        instr_set_target(jumpOver, opnd_create_instr(trigger));
        instr_set_translation(jumpOver, 0x0);
        if (rule->reg1 == 1){
            instr_invert_cbr(jumpOver);
        }
        instrlist_meta_preinsert(bb, trigger, jumpOver);
        if (instr_is_cti_short(jumpOver))  //We convert any 8-bit jumps to 32-bit jumps
            instr_convert_short_meta_jmp_to_long(drcontext, bb, jumpOver);

        dr_insert_clean_call(drcontext, bb, trigger, (void*)loop_start, false, 2, OPND_CREATE_INT32(rule->reg0),OPND_CREATE_INT32(bbAddr));
    }
    else {
        insert_call(loop_start, 2, OPND_CREATE_INT32(rule->reg0),OPND_CREATE_INT32(bbAddr));
    }
}

DR_EXPORT void 
dr_init(client_id_t id)
{
#ifdef JANUS_VERBOSE
    dr_fprintf(STDOUT,"\n---------------------------------------------------------------\n");
    dr_fprintf(STDOUT,"               Janus Loop Coverage Profiling\n");
    dr_fprintf(STDOUT,"---------------------------------------------------------------\n\n");
#endif
    /* Register event callbacks. */
    dr_register_bb_event(event_basic_block); 
    //dr_register_signal_event(event_timer_handler); /*sample-based profiling*/  
    dr_register_thread_init_event(coverage_thread_init);
    dr_register_thread_exit_event(coverage_thread_exit);

    /*Initialise symbol library*/
    if (drsym_init(0) != DRSYM_SUCCESS) {
            printf("WARNING: unable to initialize symbol translation\n");
    }
   /* Initialise timer and register interrupt handling routine*/
    dr_set_itimer(ITIMER_REAL, 1, timer_handler);
   
   /* Initialise janus components */
    janus_init(id);

    app_name = dr_get_application_name();
    module_data_t *data = dr_lookup_module_by_name(app_name);
    app_full_name = data->full_path;
    numLoops = rsched_info.header->numLoops;
    printf("%s contains %d loops for coverage analysis\n",app_name, numLoops);
    //allocate loop info array
    infos = (loop_info *)malloc(sizeof(loop_info) * (numLoops+1));
    memset(infos, 0, sizeof(loop_info) * (numLoops+1));

    //allocat array to store application PC
    addrMap = (PCAddress *)malloc(sizeof(PCAddress) * (numLoops+1));

    if(rsched_info.mode != JLCOV) {
        dr_fprintf(STDOUT,"Static rules not intended for %s!\n",print_janus_mode(rsched_info.mode));
        return;
    }

#ifdef JANUS_VERBOSE
    dr_fprintf(STDOUT,"Dynamorio client initialised\n");
#endif
}


/* Main execution loop: this will be executed at every initial encounter of new basic block */
static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating)
{
    RuleOp rule_opcode;
    uint32_t bb_size = 0;
    instr_t *instr, *first = instrlist_first_app(bb);
    //get current basic block starting address
    PCAddress bbAddr = (PCAddress)dr_fragment_app_pc(tag);
    //lookup in the hashtable to check if there is any rule attached to the block
    RRule *rule = get_static_rule(bbAddr);

    /*for (instr = first; instr != NULL; instr = instr_get_next_app(instr)) {
        bb_size++;
    }*/ // we dont need to use it with sample-based profiling

    //if it is a normal basic block, then omit it, else check the attached rules.
    if(rule != NULL){
	do {

	    #ifdef JANUS_VERBOSE
	    thread_print_rule(0, rule);
	    #endif

	    rule_opcode = rule->opcode;

	    switch (rule_opcode) {
        case APP_SPLIT_BLOCK:
            split_block_handler(janus_context);
            return DR_EMIT_DEFAULT;
		case PROF_LOOP_START:
            prof_loop_start_handler(janus_context, bbAddr);
		    //insert_call(loop_start, 1, OPND_CREATE_INT32(rule->reg0));
		    break;
		case PROF_LOOP_ITER:
		    insert_call(loop_iter, 1, OPND_CREATE_INT32(rule->reg0));
		    break;
		case PROF_LOOP_FINISH:
		    insert_call(loop_end, 1, OPND_CREATE_INT32(rule->reg0));
		    break;
		default:
		    fprintf(stderr,"In basic block 0x%lx static rule not recognised %d\n",bbAddr,rule_opcode);
                break;
	    }
	    rule = rule->next;
	}while(rule);
    }

    /*dr_insert_clean_call(drcontext, bb, first,
                         (void *)count, false, 1, OPND_CREATE_INT32(bb_size));*/ //counters now updated in timer interrupt handling rountine.
    return DR_EMIT_DEFAULT;
}
/* Sample-based profiling main execution loop: this will be executed at every timer interrupt */
static void
timer_handler(void *drcontext, dr_mcontext_t *mcontext){
    //increment the interrupt counts 
    global_interrupt_count++;

    if(livingloopsCount) {
        Node* current = head_livingloops;
        while(current != NULL){
            infos[current->data].interrupt_count++;
            current= current->next;
        }
    }
}
