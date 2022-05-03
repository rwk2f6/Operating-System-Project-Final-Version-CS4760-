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
int last_print = 0;
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
bool forkTime();
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

    //Initialize semaphores for resource access
    key_t sem_key = ftok("oss.c", 'a');

    if((sem_id = semget(sem_key, MAX_SEM, IPC_CREAT | 0666)) == -1)
    {
        perror("oss.c: Error with semget, exiting\n");
        cleanup();
    }

    semctl(sem_id, RESOURCE_SEM, SETVAL, 1);
    semctl(sem_id, CLOCK_SEM, SETVAL, 1);

    writeToLog("Semaphores for resources and clock initialized\n");
    //printf("Semaphores initialized\n");

    //Initialize shared memory
    key_t shmem_key = ftok("process.c", 'a');

    if((shmem_id = shmget(shmem_key, (sizeof(resource_struct) * MAX_PROC) + sizeof(sh_mem_struct), IPC_CREAT | 0666)) == -1)
    {
        perror("oss.c: Error with shmget, exiting\n");
        cleanup();
    }

    //printf("Shared memory gotten, attaching now\n");

    if((sh_mem_ptr = (sh_mem_struct*)shmat(shmem_id, 0, 0)) == (sh_mem_struct*)-1)
    {
        perror("oss.c: Error with shmat, exiting\n");
        cleanup();
    }

    int i, j;
    for (i = 0; i < MAX_RESOURCE; i++)
    {
        sh_mem_ptr->allocated_resources[i].numOfInstances = 1 + (rand() % MAX_INSTANCE);
        sh_mem_ptr->allocated_resources[i].numOfInstancesFree = sh_mem_ptr->allocated_resources[i].numOfInstances;
        sprintf(stringBuf, "%d instances of R%d have been made\n", sh_mem_ptr->allocated_resources[i].numOfInstances, i);
        writeToLog(stringBuf);

        for (j = 0; j < MAX_PROC; j++)
        {
            sh_mem_ptr->allocated_resources[i].request_arr[j] = 0;
            sh_mem_ptr->allocated_resources[i].allocated_arr[j] = 0;
            sh_mem_ptr->allocated_resources[i].release_arr[j] = 0;
        }
    }

    //printf("Shared memory initialized\n");

    for (i = 0; i < MAX_PROC; i++)
    {
        //Fill out PID table
        sh_mem_ptr->running_proc_pid[i] = 0;
    }

    //printf("PID Table filled out\n");

    //Initialize the logical clock
    sh_mem_ptr->sec_timer = 0;
    sh_mem_ptr->nsec_timer = 0;

    writeToLog("Shared memory initialized\n");

    //Print shared resources in their initial state

    //Set alarm for 5 seconds
    alarm(5);
    writeToLog("Alarm has been set for 5 real seconds\n");

    writeToLog("Main oss.c loop starting, logical clock begins\n");
    //Begin main loop
    while(1)
    {
        //Sem wait
        sem_wait(RESOURCE_SEM);

        //Fork processes, but make sure not to create more than 18 at a time, 
        if(numOfProcs == 0)
        {
            //fork
            fork_process();
        }
        else if(numOfProcs < MAX_PROC)
        {
            //Check if there is room in the process table
            if(timePassed())
            {
                //Enough time has passed for another fork
                fork_process();
            }
        }

        //If there are processes in the system
        if(numOfProcs > 0)
        {
            //Check if any processes have finished
            completed_process();
            //Release resources if so
            release_resources();
            //Allocate resources that are available
            allocate_resources();
            deadlock_detection();
            deadlockdet_run++;
        }

        // if (sh_mem_ptr->sec_timer - deadlockDetTimer >= 1)
        // {
        //     deadlock_detection();
        //     deadlockdet_run++;
        //     deadlockDetTimer = sh_mem_ptr->sec_timer;
        // }

        //Sem signal
        sem_signal(RESOURCE_SEM);

        //Update logical clock
        sem_wait(CLOCK_SEM);

        nano_time_pass = 1 + (rand() % 100000000);
        sh_mem_ptr->nsec_timer += nano_time_pass;
        //If too many nanoseconds pass, update seconds
        if (sh_mem_ptr->nsec_timer >= 1000000000)
        {
            sh_mem_ptr->sec_timer += 1;
            sh_mem_ptr->nsec_timer -= 1000000000;
        }

        sem_signal(CLOCK_SEM);

        if (sh_mem_ptr->sec_timer - curResTimer >= 1000)
        {
            curResourceAllo();
            curResTimer = sh_mem_ptr->sec_timer;
        }
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
    curResourceAllo();
    finalReport();
    fputs("OSS is terminating, cleaning up shared memory, semaphores, and child processes\n", logfile_ptr);
    //Output resource report here
    if (logfile_ptr != NULL)
    {
        fclose(logfile_ptr);
    }
    system("killall process");
    sleep(5);
    shmdt(sh_mem_ptr);
    shmctl(shmem_id, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID, NULL);
    exit(0);
}

