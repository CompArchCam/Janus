//
//  frontend.c
//  JANUS
//
//  Created by Kevin Zhou on 17/12/2014.
//
//
#include "janus_api.h"
#include "hashtable.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <set>
#include <map>
#include <iostream>
//#include "instructions.h"

//globals
#define HASH_KEY_WIDTH 8
#define KEYBASE 0x400000
#define MAX_OPTION_STRING_LENGTH 256

#define MAX_MODS 32
using namespace std;
/* Public information panel */
//Header of the rule file, contains meta informations

RSchedHeader         ruleHeader;

/* The current channel which is loaded (Loop ID) */
static uint32_t     channel;
/* All static rules stores in this buffer */
static RRule *      rule_buffer;
static char *       file_buffer;
/* We retrieve static rules using this table */
//static hashtable_t  rule_table[MAX_MODS];
static char         rulepath[MAX_OPTION_STRING_LENGTH];
map<PCAddress, hashtable_t> rule_table;
set<uint64_t>  addrmap;
typedef struct node{
    node* left;
    node* right;
    uint64_t value;
}base_tree;
base_tree *root_tree;
PCAddress curr_base;
/* load static rules from static rule file */
void         load_static_rules(char *rule_path, uint64_t base);

static void         fill_in_hashtable(hashtable_t *table, uint32_t channel, RRule *instr, uint32_t size, JMode mode, uint64_t base);

static void         check_options(client_id_t id);

static bool search_base_key(base_tree* node, PCAddress addr);

static base_tree* insert_base(base_tree* node, PCAddress base);
/* Initialise JANUS system */
void
janus_init(client_id_t id)
{
    //check DR client options
    check_options(id);

    //if options are correct, load the rule headers and static rules into the hashtable
    load_static_rules(rulepath, KEYBASE);
}

/* Check options from the strings provided by DynamoRIO */
static void 
check_options(client_id_t id)
{

    char *option_string = (char*)malloc(MAX_OPTION_STRING_LENGTH);

    //copy option string from DynamoRIO
    option_string = strcpy(option_string,dr_get_options(id));

    #ifdef JANUS_VERBOSE
    dr_fprintf(STDERR,"Dr option string %s\n",option_string);
    #endif

    strcpy(rulepath,strtok(option_string," @"));

    //register number of threads
    uint32_t nthreads = atoi(strtok(NULL," @"));
    #ifdef JANUS_VERBOSE
    dr_fprintf(STDERR,"Specified number of threads : %d\n",nthreads);
    #endif

    if(!nthreads) {
        dr_fprintf(STDERR,"Specified number of threads not valid!\n");
        exit(-1);
    }

    if(nthreads > 65536) {
        dr_fprintf(STDERR,"The maximum allowed threads are 65536!\n");
        exit(-1);
    }

    //register the number of threads
    rsched_info.number_of_threads = nthreads;

    //buffer it as a global variable
    channel = atoi(strtok(NULL," @"));

    rsched_info.channel = channel;

    free(option_string);
}

/* Returns a static rule structure based on the basic block address */
RRule *
get_static_rule(PCAddress addr)
{
    uint64_t base;
    if(!search_base_key(root_tree, addr)){
        dr_fprintf(STDERR,"ERROR!! Module base not found or loaded yet\n");
        return NULL;
    }
    return (RRule *)hashtable_lookup(&rule_table[curr_base],(void *)(addr-curr_base));
}

/* Static rule format 
 * Rule Type: 4-byte int
 * Rule Size: 4-byte int specifies the number of rule instructions in rule blocks
 * Rule Blocks:
 * Rule Data Blocks:
 * */
void
load_static_rules(char *rule_path, uint64_t base)
{
    long file_size;
    uint32_t rule_size;
    uint8_t status = 1;
    
    FILE *file = fopen(rule_path, "r");
    RRule *rule_buffer;
    RSchedHeader *header;
    if(file == NULL) {
        dr_fprintf(STDERR,"Error: static rule file %s not found\n",rule_path);
        exit(-1);
    }
    
    //obtain file size
    fseek(file,0,SEEK_END);
    file_size = ftell(file);
    rewind(file);

#ifdef JANUS_VERBOSE
    dr_fprintf(STDERR,"Rule file \"%s\" loaded: %ld bytes\n",rulepath,file_size);
#endif

    //read the file
    file_buffer= (char *)malloc(file_size);
    status = fread(file_buffer, file_size, 1, file);
    header = (RSchedHeader *) file_buffer;

#ifdef JANUS_VERBOSE
    dr_fprintf(STDERR,"Rule type %s\n",print_janus_mode(header->ruleFileType));
    dr_fprintf(STDERR,"No. of rule block %d\n",header->numRules);
    dr_fprintf(STDERR,"No. of loops %d\n",header->numLoops);
#endif

    rsched_info.mode = (JMode)header->ruleFileType;
    rsched_info.number_of_functions = header->numFuncs;

    //if it is in parallel mode, we need to get the number of actual cores
#ifdef BIND_THREAD_WITH_CORE
    if(rsched_info.mode == JPARALLEL) {
        rsched_info.number_of_cores = sysconf(_SC_NPROCESSORS_ONLN);

        if(rsched_info.number_of_threads>rsched_info.number_of_cores) {
            printf("Specified number of threads %d is greater than the actual hardware core %d\n",
                rsched_info.number_of_threads,rsched_info.number_of_cores);
        }
    }
#endif

    if (rsched_info.mode == JPARALLEL) {
        rule_buffer= (RRule *)(file_buffer + header->ruleInstOffset);
        rsched_info.loop_header = (RSLoopHeader *)((uint64_t)header + header->loopHeaderOffset);
    }
    else
        rule_buffer= (RRule *)(file_buffer + sizeof(RSchedHeader));

    //add base into the set  
    root_tree = insert_base(root_tree, base);
    //initialise hash table
    hashtable_init(&rule_table[base], HASH_KEY_WIDTH, HASH_INTPTR , false);
    //fill all rules into the hashtable
    fill_in_hashtable(&rule_table[base], channel, rule_buffer, header->numRules, rsched_info.mode, base);
    
    //copy to shared structure
    rsched_info.header = header;
    fclose(file);
}

