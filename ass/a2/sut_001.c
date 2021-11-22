#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <ucontext.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>
#include <fcntl.h>

#include "sut.h"
#include "queue.h"
#include "YAUThreads.h"


pthread_t cexec1;
//pthread_t *cexec1;
//cexec1=(pthread_t*)malloc(sizeof(pthread_t));

pthread_mutex_t c1_mutex;
//pthread_mutex_t c1_mutex = PTHREAD_MUTEX_INITIALIZER;

int id;

pthread_t iexec;
pthread_mutex_t io_mutex;

struct queue readyQ;
struct queue waitQ;
//struct queue ioQ;
//struct queue ioto;

bool readyQ_empty = false;
bool waitQ_empty = false;
bool first_task_done = false;


ucontext_t main_c;

void *c_next_task(){

    struct queue_entry *next_task;

    while(!readyQ_empty) {
    
        pthread_mutex_lock(&c1_mutex);
        next_task = queue_peek_front(&readyQ);
        pthread_mutex_unlock(&c1_mutex);

        if(next_task) {

            pthread_mutex_lock(&c1_mutex);
            queue_pop_head(&readyQ);
            pthread_mutex_unlock(&c1_mutex);

            ucontext_t context = *(ucontext_t *)next_task->data;
            
            swapcontext(&main_c, &context);

        }else{
            readyQ_empty = true;
        }
        nanosleep((const struct timespec[]){{0, 100000L}}, NULL);    
    }
    //return 0;
}

void *i_next_task(){
    struct queue_entry *next_task;

    //printf("I am in i-exec, out of while\n");
    while(true){
        //printf("I am in i-exec while\n");
        pthread_mutex_lock(&c1_mutex);
        next_task=queue_pop_head(&waitQ);
        pthread_mutex_unlock(&c1_mutex);

        if(next_task){
            //printf("I am in i-exec next task\n");
            /*pthread_mutex_lock(&c1_mutex);
            queue_pop_head(&waitQ);
            pthread_mutex_unlock(&c1_mutex);*/

            ucontext_t context = *(ucontext_t *)next_task->data;            
            ucontext_t current_context;
            getcontext(&current_context);
            
            //struct queue_entry *p = queue_new_node(&context.uc_link);
            
            //pthread_mutex_lock(&c1_mutex);
            //queue_insert_tail(&readyQ, p);
            //pthread_mutex_unlock(&c1_mutex);

            swapcontext(&current_context, &context);

        }else{
            waitQ_empty=true;
        }
        nanosleep((const struct timespec[]){{0, 100000L}}, NULL); 
    }
}


void sut_init() {
    id=0;

    readyQ = queue_create();
    queue_init(&readyQ);

    waitQ = queue_create();
    queue_init(&waitQ);

    pthread_mutex_init(&c1_mutex,NULL);
    //pthread_mutex_init(&io_mutex,NULL);

    getcontext(&main_c);
    pthread_create(&cexec1, NULL, c_next_task, NULL);
    pthread_create(&iexec, NULL, i_next_task, NULL);
}

bool sut_create(sut_task_f fn) {

    char* t_stack=(char *)malloc(1024*64);
    //id++;
    /*ucontext_t t; 
    getcontext(&t);
    t.uc_stack.ss_sp=t_stack;
    t.uc_stack.ss_size = sizeof(t_stack);
    t.uc_link = &main_c;    // setting up parent context, in case task returns or has an error
    makecontext(&t, fn, 0);  
    struct queue_entry *task = queue_new_node(&t);  */

    threaddesc *tdescptr;
    tdescptr = malloc(sizeof(threaddesc));
    getcontext(&(tdescptr->threadcontext)); 
    tdescptr->threadcontext.uc_stack.ss_sp = t_stack; 
    tdescptr->threadcontext.uc_stack.ss_size = (1024*64);
    tdescptr->threadcontext.uc_link = &main_c; 
    //tdescptr->threadid = id;

    makecontext(&(tdescptr->threadcontext), fn, 0);

    //printf("hello1 context \t%p \n", &(tdescptr->threadcontext));
    //printf("hello1 uc link \t%p \n", &(tdescptr->threadcontext.uc_link));

    struct queue_entry *task = queue_new_node(&(tdescptr->threadcontext));
    pthread_mutex_lock(&c1_mutex);
    queue_insert_tail(&readyQ, task);
    pthread_mutex_unlock(&c1_mutex); 

    return 1;
}