void sem_signal(int sem_id)
{
    //Semaphore signal function
    struct sembuf sem;
    sem.sem_num = 0;
    sem.sem_op = 1;
    sem.sem_flg = 0;
    semop(sem_id, &sem, 1);
}

void sem_wait(int sem_id)
{
    //Semaphore wait function
    struct sembuf sem;
    sem.sem_num = 0;
    sem.sem_op = -1;
    sem.sem_flg = 0;
    semop(sem_id, &sem, 1);
}

bool timePassed()
{
    //Check if enough time has passed for another fork
    if(sec_until_fork == sh_mem_ptr->sec_timer)
    {
        if(nsec_until_fork <= sh_mem_ptr->nsec_timer)
        {
            //Enough time has passed
            return true;
        }
    }
    else if(sec_until_fork < sh_mem_ptr->sec_timer)
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
        if(sh_mem_ptr->running_proc_pid[index] == 0)
        {
            numOfProcs++;
            numOfForks++;
            pid = fork();

            if(pid != 0)
            {
                sprintf(stringBuf, "P%d with PID: %d was forked at %d : %d\n", index, pid, sh_mem_ptr->sec_timer, sh_mem_ptr->nsec_timer);
                writeToLog(stringBuf);
                sh_mem_ptr->running_proc_pid[index] = pid;
                //Determine next time to fork
                writeToLog("Determining when next process will fork\n");
                nsec_until_fork = sh_mem_ptr->nsec_timer + (rand() % 500000000);
                unsigned long nano_time_fork = nsec_until_fork;
                if (nano_time_fork >= 1000000000)
                {
                    sec_until_fork += 1;
                    nsec_until_fork -= 1000000000;
                }
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
    printf("alarm_handler called, 5 seconds have passed\n");
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

void finalReport()
{
    fputs("\n\nFinal Report:\n\n", logfile_ptr);
    sprintf(stringBuf, "Number of processes that immediately received resources: %d\n", instant_resource_allo);
    fputs(stringBuf, logfile_ptr);
    sprintf(stringBuf, "Number of processes that waited to receive resources: %d\n", waited_for_allo);
    fputs(stringBuf, logfile_ptr);
    sprintf(stringBuf, "Number of processes terminated successfully: %d\n", total_completed_procs);
    fputs(stringBuf, logfile_ptr);
    sprintf(stringBuf, "Number of processes terminated by deadlock detection: %d\n", term_by_deadlock);
    fputs(stringBuf, logfile_ptr);
    sprintf(stringBuf, "Number of time deadlock detection ran: %d\n", deadlockdet_run);
    fputs(stringBuf, logfile_ptr);
    double percent = (double)term_by_deadlock / (double)deadlockdet_run;
    percent *= 100;
    sprintf(stringBuf, "%.8f percent of the time, deadlock detection was run, found a deadlock, and killed a process\n", percent);
    fputs(stringBuf, logfile_ptr);
}





