/* JANUS DDG Header
 * Definations for JANUS DDG FILE */

#ifndef _JANUS_DDG_FORMAT_
#define _JANUS_DDG_FORMAT_

#include <stdint.h>

/* A DDG file contains the following structure
 * Header
 * Node array
 * Edges for cross iteration depdences
 * Edges for inter-iteration raw dependences
 * Edges for inter-iteration war dependences
 * Edges for inter-iteration waw dependences
 */
typedef uint64_t node_id_t;

typedef struct _ddg_header {
    uint64_t            loop_id;
    uint64_t            size;
    double              fraction;
    double              average_iteration;
    uint64_t            average_cycle;
    uint64_t            total_invocation;
    int                 node_size;
    int                 cross_size;
    int                 raw_size;
    int                 war_size;
    int                 waw_size;
} DDGHeader;

typedef enum _ddg_type {
    DDG_CROSS_ITER,         /* Cross iteration */
    DDG_INTRA_RAW,          /* RAW: Read after write dep */
    DDG_INTRA_WAR,          /* WAR: Anti dependences */
    DDG_INTRA_WAW,          /* WAW: Write dependences */
} DDGEdgeType;

typedef struct _ddg_node {
    node_id_t           id;
    double              occurence;
} DDGNode;

/* An ddg edge represents the direction of the flow */
typedef struct _ddg_edge {
    node_id_t           from;
    node_id_t           to;
    uint64_t            addr;
    float               probability;
    uint32_t            type;
} DDGEdge;

#endif