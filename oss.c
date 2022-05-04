#include "config.h"

//Global variables
int shm_id;
int sem_id;
int proc_count = 0;
char stringBuf[200];
int numOfForks = 0;
int filled_frames = 0;
int frame_num;
int mem_acc = 0;
int page_faults = 0;
int prevPrint = 0;
int log_line_num = 0;
FILE* logfile_ptr = NULL;
shm_container* shm_ptr = NULL;

//Timer variables
unsigned long sec_until_fork = 0;
unsigned long nsec_until_fork = 0;
unsigned long nano_time_pass = 0;

typedef struct {
    int address;
    int proc;
} req_info;

req_info req_queue[MAX_PROC];

void print_stats();
void print_mem();
void alarm_handler();
int nextSwap();
bool inFrameTable();
void findPage();
void check_died();
void printPIDTable();
void fork_proc();
void nsecsToSecs();
void forkClockFix();
bool timePassed();
void init_shm();
void init_sem();
void child_handler(int);

int main(int argc, char *argv[])
{
    //Signal handlers for timer, CTRL-C, and random termination
    signal(SIGALRM, alarm_handler); //Catches alarm
    signal(SIGINT, sig_handler); //Catches ctrl-c
    signal(SIGSEGV, sig_handler); //Catches seg fault
    signal(SIGKILL, sig_handler); //Catches a kill signal

    //Signal handling for child termination to prevent zombies
    struct sigaction siga;
    memset(&siga, 0, sizeof(siga));
    siga.sa_handler = child_handler;
    sigaction(SIGCHLD, &siga, NULL);

    //Open logfile
    logfile_ptr = fopen("logfile", "a");
    writeToLog("Oss.c Logfile:\n\n");
    writeToLog("Logfile created successfully!\n");

    //printf("Logfile created\n");

    //Initialize semaphores
    if(set_sem() == -1)
    {
        cleanup();
    }

    //printf("Semaphores initialized\n");

    //Initialize shared memory
    if(set_shm() == -1)
    {
        cleanup();
    }

    init_sem();
    init_shm();

    writeToLog("Semaphores initialized\n");
    writeToLog("Shared memory initialized\n");

    //Set alarm for 2 seconds
    alarm(2);
    writeToLog("Alarm has been set for 2 real seconds\n");

    writeToLog("Main oss.c loop starting, logical clock begins\n");
    //Begin main loop
    while(1)
    {
        //Print memory allocation every second

        //Check for processes that died

        //Fork if enough time has passed and there aren't 18 processes

        //Check how many frames were filled

        //Check for waiting processes, and try to complete their request. If the request isnt in the page table, FIFO

        //Update clock
    }

    return 0;
}

void writeToLog(char * string)
{
    log_line_num++;
    //Make sure log file isn't 10000 lines long, if it is terminate
    if (log_line_num <= 10000)
    {
        fputs(string, logfile_ptr);
    }
    else   
    {
        printf("Line count is greater than 10,000, exiting...\n");
        fputs("OSS has reached 10,000 lines in the logfile and terminated\n", logfile_ptr);
        cleanup();
    }
}

void cleanup()
{
    printf("Cleanup called\n");
    

    fputs("OSS is terminating, cleaning up shared memory, semaphores, and child processes\n", logfile_ptr);
    //Output resource report here
    if (logfile_ptr != NULL)
    {
        fclose(logfile_ptr);
    }
    system("killall process");
    sleep(5);
    shmdt(shm_ptr);
    shmctl(shm_id, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID, NULL);
    exit(0);
}

int set_sem()
{
    //Initialize semaphores for resource access
    key_t sem_key = ftok("oss.c", 'a');

    if((sem_id = semget(sem_key, MAX_SEM, IPC_CREAT | 0666)) == -1)
    {
        perror("oss.c: Error with semget, exiting\n");
        return -1;
    }
    return 0;
}

int set_shm()
{
    key_t shm_key = ftok("process.c", 'a');

    if((shm_id = shmget(shm_key, (sizeof(frame) * MAX_MEM) + sizeof(shm_container) + (sizeof(proc_stats) * MAX_PROC) + (sizeof(page) * MAX_MEM), IPC_CREAT | 0666)) == -1)
    {
        perror("oss.c: Error with shmget, exiting\n");
        return -1;
    }

    //printf("Shared memory gotten, attaching now\n");

    if((shm_ptr = (shm_container*)shmat(shm_id, 0, 0)) == (shm_container*)-1)
    {
        perror("oss.c: Error with shmat, exiting\n");
        return -1;
    }

    return 0;
}

