#include <stdio.h>
#include <string.h>

#include <matmul.h>

int main(void)
{
    int i=0;
    int j;
    int k;
    TYPE sum;

    for (i=0; i<SIZE; i++)
    {
        for (j=0; j<SIZE; j++)
        {
            sum = 0;
            for(k = 0; k<SIZE; k++)
                sum += A[(i<<SHIFT) + k] * B[(k<<SHIFT) + j];
            result[(i<<SHIFT) + j] = sum;
        }
    }
    
#define STR1(x) #x
#define STR(x) STR1(x)
    for(i = 0; i < SIZE; ++i)
    {
        for(j = 0; j < SIZE; ++j)
        {
            if(strcmp(STR(TYPE), "float") == 0)
                printf("%f ", result[(i<<SHIFT) + j]);
            else if(strcmp(STR(TYPE), "int64") == 0 || strcmp(STR(TYPE), "uint64") == 0)
                printf("%lld ", result[(i<<SHIFT) + j]);
            else
                printf("%d ", result[(i<<SHIFT) + j]);
        }
        printf("\n");
    }

    return 0;
}
