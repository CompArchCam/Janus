#include "DDG.h"
#include <sstream>
#include <fstream>
#include <cstdio>
#include <stack>
#include <algorithm>

using namespace std;
using namespace janus;

static char            *buffer;
static int             fileSize;
static int             scc_id;

class TarjanNode {
public:
    uint32_t id;
    uint32_t index;
    uint32_t lowlink;
    bool onStack;
};

class Tarjan {
public:
    TarjanNode                  *tarjans;
    map<uint32_t,TarjanNode *>    nodes;

    stack<TarjanNode *>         stacks;
    uint32_t                      index;

    DDG                         *graph;

    Tarjan(DDG *graph);
    TarjanNode                  *getTarjanNode(uint32_t id);
};

static void regonise_scc(DDG *ddg, TarjanNode &v, Tarjan *tarjan);

Tarjan::Tarjan(DDG *graph)
:graph(graph)
{
    if(!graph) return;

    uint32_t size = graph->nodes.size();
    index = 0;

    tarjans = (TarjanNode *)malloc(sizeof(TarjanNode)*size); 

    //initialise
    for(auto n:graph->nodes) {
        //removes duplication
        auto query = nodes.find(n);
        if(query != nodes.end()) continue;

        TarjanNode &node = tarjans[index];
        index++;
        nodes[n] = &node;
        node.id = n;
        node.index = 0;
        node.lowlink = 0;
        node.onStack = false;
    }

    index = 0;
}

TarjanNode *Tarjan::getTarjanNode(uint32_t id) {
    auto query = nodes.find(id);

    if(query == nodes.end()) return NULL;
    else return (*query).second;
}

void loadDDG(char *name, DDG *ddg)
{
    ifstream ddgFile(name, ios::in|ios::ate|ios::binary);

    if (!ddgFile.is_open()) {
        cerr << "ddg file "<<name<<" does not exist"<<endl;
        return;
    }

    fileSize = ddgFile.tellg();

    buffer = new char[fileSize];
    ddgFile.seekg (0, ios::beg);
    ddgFile.read((char *)buffer,fileSize);
    ddgFile.close();

    ddg->header = (DDGHeader *)buffer;
    ddg->nodeinfo = (DDGNode *)(buffer + sizeof(DDGHeader));
    ddg->edges = (DDGEdge *)(buffer + sizeof(DDGHeader) + sizeof(DDGNode) * ddg->header->node_size);
}

void loadDDG(string &name, janus::DDG *ddg)
{
    ifstream ddgFile(name, ios::in|ios::ate|ios::binary);

    if (!ddgFile.is_open()) {
        cerr << "ddg file "<<name<<" does not exist"<<endl;
        return;
    }

    fileSize = ddgFile.tellg();

    buffer = new char[fileSize];
    ddgFile.seekg (0, ios::beg);
    ddgFile.read((char *)buffer,fileSize);
    ddgFile.close();

    ddg->header = (DDGHeader *)buffer;
    ddg->nodeinfo = (DDGNode *)(buffer + sizeof(DDGHeader));
    ddg->edges = (DDGEdge *)(buffer + sizeof(DDGHeader) + sizeof(DDGNode) * ddg->header->node_size);
}

void DDG::init(int mode) {
    uint64_t i;
    scc_id = 1;
    /* Assign unit node for the edges */
    for (i=0; i<header->size; i++) {
        if (mode == 2) {
            /* skip WAR and WAW dependences */
            if (edges[i].type == DDG_INTRA_WAR || edges[i].type == DDG_INTRA_WAW)
                continue;
        }
        if (!edges[i].from) continue;
        if (!edges[i].to) continue;
        nodes.insert(edges[i].to);
        nodes.insert(edges[i].from);
        //skip cross-iteration edge
        if (mode !=0 && edges[i].type == DDG_CROSS_ITER) continue;
        graph[edges[i].from].insert(i);
        graph_reverse[edges[i].to].insert(i);
    }

    //record dictionary for querying the occurence of nodes
    for (int i=0; i<header->node_size; i++) {
        node_cycles[nodeinfo[i].id] = nodeinfo[i].occurence;
    }
}

