#include "config.h"

//Global variables
int shm_id;
int sem_id;
int proc_count = 0;
char stringBuf[200];
int numOfForks = 0;
int usedFrames = 0;
int frame_num;
int mem_access = 0;
int page_faults = 0;
int prevPrint = 0;
int log_line_num = 0;
unsigned int nextEntry = 0;
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
bool inFrameTable(int);
void findPage();
void check_died();
void checkForDeadlock();
void forkNewProc();
void nsecsToSecs();
void forkClockFix();
bool timePassed();
void init_shm();
void init_sem();
void child_handler(int);
int getNextFrameLocation(unsigned int);

int main(int argc, char *argv[])
{
    srand(time(NULL));

    unsigned int frameLoc;

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
    logfile_ptr = fopen("logfile.txt", "a");
    writeToLog("Oss.c Logfile:\n\n");
    writeToLog("Logfile created successfully!\n");

    printf("Logfile created\n");

    //Initialize semaphores
    if(set_sem() == -1)
    {
        cleanup();
    }

    //Initialize shared memory
    if(set_shm() == -1)
    {
        cleanup();
    }

    init_sem();
    init_shm();

    printf("Semaphores & shared memory initialized\n");

    writeToLog("Semaphores initialized\n");
    writeToLog("Shared memory initialized\n");

    //Set alarm for 2 seconds
    alarm(2);
    writeToLog("Alarm has been set for 2 real seconds\n");
    printf("Alarm set for 2 seconds\n");

    writeToLog("Main oss.c loop starting, logical clock begins\n");
    printf("Main loop begins\n");

    //Begin main loop
    while(1)
    {
        //Print memory allocation every second
        if (shm_ptr->secs > prevPrint)
        {
            printf("Printing memory layout\n");
            prevPrint = shm_ptr->secs;
            print_mem();
        }

        //Check for processes that died
        check_died();

        //Check for a deadlock
        checkForDeadlock();

        //Fork if enough time has passed and there aren't 18 processes
        if (sec_until_fork == 0 && nsec_until_fork == 0)
        {
            forkNewProc();
        }
        else
        {
            if (timePassed)
            {
                int semVarCount = semctl(sem_id, PROC_CT_SEM, GETVAL, 0);

                if (semVarCount < MAX_PROC)
                {
                    forkNewProc();
                }
            }
        }

        //Check how many frames were filled
        findPage();

        //Check for waiting processes, and try to complete their request. If the request isnt in the page table, FIFO
        for (int i = 0; i < MAX_PROC; i++)
        {
            int tempVar = semctl(sem_id, i, GETVAL, 0);
            //Found a waiting process
            if (tempVar == 0)
            {
                if (usedFrames < MAX_MEM)
                {
                    //There are empty frames
                    if (shm_ptr->procs[i].type == READ)
                    {
                        printf("Read process request\n");
                        sprintf(stringBuf, "P%d requesting read of address %d at time %d : %d\n", i, shm_ptr->procs[i].waitingFor, shm_ptr->secs, shm_ptr->nsecs);
                        writeToLog(stringBuf);
                        if (inFrameTable(shm_ptr->procs[i].waitingFor))
                        {
                            printf("Already in frame table\n");
                            //Already in frame table
                            shm_ptr->nsecs += 14000000;
                            nsecsToSecs();
                            sprintf(stringBuf, "Address %d in frame table, giving data to P%d at time %d : %d\n", shm_ptr->procs[i].waitingFor, i, shm_ptr->secs, shm_ptr->nsecs);
                            writeToLog(stringBuf);
                            mem_access++;
                            sem_signal(i);
                        }
                        else
                        {
                            printf("Not in the frame table\n");
                            //No in frame table
                            shm_ptr->nsecs += 14000000;
                            nsecsToSecs();
                            sprintf(stringBuf, "Address %d is not in frame table, pagefault!\n", shm_ptr->procs[i].waitingFor);
                            writeToLog(stringBuf);
                            page_faults++;
                            frameLoc = nextEntry;
                            //Find next empty frame, FIFO
                            frameLoc = getNextFrameLocation(frameLoc);
                            //Insert
                            printf("SETTING FRAME ADDRESS TO %d: Frame Location: %d P%d\n", shm_ptr->procs[i].waitingFor, frameLoc, i);
                            shm_ptr->frames[frameLoc].address = shm_ptr->procs[i].waitingFor;
                            shm_ptr->frames[frameLoc].dirtyBit = 0;
                            shm_ptr->frames[frameLoc].proc_num = i;
                            nextEntry = (frameLoc + 1) % MAX_MEM;
                            sprintf(stringBuf, "Address %d in frame %d, giving data to P%d at time %d : %d\n", shm_ptr->procs[i].waitingFor, frameLoc, i, shm_ptr->secs, shm_ptr->nsecs);
                            writeToLog(stringBuf);
                            shm_ptr->procs[i].pageTable[shm_ptr->procs[i].pageIndex].address = shm_ptr->procs[i].waitingFor;
                            shm_ptr->procs[i].pageTable[shm_ptr->procs[i].pageIndex].frame_num = frameLoc;
                            shm_ptr->procs[i].pageIndex += 1;
                            shm_ptr->procs[i].pageCount++;
                            shm_ptr->nsecs += 14000000;
                            usedFrames++;
                            nsecsToSecs();
                            sem_signal(i);
                        }
                    }
                    else
                    {
                        printf("Write process request\n");
                        //Write type
                        sprintf(stringBuf, "P%d requesting write of address %d at time %d : %d\n", i, shm_ptr->procs[i].waitingFor, shm_ptr->secs, shm_ptr->nsecs);
                        writeToLog(stringBuf);

                        if (inFrameTable(shm_ptr->procs[i].waitingFor))
                        {
                            printf("Already in frame table\n");
                            //Check if address is already in frame table
                            shm_ptr->nsecs += 14000000;
                            nsecsToSecs();
                            sprintf(stringBuf, "Address %d in frame table, writing data to frame at time %d : %d\n", shm_ptr->procs[i].waitingFor, shm_ptr->secs, shm_ptr->nsecs);
                            writeToLog(stringBuf);
                            mem_access++;
                            sem_signal(i);
                        }
                        else
                        {
                            printf("Not in frame table\n");
                            //Not in frame table
                            shm_ptr->nsecs += 14000000;
                            nsecsToSecs();
                            sprintf(stringBuf, "Address %d is not in a frame, pagefault!\n", shm_ptr->procs[i].waitingFor);
                            writeToLog(stringBuf);
                            page_faults++;
                            frameLoc = nextEntry;
                            //Find a blank frame
                            frameLoc = getNextFrameLocation(frameLoc);
                            //Insert
                            shm_ptr->frames[frameLoc].dirtyBit = 1;
                            shm_ptr->frames[frameLoc].proc_num = i;
                            printf("SETTING FRAME ADDRESS TO %d: Frame Location: %d P%d\n", shm_ptr->procs[i].waitingFor, frameLoc, i);
                            shm_ptr->frames[frameLoc].address = shm_ptr->procs[i].waitingFor;
                            nextEntry = (frameLoc + 1) % MAX_MEM;
                            sprintf(stringBuf, "Address %d in frame %d, writing data to frame at time %d : %d\n", shm_ptr->procs[i].waitingFor, frameLoc, shm_ptr->secs, shm_ptr->nsecs);
                            writeToLog(stringBuf);
                            mem_access++;
                            shm_ptr->procs[i].pageTable[shm_ptr->procs[i].pageIndex].address = shm_ptr->procs[i].waitingFor;
                            shm_ptr->procs[i].pageTable[shm_ptr->procs[i].pageIndex].frame_num = frameLoc;
                            shm_ptr->procs[i].pageIndex += 1;
                            shm_ptr->procs[i].pageCount++;
                            shm_ptr->nsecs += 14000000;
                            usedFrames++;
                            nsecsToSecs();
                            sem_signal(i);
                        }
                    }
                }
                else
                {
                    //Find the next frame that is replacible, ie dirtyBity = 0
                    if (shm_ptr->procs[i].type == READ)
                    {
                        printf("Read request looking for frame to replace\n");
                        sprintf(stringBuf, "P%d requesting read of address %d at time %d : %d\n", i, shm_ptr->procs[i].waitingFor, shm_ptr->secs, shm_ptr->nsecs);
                        writeToLog(stringBuf);
                        if (inFrameTable(shm_ptr->procs[i].waitingFor))
                        {
                            printf("Already in frame table\n");
                            //Already in frame table
                            shm_ptr->nsecs += 14000000;
                            nsecsToSecs();
                            sprintf(stringBuf, "Address %d in frame table, giving data to P%d at time %d : %d\n", shm_ptr->procs[i].waitingFor, i, shm_ptr->secs, shm_ptr->nsecs);
                            writeToLog(stringBuf);
                            mem_access++;
                            sem_signal(i);
                        }
                        else
                        {
                            printf("Not in frame table\n");
                            //No in frame table
                            shm_ptr->nsecs += 14000000;
                            nsecsToSecs();
                            sprintf(stringBuf, "Address %d is not in frame table, pagefault!\n", shm_ptr->procs[i].waitingFor);
                            writeToLog(stringBuf);
                            page_faults++;
                            if ((frameLoc = nextSwap()) != -1)
                            {
                                printf("REPLACING A FRAME\n");
                                //There is a frame that can be replaced
                                shm_ptr->nsecs += 14000000;
                                nsecsToSecs();
                                shm_ptr->frames[frameLoc].proc_num = i;
                                shm_ptr->frames[frameLoc].dirtyBit = 1;
                                printf("SETTING FRAME ADDRESS TO %d: Frame Location: %d P%d\n", shm_ptr->procs[i].waitingFor, frameLoc, i);
                                shm_ptr->frames[frameLoc].address = shm_ptr->procs[i].waitingFor;
                                sprintf(stringBuf, "Clearing frame %d and swapping in P%d at address %d\n", frameLoc, i, shm_ptr->procs[i].waitingFor);
                                writeToLog(stringBuf);
                                shm_ptr->nsecs += 14000000;
                                nsecsToSecs();
                                sprintf(stringBuf, "Address %d in frame, giving data to P%d at time %d : %d\n", shm_ptr->procs[i].waitingFor, i, shm_ptr->secs, shm_ptr->nsecs);
                                writeToLog(stringBuf);
                                mem_access++;
                                shm_ptr->procs[i].pageTable[shm_ptr->procs[i].pageIndex].frame_num = frameLoc;
                                shm_ptr->procs[i].pageTable[shm_ptr->procs[i].pageIndex].address = shm_ptr->procs[i].waitingFor;
                                shm_ptr->procs[i].pageIndex += 1;
                                shm_ptr->procs[i].pageCount++;
                                sem_signal(i);
                            }
                        }
                    }
                    else
                    {
                        printf("Write request looking for frame to replace\n");
                        //Write
                        sprintf(stringBuf, "P%d requesting write of address %d at time %d : %d\n", i, shm_ptr->procs[i].waitingFor, shm_ptr->secs, shm_ptr->nsecs);
                        writeToLog(stringBuf);

                        if (inFrameTable(shm_ptr->procs[i].waitingFor))
                        {
                            printf("Already in frame table\n");
                            //Check if address is already in frame table
                            shm_ptr->nsecs += 14000000;
                            nsecsToSecs();
                            sprintf(stringBuf, "Address %d in frame table, writing data to frame at time %d : %d\n", shm_ptr->procs[i].waitingFor, shm_ptr->secs, shm_ptr->nsecs);
                            writeToLog(stringBuf);
                            mem_access++;
                            sem_signal(i);
                        }
                        else
                        {
                            printf("Not in frame table\n");
                            //Not in frame table
                            shm_ptr->nsecs += 14000000;
                            nsecsToSecs();
                            sprintf(stringBuf, "Address %d is not in a frame, pagefault!\n", shm_ptr->procs[i].waitingFor);
                            writeToLog(stringBuf);
                            page_faults++;
                            if ((frameLoc = nextSwap()) != -1)
                            {
                                printf("REPLACING A FRAME\n");
                                //There is a frame that can be replaced
                                shm_ptr->nsecs += 14000000;
                                nsecsToSecs();
                                shm_ptr->frames[frameLoc].proc_num = i;
                                shm_ptr->frames[frameLoc].dirtyBit = 1;
                                printf("SETTING FRAME ADDRESS TO %d: Frame Location: %d P%d\n", shm_ptr->procs[i].waitingFor, frameLoc, i);
                                shm_ptr->frames[frameLoc].address = shm_ptr->procs[i].waitingFor;
                                sprintf(stringBuf, "Clearing frame %d and swapping in P%d at address %d\n", frameLoc, i, shm_ptr->procs[i].waitingFor);
                                writeToLog(stringBuf);
                                shm_ptr->nsecs += 14000000;
                                nsecsToSecs();
                                sprintf(stringBuf, "Address %d in frame, giving data to P%d at time %d : %d\n", shm_ptr->procs[i].waitingFor, i, shm_ptr->secs, shm_ptr->nsecs);
                                writeToLog(stringBuf);
                                mem_access++;
                                shm_ptr->procs[i].pageTable[shm_ptr->procs[i].pageIndex].frame_num = frameLoc;
                                shm_ptr->procs[i].pageTable[shm_ptr->procs[i].pageIndex].address = shm_ptr->procs[i].waitingFor;
                                shm_ptr->procs[i].pageIndex += 1;
                                shm_ptr->procs[i].pageCount++;
                                sem_signal(i);
                            }
                        }
                    }
                }
            }
            else
            {
                //Do nothing, wait for a request to be put in
            }
        }
        //Update clock
        sem_wait(CLOCK_SEM);
        printf("Updating logical clock\n");
        nano_time_pass = 1 + (rand() % 100000000);
        shm_ptr->nsecs += nano_time_pass;
        nsecsToSecs();
        sem_signal(CLOCK_SEM);
    }

    return 0;
}

