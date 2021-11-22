#include "sut.h"
#include "queue.h"
#include "YAUThreads.h"
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <ucontext.h>
#include <errno.h>
#include <time.h>
#include <ucontext.h>
#include <fcntl.h>
#include <time.h>

int numCExec = 1; // global variable, indication number of C-EXEC available

pthread_t cThread;
pthread_t cThread2;
pthread_t iThread;

threaddesc *tdescptrList[40]; // list of all tasks created, maximum 30 tasks
int numTasks;                 // number of tasks created
int numThreads;               // number of threads not yet exited

pthread_mutex_t cMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t iMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t numThreadsMutex = PTHREAD_MUTEX_INITIALIZER; // used to protect the decrement of numThreads in sut_exit

struct queue readyQ;
struct queue waitQ;

ucontext_t cContext;
ucontext_t cContext2;
ucontext_t iContext;

pthread_barrier_t barrier; // ensure context of 2 C-EXEC are both obtained

void *c_exec()
{
    struct queue_entry *head;
    if (getcontext(&cContext) != 0)
    {
        printf("getcontext in c_exec failed\n");
    }
    if (numCExec == 2) // wait for both threads to arrive
    {
        pthread_barrier_wait(&barrier);
    }

    while (true)
    {
        pthread_mutex_lock(&cMutex);
        head = queue_pop_head(&readyQ);
        pthread_mutex_unlock(&cMutex);
        if (head) // there exists a task in ready queue
        {
            ucontext_t uContext = *(ucontext_t *)head->data;
            swapcontext(&cContext, &uContext);
            free(head);
        }
        else
        {
            struct timespec sleeptime;
            sleeptime.tv_nsec = 100000; // 100 us = 100000 ns
            nanosleep(&sleeptime, NULL);
        }
    }
}

void *c_exec2()
{
    struct queue_entry *head;
    if (getcontext(&cContext2) != 0)
    {
        printf("getcontext in c_exec2 failed\n");
    }
    pthread_barrier_wait(&barrier);

    while (true)
    {
        pthread_mutex_lock(&cMutex);
        head = queue_pop_head(&readyQ);
        pthread_mutex_unlock(&cMutex);
        if (head) // there exists a task in ready queue
        {
            ucontext_t uContext = *(ucontext_t *)head->data;
            swapcontext(&cContext2, &uContext);
            free(head);
        }
        else
        {
            struct timespec sleeptime;
            sleeptime.tv_nsec = 100000; // 100 us = 100000 ns
            nanosleep(&sleeptime, NULL);
        }
    }
}

void *i_exec()
{
    getcontext(&iContext);
    struct queue_entry *head;
    while (true)
    {
        pthread_mutex_lock(&iMutex);
        head = queue_pop_head(&waitQ);
        pthread_mutex_unlock(&iMutex);

        if (head) // there exists a task in wait queue
        {
            ucontext_t uContext = *(ucontext_t *)head->data;
            swapcontext(&iContext, &uContext);
        }
        else
        {
            struct timespec sleeptime;
            sleeptime.tv_nsec = 100000; // 100 us = 100000 ns
            nanosleep(&sleeptime, NULL);
        }
    }
}

// initialize SUT library
void sut_init()
{
    numTasks = 0;
    numThreads = 0;

    // initialize ready queue for C-EXEC and wait queue for I-EXEC
    readyQ = queue_create();
    queue_init(&readyQ);
    waitQ = queue_create();
    queue_init(&waitQ);

    if (numCExec == 2)  // 2 C-EXEC
    {
        if (pthread_create(&cThread2, NULL, c_exec2, NULL) != 0)
        {
            printf("Creation of second C-EXEC failed.\n");
        }
        srand(time(NULL));
        pthread_barrier_init(&barrier, NULL, 2);
    }

    // create C-EXEC and I-EXEC
    if (pthread_create(&cThread, NULL, c_exec, NULL) != 0)
    {
        printf("Creation of first C-EXEC failed.\n");
    }

    if (pthread_create(&iThread, NULL, i_exec, NULL) != 0)
    {
        printf("Creation of I-EXEC failed.\n");
    }
}

