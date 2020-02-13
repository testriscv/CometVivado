#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dijkstra.h"

Graphe* createGraphe(const char* nom, char* sSommets[], int nSommets, sArc* sArcs, int nArcs)
{
  Graphe* graphe  = malloc(sizeof(Graphe));
  Sommet* sommets = malloc(nSommets * sizeof(Sommet));
  Arc* arcs       = malloc(nArcs * sizeof(Arc));

  graphe->sommets  = sommets;
  graphe->arcs     = arcs;
  graphe->nSommets = nSommets;
  graphe->nArcs    = nArcs;
  graphe->nom      = nom;

  int i, j, k, na;
  int* iArcs = malloc(nArcs * sizeof(int));
  for (i = 0; i < nSommets; ++i)
    sommets[i].nom = sSommets[i];
  for (i = 0; i < nSommets; ++i) {
    na = 0;
    for (j = 0; j < nArcs; ++j) {
      if (strcmp(sSommets[i], sArcs[j].depart) == 0)
        iArcs[na++] = j;
    }
    sommets[i].arcs = malloc(na * sizeof(Arc*));
    for (j = 0; j < na; ++j) {
      sommets[i].arcs[j]         = &arcs[iArcs[j]];
      sommets[i].arcs[j]->poids  = sArcs[iArcs[j]].poids;
      sommets[i].arcs[j]->depart = &sommets[i];
      for (k = 0; k < nSommets; ++k)
        if (strcmp(sSommets[k], sArcs[iArcs[j]].arrivee) == 0)
          sommets[i].arcs[j]->arrivee = &sommets[k];
    }
    sommets[i].narcs = na;
  }
  free(iArcs);

  return graphe;
}

void initSommets(Sommet* sommets, int nSommets)
{
  int i;
  for (i = 0; i < nSommets; ++i) {
    sommets[i].precedent = NULL;
    sommets[i].poids     = -1;
    sommets[i].parcouru  = 0;
  }
}

int dijkstra(Graphe* graphe, int depart, int arrivee)
{
  initSommets(graphe->sommets, graphe->nSommets);
  Sommet* sommets          = graphe->sommets;
  sommets[depart].poids    = 0;
  sommets[depart].parcouru = 1;
  int i;
  int iterations = 0;
  do {
    iterations++;
    for (i = 0; i < sommets[depart].narcs; ++i) {
      if (sommets[depart].arcs[i]->arrivee->parcouru)
        continue;
      if ((sommets[depart].arcs[i]->arrivee->poids == -1 ||
           sommets[depart].arcs[i]->arrivee->poids > sommets[depart].poids + sommets[depart].arcs[i]->poids)) {
        sommets[depart].arcs[i]->arrivee->poids     = sommets[depart].poids + sommets[depart].arcs[i]->poids;
        sommets[depart].arcs[i]->arrivee->precedent = &sommets[depart];
      }
    }
    int minDistance = INT_MAX;
    depart          = -1;
    for (i = 0; i < graphe->nSommets; ++i) {
      if (sommets[i].parcouru)
        continue;
      if (sommets[i].precedent == NULL)
        continue;

      if (sommets[i].poids < minDistance) {
        minDistance = sommets[i].poids;
        depart      = i;
      }
    }
    if (depart == -1)
      break;
    sommets[depart].parcouru = 1;
  } while (depart != arrivee);
  return graphe->sommets[arrivee].poids;
}

void pluslongdespluscourts(Graphe* graphe)
{
  int i, j, k, numSommets, maxDistance = -1;
  Sommet* sommets = malloc(graphe->nSommets * sizeof(Sommet));
  for (i = 0; i < graphe->nSommets; ++i) {
    for (j = 0; j < graphe->nSommets; ++j) {
      if (i == j)
        continue;
      int foo = dijkstra(graphe, i, j);
      if (foo > maxDistance) {
        maxDistance = foo;
        Sommet* dep = &graphe->sommets[i];
        Sommet* arr = &graphe->sommets[j];
        k           = 0;
        while (arr != dep) {
          sommets[k++] = *arr;
          arr          = arr->precedent;
        }
        sommets[k--] = *dep;
        numSommets   = k;
      }
    }
  }
  printf("Plus long chemin entre 2 sommets de %s a %s : %d en %d sauts\n", sommets[k + 1].nom, sommets[0].nom,
         sommets[0].poids, k + 1);
  for (; k >= 0; --k) {
    printf("De %s a %s : %d\n", sommets[k + 1].nom, sommets[k].nom, sommets[k].poids - sommets[k + 1].poids);
  }

  free(sommets);
}

int main(int argc, char** argv)
{
  int i;
  Graphe* graphe = createGraphe("Graphe 1", sSommets, _nSommets, sArcs, _nArcs);

  pluslongdespluscourts(graphe);

  free(graphe->arcs);
  for (i = 0; i < graphe->nSommets; ++i) {
    free(graphe->sommets[i].arcs);
  }
  free(graphe->sommets);
  free(graphe);

  return 0;
}