void writeToLog(char * string)
{
    //log_line_num never increments, so log_line_num is always <= 10000
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
    print_mem();
    print_stats();
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

int getNextFrameLocation(unsigned int location)
{
    printf("getNextFrameLocation called\n");
    int storedOffLocation = location;

    while(1)
    {
        if (shm_ptr->frames[location].address == 0)
        {
            break;
        }
        else
        {
            location = (location + 1) % MAX_MEM;
            return location;
        }
    }
    //Incase pageCount doesn't increment properly, prevents an infinite loop where it can't find an open frame
    if (location == storedOffLocation && shm_ptr->frames[location].address != 0)
    {
        cleanup();
    }

    return location; 
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
        shm_ptr->procs[proc_i].pageCount = 0;
        shm_ptr->procs[proc_i].pageIndex = 0;
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
    printf("timePassed called, seeing if it is time to fork\n");
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

void forkNewProc()
{
    //Keep track of total processes and terminate if it reaches 100
    if (numOfForks >= 100)
    {
        printf("oss.c: Terminating as 100 total children have been forked\n");
        writeToLog("oss.c: Terminating as 100 total children have been forked\n");
        cleanup();
    }

    int index, pid;
    for (index = 0; index < MAX_PROC; index++)
    {
        if(shm_ptr->running_pids[index] == 0)
        {
            printf("Forking a new process\n");
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
    writeToLog("FINAL STATS:\n");
    sprintf(stringBuf, "Number of memory accesses per second: %.5f\n", (double)mem_access / (double)shm_ptr->secs);
    writeToLog(stringBuf);
    sprintf(stringBuf, "Number of page faults per memory access: %.5f\n", (double)page_faults / (double)mem_access);
    writeToLog(stringBuf);
    sprintf(stringBuf, "Average memory access speed: %.5f\n", (double)shm_ptr->secs / (double)mem_access);
    writeToLog(stringBuf);
}

void print_mem()
{
    printf("print_mem called\n");
    sprintf(stringBuf, "Current memory layout at time %d : %d is:\n", shm_ptr->secs, shm_ptr->nsecs);
    writeToLog(stringBuf);
    writeToLog("         Occupied    DirtyBit\n");
    for (int i = 0; i < MAX_MEM; i++)
    {
        //sprintf(stringBuf, "Frame %d: %d    %d\n", i, shm_ptr->frames[i].address, shm_ptr->frames[i].dirtyBit);
        sprintf(stringBuf, "Frame %d: ", i);
        writeToLog(stringBuf);
        if (shm_ptr->frames[i].address == 0)
        {
            writeToLog("No          ");
        }
        else
        {
            writeToLog("Yes         ");
        }

        if (shm_ptr->frames[i].dirtyBit == true)
        {
            writeToLog("1\n");
        }
        else
        {
            writeToLog("0\n");
        }
    }
    writeToLog("\n");
}

int nextSwap()
{
    printf("NextSwap called\n");
    int tempVar;
    tempVar = shm_ptr->lookingFor;

    for (int index = 0; index < MAX_MEM; index++)
    {
        if (shm_ptr->frames[tempVar].dirtyBit == 0)
        {
            shm_ptr->lookingFor = (tempVar + 1) % MAX_MEM;
            return tempVar;
        }
        tempVar = (tempVar + 1) % MAX_MEM;
    }
    //If no frames are swappable, return -1
    return -1;
}

bool inFrameTable(int frame)
{
    printf("inFrameTable called\n");
    //See if a frame is in a frametable
    for (int i = 0; i < MAX_MEM; i++)
    {
        if (shm_ptr->frames[i].address == frame)
        {
            frame_num = i;
            shm_ptr->frames[frame_num].dirtyBit = 1;
            sprintf(stringBuf, "Dirty bit of frame %d set at time %d : %d, adding additional time to the clock", frame_num, shm_ptr->secs, shm_ptr->nsecs);
            writeToLog(stringBuf);
            shm_ptr->nsecs += 107;
            nsecsToSecs();
            return true;
        }
    }
    return false;
}

void findPage()
{
    printf("findPage called\n");
    int tempVar;
    usedFrames = 0;
    tempVar = shm_ptr->lookingFor;

    for (int i = 0; i < MAX_PROC; i++)
    {
        usedFrames += shm_ptr->procs[i].pageCount;
    }

}

void check_died()
{
    printf("check_died called\n");
    for (int a = 0; a < MAX_PROC; a++)
    {
        if (shm_ptr->procs[a].died == true)
        {
            shm_ptr->procs[a].pageCount = 0;
            shm_ptr->procs[a].pageIndex = 0;
            shm_ptr->procs[a].waitingFor = 0;
            shm_ptr->procs[a].type = 0;
            shm_ptr->procs[a].died = false;
            semctl(sem_id, a, SETVAL, 1);
            //Clean frame table
            for (int b = 0; b < MAX_FRAMES; b++)
            {
                if (shm_ptr->procs[a].pageTable[b].frame_num != -1)
                {
                    shm_ptr->frames[shm_ptr->procs[a].pageTable[b].frame_num].proc_num = 0;
                    shm_ptr->frames[shm_ptr->procs[a].pageTable[b].frame_num].dirtyBit = 0;
                    shm_ptr->frames[shm_ptr->procs[a].pageTable[b].frame_num].address = 0;
                }
            }
            shm_ptr->running_pids[a] = 0;
        }
    }
}

void checkForDeadlock()
{
    printf("checkForDeadlock called\n");
    //Make sure all the processes aren't waiting
    unsigned int waitingCount = 0;
    for (int a = 0; a < MAX_PROC; a++)
    {
        if (semctl(sem_id, a, GETVAL, 0) == 0)
        {
            waitingCount++;
        }
    }

    //If the waitingCount >= 18, then all the processes are waiting and a deadlock has occurred
    if (waitingCount >= 18)
    {
        printf("18 waiting processes, calling cleanup\n");
        cleanup();
    }
}