// create a task, return 1 on success, otherwise return 0
bool sut_create(sut_task_f fn)
{
    threaddesc *tdescptr;
    tdescptr = malloc(sizeof(threaddesc));

    if (getcontext(&(tdescptr->threadcontext)) != 0)
    {
        printf("getcontext failed in sut_create.\n");
        return false;
    }

    tdescptr->threadid = numThreads;
    tdescptr->threadstack = (char *)malloc(THREAD_STACK_SIZE);
    tdescptr->threadcontext.uc_stack.ss_sp = tdescptr->threadstack;
    tdescptr->threadcontext.uc_stack.ss_size = THREAD_STACK_SIZE;
    tdescptr->threadcontext.uc_link = &cContext;
    tdescptr->threadcontext.uc_stack.ss_flags = 0;
    tdescptr->threadfunc = &fn;

    makecontext(&(tdescptr->threadcontext), fn, 0);
    if (errno != 0) // makecontext failed
    {
        return false;
    }

    tdescptrList[numTasks] = tdescptr;
    numTasks++;   // keep track of number of tasks created
    numThreads++; // keep track of number of task not yet exited

    // insert task into ready queue
    struct queue_entry *task = queue_new_node(&(tdescptr->threadcontext));
    pthread_mutex_lock(&cMutex);
    queue_insert_tail(&readyQ, task);
    pthread_mutex_unlock(&cMutex);

    return true;
}

// yield execution of running task
void sut_yield()
{
    ucontext_t uContext;
    if (getcontext(&uContext) != 0)
    {
        printf("getcontext failed in sut_yield\n");
    }

    struct queue_entry *curTask = queue_new_node(&uContext);

    pthread_mutex_lock(&cMutex);
    queue_insert_tail(&readyQ, curTask); // enqueue running task at the back of ready queue
    pthread_mutex_unlock(&cMutex);

    int cNum = rand() % 2 + 1;
    if (cNum == 1 || numCExec == 1)
    {
        swapcontext(&uContext, &cContext); // give back control to C-EXEC
    }
    else
    {
        swapcontext(&uContext, &cContext2);
    }
}

// terminate current task execution
// if there is no other task, terminate the program
// else, go to the next task
void sut_exit()
{
    int exitFlg = 0;
    pthread_mutex_lock(&numThreadsMutex);
    numThreads--;
    if (numThreads == 0)
    {
        exitFlg = 1;
    }
    pthread_mutex_unlock(&numThreadsMutex);

    if (!(queue_peek_front(&readyQ)) && !(queue_peek_front(&waitQ)) && exitFlg == 1) // both queues are empty, all tasks are done
    {
        printf("about to cancel\n");
        if (pthread_cancel(cThread) != 0)
        {
            printf("cancel t1 errno: %d\n", errno);
        }
        else
        {
            printf("cancelled t1\n");
        }

        printf("numCEXEC: %d\n", numCExec);
        if (numCExec == 2)
        {
            printf("about to cancel t2\n");
            if (pthread_cancel(cThread2) != 0)
            {
                printf("cancel t2 failed, errno:%d \n", errno);
            }
            else
            {
                printf("cancelled t2\n");
            }
        }

        printf("about to cancel i\n");
        if (pthread_cancel(iThread) != 0)
        {
            printf("cancel i failed, errno: %d\n", errno);
        }
        else
        {
            printf("cancelled i\n");
        }
        // pthread_exit(NULL);
    }
    else // terminate the currently running task
    {
        ucontext_t uContext;
        if (getcontext(&uContext) != 0)
        {
            printf("getcontext failed in sut_exit\n");
        }

        int cNum = rand() % 2 + 1;
        if (cNum == 1 || numCExec == 1)
        {
            swapcontext(&uContext, &cContext); // give back control to C-EXEC
        }
        else
        {
            swapcontext(&uContext, &cContext2);
        }
    }
}

// open operation is put into waitQ until I-EXEC is ready to execute the task
// open file specified by name dest
// return non-negative value if successful, otherwise, return a negative value
int sut_open(char *dest)
{
    ucontext_t openContext;
    if (getcontext(&openContext) != 0)
    {
        printf("getcontext failed in sut_open\n");
    }

    struct queue_entry *openTask = queue_new_node(&openContext);

    // put open operation into waitQ
    pthread_mutex_lock(&iMutex);
    queue_insert_tail(&waitQ, openTask);
    pthread_mutex_unlock(&iMutex);

    swapcontext(&openContext, &iContext);

    // open file
    int fd;
    fd = open(dest, O_RDWR, S_IRWXU);
    if (errno != 0)
    {
        printf("Open file failed, errno = %d\n", errno);
    }

    // add task to ready queue
    pthread_mutex_lock(&cMutex);
    queue_insert_tail(&readyQ, openTask);
    pthread_mutex_unlock(&cMutex);

    int cNum = rand() % 2 + 1;
    if (cNum == 1 || numCExec == 1)
    {
        swapcontext(&openContext, &cContext); // give back control to C-EXEC
    }
    else
    {
        swapcontext(&openContext, &cContext2);
    }

    return fd;
}

