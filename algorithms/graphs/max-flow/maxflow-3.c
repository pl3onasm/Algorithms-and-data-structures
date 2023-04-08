/* file: maxflow-3.c  
   author: David De Potter
   email: pl3onasm@gmail.com
   license: MIT, see LICENSE file in repository root folder
   description: implements the Dinitz version of the 
     max-flow algorithm. This is Ford-Fulkerson with 
     level graphs and DFS.
   time complexity: 
*/

#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <string.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define INF DBL_MAX
#define true 1
#define false 0
typedef short bool;

//:::::::::::::::::::::::: data structures ::::::::::::::::::::::::://

typedef struct edge {
  int from, to;           // ids of the endpoints of the edge (u->v)
  double cap;             // capacity of the edge
  double flow;            // flow on the edge
  bool residual;          // true if the edge is a residual edge
} edge;

typedef struct node {
  int id;                 // id of the node
  int *adj;               // adj list is an array of indices of outgoing edges
  int adjCap;             // capacity of the adjacency list
  int nAdj;               // number of adjacent nodes
  int level;              // level of the node in the level graph
  int adjIdx;             // current adjacency index in the DFS traversals
} node;

typedef struct graph {
  int nNodes, nEdges;     // number of nodes and edges in the graph
  node **nodes;           // array of pointers to nodes
  edge **edges;           // array of pointers to edges
  int edgeCap;            // capacity of the edge array
  double maxFlow;         // maximum flow in the graph
} graph;

typedef struct queue {
  int front, back, size;   // front and back of the queue, and its size
  int *array;              // array of elements in the queue
} queue;

//::::::::::::::::::::::: memory management :::::::::::::::::::::::://

void *safeCalloc (int n, int size) {
  /* allocates n elements of size size, initializing them to 0, and
     checks whether the allocation was successful */
  void *ptr = calloc(n, size);
  if (ptr == NULL) {
    printf("Error: calloc(%d, %d) failed. Out of memory?\n", n, size);
    exit(EXIT_FAILURE);
  }
  return ptr;
}

void *safeRealloc (void *ptr, int newSize) {
  /* reallocates memory and checks whether the allocation was successful */
  ptr = realloc(ptr, newSize);
  if (ptr == NULL) {
    printf("Error: realloc(%d) failed. Out of memory?\n", newSize);
    exit(EXIT_FAILURE);
  }
  return ptr;
}

//:::::::::::::::::::::::: graph functions ::::::::::::::::::::::::://

node *newNode(int id) {
  /* creates a node with given id */
  node *n = safeCalloc(1, sizeof(node));
  n->id = id;
  n->adjCap = 0;
  n->nAdj = 0;
  n->level = -1;
  return n;
}

graph *newGraph(int n) {
  /* creates a graph with n vertices */
  graph *G = safeCalloc(1, sizeof(graph));
  G->nNodes = n;
  G->nEdges = 0;
  G->edgeCap = 0;
  G->maxFlow = 0;
  G->nodes = safeCalloc(n, sizeof(node*));
  for (int i = 0; i < n; i++)
    G->nodes[i] = newNode(i);
  return G;
}

void loopReset(graph *G) {
  /* resets all level fields and current adjacency list indices */
  for (int i = 0; i < G->nNodes; i++){
    G->nodes[i]->level = -1;
    G->nodes[i]->adjIdx = 0;
  }
}

void freeGraph(graph *G) {
  /* frees all memory allocated for the graph */
  for (int i = 0; i < G->nNodes; i++){
    free(G->nodes[i]->adj);
    free(G->nodes[i]);
  }
  free(G->nodes);
  for (int i = 0; i < G->nEdges; i++)
    free(G->edges[i]);
  free(G->edges);
  free(G);
}

edge *addEdge(graph *G, int uId, int vId, double cap, bool residual) {
  /* adds an edge from u to v with capacity cap */
  edge *e = safeCalloc(1, sizeof(edge));
  e->from = uId;
  e->to = vId;
  e->cap = cap;
  e->flow = 0;
  e->residual = residual;
  // check if we need to resize the edge array
  if (G->edgeCap == G->nEdges) {
    G->edgeCap += 10;
    G->edges = safeRealloc(G->edges, G->edgeCap * sizeof(edge*));
  }
  // check if we need to resize the adjacency list
  node *u = G->nodes[uId];
  if (u->adjCap == u->nAdj) {
    u->adjCap += 10;
    u->adj = safeRealloc(u->adj, u->adjCap * sizeof(int));
  }
  u->adj[u->nAdj++] = G->nEdges; // add the edge index to the adj list
  G->edges[G->nEdges++] = e;     // add the edge to the edge array
  return e;
}

void buildGraph(graph *G) {
  /* reads undirected graph from stdin and builds the adjacency lists */
  int u, v; double cap;
  while (scanf("%d %d %lf", &u, &v, &cap) == 3) {
    addEdge(G, u, v, cap, false); // add forward edge
    addEdge(G, v, u, 0, true);    // add residual edge
  }
}

