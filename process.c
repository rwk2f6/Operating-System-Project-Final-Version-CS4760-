#include "config.h"

//Global variables
int shm_id;
int sem_id;
shm_container* shm_ptr = NULL;
int cur_pid;
int cur_index;
char stringBuf[200];
unsigned long starting_nsec, starting_sec, wait_nsec, wait_sec;
int random_resource, random_instance, req_address;

bool checkSysClock();

int main(int argc, char *argv[])
{
    //Initialize local variables
    int index, numOfUsedRes;

    cur_pid = getpid();
    //Seed rand based off of pid so each process is different
    srand(cur_pid * 12);

    //Signal handlers for timer, CTRL-C, and random termination
    signal(SIGALRM, sig_handler); //Catches alarm
    signal(SIGINT, sig_handler); //Catches ctrl-c
    signal(SIGSEGV, sig_handler); //Catches seg fault
    signal(SIGKILL, sig_handler); //Catches a kill signal
    signal(SIGTERM, sig_handler);

    //Initialize semaphore
    if(set_sem() == -1)
    {
        cleanup();
    }

    //Initialize shared memory
    if(set_shm() == -1)
    {
        cleanup();
    }

    //Find the index of the process in the process table using PID
    cur_index = findIndex(cur_pid);

    printf("P%d has been forked\n", cur_index);

    //Update timer before process starts
    sem_wait(CLOCK_SEM);

    starting_nsec = shm_ptr->nsecs;
    starting_sec = shm_ptr->secs;

    sem_signal(CLOCK_SEM);

    while (1)
    {
        //See if process has used all of its memory requests
        if (shm_ptr->procs[cur_index].pageCount == 32)
        {
            printf("All available memory used, dying\n");
            shm_ptr->procs[cur_index].died = true;
            break;
        }

        req_address = ((rand() % 32) * 1024) + (rand() % 1023);

        int temp = rand() % 100;

        //Write is less frequent than READ
        if (temp <= 40)
        {
            printf("P%d is requesting a write\n", cur_index);
            shm_ptr->procs[cur_index].type = WRITE;
        }
        else
        {
            printf("P%d is requesting a read\n", cur_index);
            shm_ptr->procs[cur_index].type = READ;
        }

        //Send request
        shm_ptr->procs[cur_index].waitingFor = req_address;

        sem_wait(cur_index);

        //Wait for request to be completed
        // while (1)
        // {
        //     temp = semctl(sem_id, cur_index, GETVAL, 0);
        //     if (temp == 1)
        //     {
        //         break;
        //     }
        // }

        while (semctl(sem_id, cur_index, GETVAL, 0) != 1);

        //Chance to die
        temp = rand() % 150;
        if (temp = 15)
        {
            shm_ptr->procs[cur_index].died = true;
            break;
        }
    }

    cleanup();
}

bool checkSysClock()
{
    if (shm_ptr->secs > wait_sec)
    {
        return true;
    }
    else if (shm_ptr->secs == wait_sec)
    {
        if (shm_ptr->nsecs > wait_nsec)
        {
            return true;
        }
    }
    return false;
}

void sig_handler()
{
    cleanup();
    exit(0);
}

void cleanup()
{
    shmdt(shm_ptr);
    sem_wait(PROC_CT_SEM);
    exit(0);
}

int findIndex(int pid)
{
    for (int i = 0; i < MAX_PROC; i++)
    {
        if (pid == shm_ptr->running_pids[i])
        {
            return i;
        }
    }
    return -1;
}

void sem_signal(int sem)
{
    //Semaphore signal function
    struct sembuf sema;
    sema.sem_num = sem;
    sema.sem_op = 1;
    sema.sem_flg = 0;
    semop(sem_id, &sema, 1);
}

void sem_wait(int sem)
{
    //Semaphore wait function
    struct sembuf sema;
    sema.sem_num = sem;
    sema.sem_op = -1;
    sema.sem_flg = 0;
    semop(sem_id, &sema, 1);
}

int set_sem()
{
    //Initialize semaphores for resource access
    key_t sem_key = ftok("oss.c", 'a');

    if((sem_id = semget(sem_key, MAX_SEM, 0)) == -1)
    {
        perror("oss.c: Error with semget, exiting\n");
        return -1;
    }
    return 0;
}

int set_shm()
{
    key_t shm_key = ftok("process.c", 'a');

    if((shm_id = shmget(shm_key, (sizeof(frame) * MAX_MEM) + sizeof(shm_container) + (sizeof(proc_stats) * MAX_PROC) + (sizeof(page) * MAX_MEM), IPC_EXCL)) == -1)
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