#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/time.h>

#define STR1(x) #x
#define STR(x) STR1(x)

#ifndef __unix

#define NBCORESYSCALL 0x4321
#define __getnbcore(ret) \
    asm volatile ("li x17, " STR(NBCORESYSCALL) "\n" \
                  "ecall\n" \
                  "mv %0, a0\n" \
                  : "=r" (ret) :: "x10", "x17");

int getnbcore()
{
    int nbcore;
    __getnbcore(nbcore);
    return nbcore;
}

#define THREADSYSCALL 0x1234
#define __threadstart(ret, i, fn) do { int myi = i; \
    asm volatile ("li x17, " STR(THREADSYSCALL) "\n" \
        "mv x10, %1\n" \
        "mv x11, %2\n" \
        "ecall\n" \
        "mv %0, a0\n" \
        : "=r" (ret) : "rK" (myi), "rK" (fn) : "x10", "x11", "x17" ); \
    } while(0)

#define __getCID(cid) asm volatile ("csrr %0, mhartid\n" : "=r" (cid)) 

int getCID()
{
    int cid;
    __getCID(cid);
    return cid;
}

int threadstart(int threadid, void* fn, void* args)
{
   int ret;
   __threadstart(ret, threadid, fn);
   return ret;
}

#endif

static void __attribute__((noinline)) barrier(int ncores)
{
    static volatile int sense;
    static volatile int count;
    static __thread int threadsense;

    __sync_synchronize();

    threadsense = !threadsense;
    if (__sync_fetch_and_add(&count, 1) == ncores-1)
    {
        count = 0;
        sense = threadsense;
    }
    else while(sense != threadsense)
        ;

    __sync_synchronize();
}

typedef struct Arg
{
    int cid;
    int ncore;
} A;

void salut(int cid, int ncore)
{
    printf("salut, ça va? \n");
    printf("Je suis le coeur %d sur %d\n", cid, ncore);
}

void* threadsalut(void* fnargs)
{
    A* a = fnargs;
    salut(a->cid, a->ncore);
    return NULL;
}

void testthread()
{
#ifndef __unix
    A* a = malloc(sizeof(A));
    a->cid = 1;
    a->ncore = 2;
    int i = 1; 
    printf("Trying some asm statement\n");
    printf("Address of thread function : %p with args @%p\n", threadsalut, a); 
    threadstart(i, threadsalut, a);

    //barrier(2);
    printf("Number of core : %d \n", getnbcore());
    printf("Current running core : %d\n", getCID());
    salut(getCID(), getnbcore());
    free(a);
#endif
}

int main()
{
    fprintf(stderr, "CSR test\n");

    int writecsr = 0x12345678, readcsr = 0;
    asm volatile ("csrrw %0, minstreth, %1\n" : "=r" (readcsr) : "r" (writecsr)); 
    fprintf(stderr, "Read %08x (%d) instructions done\n", readcsr, readcsr);

    int maskcsr = 0x80000000;
    asm volatile ("csrrs %0, mcycle, %1\n" : "=r" (readcsr) : "r" (maskcsr));
    fprintf(stderr, "Read %08x (%d) cycles done\n", readcsr, readcsr);

    asm volatile ("csrrc %0, mcycle, %1\n" : "=r" (readcsr) : "r" (maskcsr));
    fprintf(stderr, "Read %08x (%d) cycles done\n", readcsr, readcsr);

    asm volatile ("csrrwi %0, minstreth, 0\n" : "=r" (readcsr));
    fprintf(stderr, "Read %08x (%d) instructions done\n", readcsr, readcsr);

    return 0;
}
