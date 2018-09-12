#include <stdio.h>
#include <string.h>

#include "qsort.h"
int partition(int low, int hi)
{
    TYPE pivot = A[hi];
    int i = low-1,j;
    TYPE temp;
    for(j = low; j<hi; j++)
    {
        if(A[j] < pivot)
        {
            i = i+1;
            temp = A[i];
            A[i] = A[j];
            A[j] = temp;
        }
    }
    if(A[hi] < A[i+1])
    {
        temp = A[i+1];
        A[i+1] = A[hi];
        A[hi] = temp;
    }
    return i+1;
}

void qsort(int low, int hi)
{
    if(low < hi)
    {
        int p = partition(low, hi);
        qsort(low,p-1);
        qsort(p+1,hi);
    }
}

int main()
{
    int i;
    qsort(0,SIZE-1);
#ifndef __HLS__
#define STR1(X) #X
#define STR(X)  STR1(X)
    for(i = 0; i < SIZE; ++i)
    {
        if(strcmp(STR(TYPE), "float") == 0)
            printf("%f\n", A[i]);
        else if(strcmp(STR(TYPE), "int64") == 0 || strcmp(STR(TYPE), "uint64") == 0)
            printf("%lld\n", A[i]);
        else
            printf("%d\n", A[i]);                    
    }
#endif
}
