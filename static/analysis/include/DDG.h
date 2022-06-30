#ifndef _Janus_DYN_DDG_
#define _Janus_DYN_DDG_

// Data flow related analysis
#include "janus.h"
#include "janus_ddg.h"
#include <climits>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

/* A graph structure */
namespace janus
{
/* strong connected components */
struct SCC {
    int id;
    int size;
    int weight;
    int priority;
    node_id_t start_node;
    node_id_t end_node;
    double cycle;
    std::set<node_id_t> nodes;
    SCC(int scc_id)
    {
        id = scc_id;
        priority = INT_MAX;
        weight = 0;
    }
};

struct DDG {
    DDGHeader *header;
    DDGNode *nodeinfo;
    DDGEdge *edges;
    std::set<node_id_t> nodes;
    std::map<node_id_t, double> node_cycles;
    std::map<node_id_t, std::set<int>> graph;
    std::map<node_id_t, std::set<int>> graph_reverse;
    std::vector<SCC> sccs;
    std::map<node_id_t, int> scc_map;
    std::map<int, std::set<int>> cedges;
    std::map<int, std::set<int>> cedges_reverse;
    /* init : construct the graph */
    void init(int mode);
    void find_scc();
    void coalesce();
    void clear();
};
} // namespace janus

void loadDDG(char *name, janus::DDG *ddg);
void loadDDG(std::string &name, janus::DDG *ddg);
void printDDG(std::ostream &os, janus::DDG &ddg);
void convertToDot(std::ostream &os, janus::DDG &ddg);
void convertToDotX(std::ostream &os, janus::DDG &ddg);

#endif