typedef struct sArc {
  char* depart;
  char* arrivee;
  int poids;
} sArc;

char* sSommets[] = {"A", "B", "C", "D", "E", "F", "G", "H", "I", "J"};
sArc sArcs[]     = {{"A", "A", 2}, {"A", "B", 5}, {"A", "C", 8}, {"B", "C", 6}, {"B", "D", 8}, {"B", "E", 6},
                {"C", "B", 2}, {"C", "D", 1}, {"C", "E", 2}, {"D", "E", 3}, {"D", "F", 1}, {"E", "A", 5},
                {"E", "D", 1}, {"E", "G", 5}, {"F", "D", 4}, {"F", "E", 1}, {"F", "G", 3}, {"F", "H", 6},
                {"E", "I", 3}, {"G", "H", 2}, {"H", "B", 6}, {"H", "B", 7}, {"I", "J", 4}, {"J", "I", 5}};

#define _nSommets (sizeof(sSommets) / sizeof(sSommets[0]))
#define _nArcs (sizeof(sArcs) / sizeof(sArcs[0]))

typedef struct Sommet Sommet;
typedef struct Arc {
  Sommet* depart;
  Sommet* arrivee;
  int poids;
} Arc;

struct Sommet {
  const char* nom;
  Arc** arcs;
  int narcs;

  struct Sommet* precedent;
  int poids;
  int parcouru;
};

typedef struct Graphe {
  const char* nom;
  Sommet* sommets;
  Arc* arcs;
  int nSommets;
  int nArcs;
} Graphe;
