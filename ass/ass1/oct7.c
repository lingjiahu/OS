#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>

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
static void sigHandler(int sig)
{
    if (sig == SIGINT) // ctrl c, kill a process running in shell
    {
        // issue: getpid returns pid of parent not child
        // kill(pid, SIGKILL);
    }
    else if (sig == SIGTSTP) // ctrl z, ignore, does nothing
    {
        printf("\nThe SIGTSTP signal is ignored.\n");
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
// void exit()
// {

// }

// // brings specified job at idx to fg
// // optional idx, brings the first job to fg by default
// void fg(int idx)
// {

// }

// // lists all jobs running in bg
// void jobs()
// {

// }

// output redirection, return new file descriptor
int redirect(char *args[], int idx)
{
    int out = dup(STDOUT_FILENO);
    close(STDOUT_FILENO);
    char *fn = args[idx + 1]; // dest file for redirection
    int fd = open(fn, O_CREAT | O_WRONLY | O_RDONLY | O_TRUNC, S_IRWXU);
    args[idx] = NULL; // discard tokens from redirection sign '>'
    return fd;
}

int main(void)
{
    char *args[20];
    int bg;
    int p[10000][2]; // table for jobs: idx, pid

    int rdFlg = 0;        // 0: no redirection; 1: output redirection
    int newFd;            // value of new file descriptor for redirection
    int ppFlg = 0;        // 0: no piping; 1: piping
    char *firstArgs[20];  // first part of pipe
    char *secondArgs[20]; // second part of pipe
    int fd[2];            // fd used for piping

    while (1)
    {
        if (signal(SIGTSTP, sigHandler) == SIG_ERR || signal(SIGINT, sigHandler) == SIG_ERR)
        {
            printf("ERROR! Could not bind the signal hander.\n");
            exit(1);
        }

        bg = 0; // 0: run in fg; 1: run in bg
        int cnt = getcmd("\n>> ", args, &bg);
        int pid = fork(); // fork a child
        if (pid < 0)
        {
            printf("Forking failed.\n");
        }
        else if (pid == 0) // child process
        {
            printf(">>>>>>>>>>>>>>>>>>>in child pid = %d:\n", getpid());

            // check if '>' is present in args for output redirection
            for (int i = 0; i < cnt; i++)
            {
                if (*(args[i]) == '>')
                {
                    printf(">>>>>! arg[%d] = %s\n", i, args[i + 1]);
                    rdFlg = 1;
                    newFd = redirect(args, i);
                    break;
                }
                else if (*(args[i]) == 124) // 124 = ascii value for vertical bar used for piping
                {
                    ppFlg = 1;
                    // split the args
                    for (int j = 0; j < i; j++)
                    {
                        // strcpy(firstArgs[j], args[i]);
                        firstArgs[j] = args[j];
                    }
                    for (int k = (i + 1); k < cnt; k++)
                    {
                        secondArgs[k - i - 1] = args[k];
                    }

                    dup2(fd[1], STDOUT_FILENO);
                    close(fd[0]);
                    close(fd[1]);
                }
            }

            // check for built in commands
            char *cmd = args[0];
            if (strcmp(cmd, "cd") == 0)
            {
                cdCmd(args[1]);
            }
            else if (strcmp(cmd, "pwd") == 0)
            {
                pwdCmd();
            }
            else if (strcmp(cmd, "exit") == 0)
            {
            }
            else if (strcmp(cmd, "fg") == 0)
            {
            }
            else if (strcmp(cmd, "jobs") == 0)
            {
            }
            else // execute command
            {
                if (ppFlg == 1)
                {
                    printf("!!!!!pipe\n");
                    dup2(fd[0], STDIN_FILENO);
                    close(fd[0]);
                    close(fd[1]);
                    if (execvp(firstArgs[0], firstArgs))
                    {
                        printf("Error: Command execution failed.\n");
                    }
                }
                else
                {
                    if (execvp(args[0], args) < 0)
                    {
                        printf("Error: Command execution failed.\n");
                    }
                }
            }

            if (rdFlg == 1)
            {
                dup2(1, STDOUT_FILENO); // restore old fd
                close(newFd);           // close new fd
            }
            // else if (ppFlg == 1)
            // {
            //     int pid2 = fork();
            //     if (pid2 < 0)
            //     {
            //         printf("Forking failed.\n");
            //     }
            //     else if (pid2 == 0)
            //     {
            //         dup2(fd[0], STDIN_FILENO);
            //         close(fd[0]);
            //         close(fd[1]);
            //         if (execvp(secondArgs[0], secondArgs) < 0)
            //         {
            //             printf("Error: Command execution failed.\n");
            //         }
            //     }
            // }
        }

        else
        {
            printf("maybe here\n");
            if (ppFlg == 1)
            {
                printf("?????yes pipe\n");
                int pid2 = fork();
                if (pid2 < 0)
                {
                    printf("Forking failed.\n");
                }
                else if (pid2 == 0)
                {
                    if (execvp(secondArgs[0], secondArgs) < 0)
                    {
                        printf("Error: Command execution failed.\n");
                    }
                }
                else
                {
                    close(fd[0]);
                    close(fd[1]);
                    wait(NULL);
                    printf("pipeeeeee pid parent: %d\n", getpid());
                }
            }

            if (bg == 0) // parent will wait if bg is not specified
            {
                wait(NULL);
                printf("pid parent: %d\n", getpid());
            }
        }
    }
}
