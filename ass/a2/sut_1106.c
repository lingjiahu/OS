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

void *c_exec()
{
    struct queue_entry *head;
    getcontext(&cContext); 

    while (true)
    {
        pthread_mutex_lock(&cMutex);

        head = queue_pop_head(&readyQ);
        if (head) // there exists a task in ready queue
        {
            ucontext_t uContext = *(ucontext_t *)head->data;
            pthread_mutex_unlock(&cMutex);

            // printf("cContext before open in c_exec: %p\n", &cContext);

            swapcontext(&cContext, &uContext);
        }
        else
        {
            pthread_mutex_unlock(&cMutex);
            struct timespec sleeptime;
            sleeptime.tv_nsec = 100000; // 100 us = 100000 ns
            nanosleep(&sleeptime, NULL);
        }
    }
}

void *i_exec()
{
    struct queue_entry *head;
    while (true)
    {
        pthread_mutex_lock(&iMutex);
        head = queue_pop_head(&waitQ);
        if (head) // there exists a task in wait queue
        {
            ucontext_t uContext = *(ucontext_t *)head->data;
            pthread_mutex_unlock(&iMutex);
            ucontext_t iContext;
            getcontext(&iContext);

            struct queue_entry *parent = queue_new_node(&(uContext.uc_link));
            printf("uclink: %p\n", &(uContext.uc_link));
            pthread_mutex_lock(&cMutex);
            queue_insert_tail(&readyQ, parent);
            pthread_mutex_unlock(&cMutex);
            swapcontext(&iContext, &uContext);
        }
        else
        {
            pthread_mutex_unlock(&iMutex);
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

    getcontext(&(tdescptr->threadcontext));
    // printf("\nready to create uContext: %p\n", &(tdescptr->threadcontext));
    tdescptr->threadid = numThreads;
    tdescptr->threadstack = (char *)malloc(THREAD_STACK_SIZE);
    tdescptr->threadcontext.uc_stack.ss_sp = tdescptr->threadstack;
    tdescptr->threadcontext.uc_stack.ss_size = THREAD_STACK_SIZE;
    tdescptr->threadcontext.uc_link = &cContext;
    tdescptr->threadcontext.uc_stack.ss_flags = 0;
    tdescptr->threadfunc = &fn;

    numThreads++;

    printf("create: context of hello1: %p\n", &(tdescptr->threadcontext));
    makecontext(&(tdescptr->threadcontext), fn, 0);
    if (errno != 0) // makecontext failed
    {
        return false;
    }

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

    printf("cContext in open: %p\n", &cContext);

    printf("open: parent of openContext: %p\n", &(openContext.uc_link));
    openContext.uc_link = &cContext;
    printf("sut_open uclink: %p\n", &(openContext.uc_link));
    setcontext(&cContext); // TODO changed from swapcontext(...)

    printf("the line after swapping to cContext in open\n");

    int fd;
    fd = open(dest, O_RDWR);
    if (errno != 0)
    {
        printf("errno = %d\n", errno);
    } else
    {
        printf("fd = %d\n", fd);
    }

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

    swapcontext(&readContext, &cContext);

    // TODO what to return
    if (read(fd, &buf, size) < 0)
    {
        return NULL;
    } else
    {
        return 0;
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

    swapcontext(&writeContext, &cContext);

    write(fd, &buf, size);
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

    swapcontext(&closeContext, &cContext);

    close(fd);
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