void DDG::find_scc()
{
    uint32_t size = nodes.size();

    Tarjan *tarjan = new Tarjan(this);

    for(uint32_t i=0; i<size; i++) {
        TarjanNode &tnode = tarjan->tarjans[i];
        if(tnode.index==0)
            regonise_scc(this, tnode, tarjan);
    }

    delete tarjan;
}

void DDG::clear()
{
    nodes.clear();
    graph.clear();
    graph_reverse.clear();
    sccs.clear();
    scc_map.clear();
    cedges.clear();
    cedges_reverse.clear();
}

static void regonise_scc(DDG *ddg, TarjanNode &v, Tarjan *tarjan)
{
    stack<TarjanNode *> &stacks = tarjan->stacks;
    auto &edges = tarjan->graph->graph;

    v.index = tarjan->index;
    v.lowlink = tarjan->index;

    (tarjan->index)++;

    stacks.push(&v);
    v.onStack = true;

    //find corresponding edges
    auto query = edges.find(v.id);

    // Consider successors of v
    if(query != edges.end()) {
        for(auto succ: (*query).second) {
            TarjanNode *w = tarjan->getTarjanNode(ddg->edges[succ].to);
            if(w) {
                if(w->index==0) {
                    regonise_scc(ddg, *w, tarjan);
                    v.lowlink = min(v.lowlink,w->lowlink);
                }
                else if(w->onStack) {
                    v.lowlink = min(v.lowlink,w->index);
                }
            }
        }
    }

    // If v is a root node, pop the stack and generate an SCC
    if (v.lowlink == v.index) {
        TarjanNode *w;
        ddg->sccs.emplace_back(scc_id);
        do {
            //if(stacks.empty()) break;
            w = stacks.top();
            stacks.pop();
            w->onStack = false;
            ddg->sccs[scc_id-1].nodes.insert(w->id);
            ddg->scc_map[w->id] = scc_id;
            ddg->sccs[scc_id-1].start_node = w->id;
        } while(v.id != w->id);
        scc_id++;
    }
}

/* Called after SCC has been recognised */
void DDG::coalesce()
{
    /* coalesce edges */
    for (auto edge: graph) {
        for (auto to: edge.second) {
            int from_scc_id = scc_map[edge.first];
            int to_scc_id = scc_map[edges[to].to];
            if (from_scc_id != to_scc_id) {
                cedges[from_scc_id].insert(to_scc_id);
                cedges_reverse[to_scc_id].insert(from_scc_id);
            }
        }
    }
    /*
    for (auto scc: sccs) {
        cout <<scc.id<<": "<<scc.nodes.size()<<" :";
        for (auto n:scc.nodes)
            cout <<n<<" ";
        cout <<endl;
    }
    for (auto e: cedges) {
        for (auto t:e.second) {
            cout <<sccs[e.first-1].start_node<<" -> "<<sccs[t-1].start_node<<endl;
        }
    }
    */
}

