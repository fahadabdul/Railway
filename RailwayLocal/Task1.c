/*
Name - Fahad Abdullah
Roll No - 2K21/SWE/09
Task 1
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#define cancelled -1
#define reserved 1
#define waitlisted 0
#define P(s) semop(s, &pop, 1)
#define V(s) semop(s, &vop, 1)

struct reservation
{
    int pnr;
    char passengername[40], age, sex[2];
    char class[4]; // AC2, AC3, or SC
    int status;    // waitlisted or confirmed
};
struct train
{
    int trainid;
    int AC2, AC3, SC; // No. of available berths
    int rlstsize;
    struct reservation rlist[1000];
};
int rsem[3], wsem[3];
int readcntacess[3], wrtcntacess[3], mutex[3];
struct sembuf pop, vop;
int readcntid[3], *readcnt[3];
int writecntid[3], *writecnt[3];

void get_read_lock(int train_id) // Reader function to allow read lock
{
    P(mutex[train_id]);
    P(rsem[train_id]);
    P(readcntacess[train_id]);
    *readcnt[train_id]++;
    if (*readcnt[train_id] == 1)
        P(wsem[train_id]);
    V(readcntacess[train_id]);
    V(rsem[train_id]);
    V(mutex[train_id]);
}
void release_read_lock(int train_id) // Reader function to release read lock
{
    P(readcntacess[train_id]);
    *readcnt[train_id]--;
    if (*readcnt[train_id] == 0)
        V(wsem[train_id]);
    V(readcntacess[train_id]);
}
void get_write_lock(int train_id) // Writer function to get write lock
{
    P(wrtcntacess[train_id]);
    *writecnt[train_id]++;
    if (*writecnt[train_id] == 1)
        P(rsem[train_id]);
    V(wrtcntacess[train_id]);
    P(wsem[train_id]);
}
void release_write_lock(int train_id) // Writer function to release write lock
{
    V(wsem[train_id]);
    P(wrtcntacess[train_id]);
    *writecnt[train_id]--;
    if (*writecnt[train_id] == 0)
        V(rsem[train_id]);
    V(wrtcntacess[train_id]);
}
int main(int argc, char *argv[])
{
    int pid;
    struct train *raildata[3], *shmptr;
    int i;
    int shmid;
    shmid = shmget(IPC_PRIVATE, 3 * sizeof(struct train), 0777 | IPC_CREAT); // 3 blocks of struct train to get access to memory
    shmptr = (struct train *)shmat(shmid, 0, 0);                             // get pointer to the block
    for (i = 0; i < 3; i++)                                                  // Initialize the pointers
    {
        raildata[i] = shmptr + i;
        raildata[i]->trainid = i;
        raildata[i]->AC2 = 10;
        raildata[i]->AC3 = 10;
        raildata[i]->SC = 10;
        raildata[i]->rlstsize = 0;
    }

    for (i = 0; i < 3; i++) // Initialize the semaphores
    {
        rsem[i] = semget(IPC_PRIVATE, 1, 0777 | IPC_CREAT);
        wsem[i] = semget(IPC_PRIVATE, 1, 0777 | IPC_CREAT);
        readcntacess[i] = semget(IPC_PRIVATE, 1, 0777 | IPC_CREAT);
        wrtcntacess[i] = semget(IPC_PRIVATE, 1, 0777 | IPC_CREAT);
        mutex[i] = semget(IPC_PRIVATE, 1, 0777 | IPC_CREAT);
        semctl(rsem[i], 0, SETVAL, 1);
        semctl(wsem[i], 0, SETVAL, 1);
        semctl(readcntacess[i], 0, SETVAL, 1);
        semctl(wrtcntacess[i], 0, SETVAL, 1);
        semctl(mutex[i], 0, SETVAL, 1);
    }

    pop.sem_num = vop.sem_num = 0; // We now initialize the sembufs pop and vop so that pop is used for P(semid) and vop is used for V(semid).
    pop.sem_flg = vop.sem_flg = SEM_UNDO;
    pop.sem_op = -1;
    vop.sem_op = 1;
    for (i = 0; i < 3; i++) // Allowing shared memory for readcnt and wrtcnt
    {
        readcntid[i] = shmget(IPC_PRIVATE, sizeof(int), 0777 | IPC_CREAT);
        writecntid[i] = shmget(IPC_PRIVATE, sizeof(int), 0777 | IPC_CREAT);
        readcnt[i] = (int *)shmat(readcntid[i], 0, 0);
        *readcnt[i] = 0;
        writecnt[i] = (int *)shmat(writecntid[i], 0, 0);
        *writecnt[i] = 0;
    }

    int pnr, trainid, age, index;
    char type[20], fname[30], lname[15], sex[2], class[4];
    FILE *fp;
    int count = 0, flag = 0;
    if (fork() == 0)
        pid = 1;
    else
    {
        if (fork() == 0)
            pid = 2;
        else
        {
            if (fork() == 0)
                pid = 3;
            else
            {
                if (fork() == 0)
                    pid = 4;
                else
                    pid = 0;
            }
        }
    }

    if (pid != 0)
    {
        fp = fopen(argv[pid], "r");
        if (fp == NULL)
        {
            printf("NO FILE READ\n");
            return 0;
        }
        while (fscanf(fp, "%s", type) == 1) // Read till nothing is read
        {
            if (strcmp(type, "reserve") == 0)
            {
                fscanf(fp, "%s %s %d %s %d %s", fname, lname, &age, sex, &trainid, class); // Read the formatted input
                strcat(fname, " ");
                strcat(fname, lname);
                get_read_lock(trainid);        // Get read lock
                if (strcmp(class, "AC3") == 0) // Check if seats are available for each class
                {
                    if (raildata[trainid]->AC3 > 0)
                        flag = 1;
                    else
                        flag = 0;
                }
                else if (strcmp(class, "AC2") == 0)
                {
                    if (raildata[trainid]->AC2 > 0)
                        flag = 1;
                    else
                        flag = 0;
                }
                else
                {
                    if (raildata[trainid]->SC > 0)
                        flag = 1;
                    else
                        flag = 0;
                }
                release_read_lock(trainid); // Release the read lock

                get_write_lock(trainid); // Get the write lock
                strcpy((raildata[trainid]->rlist[raildata[trainid]->rlstsize]).passengername, fname);
                strcpy(raildata[trainid]->rlist[raildata[trainid]->rlstsize].class, class);
                strcpy(raildata[trainid]->rlist[raildata[trainid]->rlstsize].sex, sex);
                raildata[trainid]->rlist[raildata[trainid]->rlstsize].pnr = raildata[trainid]->rlstsize * 10 + trainid;
                raildata[trainid]->rlist[raildata[trainid]->rlstsize].age = age;

                if (strcmp(class, "SC") == 0) // Compare the class
                {
                    if (raildata[trainid]->SC > 0) // If seats are greater than zero
                    {
                        raildata[trainid]->SC--;
                        raildata[trainid]->rlist[raildata[trainid]->rlstsize].status = reserved; // Make it reserved
                        printf("Reservation made successfully for passenger %s in process %d allocated PNR %d in class %s\n", fname, pid, raildata[trainid]->rlist[raildata[trainid]->rlstsize].pnr, class);
                        raildata[trainid]->rlstsize++; // Increment current last index of array
                    }
                    else
                    {
                        raildata[trainid]->rlist[raildata[trainid]->rlstsize].status = waitlisted; // Make it waitlisted
                        printf("Added Passenger %s to waitlisted by process %d with PNR %d in class %s\n", fname, pid, raildata[trainid]->rlist[raildata[trainid]->rlstsize].pnr, class);
                        raildata[trainid]->rlstsize++; // Increment current last index of array
                    }
                }
                else if (strcmp(class, "AC3") == 0)
                {
                    if (raildata[trainid]->AC3 > 0)
                    {
                        raildata[trainid]->AC3--;
                        raildata[trainid]->rlist[raildata[trainid]->rlstsize].status = reserved; // Make it reserved
                        printf("Reservation made successfully for passenger %s in process %d allocated PNR %d in class %s\n", fname, pid, raildata[trainid]->rlist[raildata[trainid]->rlstsize].pnr, class);
                        raildata[trainid]->rlstsize++;
                    }
                    else
                    {
                        raildata[trainid]->rlist[raildata[trainid]->rlstsize].status = waitlisted; // Make it waitlisted
                        printf("Added Passenger %s to waitlisted by process %d with PNR %d in class %s\n", fname, pid, raildata[trainid]->rlist[raildata[trainid]->rlstsize].pnr, class);
                        raildata[trainid]->rlstsize++; // Increment current last index of array
                    }
                }
                else
                {
                    if (raildata[trainid]->AC2 > 0)
                    {
                        raildata[trainid]->AC2--;
                        raildata[trainid]->rlist[raildata[trainid]->rlstsize].status = reserved; // Make it reserved
                        printf("Reservation made successfully for passenger %s in process %d allocated PNR %d in class %s\n", fname, pid, raildata[trainid]->rlist[raildata[trainid]->rlstsize].pnr, class);
                        raildata[trainid]->rlstsize++; // Increment current last index of array
                    }
                    else
                    {
                        raildata[trainid]->rlist[raildata[trainid]->rlstsize].status = waitlisted; // Make it waitlisted
                        printf("Added Passenger %s to waitlisted by process %d with PNR %d in class %s\n", fname, pid, pnr, class);
                        raildata[trainid]->rlstsize++; // Increment current last index of array
                    }
                }
                release_write_lock(trainid);
                //++count;
            }
            else
            {
                fscanf(fp, "%d", &pnr);
                index = pnr / 10;
                trainid = pnr % 10;

                get_write_lock(trainid);
                if (trainid < 0 || trainid > 2 || index < 0 || index > raildata[trainid]->rlstsize) // If pnr is invalid
                {
                    printf("%d is Invalid PNR\n", pnr);
                    release_write_lock(trainid);
                    continue;
                }
                if (raildata[trainid]->rlist[index].status == cancelled) // If pnr is already cancelled
                {
                    printf("Process : %d, %d is Already Cancelled\n", pid, pnr);
                    release_write_lock(trainid);
                    continue;
                }
                raildata[trainid]->rlist[index].status = cancelled; // Cancel the seat
                printf("Reservation Cancelled for passenger %s by process %d with PNR %d\n", fname, pid, pnr);
                for (i = 0; i < 1000; i++)
                {
                    if ((raildata[trainid]->rlist[i].status == waitlisted) && strcmp(raildata[trainid]->rlist[i].class, raildata[trainid]->rlist[index].class) == 0) // Find next waitlist
                        break;
                }
                if (i < 1000)
                {
                    raildata[trainid]->rlist[i].status == reserved; // Make it reserved
                    printf("Removed passenger %s from waitlist with PNR %d and class %s and status changed to confirmed by process %d\n", fname, raildata[trainid]->rlist[i].pnr, class, pid);
                }
                release_write_lock(trainid);
                ++count;
            }
            sleep(1);
        }
        fclose(fp);
    }
    else // Parent waits for everyone to finish their job
    {
        for (i = 0; i < 4; i++)
            wait(NULL);
    }
    return 0;
}