void sut_yield() {
 
    ucontext_t current_t;
    getcontext(&current_t);

    struct queue_entry *task = queue_new_node(&current_t);

    pthread_mutex_lock(&c1_mutex);
    queue_insert_tail(&readyQ, task);
    pthread_mutex_unlock(&c1_mutex);

    //printf
    swapcontext(&current_t, &main_c);
}

void sut_exit() {
    //TODO: terminate the whole program if task is empty
    ucontext_t current_t;
    getcontext(&current_t);
    swapcontext(&current_t, &main_c);
}

void sut_shutdown() {
    pthread_join(cexec1, NULL);
}

int sut_open(char* dest){
    ucontext_t current_context;
    getcontext(&current_context);
    //printf("current context \t%p \n", &current_context);
    printf("parent context \t%p \n", &(current_context.uc_link));
    printf("c main \t\t%p \n", &(main_c));
    //current_context.uc_link=&main_c;
    //makecontext(&current_context, NULL, 0);

    //printf("parent context \t%p \n", &(current_context.uc_link));
    printf("inside sut open");
    struct queue_entry *task = queue_new_node(&current_context);

    //threaddesc *tmp=task->data;
    //int id=tmp->threadid;

    pthread_mutex_lock(&c1_mutex);
    printf("before insert");
    queue_insert_tail(&waitQ, task);
    printf("%p",queue_peek_front(&waitQ));
    pthread_mutex_unlock(&c1_mutex);

    
    printf("insert done\n");
    swapcontext(&current_context,&main_c);

    printf("I am bcak to open, about to open file\n");
    return open(dest,O_RDWR);

    /*struct queue_entry *io_peak;

    while(1){
        pthread_mutex_lock(&io_mutex);
        io_peak=queue_peek_front(&ioQ);
        pthread_mutex_unlock(&io_mutex);

        threaddesc *tmp2=io_peak->data;
        if(tmp2->threadid==id){
            queue_pop_head(&ioQ);
            return open(dest,O_RDWR);

        }else{
            nanosleep((const struct timespec[]){{0, 100000L}}, NULL); 
        }

    }*/

} 

/*char* sut_read(char* dest){
    ucontext_t current_context;
    getcontext(&current_context);
    struct queue_entry *task = queue_new_node(&current_context);

    threaddesc *tmp=task->data;
    int id=tmp->threadid;

    pthread_mutex_lock(&c1_mutex);
    queue_insert_tail(&waitQ, task);
    pthread_mutex_unlock(&c1_mutex);

    struct queue_entry *io_peak;

    while(1){
        pthread_mutex_lock(&io_mutex);
        io_peak=queue_peek_front(&ioQ);
        pthread_mutex_unlock(&io_mutex);

        threaddesc *tmp2=io_peak->data;
        if(tmp2->threadid==id){
            queue_pop_head(&ioQ);
            return open(dest,O_RDWR);

        }else{
            nanosleep((const struct timespec[]){{0, 100000L}}, NULL); 
        }
    }
}*/

void sut_write(int fd, char *buf, int size){
    ucontext_t current_context;
    getcontext(&current_context);
    struct queue_entry *task = queue_new_node(&current_context);

    pthread_mutex_lock(&c1_mutex);
    queue_insert_tail(&waitQ, task);
    pthread_mutex_unlock(&c1_mutex);

    swapcontext(&current_context,&main_c);
    write(fd, &buf,size);

} 

void sut_close(int fd){
    close(fd);

}