void printDDG(ostream &os, DDG &ddg)
{
    int i;
    DDGHeader *header = ddg.header;
    DDGEdge *cross_edges = ddg.edges; 
    DDGEdge *raw_edges = ddg.edges + header->cross_size;
    DDGEdge *war_edges = ddg.edges + (header->cross_size + header->raw_size);
    DDGEdge *waw_edges = ddg.edges + (header->cross_size + header->raw_size + header->war_size);

    os << "Loop "<<header->loop_id<<" DDG:"<<endl;
    os << "Time fraction :"<<header->fraction<<endl;
    os << "Average iteration :"<<header->average_iteration<<endl;
    os << "Total invocation :"<<header->total_invocation<<endl;
    os << "Averge cycle per iteration :"<<header->average_cycle<<endl;
    os << "DDG Size :"<<header->size<<endl;

    os << "DDG Nodes:"<<endl;
    for (i=0; i<header->node_size; i++) {
        DDGNode &node = ddg.nodeinfo[i];
        os <<node.id<<" weight "<<node.occurence<<endl;
    }

    os << "Cross iteration dependences:"<<endl;
    for (i=0; i<header->cross_size; i++) {
        DDGEdge &edge = cross_edges[i];
        os <<edge.from<<" -> "<<edge.to<<" prob: "<<edge.probability<<endl;
    }

    os << "Read after write dependences:"<<endl;
    for (i=0; i<header->raw_size; i++) {
        DDGEdge &edge = raw_edges[i];
        os <<edge.from<<" -> "<<edge.to<<" prob: "<<edge.probability<<endl;
    }

    os << "Write after read dependences:"<<endl;
    for (i=0; i<header->war_size; i++) {
        DDGEdge &edge = war_edges[i];
        os <<edge.from<<" -> "<<edge.to<<" prob: "<<edge.probability<<endl;
    }

    os << "Write after write dependences:"<<endl;
    for (i=0; i<header->waw_size; i++) {
        DDGEdge &edge = waw_edges[i];
        if (!edge.from) continue;
        if (!edge.to) continue;
        os <<edge.from<<" -> "<<edge.to<<" prob: "<<edge.probability<<endl;
    }
}

void convertToDot(ostream &os, DDG &ddg)
{
    int i;
    DDGHeader *header = ddg.header;
    DDGEdge *cross_edges = ddg.edges; 
    DDGEdge *raw_edges = ddg.edges + header->cross_size;
    DDGEdge *war_edges = ddg.edges + (header->cross_size + header->raw_size);
    DDGEdge *waw_edges = ddg.edges + (header->cross_size + header->raw_size + header->war_size);

    os << "digraph loop_"<<header->loop_id<<"_DDG {";
    os << "label=\"loop_"<<header->loop_id<<"\""<<endl;

    /* Print cross-iteration edges */
    for (i=0; i<header->cross_size; i++) {
        DDGEdge &edge = cross_edges[i];
        uint64_t from = edge.from;
        uint64_t to = edge.to;
        if (from < 2000)
            os<<"\t"<<dec<<from<<" -> ";
        else
            os<<"\t"<<hex<<"\"0x"<<from<<"\" -> ";
        if (to < 2000)
            os<<"\t"<<dec<<to;
        else
            os<<"\t"<<hex<<"\"0x"<<to<<"\"";
        os <<" [style=dashed,color=\"red\"];"<<endl;
    }

    /* Print RAW edges */
    for (i=0; i<header->raw_size; i++) {
        DDGEdge &edge = raw_edges[i];
        uint64_t from = edge.from;
        uint64_t to = edge.to;
        if (from < 2000)
            os<<"\t"<<dec<<from<<" -> ";
        else
            os<<"\t"<<hex<<"\"0x"<<from<<"\" -> ";
        if (to < 2000)
            os<<"\t"<<dec<<to;
        else
            os<<"\t"<<hex<<"\"0x"<<to<<"\"";
        os <<" [color=\"green\"];"<<endl;
    }

    /* Print WAR edges */
    for (i=0; i<header->war_size; i++) {
        DDGEdge &edge = war_edges[i];
        uint64_t from = edge.from;
        uint64_t to = edge.to;
        if (from < 2000)
            os<<"\t"<<dec<<from<<" -> ";
        else
            os<<"\t"<<hex<<"\"0x"<<from<<"\" -> ";
        if (to < 2000)
            os<<"\t"<<dec<<to;
        else
            os<<"\t"<<hex<<"\"0x"<<to<<"\"";
        os <<" [color=\"blue\"];"<<endl;
    }

    /* Print WAW edges */
    for (i=0; i<header->waw_size; i++) {
        DDGEdge &edge = waw_edges[i];
        uint64_t from = edge.from;
        uint64_t to = edge.to;
        if (from < 2000)
            os<<"\t"<<dec<<from<<" -> ";
        else
            os<<"\t"<<hex<<"\"0x"<<from<<"\" -> ";
        if (to < 2000)
            os<<"\t"<<dec<<to;
        else
            os<<"\t"<<hex<<"\"0x"<<to<<"\"";
        os <<" [color=\"black\"];"<<endl;
    }
    os << "}"<<endl;
}