// buf with memory pre-allocated by caller
// return NULL upon failure
char *sut_read(int fd, char *buf, int size)
{
    ucontext_t readContext;
    if (getcontext(&readContext) != 0)
    {
        printf("getcontext failed in sut_read\n");
    }

    struct queue_entry *readTask = queue_new_node(&readContext);

    // put read operation into waitQ
    pthread_mutex_lock(&iMutex);
    queue_insert_tail(&waitQ, readTask);
    pthread_mutex_unlock(&iMutex);

    swapcontext(&readContext, &iContext);

    int res = read(fd, buf, size);

    // add task to ready queue
    pthread_mutex_lock(&cMutex);
    queue_insert_tail(&readyQ, readTask);
    pthread_mutex_unlock(&cMutex);

    int cNum = rand() % 2 + 1;
    if (cNum == 1 || numCExec == 1)
    {
        swapcontext(&readContext, &cContext); // give back control to C-EXEC
    }
    else
    {
        swapcontext(&readContext, &cContext2);
    }

    if (errno != 0)
    {
        printf("read failed, errno=%d\n", errno);
        return NULL;
    }
    else
    {
        return buf;
    }
}

// write bytes in buff to open file pointed by fd
void sut_write(int fd, char *buf, int size)
{
    ucontext_t writeContext;
    if (getcontext(&writeContext) != 0)
    {
        printf("getcontext failed in sut_write\n");
    }

    struct queue_entry *writeTask = queue_new_node(&writeContext);

    // put open operation into waitQ
    pthread_mutex_lock(&iMutex);
    queue_insert_tail(&waitQ, writeTask);
    pthread_mutex_unlock(&iMutex);

    swapcontext(&writeContext, &iContext);

    write(fd, buf, size);

    // add task to ready queue
    pthread_mutex_lock(&cMutex);
    queue_insert_tail(&readyQ, writeTask);
    pthread_mutex_unlock(&cMutex);

    int cNum = rand() % 2 + 1;
    if (cNum == 1 || numCExec == 1)
    {
        swapcontext(&writeContext, &cContext); // give back control to C-EXEC
    }
    else
    {
        swapcontext(&writeContext, &cContext2);
    }
}

// close file pointed by fd
void sut_close(int fd)
{
    ucontext_t closeContext;
    if (getcontext(&closeContext) != 0)
    {
        printf("getcontext failed in sut_close\n");
    }

    struct queue_entry *closeTask = queue_new_node(&closeContext);

    // put open operation into waitQ
    pthread_mutex_lock(&iMutex);
    queue_insert_tail(&waitQ, closeTask);
    pthread_mutex_unlock(&iMutex);

    swapcontext(&closeContext, &iContext);

    close(fd);

    // add task to ready queue
    pthread_mutex_lock(&cMutex);
    queue_insert_tail(&readyQ, closeTask);
    pthread_mutex_unlock(&cMutex);

    int cNum = rand() % 2 + 1;
    if (cNum == 1 || numCExec == 1)
    {
        swapcontext(&closeContext, &cContext); // give back control to C-EXEC
    }
    else
    {
        swapcontext(&closeContext, &cContext2);
    }
}

void sut_shutdown()
{
    if (pthread_join(cThread, NULL) != 0)
    {
        printf("failed to join C-EXEC in sut_shutdown\n");
    }
    printf("c1 joined\n");

    if (numCExec == 2)
    {   
        printf("waiting in shutdown for t2\n");
        if (pthread_join(cThread2, NULL) != 0)
        {
            printf("failed to join C-EXEC2 in sut_shutdown\n");
        }
        printf("c2 joined\n");
    }

    if (pthread_join(iThread, NULL) != 0)
    {
        printf("failed to join I-EXEC in sut_shutdown\n");
    }
    printf("i joined\n");


    // free all memory before shutdown :)
    for (int i = 0; i < numTasks; i++)
    {
        free(tdescptrList[i]->threadstack);
        free(tdescptrList[i]);
    }

    // destroy mutex
    pthread_mutex_destroy(&cMutex);
    pthread_mutex_destroy(&iMutex);
    pthread_mutex_destroy(&numThreadsMutex);
}
