#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>

int jobsList[10000]; // table for jobs: stroing pid of child processes in order of invoking
int lastIdx = 0;

int getcmd(char *prompt, char *args[], int *background)
{
    int length, i = 0;
    char *token, *loc;
    char *line = NULL;
    size_t linecap = 0;

    printf("%s", prompt);
    length = getline(&line, &linecap, stdin);

    char *linecp = line; // keep a copy of initial address of line

    if (length <= 0)
    {
        exit(-1);
    }

    if ((loc = index(line, '&')) != NULL)
    {
        *background = 1;
        *loc = ' ';
    }
    else
        *background = 0;

    while ((token = strsep(&line, " \t\n")) != NULL)
    {
        for (int j = 0; j < strlen(token); j++)
        {
            if (token[j] <= 32)
                token[j] = '\0'; // zero character, termination of string
        }
        if (strlen(token) > 0)
        {
            args[i++] = token;
        }
    }
    args[i] = NULL; // null terminating

    return i;
}

// handle signals SIGINT and SIGTSTP
void sigHandler(int sig)
{
    if (sig == SIGINT) // ctrl c, kill a process running in shell
    {
        int pid = getpid();
        printf("kill pid: %d\n", pid);
        kill(pid, SIGKILL);
    }
    else if (sig == SIGTSTP) // ctrl z, ignore, does nothing
    {
        printf("\nThe SIGTSTP signal is ignored.\n");
    }
}

// handle child termination signal, param sig is pid of terminated child
void cSigHandler(int sig)
{
    int pid;
    pid = wait(NULL);
    printf("c sig handler got pid: %d\n", pid);

    for (int i = 0; i < lastIdx; i++)
    {
        if (jobsList[i] == pid)
        {
            printf("sig handler caught SIGCHLD, removed pid !started!: %d\n", pid);
            childRemove(jobsList, &lastIdx, pid);
            printf("sig handler caught SIGCHLD, removed pid done: %d\n", pid);
        }
    }
}

// changes directory to param dir
void cdCmd(char *dir)
{
    if (chdir(dir) == 0) // returns 0 on success
    {
        printf("Successfully changed to dir %s.\n", dir);
    }
}

// takes no argument, prints present working directory
void pwdCmd()
{
    char buffer[10000]; // used to store pwd
    getcwd(buffer, sizeof(buffer));
    printf("%s\n", buffer);
}

// terminates all jobs running in bg and terminates the shell
void exitCmd()
{
    for (int i = 0; i < lastIdx; i++)
    {
        kill(jobsList[i], SIGTERM);
    }
    kill(getpid(), SIGTERM);
}

// brings specified job at idx to fg if passed a param
void fgCmdP(int idx)
{
    printf("brought pid [%d] to fg\n", jobsList[idx]);
    int pid = waitpid(jobsList[idx], NULL, 0);
    printf("gotta\n", jobsList[idx]);
    childRemove(jobsList, &lastIdx, pid);
}

// no param, brings the first job to fg by default
void fgCmdN()
{
    int status;
    printf("brought pid [%d] to fg\n", jobsList[0]);
    int pid = waitpid(jobsList[0], NULL, 0);
    printf("gotta\n", jobsList[0]);
    childRemove(jobsList, &lastIdx, pid);
}

// lists all jobs running in bg
void jobsCmd(int jobsList[], int lastIdx)
{
    printf("ready to print jobs, lastIdx = %d\n", lastIdx);
    printf("[Index] pid\n");
    for (int i = 0; i < lastIdx; i++)
    {
        printf("[%d]     %d\n", i, jobsList[i]);
    }
}

// remove terminated child process from jobs
void childRemove(int jobsList[], int *lastIdx, int pid)
{
    printf("start removing child, pid: %d\n", pid);
    int i;
    for (i = 0; i < *lastIdx; i++)
    {
        if (jobsList[i] == pid)
        {
            if (i == 0 && *lastIdx > 1) // remove the first and not the only
            {
            }
            else
            {
                jobsList[i] = '\0'; // remove child
            }
            printf("child removed pid: %d\n", pid);
            break;
        }
    }

    *lastIdx = *lastIdx - 1;

    for (int j = i; j < *lastIdx; j++)
    {
        jobsList[j] = jobsList[j + 1];
    }

    printf("print jobsList after pid[%d] removed:\n", pid);
    printf("number of running children: %d\n", *lastIdx);
    for (int k = 0; k < *lastIdx; k++)
    {
        printf("jobsList[%d] = pid[%d]\n", k, jobsList[k]);
    }
    printf("\n");
}

// output redirection, return new file descriptor
int redirect(char *args[], int idx)
{
    int out = dup(STDOUT_FILENO);
    close(STDOUT_FILENO);
    char *fn = args[idx + 1]; // dest file for redirection
    int fd = open(fn, O_CREAT | O_WRONLY | O_RDONLY | O_TRUNC, S_IRWXU);
    args[idx] = NULL; // discard tokens from redirection sign '>'
    return out;
}

int checkPipe(int cnt, char *args[])
{
    for (int i = 0; i < cnt; i++)
    {
        if (*(args[i]) == 124)
        {
            return i;
        }
    }
    return -1;
}

