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

int main(int argc, char** argv)
{
   
    printf("This is %s called with %d arguments :\n", argv[0], argc-1);
    for(int i = 1; i < argc; ++i)
        printf("%d      %s\n", i, argv[i]);

    FILE* fichier = fopen("test_syscalls.c", "r");
    
    if(fichier == NULL)
        return -1;

    // get file size
    fseek(fichier, 0, SEEK_END);
    long taille = ftell(fichier);
    // go back to start
    fseek(fichier, 0, SEEK_SET);

    char* content = malloc(taille + 1);
    if(content == NULL)
        return -1;

    clock_t debut = clock();
    time_t tdebut = time(NULL);
    struct timeval tvdebut;
    gettimeofday(&tvdebut, NULL);
    
    // read all file
    long result = fread(content, 1, taille, fichier);
    content[taille] = '\0';

    printf("%d  %d\n", taille, result);

    printf("Read %d bytes of file :\n", strlen(content));
    printf("%s\n", content);
    fflush(stdout);
    free(content);


    printf("statsize %d\n", sizeof(struct stat));

    struct stat stat_sys;
#ifdef __unix
    printf("fstat : %d  @%p\n", fstat(fichier->_fileno, &stat_sys), &stat_sys);
#else
    printf("fstat : %d  @%p\n", fstat(fichier->_file, &stat_sys), &stat_sys);
#endif
    
    printf("\n\nContent of the fstat : \n\n");
    printf("st_dev     (%d @%p) :   %x\n", sizeof(stat_sys.st_dev), &stat_sys.st_dev, stat_sys.st_dev);
    printf("st_ino     (%d @%p) :   %x\n", sizeof(stat_sys.st_ino), &stat_sys.st_ino, stat_sys.st_ino);
    printf("st_mode    (%d @%p) :   %o\n", sizeof(stat_sys.st_mode), &stat_sys.st_mode, stat_sys.st_mode);
    printf("st_nlink   (%d @%p) :   %x\n", sizeof(stat_sys.st_nlink), &stat_sys.st_nlink, stat_sys.st_nlink);
    printf("st_uid     (%d @%p) :   %x\n", sizeof(stat_sys.st_uid), &stat_sys.st_uid, stat_sys.st_uid);
    printf("st_gid     (%d @%p) :   %x\n", sizeof(stat_sys.st_gid), &stat_sys.st_gid, stat_sys.st_gid);
    printf("st_rdev    (%d @%p) :   %x\n", sizeof(stat_sys.st_rdev), &stat_sys.st_rdev, stat_sys.st_rdev);
    printf("st_size    (%d @%p) :   %d\n", sizeof(stat_sys.st_size), &stat_sys.st_size, stat_sys.st_size);
#ifdef __unix
    printf("st_atime   (%d @%p) :   %x\n", sizeof(stat_sys.st_atim.tv_sec), &stat_sys.st_atim.tv_sec, stat_sys.st_atim.tv_sec);
    printf("st_mtime   (%d @%p) :   %x\n", sizeof(stat_sys.st_mtim.tv_sec), &stat_sys.st_mtim.tv_sec, stat_sys.st_mtim.tv_sec);
    printf("st_ctime   (%d @%p) :   %x\n", sizeof(stat_sys.st_ctim.tv_sec), &stat_sys.st_ctim.tv_sec, stat_sys.st_ctim.tv_sec);
#else
    printf("st_atime   (%d @%p) :   %x\n", sizeof(stat_sys.st_atime), &stat_sys.st_atime, stat_sys.st_atime);
    printf("st_mtime   (%d @%p) :   %x\n", sizeof(stat_sys.st_mtime), &stat_sys.st_mtime, stat_sys.st_mtime);
    printf("st_ctime   (%d @%p) :   %x\n", sizeof(stat_sys.st_ctime), &stat_sys.st_ctime, stat_sys.st_ctime);
#endif
    printf("st_blksize (%d @%p) :   %d\n", sizeof(stat_sys.st_blksize), &stat_sys.st_blksize, stat_sys.st_blksize);
    printf("st_blocks  (%d @%p) :   %d\n", sizeof(stat_sys.st_blocks), &stat_sys.st_blocks, stat_sys.st_blocks);

    fclose(fichier);

#ifdef __unix
    printf("%d\n", isatty(stdout->_fileno));
#else
    printf("%d\n", isatty(stdout->_file));
#endif

    printf("\033[1;31mbold red text\033[0m\n");

    // seems like there's a bug in newlib for clock that always return -1
    struct timeval tv;
    printf("gettimeofday    %d\n", gettimeofday(&tv, NULL));    
    printf("%s\n", ctime(&tv.tv_sec));

    clock_t end = clock();
    time_t tend = time(NULL);
    
    int64_t microdebut = (int64_t)tvdebut.tv_sec*1000000 + tvdebut.tv_usec;
    int64_t microfin = (int64_t)tv.tv_sec*1000000 + tv.tv_usec;
    printf("Temps : %lld (%lld - %lld)\n", microfin-microdebut, microfin, microdebut);
    printf("Temps : %d (%d - %d)\n", end-debut, end, debut);
    printf("Temps : %d (%d - %d)\n", tend-tdebut, tend, tdebut);
    
#define printmacro(x)   printf("%-10s : %x\n", #x, x)    
    printf("Values for open flags :\n");
    printmacro(O_RDONLY);
    printmacro(O_WRONLY);
    printmacro(O_RDWR);
    printmacro(O_CREAT);
    printmacro(O_EXCL);
    printmacro(O_TRUNC);
    printmacro(O_NOCTTY);
    printmacro(O_APPEND);
    printmacro(O_NONBLOCK);
    printmacro(O_SYNC);
    printmacro(SEEK_SET);
    printmacro(SEEK_CUR);
    printmacro(SEEK_END);

    int64_t a = 0x123456789;
    int64_t b = 0x987654321;
    printf("%lld x %lld = %lld\n", a, b, a*b);

    testthread();

    printf("End of program");
    fflush(stdout);
    printf("\n");
    
    return 0;
}
