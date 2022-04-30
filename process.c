#include "config.h"

//Global variables
int shmem_id;
int sem_id;
sh_mem_struct* sh_mem_ptr = NULL;
int cur_pid;
int cur_index;
char stringBuf[200];
unsigned long starting_nsec, starting_sec, wait_nsec, wait_sec;
int random_resource, random_instance;

int main(int argc, char *argv[])
{
    //Initialize local variables
    int index;

    cur_pid = getpid();
    //Seed rand based off of pid so each process is different
    srand(cur_pid * 12);

    //Signal handlers for timer, CTRL-C, and random termination
    signal(SIGALRM, sig_handler); //Catches alarm
    signal(SIGINT, sig_handler); //Catches ctrl-c
    signal(SIGSEGV, sig_handler); //Catches seg fault
    signal(SIGKILL, sig_handler); //Catches a kill signal

    //Initialize semaphore for resource access
    key_t sem_key = ftok("oss.c", 'a');

    if((sem_id = semget(sem_key, MAX_SEM, 0)) == -1)
    {
        perror("process.c: semget failed!\n");
        cleanup();
        exit(0);
    }

    //Initialize shared memory
    key_t shmem_key = ftok("process.c", 'a');

    if((shmem_id = shmget(shmem_key, (sizeof(resource_struct) * MAX_PROC) + sizeof(sh_mem_struct), IPC_EXCL)) == -1)
    {
        perror("process.c: shmget failed!\n");
        cleanup();
        exit(0);
    }

    if((sh_mem_ptr = (sh_mem_struct*)shmat(shmem_id, 0, 0)) == (sh_mem_struct*)-1)
    {
        perror("process.c: shmat failed!\n");
        cleanup();
        exit(0);
    }

    //Find the index of the process in the process table using PID
    cur_index = findIndex(cur_pid);

    //Initialize elements inside of shared memory
    sh_mem_ptr->needs[cur_index] = -1;
    sh_mem_ptr->blocked[cur_index] = false;
    sh_mem_ptr->complete[cur_index] = ND;
    sh_mem_ptr->sleeping_proc_arr[cur_index] = 0;

    for (index = 0; index < MAX_RESOURCE; index++)
    {
        sh_mem_ptr->allocated_resources[index].allocated_arr[cur_index] = 0;
        sh_mem_ptr->allocated_resources[index].request_arr[cur_index] = 0;
        sh_mem_ptr->allocated_resources[index].release_arr[cur_index] = 0; 
    }

    //Update timer before process starts
    sem_wait(CLOCK_SEM);

    starting_nsec = sh_mem_ptr->nsec_timer;
    starting_sec = sh_mem_ptr->sec_timer;

    sem_signal(CLOCK_SEM);

    wait_nsec = starting_nsec;
    wait_sec = starting_sec;

    //Set the random time to wait
    int random_time = rand() % 250000000;
    wait_nsec += random_time;
    if (wait_nsec >= 1000000000)
    {
        wait_nsec -= 1000000000;
        wait_sec += 1;
    }

    //Make sure process wasn't set to be killed
    while(sh_mem_ptr->complete[cur_index] != OSS_KILL)
    {
        //Wait until enough time has passed
        while(1)
        {
            if (checkSysClock())
            {
                break;
            }
        }
        //Process now checks to see if it got its resources
        if (sh_mem_ptr->sleeping_proc_arr[cur_index] == 1 || sh_mem_ptr->blocked[cur_index] == true)
        {
            //Do nothing, as these processes are waiting
        }
        else
        {
            //The process isn't waiting
            //Make sure the process has been running for at least a second
            if (sh_mem_ptr->sec_timer - starting_sec > 1)
            {
                if (sh_mem_ptr->nsec_timer > starting_nsec )
                {
                    if (rand() % 100 == 22)
                    {
                        //Small chance to randomly terminate early
                        sem_wait(RESOURCE_SEM);
                        sh_mem_ptr->complete[cur_index] = EARLY_TERM;
                        sem_signal(RESOURCE_SEM);
                        break;
                    }
                }
            }
            //Didn't terminate early, so pick a random resource to request or release
            random_resource = rand() % MAX_RESOURCE;
            sem_wait(RESOURCE_SEM);

            if (sh_mem_ptr->allocated_resources[random_resource].allocated_arr[cur_index] > 0)
            {
                //Has resources allocated, 50% chance to release
                if (rand() % 100 < 50)
                {
                    //printf("P%d is releasing R%d\n", cur_index, random_resource);
                    sh_mem_ptr->allocated_resources[random_resource].release_arr[cur_index] = sh_mem_ptr->allocated_resources[random_resource].allocated_arr[cur_index];
                    sh_mem_ptr->blocked[cur_index] = true;
                }
            }
            else
            {
                //No resources allocated
                if (rand() % 100 < 50)
                {
                    random_instance = 1 + (rand() % sh_mem_ptr->allocated_resources[random_resource].numOfInstances);
                    if (random_instance > 0)
                    {
                        //printf("P%d is requesting to get %d instances of R%d\n", cur_index, random_instance, random_resource);
                        sh_mem_ptr->allocated_resources[random_resource].request_arr[cur_index] = random_instance;
                        sh_mem_ptr->blocked[cur_index] = true;
                    }
                }
            }
            sem_signal(RESOURCE_SEM);

            //Make a wait time for next request/release
            sem_wait(CLOCK_SEM);

            random_time = rand() % 250000000;
            wait_nsec = sh_mem_ptr->nsec_timer;
            wait_nsec += random_time;
            if (wait_nsec >= 1000000000)
            {
                wait_nsec -= 1000000000;
                wait_sec += 1;
            }

            sem_signal(CLOCK_SEM);

        }
    }

    cleanup();
    return 0;
}

bool checkSysClock()
{
    if (sh_mem_ptr->sec_timer > wait_sec)
    {
        return true;
    }
    else if (sh_mem_ptr->sec_timer == wait_sec)
    {
        if (sh_mem_ptr->nsec_timer > wait_nsec)
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
    shmdt(sh_mem_ptr);
}

int findIndex(int pid)
{
    for (int i = 0; i < MAX_PROC; i++)
    {
        if (pid == sh_mem_ptr->running_proc_pid[i])
        {
            return i;
        }
    }
    return -1;
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