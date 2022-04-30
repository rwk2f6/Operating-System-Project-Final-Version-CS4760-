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
#define MAX_RESOURCE 20
#define MAX_INSTANCE 10
#define MAX_SEM 2
#define RESOURCE_SEM 0
#define CLOCK_SEM 1

//States the processes can terminate to
#define ND 0
#define OSS_KILL 1
#define EARLY_TERM 2

//Structures for shared memory and resources
typedef struct {

    int numOfInstances;
    int numOfInstancesFree;

    int request_arr[MAX_PROC];
    int allocated_arr[MAX_PROC];
    int release_arr[MAX_PROC];

} resource_struct;

typedef struct {

    unsigned int sec_timer;
    unsigned int nsec_timer;

    resource_struct allocated_resources[MAX_RESOURCE];

    int running_proc_pid[MAX_PROC];

    int sleeping_proc_arr[MAX_PROC];
    int complete[MAX_PROC];
    bool blocked[MAX_PROC];
    int needs[MAX_PROC];

} sh_mem_struct;

void writeToLog(char *);
void cleanup();
void sem_signal(int);
void sem_wait(int);
bool timePassed();
void fork_process();
void sig_handler();
bool checkSysClock();
int findIndex(int);
void allocate_resources();
void completed_process();
void release_resources();
void deadlock_detection();
void stopZombies(int);
void curResourceAllo();
void finalReport();
void alarm_handler();