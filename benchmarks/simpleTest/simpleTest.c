#include <stdio.h>
#define VECSIZE 6

int main(void)
{
    int vector[] = {0, 1, 2, 3, 4, 5};
    int sum, i;

    sum = 0;
    for(i=0; i<VECSIZE; i++)
      sum += vector[i];

    printf("sum of vector elements : %d\n", sum);

    return 0;
}
