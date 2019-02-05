/* Rule Dump Tool:
 * Dump static rule file to standard output
 */

#include <iostream>
#include <fstream>
#include <string>
#include "janus.h"
#include "Arch.h"
#include "IO.h"

using namespace std;

char *buffer;
int fileSize;
int channel;

static void usage()
{
    cout<<"Usage: schedump {file}.jrs             : dump all rules"<<endl;
    cout<<"       schedump -h {file}.jrs          : dump rule header only"<<endl;
    cout<<"       schedump -c channel {file}.jrs  : dump rules at specified channel"<<endl;
}

static bool checkFileFormat(char *name)
{
    /* Currently we just search for sub string .rule
     * XXX: optimise this to check file extension properly */
    string rulename = string(name);

    auto result = rulename.find(".jrs");

    return result != string::npos;
}

static void loadRewriteRule(char *name)
{
    ifstream ruleFile(name, ios::in|ios::ate|ios::binary);

    if (!ruleFile.is_open()) {
        cerr << "Janus Rewrite Schedule file "<<name<<" does not exist"<<endl;
    }

    fileSize = ruleFile.tellg();

    buffer = new char[fileSize];
    ruleFile.seekg (0, ios::beg);
    ruleFile.read((char *)buffer,fileSize);
    ruleFile.close();
}

static void
usageAndExit()
{
    usage();
    exit(-1);
}

int main(int argc, char **argv) {
    
    int mode = 0;
    if (argc == 2) {
        /* Dump all rules mode */
        if (checkFileFormat(argv[1])) {
            loadRewriteRule(argv[1]);
            mode = 1;
        } else {
            cerr << "Please supply a rewrite schedule file ending with *.jrs"<<endl;
            return 0;
        }
    } else if (argc == 3) {
        /* Dump header mode */
        //check -h
        if (argv[1][0] != '-') usageAndExit();
        if (argv[1][1] != 'h') usageAndExit();
        if (!checkFileFormat(argv[2])) {
            cerr << "Please supply a rewrite schedule file ending with *.jrs"<<endl;
            return 0;
        } else {
            loadRewriteRule(argv[2]);
            mode = 2;
        }
    } else if (argc == 4) {
        /* Dump channel mode */
        //check -c
        if (argv[1][0] != '-') usageAndExit();
        if (argv[1][1] != 'c') usageAndExit();
        if (argv[2][0] == '0') channel = 0;
        else {
            channel = atoi(argv[2]);
            if (channel <= 0) {
                cerr << "Channel number not recognised"<<endl;
                exit(-1);
            }
        }
        if (!checkFileFormat(argv[3])) {
            cerr << "Please supply a rewrite schedule file ending with *.jrs"<<endl;
            return 0;
        } else {
            loadRewriteRule(argv[3]);
            mode = 3;
        }
    } else {
        usage();
        exit(-1);
    }

    /* Cast the buffer to file header */
    RSchedHeader *header = (RSchedHeader *)buffer;
    cout <<"Rewrite schedule type:  "<<print_janus_mode((JMode)header->ruleFileType)<<endl;
    cout <<"Number of rewrite rules: "<<header->numRules<<endl;
    cout <<"Number of loops: "<<header->numLoops<<endl;
    if (header->multiMode) cout <<"Multiple loop mode"<<endl;
    if (header->ruleDataOffset)
        cout <<"Data section offset: "<<header->ruleDataOffset<<endl;

    /* For mode 2 it is enough */
    if (mode == 2) return 1;

    if (header->multiMode) {
        /* Parse loop headers */
        RSLoopHeader *loops = (RSLoopHeader *)(buffer + sizeof(RSchedHeader));

        for (int i=0; i<header->numLoops; i++) {
            cout <<"Loop "<<loops[i].id<<dec<<" static id "<<loops[i].staticID<<" : number of rules "<<dec<<loops[i].ruleInstSize<<" number of data "<<loops[i].ruleDataSize<<endl;
            RRule *rule = (RRule *)(buffer + loops[i].ruleInstOffset);
            for (int j=0; j<loops[i].ruleInstSize; j++) {
                print_rule(rule+j);
            }
            JVarProfile *profile = (JVarProfile *)(buffer + loops[i].ruleDataOffset);
            for (int j=0; j<loops[i].ruleDataSize; j++) {
                print_profile(profile+j);
            }
        }
        return 0;
    }

    /* Bypass the header and parse static rules */
    RRule *ruleArray = (RRule *)(buffer + sizeof(RSchedHeader));

    if (mode == 3) {
        cout <<"Selected channel : "<<channel<<endl;
        for (int i=0; i<header->numRules; i++) {
            if (channel == (int)ruleArray[i].channel)
                print_rule(ruleArray+i);
        }
        if (header->ruleDataOffset) {
            /* Get the offset the data in this channel */
            uint32_t *loopArray = (uint32_t *)(buffer + header->ruleDataOffset);
            uint32_t dataOffset = loopArray[channel];
            cout <<"Loop data: "<<endl;
            uint32_t i = 1;
            while (loopArray[channel+i] == 1) {
                i++;
            }
            uint32_t dataOffsetEnd = loopArray[channel+i];

            if (dataOffset == 1) cout <<"No data for this loop"<<endl;
            else {
                while (dataOffset < dataOffsetEnd) {
                    JVarProfile *profile = (JVarProfile *)(buffer + dataOffset);
                    print_profile(profile);
                    dataOffset += sizeof(JVarProfile);
                }
            }
        }
        return 1;
    }
    //mode 1
    int currentChannel = -1;
    for (int i=0; i<header->numRules; i++) {
        if (currentChannel != (int)ruleArray[i].channel) {
            if (header->ruleDataOffset && currentChannel != -1 && currentChannel != 0) {
                /* Get the offset the data in this channel */
                uint32_t *loopArray = (uint32_t *)(buffer + header->ruleDataOffset);
                uint32_t dataOffset = loopArray[currentChannel];
                cout <<"Loop data: "<<endl;
                uint32_t i = 1;
                while (loopArray[currentChannel+i] == 1) {
                    i++;
                }
                uint32_t dataOffsetEnd = loopArray[currentChannel+i];

                if (dataOffset == 1) cout <<"No data for this loop"<<endl;
                else {
                    while (dataOffset < dataOffsetEnd) {
                        JVarProfile *profile = (JVarProfile *)(buffer + dataOffset);
                        print_profile(profile);
                        dataOffset += sizeof(JVarProfile);
                    }
                }
            }
            cout <<"Channel "<< (int)ruleArray[i].channel <<": -----------------------------"<<endl;
            currentChannel = (int)ruleArray[i].channel;
        }
        print_rule(ruleArray+i);
    }
}
