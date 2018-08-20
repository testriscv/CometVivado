#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#ifdef __unix
typedef unsigned long long uint64_t;
#endif

uint64_t myclock()
{
    struct timeval tv;
    int res = gettimeofday(&tv, NULL);

    if(res < 0)
        return 0;
    else
        return (uint64_t)tv.tv_sec*1000000 + tv.tv_usec;
}
#define clock() myclock()

#include "dijkstra.h"

void printGraphe(const Graphe* graphe)
{
    int i, j;
    printf("%s :\t%d sommets\n"
           "\t\t%d arcs\n", graphe->nom, graphe->nSommets, graphe->nArcs);
    for(i = 0; i < graphe->nSommets; ++i)
    {
        printf("%s : %d arcs\n", graphe->sommets[i].nom, graphe->sommets[i].narcs);
        for(j = 0; j < graphe->sommets[i].narcs; ++j)
            printf("\t%s --> %s (%d)\n", graphe->sommets[i].arcs[j]->depart->nom
                                       , graphe->sommets[i].arcs[j]->arrivee->nom
                                       , graphe->sommets[i].arcs[j]->poids);
    }
}

Graphe* createGraphe(const char* nom, char* sSommets[], int nSommets, sArc* sArcs, int nArcs)
{
    Graphe* graphe = malloc(sizeof(Graphe));
    Sommet* sommets = malloc(nSommets*sizeof(Sommet));
    Arc* arcs = malloc(nArcs*sizeof(Arc));

    graphe->sommets = sommets;
    graphe->arcs = arcs;
    graphe->nSommets = nSommets;
    graphe->nArcs = nArcs;
    graphe->nom = nom;

    int i, j, k, na;
    int* iArcs = malloc(nArcs*sizeof(int));
    for(i = 0; i < nSommets; ++i)
        sommets[i].nom = sSommets[i];
    for(i = 0; i < nSommets; ++i)
    {
        na = 0;
        for(j = 0; j < nArcs; ++j)
        {
            if(strcmp(sSommets[i], sArcs[j].depart) == 0)
                iArcs[na++] = j;
        }
        sommets[i].arcs = malloc(na*sizeof(Arc*));
        for(j = 0; j < na; ++j)
        {
            sommets[i].arcs[j] = &arcs[iArcs[j]];
            sommets[i].arcs[j]->poids = sArcs[iArcs[j]].poids;
            sommets[i].arcs[j]->depart = &sommets[i];
            for(k = 0; k < nSommets; ++k)
                if(strcmp(sSommets[k], sArcs[iArcs[j]].arrivee) == 0)
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
    for(i = 0; i < nSommets; ++i)
    {
        sommets[i].precedent = NULL;
        sommets[i].poids = -1;
        sommets[i].parcouru = 0;
    }
}

int dijkstra(Graphe* graphe, int depart, int arrivee)
{
    initSommets(graphe->sommets, graphe->nSommets);
    Sommet* sommets = graphe->sommets;
    sommets[depart].poids = 0;
    sommets[depart].parcouru = 1;
    int i;
    int iterations = 0;
    do {
        iterations++;
        for(i = 0; i < sommets[depart].narcs; ++i)
        {
            if(sommets[depart].arcs[i]->arrivee->parcouru)
                continue;
            if((sommets[depart].arcs[i]->arrivee->poids == -1 ||
                sommets[depart].arcs[i]->arrivee->poids > sommets[depart].poids + sommets[depart].arcs[i]->poids))
            {
                sommets[depart].arcs[i]->arrivee->poids = sommets[depart].poids + sommets[depart].arcs[i]->poids;
                sommets[depart].arcs[i]->arrivee->precedent = &sommets[depart];
                /*printf("%s --> %s (%d) %d\n", sommets[depart].arcs[i]->depart->nom, sommets[depart].arcs[i]->arrivee->nom
                                     , sommets[depart].arcs[i]->poids, iterations);*/
            }
        }
        int minDistance = INT_MAX;
        depart = -1;
        for(i = 0; i < graphe->nSommets; ++i)
        {
            if(sommets[i].parcouru)
                continue;
            if(sommets[i].precedent == NULL)
                continue;

            if(sommets[i].poids < minDistance)
            {
                minDistance = sommets[i].poids;
                depart = i;
            }
        }
        if(depart == -1)
            break;
        sommets[depart].parcouru = 1;
    } while(depart != arrivee);
    //printf("%d iterations\n", iterations);
    return graphe->sommets[arrivee].poids;
}

void pluslongdespluscourts(Graphe* graphe)
{
    int i, j, k, numSommets, maxDistance = -1;
    Sommet* sommets = malloc(graphe->nSommets*sizeof(Sommet));
    for(i = 0; i < graphe->nSommets; ++i)
    {
        printf("%d\n", i+1);
        for(j = 0; j < graphe->nSommets; ++j)
        {
            if(i == j)
                continue;
            int foo = dijkstra(graphe, i, j);
            if(foo > maxDistance)
            {
                maxDistance = foo;
                Sommet* dep = &graphe->sommets[i];
                Sommet* arr = &graphe->sommets[j];
                k = 0;
                while(arr != dep)
                {
                    sommets[k++] = *arr;
                    /*printf("De %s a %s : %d\n", arr->precedent->nom, arr->nom,
                                                arr->poids-arr->precedent->poids);*/
                    arr = arr->precedent;
                }
                sommets[k--] = *dep;
                numSommets = k;
                /*printf("Chemin le plus court entre %s et %s : %d \n",
                       graphe->sommets[i].nom, graphe->sommets[j].nom,
                       graphe->sommets[j].poids);
                for(; k >= 0; --k)
                {
                    printf("De %s a %s : %d\n", sommets[k+1].nom, sommets[k].nom,
                                                sommets[k].poids-sommets[k+1].poids);
                }*/
            }
        }
    }
    printf("Plus long chemin entre 2 sommets de %s a %s : %d en %d sauts\n",
           sommets[k+1].nom, sommets[0].nom, sommets[0].poids, k+1);
    for(; k >= 0; --k)
    {
        printf("De %s a %s : %d\n", sommets[k+1].nom, sommets[k].nom,
                                    sommets[k].poids-sommets[k+1].poids);
    }

    free(sommets);
}

int main(int argc, char **argv)
{
    int i;
    /*printf("%d sommets : ", nSommets);
    for(i = 0; i < nSommets; ++i)
        printf("%s, ", sSommets[i]);

    printf("\n%d arcs : ", nArcs);
    for(i = 0; i < nArcs; ++i)
        printf("%s --> %s (%d)\n", sArcs[i].depart, sArcs[i].arrivee, sArcs[i].poids);*/

    Graphe* graphe = createGraphe("Graphe 1", sSommets, _nSommets, sArcs, _nArcs);
    //printGraphe(graphe);

    uint64_t debut = clock();
    pluslongdespluscourts(graphe);
    uint64_t fin = clock();

    //printf("Determination du plus long des plus courts en %lld ms\n", (fin-debut)/1000);
#if 0
    Sommet** iSommet = malloc(graphe->nSommets*sizeof(Sommet*));
    int depart = 6;
    int arrivee = 9;
    for(depart = 0; depart < graphe->nSommets; ++depart)
    {
        for(arrivee = 0; arrivee < graphe->nSommets; ++arrivee)
        {
            if(arrivee == depart)
                continue;

            clock_t debut = clock();
            printf("%d\n", dijkstra(graphe, depart, arrivee));
            clock_t fin = clock();
            Sommet* dep = &graphe->sommets[depart];
            Sommet* arr = &graphe->sommets[arrivee];
            if(arr->precedent == NULL)
                printf("Aucun chemin trouve de %s a %s\n", graphe->sommets[depart].nom, graphe->sommets[arrivee].nom);
            else
            {

                i = 0;
                while(arr != dep)
                {
                    iSommet[i++] = arr;
                    /*printf("De %s a %s : %d\n", arr->precedent->nom, arr->nom,
                                                arr->poids-arr->precedent->poids);*/
                    arr = arr->precedent;
                }
                iSommet[i--] = dep;
                printf("Chemin le plus court entre %s et %s : %d (en %lf ms)\n",
                       graphe->sommets[depart].nom, graphe->sommets[arrivee].nom,
                       graphe->sommets[arrivee].poids, 1000*(fin-debut)/(float)CLOCKS_PER_SEC);
                /*for(; i >= 0; --i)
                {
                    printf("De %s a %s : %d\n", iSommet[i+1]->nom, iSommet[i]->nom,
                                                iSommet[i]->poids-iSommet[i+1]->poids);
                }*/
            }
        }
    }

    // Nettoyage
    free(iSommet);
#endif
    free(graphe->arcs);
    for(i = 0; i < graphe->nSommets; ++i)
        free(graphe->sommets[i].arcs);
    free(graphe->sommets);
    free(graphe);
   
    return 0;
}
