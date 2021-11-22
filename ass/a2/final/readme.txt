Dear grader,

In this readme, you will find 
Part 1: description of my solution and extra details in additon to inline comments
Part 2: issues encoutered and solutions proposed


Part 1:

Part A (1 C-EXEC, 1 I-EXEC): set global variable numCExec to 1
Part B (2 C-EXEC, 1 I-EXEC): set global variable numCExec to 2

write behavior: overwrite if file already exits

exit: if the current task is the last one (both readyQ and waitQ are empty, all other tasks created have exited),
set exitFlg to true so that terminates all the threads in exec functions

shutdown: waits for exec threads to join after they get terminated by calling pthread_exit()

swapcontext for 2 C-EXEC: always swaps back to the caller thread

I/O tasks: 
once a call is made to sut_open/write/close/read, the task is enqued into the waitQ and will be executed when I-EXEC get it popped from the queue
the actual operation is done inside sut_*() functions and then the task will be handed back to C-EXEC as they are enqued in the readyQ


Part 2:

Only 1 unresolved issue: behavoir of 2 C-EXEC mode is not stable
I have been successfully getting expected results but errors do occur from time to time.

Cause found: pthread_join(cThread2, NULL) hangs even if cThread2 has terminated

How I found the cause: added a lot of print statements

My attempt to solve the problem: 
1. used different methods for the termination of threads,
    i) call thread_cancel() on each thread in sut_exit() when ready to exit
    ii) call thread_cancel() on all other threads before call thread_cancel() on the thread which sut_exit() is called on
    iii) (submitted implementation) use a flg inside while(true) of each thread function which will let the all threads exit 
2. went to OH and discussed my idea with the TA, both of us believe that I am on the right track but there is no obvious reason 
for why the cThread2, only cThread2, hangs, no problem with the rest

It would be great if you could briefly indicate a fix to the problem, thank you!


Lastly, I really appreciate your patience reading till the end of this readme doc. 
Apologies for making it this long. 
Have a great day!