//:::::::::::::::::::::::: queue functions ::::::::::::::::::::::::://

bool isEmpty(queue *Q) {
  /* returns 1 if the queue is empty, 0 otherwise */
  return Q->front == Q->back;
}

queue *newQueue(int n) {
  /* creates a queue with n elements */
  queue *Q = safeCalloc(1, sizeof(queue));
  Q->array = safeCalloc(n, sizeof(int));
  Q->front = 0;
  Q->back = 0;
  Q->size = n;
  return Q;
}

void freeQueue(queue *Q) {
  /* frees all memory allocated for the queue */
  free(Q->array);
  free(Q);
}

void doubleQueueSize(queue *Q) {
  /* doubles the size of the queue */
  Q->array = safeRealloc(Q->array, 2 * Q->size * sizeof(int));
  for (int i = 0; i < Q->back; ++i)
    Q->array[i + Q->size] = Q->array[i];
  Q->back += Q->size;
  Q->size *= 2;
}

void enqueue (queue *Q, int n) {
  /* adds n to the back of the queue */
  Q->array[Q->back] = n; 
  Q->back = (Q->back + 1) % Q->size;
  if (Q->back == Q->front) doubleQueueSize(Q);
}

int dequeue (queue *Q) {
  /* removes and returns the first element of the queue */
  if (isEmpty(Q)) {
    printf("Error: dequeue() called on empty queue.\n");
    exit(EXIT_FAILURE);
  }
  int n = Q->array[Q->front];
  Q->front = (Q->front + 1) % Q->size;
  return n;
}

//::::::::::::::::::::::::::::: Dinic :::::::::::::::::::::::::::::://

bool bfs(graph *G, int s, int t) {
  /* builds a BFS tree from s to t and returns if there is a path */
  queue *q = newQueue(G->nNodes); 
  enqueue(q, s);                           // enqueue source node
  G->nodes[s]->level = 0;                  // set source level to 0
  while (!isEmpty(q)) {
    node *n = G->nodes[dequeue(q)];
   
    // check each edge from n
    for (int i = 0; i < n->nAdj; i++) {
      int eId = n->adj[i];
      edge *e = G->edges[eId];
      node *a = G->nodes[e->to];
      if (e->cap > 0 && a->level == -1) {
        a->level = n->level + 1;           // set level of child node
        enqueue(q, a->id);                
      }
    }
  }
  freeQueue(q);
  return G->nodes[t]->level != -1;         // return if t is reachable
}

double dfs(graph *G, int s, int t, double flow) {
  /* finds the blocking flow from s to t */
  if (s == t) return flow;                 // reached sink
  node *n = G->nodes[s];
  // check each edge from n and prune those that don't lead to t
  // so that we don't have to check them again in the next DFS call
  for (int i = n->adjIdx; i < n->nAdj; i++) {
    n->adjIdx = i;                         // update current adjacency index
    int eId = n->adj[i];
    edge *e = G->edges[eId];
    node *a = G->nodes[e->to];
    if (e->cap > 0 && a->level == n->level + 1) {
      double bneck = dfs(G, e->to, t, MIN(flow, e->cap));
      if (bneck > 0) {
        e->cap -= bneck; e->flow += bneck; // adjust forward edge
        G->edges[eId^1]->cap += bneck;     // adjust residual edge
        return bneck;
      } 
    } 
  }
  return 0;
}

void dinic(graph *G, int s, int t) {
  /* finds the maximum flow from s to t using Dinic's algorithm */
  while (bfs(G, s, t)) {
    for (double f = dfs(G, s, t, INF); f; f = dfs(G, s, t, INF)) 
      G->maxFlow += f;
    loopReset(G);  // reset all levels and current adjList indices
  }
}

void printFlow(graph *G, int s, int t) {
  /* prints the flow on each edge of the graph G */
  printf("The maximum flow from node %d to node %d"
         " is %.2lf\n\nEdges %15s\n", s, t, G->maxFlow, "Flow");
  printf("---------------------\n"); 
  for (int i = 0; i < G->nEdges; ++i) {
    edge *e = G->edges[i];
    if (!e->residual){
      printf("(%d, %d)", e->from, e->to);
      if (e->flow > 0) printf("%15.2lf\n", e->flow);
      else printf("%15c\n", '-');
    }
  }
}

//::::::::::::::::::::::::: main function :::::::::::::::::::::::::://

int main (int argc, char *argv[]) {
  int n, s, t;                    // number of nodes, source, sink
  scanf("%d %d %d", &n, &s, &t);

  graph *G = newGraph(n); 
  buildGraph(G);                  // read edges from stdin

  dinic(G, s, t);                 // find the maximum flow
  printFlow(G, s, t);             // print flow values

  freeGraph(G);                   // free memory
  return 0;
}
