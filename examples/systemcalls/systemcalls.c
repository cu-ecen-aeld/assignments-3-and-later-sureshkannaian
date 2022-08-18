#include "systemcalls.h"
#include <stdlib.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/
   if(cmd == NULL)  
   {
    return false;
   }
   
   int retVal = system(cmd);

   if(retVal == -1 || retVal == 127)  
   {
        return false;
   }

    return true;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    printf("**********************************\n");
    printf("Starting do_exec Test with %ld\n", (long) getpid());
    printf("**********************************\n");
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
        //printf("command arg %d = %s\n", i, command[i]);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    //command[count] = command[count];


        pid_t cpid, w;
        int wstatus;
        //printf("\nParent before fork\n");
        cpid = fork();
        if (cpid == -1) {
            perror("fork");
            
            return false;
        }


        if (cpid == 0) {            /* Code executed by child */
            //printf("Child PID is %ld\n", (long) getpid());

            //printf("child before execv with command=%s\n", command[0]);
            int retv = execv(command[0], command);
            //printf("child after execv with command=-->%s<--\n", command[0]);
            if (retv == -1)
            {
                printf("child execv returned error returning false\n");
                exit(EXIT_FAILURE);
            }

        } else {                    /* Code executed by parent */
            do {
                //printf("\n======================\n");
                //printf("Parent before wait\n");
                w = waitpid(cpid, &wstatus, WUNTRACED | WCONTINUED);
                //printf("Parent after wait\n");
                if (w == -1) {
                    perror("waitpid");
                    return false;
                }

                if (WIFEXITED(wstatus)) {
                    printf("Parent pid %ld had child exited, status=%d,=>%d\n", (long) getpid(), WEXITSTATUS(wstatus), wstatus);
                    if(WEXITSTATUS(wstatus) != 0)
                    {
                        printf("Parent- returning false to  main\n");
                        return false;
                    }
                    else 
                    {
                        printf("Parent- returning true to  main\n");
                        return true;
                    }
                } else if (WIFSIGNALED(wstatus)) {
                    //printf("Parent- child killed by signal %d\n", WTERMSIG(wstatus));
                } else if (WIFSTOPPED(wstatus)) {
                    //printf("Parent- child stopped by signal %d\n", WSTOPSIG(wstatus));
                } else if (WIFCONTINUED(wstatus)) {
                    //printf("parent - child continued\n");
                }
            } while (!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus));

            //exit(EXIT_SUCCESS);

        }




/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/

    va_end(args);


    return true;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/

        pid_t cpid, w;
        int wstatus;
        //printf("\nParent before fork\n");
        cpid = fork();
        if (cpid == -1) {
            perror("fork");
            
            return false;
        }


        if (cpid == 0) {            /* Code executed by child */
            //printf("Child PID is %ld\n", (long) getpid());

            //printf("child before execv with command=%s\n", command[0]);
	        int fd = open(outputfile, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	        dup2(fd, 1);            
            int retv = execv(command[0], command);
            close(fd);
            //printf("child after execv with command=-->%s<--\n", command[0]);
            if (retv == -1)
            {
                printf("child execv returned error returning false\n");
                exit(EXIT_FAILURE);
            }

        } else {                    /* Code executed by parent */
            do {
                //printf("\n======================\n");
                //printf("Parent before wait\n");
                w = waitpid(cpid, &wstatus, WUNTRACED | WCONTINUED);
                //printf("Parent after wait\n");
                if (w == -1) {
                    perror("waitpid");
                    return false;
                }

                if (WIFEXITED(wstatus)) {
                    printf("Parent pid %ld had child exited, status=%d,=>%d\n", (long) getpid(), WEXITSTATUS(wstatus), wstatus);
                    if(WEXITSTATUS(wstatus) != 0)
                    {
                        printf("Parent- returning false to  main\n");
                        return false;
                    }
                    else 
                    {
                        printf("Parent- returning true to  main\n");
                        return true;
                    }
                } else if (WIFSIGNALED(wstatus)) {
                    //printf("Parent- child killed by signal %d\n", WTERMSIG(wstatus));
                } else if (WIFSTOPPED(wstatus)) {
                    //printf("Parent- child stopped by signal %d\n", WSTOPSIG(wstatus));
                } else if (WIFCONTINUED(wstatus)) {
                    //printf("parent - child continued\n");
                }
            } while (!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus));

            //exit(EXIT_SUCCESS);

        }




    va_end(args);

    return true;
}