static void
fill_in_hashtable(hashtable_t *table, uint32_t channel, RRule *instr, uint32_t size, JMode mode, uint64_t base)
{
    RRule *query, *curr, *next;
    RRule *prev = NULL;
    PCAddress start_addr;
    int exist;
    int i;
    if (mode == JFETCH || mode == JVECTOR) {
        start_addr = 0;
        for(i=0;i<size;i++) {
            curr = instr+i;
            if (start_addr != curr->block_address) {
                start_addr = curr->block_address;
                //create an entry in the hashtable
                if(base == KEYBASE)
                    hashtable_add(table,(void *)(start_addr-KEYBASE),curr);
                else{
                    curr->pc = curr->pc + base;
                    hashtable_add(table,(void *)(start_addr),curr);
                }
                if (prev) prev->next = NULL;
            } else {
                //same block rules are added in list
                prev->next = curr;
            }
            prev = curr;
        }
        prev->next = NULL;
        return;
    }

    for(i=0;i<size;i++)
    {
        curr = instr+i;

        if ((mode == JPROF) && (!(curr->channel==0 || curr->channel==channel)))
            continue;

        start_addr = curr->block_address;

        if(base == KEYBASE)
            query = (RRule *)hashtable_lookup(table,(void *)(start_addr-KEYBASE));
        else
            query = (RRule *)hashtable_lookup(table,(void *)(start_addr));
        
        if(query==NULL) {
            if(base == KEYBASE)
                hashtable_add(table,(void *)(start_addr-KEYBASE),curr);
            else{
                curr->pc = curr->pc + base;
                hashtable_add(table,(void *)(start_addr),curr);
            }
            curr->next = NULL;
        }
        //an block rule entry is a list of operations in the same basic block
        //      ordered by id
        else {
            prev = NULL;
            exist = 0;
            if (mode == JPARALLEL) {
                while(query!=NULL) {
                    if((curr->pc) < (query->pc)) break;
                    if((curr->pc == query->pc))
                    {
                        if(curr->opcode < query->opcode) break;
                        else if(curr->opcode == query->opcode) {
                            exist = 1;
                            break;
                        }
                    }

                    prev = query;
                    query = query->next;
                }
                if(exist) continue;
                if(prev == NULL) {
                    curr->next = query;
                    if(base == KEYBASE)
                        hashtable_add_replace(table,(void *)(start_addr-KEYBASE),curr);
                    else{
                        curr->pc = curr->pc + base;
                        hashtable_add_replace(table,(void *)(start_addr),curr);
                    }
                }
                else {
                    prev->next = curr;
                    curr->next = query;
                    if(query==NULL)
                        curr->next = NULL;
                }
                continue;
            } else {
                while(query!=NULL) {
                    if((curr->pc) < (query->pc)) break;
                    if((curr->pc == query->pc))
                    {
                        //if(((curr->opcode == OPT_SAVE_REG_TO_STACK)
                        //        || (curr->opcode == OPT_RESTORE_REG_FROM_STACK))
                        //    && ((query->opcode != OPT_SAVE_REG_TO_STACK)
                        //        && (query->opcode != OPT_RESTORE_REG_FROM_STACK))) break;
                        if(((uint16_t)curr->ureg0.down) < ((uint16_t)query->ureg0.down)) break;
                    }
                    prev = query;
                    query = query->next;
                }
                
                if(prev == NULL) {
                    curr->next = query;
                    if(base == KEYBASE)
                        hashtable_add_replace(table,(void *)(start_addr-KEYBASE),curr);
                    else{
                        curr->pc = curr->pc + base;
                        hashtable_add_replace(table,(void *)(start_addr-KEYBASE),curr);
                    }

                }
                else {
                    prev->next = curr;
                    curr->next = query;
                    if(query==NULL)
                        curr->next = NULL;
                }
            }
        }
    }
}

void front_end_exit() {
    for(auto it : rule_table){
        hashtable_delete(&rule_table[it.first]);
    }
    free(file_buffer);
}
bool search_base_key(base_tree* node, PCAddress addr){
    bool found = false;
//    if(node!= NULL) printf("root value: %d\n", node->value);
    while(node!=NULL){
       if(addr == node->value){
          curr_base = addr;
          return true;
       }
       else if(addr < node->value){
          if(node->left){
             node = node->left;
          }
          else
              break;
       }
       else if(addr > node->value){
          curr_base = node->value;
          found = true;
          if(node->right){
             node = node->right;
          }
          else{
              break;
          }
       }
    }
    return found;
}
base_tree* insert_base(base_tree* node, PCAddress base){
     if(node == NULL){
        node = new base_tree();
        node->value = base;
        node->left = NULL;
        node->right = NULL;
     }
     else if(base < node->value){
         node->left = insert_base(node->left, base);
     }
     else if(base > node->value){
         node->right = insert_base(node->right, base);
     }
     return node;
     //if base == node.value, do nothing and return;
}