void init_shm()
{
    //Initialize shared memory contrainer
    int proc_i, page_i, frame_i;

    //Start w process control block
    for(proc_i = 0; proc_i < MAX_PROC; proc_i++)
    {
        shm_ptr->running_pids[proc_i] = 0;
        shm_ptr->procs[proc_i].died = false;
        shm_ptr->procs[proc_i].page_count = 0;
        shm_ptr->procs[proc_i].page_index = 0;
        shm_ptr->procs[proc_i].waitingFor = 0;
        for(page_i = 0; page_i < MAX_FRAMES; page_i++)
        {
            shm_ptr->procs[proc_i].pageTable[page_i].frame_num = -1;
            shm_ptr->procs[proc_i].pageTable[page_i].address = 0;
        }
    }
    //Initialize frameTable
    for(frame_i = 0; frame_i < MAX_MEM; frame_i++)
    {
        shm_ptr->frames[frame_i].proc_num = 0;
        shm_ptr->frames[frame_i].dirtyBit = 0;
        shm_ptr->frames[frame_i].address = 0;
    }

    //Initialize clock and frame table indexes
    shm_ptr->nsecs = 0;
    shm_ptr->secs = 0;
    shm_ptr->nextEntry = 0;
    shm_ptr->lookingFor = 0;
}

void init_sem()
{
    //Initialize semaphores for the clock and process count
    semctl(sem_id, PROC_CT_SEM, SETVAL, 1);
    semctl(sem_id, CLOCK_SEM, SETVAL, 1);

    //Initialize wait flags
    for(int index = 0; index < MAX_PROC; index++)
    {
        semctl(sem_id, index, SETVAL, 1);
    }
}

void sem_signal(int sem)
{
    //Semaphore signal function
    struct sembuf sema;
    sema.sem_num = sem;
    sem.sem_op = 1;
    sem.sem_flg = 0;
    semop(sem_id, &sema, 1);
}

void sem_wait(int sem)
{
    //Semaphore wait function
    struct sembuf sema;
    sema.sem_num = sem;
    sem.sem_op = -1;
    sem.sem_flg = 0;
    semop(sem_id, &sema, 1);
}

void forkClockFix()
{
    unsigned long nano_time_fork = nsec_until_fork;
    if (nano_time_fork >= 1000000000)  
    {
        sec_until_fork += 1;
        nsec_until_fork -= 1000000000;
    }
}

bool timePassed()
{
    //Check if enough time has passed for another fork
    if(sec_until_fork == shm_ptr->secs)
    {
        if(nsec_until_fork <= shm_ptr->nsecs)
        {
            //Enough time has passed
            return true;
        }
    }
    else if(sec_until_fork < shm_ptr->secs)
    {
        //Enough time has passed
        return true;
    }
    else
    {
        //Not enough time has passed
        return false;
    }
}

void nsecsToSecs()
{
    //When there are enough nanoseconds, convert to seconds
    unsigned long nano_time = shm_ptr->nsecs;
    if (nano_time >= 1000000000)
    {
        shm_ptr->secs += 1;
        shm_ptr->nsecs -= 1000000000;
    }
}

void fork_proc()
{
    //Keep track of total processes and terminate if it reaches 40
    if (numOfForks >= 100)
    {
        printf("oss.c: Terminating as 40 total children have been forked\n");
        writeToLog("oss.c: Terminating as 40 total children have been forked\n");
        cleanup();
    }

    int index, pid;
    for (index = 0; index < MAX_PROC; index++)
    {
        if(shm_ptr->running_pids[index] == 0)
        {
            sem_signal(PROC_CT_SEM);
            numOfForks++;
            pid = fork();

            if(pid != 0)
            {
                sprintf(stringBuf, "P%d with PID: %d was forked at %d : %d\n", index, pid, shm_ptr->secs, shm_ptr->nsecs);
                writeToLog(stringBuf);
                shm_ptr->running_pids[index] = pid;
                //Determine next time to fork
                writeToLog("Determining when next process will fork\n");
                nsec_until_fork = shm_ptr->nsecs + (rand() % 500000000);
                forkClockFix();
                return;
            }
            else
            {
                execl("./process", "./process", NULL);
            }
        }
    }
}

void sig_handler()
{
    //Called by signal functions to cleanup child processes, shared memory, and semaphores
    printf("Sig_Handler called\n");
    cleanup();
}

void alarm_handler()
{
    printf("alarm_handler called, 2 seconds have passed\n");
    cleanup();
}

void child_handler(int sig)
{
    pid_t child_pid;
    while ((child_pid = waitpid((pid_t)(-1), 0, WNOHANG)) > 0)
    {
        //Wait to prevent zombie processes
    }
}

void print_stats()
{

}

void print_mem()
{

}

int nextSwap()
{

}

bool inFrameTable()
{

}

void findPage()
{

}

void check_died()
{

}

void printPIDTable()
{
    
}