#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <ucontext.h>
#include <pthread.h>
#include <stdbool.h>

#include "queue.h"
#include "sut.h"

char hellostack[16*1024];
static pthread_t C_EXEC;
static pthread_t I_EXEC;
static ucontext_t h, m;
static struct queue q;

void * cExec(void *arg);
void * iExec(void *arg);
void sut_init();
bool sut_create(sut_task_f fn);
void sut_yield();
void sut_exit();
void sut_open(char *dest, int port);
void sut_write(char *buf, int size);
void sut_close();
char *sut_read();
void sut_shutdown();



//C-Exec runner function
void * cExec(void *arg){
  

  //Infinite loop checking if element in queue
  while(true){
    
    struct queue_entry *ptr = queue_pop_head(&q);
    //if queue not empty
    if(ptr){
      m = *(ucontext_t*)ptr->data;
      
      // usleep(1000);
      swapcontext(&m, &h);
      free(ptr);
      // printf("successful context switching");
      
    }
    else{
      //if sleep too long only other tasks carried out
      usleep(10);
    }
  }
  
  
  
}
//unfinished I-Exec runner function
void * iExec(void *arg){
  while(true){
    printf("hello from i-Exec\n");
    usleep(1000*1000);
  }
}
//initializes C-Exec I-Exec and the queue for ready tasks.
void sut_init(){
  printf("sut_init\n");
  pthread_create(&C_EXEC, NULL, cExec, NULL);
  pthread_create(&I_EXEC, NULL, iExec, NULL);
  q = queue_create();
  queue_init(&q);
}

//creates new task pushing to queue
bool sut_create(sut_task_f fn){
  printf("sut_create\n");
  getcontext(&h);
  h.uc_stack.ss_sp = hellostack;
  h.uc_stack.ss_size = sizeof(hellostack);
  h.uc_link = &m;
  makecontext(&h, fn, 0);
  struct queue_entry *node = queue_new_node(&h);
  queue_insert_tail(&q, node);
  
}
//stores context of user thread to queue and switches context back to C-EXEC
void sut_yield(){
  struct queue_entry *node = queue_new_node(&h);
  queue_insert_tail(&q, node);
	swapcontext(&h, &m);
}

//exits context back to C-Exec without saving to stack.
void sut_exit(){
  swapcontext(&h, &m);
}
//joins the pthreads (kernel threads)
void sut_shutdown(){
  pthread_join(C_EXEC, NULL);
  pthread_join(I_EXEC, NULL);
}