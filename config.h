#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <time.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <math.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/ipc.h>
#include <errno.h> 
#include <sys/types.h>
#include <sys/sem.h>

#define MAX_PROC 18
#define MAX_FRAMES 32
#define MAX_FORKS 100
#define MAX_MEM 256
#define MAX_SEM 20
#define PROC_CT_SEM 19
#define CLOCK_SEM 18

//How frames can be interacted with
#define READ 1
#define WRITE 2

//Structure for frame
typedef struct {

    int address;
    char dirtyBit;
    int proc_num;

} frame;

typedef struct {

   int address;
   int frame_num;

} page;

typedef struct {

   int waitingFor;
   int type;
   bool died;
   page pageTable[MAX_FRAMES];
   int pageIndex;
   int pageCount;

} proc_stats;

typedef struct {

   int running_pids[MAX_PROC];

   proc_stats procs[MAX_PROC];
   frame frames[MAX_MEM];

   int lookingFor;
   int nextEntry;

   unsigned int secs;
   unsigned int nsecs;

} shm_container;

//Function prototypes
void writeToLog(char *);
void cleanup();
void sig_handler();
int set_shm();
int set_sem();
void sem_signal(int);
void sem_wait(int);
int findIndex(int);