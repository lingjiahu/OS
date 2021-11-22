#include "sut.h"
#include "queue.h"
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
#include "YAUThreads.h"

int numCExec = 1; // global variable, indication number of C-EXEC available

pthread_t cThread;
pthread_t iThread;

int numThreads;

pthread_mutex_t cMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t iMutex = PTHREAD_MUTEX_INITIALIZER;

struct queue readyQ;
struct queue waitQ;

ucontext_t cContext;
ucontext_t iContext;

void *c_exec()
{
    struct queue_entry *head;
    getcontext(&cContext); 

    while (true)
    {
        pthread_mutex_lock(&cMutex);
        head = queue_pop_head(&readyQ);
        pthread_mutex_unlock(&cMutex);
        if (head) // there exists a task in ready queue
        {
            ucontext_t uContext = *(ucontext_t *)head->data;
            swapcontext(&cContext, &uContext);
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
    numThreads = 0;
    readyQ = queue_create();
    queue_init(&readyQ);
    waitQ = queue_create();
    queue_init(&waitQ);

    pthread_create(&cThread, NULL, c_exec, NULL);
    if (numCExec == 2)
    {
        pthread_create(&cThread, NULL, c_exec, NULL);
    }
    pthread_create(&iThread, NULL, i_exec, NULL);
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
    // printf("\nready to create uContext: %p\n", &(tdescptr->threadcontext));
    tdescptr->threadid = numThreads;
    tdescptr->threadstack = (char *)malloc(THREAD_STACK_SIZE);
    tdescptr->threadcontext.uc_stack.ss_sp = tdescptr->threadstack;
    tdescptr->threadcontext.uc_stack.ss_size = THREAD_STACK_SIZE;
    tdescptr->threadcontext.uc_link = &cContext;
    tdescptr->threadcontext.uc_stack.ss_flags = 0;
    tdescptr->threadfunc = &fn;

    numThreads++;

    // printf("create: context of hello1: %p\n", &(tdescptr->threadcontext));
    makecontext(&(tdescptr->threadcontext), fn, 0);
    if (errno != 0) // makecontext failed
    {
        return false;
    }

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
    getcontext(&uContext);

    struct queue_entry *curTask = queue_new_node(&uContext);

    pthread_mutex_lock(&cMutex);
    queue_insert_tail(&readyQ, curTask); // enqueue running task at the back of ready queue
    pthread_mutex_unlock(&cMutex);

    swapcontext(&uContext, &cContext); // give back control to C-EXEC
}

// terminate current task execution
// if there is no other task, terminate the program
// else, go to the next task
void sut_exit()
{
    if (!(queue_peek_front(&readyQ)) && !(queue_peek_front(&waitQ))) // both queues are empty, no other task
    {
        // TODO free task node
        exit(1); // terminate the whole program
    }
    else // terminate the currently running task
    {
        ucontext_t uContext;
        getcontext(&uContext);
        swapcontext(&uContext, &cContext);
    }
}

// open operation is put into waitQ until I-EXEC is ready to execute the task
// open file specified by name dest
// return non-negative value if successful, otherwise, return a negative value
int sut_open(char *dest)
{
    ucontext_t openContext;
    getcontext(&openContext);

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
    } else
    {
        // printf("fd = %d\n", fd);
    }

    // add task to ready queue
    pthread_mutex_lock(&cMutex);
    queue_insert_tail(&readyQ, openTask);
    pthread_mutex_unlock(&cMutex);
    swapcontext(&openContext, &cContext);

    return fd;
}

// buf with memory pre-allocated by caller
// return NULL upon failure
char *sut_read(int fd, char *buf, int size)
{
    ucontext_t readContext;
    getcontext(&readContext);

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
    swapcontext(&readContext, &cContext);

    if (errno != 0)
    {
        printf("read errno=%d\n", errno);
        return NULL;
    } else
    {
        return buf;
    }
}

// write bytes in buff to open file pointed by fd
void sut_write(int fd, char *buf, int size)
{
    ucontext_t writeContext;
    getcontext(&writeContext);

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
    swapcontext(&writeContext, &cContext);
}

// close file pointed by fd
void sut_close(int fd)
{
    ucontext_t closeContext;
    getcontext(&closeContext);

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
    swapcontext(&closeContext, &cContext);
}

void sut_shutdown()
{
    pthread_join(cThread, NULL);
    if (numCExec == 2)
    {
        pthread_join(cThread, NULL);
    }
    pthread_join(iThread, NULL);
}