int main(void)
{
    char *args[20];
    int bg;

    char *firstArgs[20];  // first part of pipe
    char *secondArgs[20]; // second part of pipe

    while (1)
    {
        signal(SIGINT, SIG_IGN);

        if (signal(SIGTSTP, sigHandler) == SIG_ERR)
        {
            printf("ERROR! Could not bind the signal hander.\n");
            exit(1);
        }

        bg = 0;        // 0: run in fg; 1: run in bg
        int rdFlg = 0; // 0: no redirection; 1: output redirection
        int newFd;     // value of new file descriptor for redirection

        int fd[2];                            // fd used for piping
        int cnt = getcmd("\n>> ", args, &bg); // cnt: number of tokens

        if (cnt < 1) // check for empty input
        {
            continue;
        }

        // check for built in commands
        char *cmd = args[0];
        if (strcmp(cmd, "cd") == 0)
        {
            cdCmd(args[1]);
            continue;
        }
        else if (strcmp(cmd, "pwd") == 0)
        {
            pwdCmd();
            continue;
        }
        else if (strcmp(cmd, "exit") == 0)
        {
            exitCmd();
            continue;
        }
        else if (strcmp(cmd, "fg") == 0)
        {
            if (cnt > 1)
            {
                int idx = atoi(args[1]);
                if (idx > lastIdx)
                {
                    printf("Error: there are only %d process(es) running in backgroud.\n", lastIdx);
                }
                fgCmdP(idx);
            }
            else if (lastIdx > 0)
            {
                fgCmdN();
            }
            continue;
        }
        else if (strcmp(cmd, "jobs") == 0)
        {
            jobsCmd(jobsList, lastIdx);
            continue;
        }

        // check if '>' is present in args for output redirection
        for (int i = 0; i < cnt; i++)
        {
            if (*(args[i]) == '>')
            {
                printf(">>>>> rd! arg[%d] = %s\n", (i + 1), args[i + 1]);
                rdFlg = i;
                newFd = redirect(args, i);
                // dup2(1, STDOUT_FILENO); // restore old fd
                // close(newFd);           // close new fd
                break;
            }
        }

        // special case: piping needs to fork to children
        int ppFlg = 0;
        if (rdFlg == 0)
        {
            ppFlg = checkPipe(cnt, args);
        }
        if (ppFlg > 0)
        {
            printf("pipe found, idx = %d \n", ppFlg);
            if (pipe(fd) < 0)
            {
                printf("Error: Pipe creatin failed.\n");
            }

            for (int k = (ppFlg + 1); k < cnt; k++)
            {
                secondArgs[k - ppFlg - 1] = args[k];
            }
            secondArgs[cnt - ppFlg - 1] = NULL;

            for (int j = 0; j < ppFlg; j++)
            {
                // strcpy(firstArgs[j], args[i]);
                firstArgs[j] = args[j];
            }
            firstArgs[ppFlg] = NULL;

            // firstArgs[0] = args[0];
            // secondArgs[0] = separate(args, ppFlg, cnt);
            printf("first args:%s, %s\n", firstArgs[0], firstArgs[1]);
            printf("second args: %s, %s\n", secondArgs[0], secondArgs[1]);

            int pid = fork();
            if (pid == 0)
            {
                if (signal(SIGINT, sigHandler) == SIG_ERR)
                {
                    printf("ERROR! Could not bind the signal hander.\n");
                    exit(1);
                }
                printf("!!!!!pipe\n");
                close(fd[0]);
                dup2(fd[1], STDOUT_FILENO);
                close(fd[1]);
                if (execvp(firstArgs[0], firstArgs) < 0)
                {
                    printf("Error: Command execution failed.\n");
                }
            }

            int pid2 = fork();
            if (pid2 == 0)
            {
                if (signal(SIGINT, sigHandler) == SIG_ERR)
                {
                    printf("ERROR! Could not bind the signal hander.\n");
                    exit(1);
                }
                printf("in parent, pipe 2nd part\n");
                close(fd[1]);
                dup2(fd[0], STDIN_FILENO);
                close(fd[0]);
                if (execvp(secondArgs[0], secondArgs) < 0)
                {
                    printf("Error: Command execution failed.\n");
                }
            }

            close(fd[0]);
            close(fd[1]);
            waitpid(pid, NULL, NULL);
            waitpid(pid2, NULL, NULL);
            continue;
        }

        int pid = fork(); // fork a child

        if (pid > 0 && bg == 1)
        {
            jobsList[lastIdx] = pid;
            lastIdx++;
            printf("add pid %d to jobs\n", pid);
            printf("current lastIdx = %d", lastIdx);
        }

        if (pid < 0)
        {
            printf("Forking failed.\n");
        }
        else if (pid == 0) // child process
        {
            printf(">>>>>>>>>>>>>>>>>>>in child pid = %d:\n", getpid());
            printf("rdFlg = %d\n", rdFlg);
            printf("ppFlg = %d\n", ppFlg);

            if (signal(SIGINT, sigHandler) == SIG_ERR)
            {
                printf("ERROR! Could not bind the signal hander.\n");
                exit(1);
            }

            // for commands other than redirection & piping
            if (execvp(args[0], args) < 0)
            {
                printf("Error: Command execution failed.\n");
            }
        }

        // parent process
        signal(SIGCHLD, cSigHandler);

        if (rdFlg > 0)
        {
            dup2(newFd, STDOUT_FILENO);
            // close(newFd);
            printf("restored fd\n");
        }

        if (bg == 0) // parent will wait if bg is not specified
        {
            wait(NULL);
            printf("pid parent: %d\n", getpid());
        }
    }
}
