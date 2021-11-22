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
int fgChildPid = 0; // stores pid of child process running in fg
char *linecp;       // used to store the address of line before it got modified by strsep, freed at the end of each iteration of the while loop

int getcmd(char *prompt, char *args[], int *background)
{
    int length, i = 0;
    char *token, *loc;
    char *line = NULL;
    size_t linecap = 0;

    printf("%s", prompt);
    length = getline(&line, &linecap, stdin);

    linecp = line; // keep a copy of initial address of line

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
        if (fgChildPid != 0)
        {
            kill(fgChildPid, SIGKILL);
        }
    }
}

// remove terminated child process from jobs
void childRemove(int jobsList[], int *lastIdx, int pid)
{
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
            break;
        }
    }

    *lastIdx = *lastIdx - 1;

    for (int j = i; j < *lastIdx; j++)
    {
        jobsList[j] = jobsList[j + 1];
    }
}

// handle child termination signal, param sig is pid of terminated child
void cSigHandler(int sig)
{
    int status;
    int pid;
    pid = waitpid(-1, &status, WNOHANG);

    for (int i = 0; i < lastIdx; i++)
    {
        if (jobsList[i] == pid)
        {
            childRemove(jobsList, &lastIdx, pid);
        }
    }
}

// changes directory to param dir
void cdCmd(char *dir)
{
    if (chdir(dir) != 0) // returns 0 on success
    {
        printf("Failed to change to dir %s.\n", dir);
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
    for (int i = 0; i < lastIdx; i++) // terminate all jobs in bg
    {
        kill(jobsList[i], SIGTERM);
    }
    free(linecp);
    exit(0); // terminate shell
}

// brings specified job at idx to fg if passed a param
void fgCmdP(int idx)
{
    fgChildPid = jobsList[idx];
    int pid = waitpid(jobsList[idx], NULL, 0);
    childRemove(jobsList, &lastIdx, pid);
}

// no param, brings the first job to fg by default
void fgCmdN()
{
    fgChildPid = jobsList[0];
    int pid = waitpid(jobsList[0], NULL, 0);
    childRemove(jobsList, &lastIdx, pid);
}

// lists all jobs running in bg
void jobsCmd(int jobsList[], int lastIdx)
{
    printf("[Index] pid\n");
    for (int i = 0; i < lastIdx; i++)
    {
        printf("[%d]     %d\n", i, jobsList[i]);
    }
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

// return the index of vertical bar if found in args, otherwise return -1
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
        if (signal(SIGTSTP, SIG_IGN) == SIG_ERR)
        {
            printf("ERROR! Could not bind the signal hander.\n");
            exit(1);
        }

        signal(SIGCHLD, cSigHandler);

        bg = 0;        // 0: run in fg; 1: run in bg
        int rdFlg = 0; // 0: no redirection; 1: output redirection
        int newFd;     // value of new file descriptor for redirection

        int fd[2];                            // fd used for piping
        int cnt = getcmd("\n>> ", args, &bg); // cnt: number of tokens

        if (bg == 0)
        {
            if (signal(SIGINT, sigHandler) == SIG_ERR)
            {
                printf("ERROR! Could not bind the signal hander.\n");
                exit(1);
            }
        }
        else
        {
            if (signal(SIGINT, SIG_IGN) == SIG_ERR)
            {
                printf("ERROR! Could not bind the signal hander.\n");
                exit(1);
            }
        }

        if (cnt < 1) // check for empty input
        {
            free(linecp); // free memory allocated by malloc from line in getcmd()
            continue;
        }

        // check for built in commands
        char *cmd = args[0];
        if (strcmp(cmd, "cd") == 0)
        {
            cdCmd(args[1]);
            free(linecp); // free memory allocated by malloc from line in getcmd()
            continue;
        }
        else if (strcmp(cmd, "pwd") == 0)
        {
            pwdCmd();
            free(linecp); // free memory allocated by malloc from line in getcmd()
            continue;
        }
        else if (strcmp(cmd, "exit") == 0)
        {
            exitCmd();
        }
        else if (strcmp(cmd, "fg") == 0)
        {
            if (cnt > 1)
            {
                int idx = atoi(args[1]);
                if (idx >= lastIdx)
                {
                    printf("Error: there are only %d process(es) running in backgroud.\n", lastIdx);
                } else
                {
                   fgCmdP(idx);
                }
            }
            else if (lastIdx > 0)
            {
                fgCmdN();
            }
            free(linecp); // free memory allocated by malloc from line in getcmd()
            continue;
        }
        else if (strcmp(cmd, "jobs") == 0)
        {
            jobsCmd(jobsList, lastIdx);
            free(linecp); // free memory allocated by malloc from line in getcmd()
            continue;
        }

        // check if '>' is present in args for output redirection
        for (int i = 0; i < cnt; i++)
        {
            if (*(args[i]) == '>')
            {
                rdFlg = i;
                newFd = redirect(args, i);
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
            if (pipe(fd) < 0)
            {
                printf("Error: Pipe creatin failed.\n");
                continue;
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

            int pid = fork();
            if (pid < 0)
            {
                printf("Forking failed.\n");
                continue;
            }
            if (pid == 0)
            {
                close(fd[0]);
                dup2(fd[1], STDOUT_FILENO);
                close(fd[1]);
                if (execvp(firstArgs[0], firstArgs) < 0)
                {
                    printf("Error: Command execution failed.\n");
                    exit(1);
                }
            }

            int pid2 = fork();
            if (pid2< 0)
            {
                printf("Forking failed.\n");
                continue;
            }
            if (pid2 == 0)
            {
                close(fd[1]);
                dup2(fd[0], STDIN_FILENO);
                close(fd[0]);
                if (execvp(secondArgs[0], secondArgs) < 0)
                {
                    printf("Error: Command execution failed.\n");
                    exit(1);
                }
            }

            close(fd[0]);
            close(fd[1]);
            waitpid(pid, NULL, 0);
            waitpid(pid2, NULL, 0);
            free(linecp); // free memory allocated by malloc from line in getcmd()
            continue;
        }

        int pid = fork(); // fork a child

        if (pid > 0 && bg == 1)
        {
            jobsList[lastIdx] = pid;
            lastIdx++;
        }

        if (pid < 0)
        {
            printf("Forking failed.\n");
            continue;
        }
        else if (pid == 0) // child process
        {
            if (bg == 0)
            {
                fgChildPid = getpid();
            }

            // for commands other than redirection & piping
            if (execvp(args[0], args) < 0)
            {
                printf("Error: Command execution failed.\n");
                exit(1);
            }
        }

        if (rdFlg > 0)
        {
            dup2(newFd, STDOUT_FILENO);
        }

        if (bg == 0) // parent will wait if bg is not specified
        {
            wait(NULL);
        }

        free(linecp); // free memory allocated by malloc from line in getcmd()
    }
}