void convertToDotX(ostream &os, DDG &ddg)
{
    int i,iter;
    DDGHeader *header = ddg.header;
    DDGEdge *cross_edges = ddg.edges; 
    DDGEdge *raw_edges = ddg.edges + header->cross_size;
    DDGEdge *war_edges = ddg.edges + (header->cross_size + header->raw_size);
    DDGEdge *waw_edges = ddg.edges + (header->cross_size + header->raw_size + header->war_size);

    os << "digraph loop_"<<header->loop_id<<"_DDG {";
    os << "label=\"loop_"<<header->loop_id<<"\""<<endl;

    for (iter=0; iter<4; iter++) {
        os <<"\tsubgraph cluster_iter_"<<iter<<"{"<<endl;
        os <<"\t\tlabel=\"Iteration "<<iter<<"\";"<<endl;
        /* Print cross-iteration edges */
        if (iter) {
            for (i=0; i<header->cross_size; i++) {
                DDGEdge &edge = cross_edges[i];
                uint64_t from = edge.from;
                uint64_t to = edge.to;
                if (from < 2000)
                    os<<"\t\t\""<<dec<<from<<"_"<<(iter-1)<<"\" -> ";
                else
                    os<<"\t\t\""<<hex<<"0x"<<from<<"_"<<dec<<(iter-1)<<"\" -> ";
                if (to < 2000)
                    os<<"\t\""<<dec<<to<<"_"<<iter<<"\"";
                else
                    os<<"\t\""<<hex<<"0x"<<to<<dec<<"_"<<iter<<"\"";
                os <<" [style=dashed,color=\"red\"];"<<endl;
            }
        }

        /* Print RAW edges */
        for (i=0; i<header->raw_size; i++) {
            DDGEdge &edge = raw_edges[i];
            uint64_t from = edge.from;
            uint64_t to = edge.to;
            if (from < 2000)
                os<<"\t\t\""<<dec<<from<<"_"<<iter<<"\" -> ";
            else
                os<<"\t\t\""<<hex<<"0x"<<from<<"_"<<dec<<iter<<"\" -> ";
            if (to < 2000)
                os<<"\t\""<<dec<<to<<"_"<<iter<<"\"";
            else
                os<<"\t\""<<hex<<"0x"<<to<<dec<<"_"<<iter<<"\"";
            os <<" [color=\"green\"];"<<endl;
        }

        /* Print WAR edges */
        for (i=0; i<header->war_size; i++) {
            DDGEdge &edge = war_edges[i];
            uint64_t from = edge.from;
            uint64_t to = edge.to;
            if (from < 2000)
                os<<"\t\t\""<<dec<<from<<"_"<<iter<<"\" -> ";
            else
                os<<"\t\t\""<<hex<<"0x"<<from<<"_"<<dec<<iter<<"\" -> ";
            if (to < 2000)
                os<<"\t\""<<dec<<to<<"_"<<iter<<"\"";
            else
                os<<"\t\""<<hex<<"0x"<<to<<dec<<"_"<<iter<<"\"";
            os <<" [color=\"blue\"];"<<endl;
        }

        /* Print WAW edges */
        for (i=0; i<header->waw_size; i++) {
            DDGEdge &edge = waw_edges[i];
            uint64_t from = edge.from;
            uint64_t to = edge.to;
            if (from < 2000)
                os<<"\t\t\""<<dec<<from<<"_"<<iter<<"\" -> ";
            else
                os<<"\t\t\""<<hex<<"0x"<<from<<"_"<<dec<<iter<<"\" -> ";
            if (to < 2000)
                os<<"\t\""<<dec<<to<<"_"<<iter<<"\"";
            else
                os<<"\t\""<<hex<<"0x"<<to<<dec<<"_"<<iter<<"\"";
            os <<" [color=\"black\"];"<<endl;
        }
        os <<"\t}"<<endl;
        os <<endl;
    }
    os << "}"<<endl;
}