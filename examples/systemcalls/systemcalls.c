#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include "systemcalls.h"

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
	
	int resp = system(cmd);
	
	if(resp == -1)
	{
		return false;
		perror("system Call Error");
	}
	else
	{
		return true;
	}
	
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
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    // command[count] = command[count];

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/
	bool retVal = false;
	pid_t pid = fork();
	
	if(pid == -1)
	{
		perror("fork error");
	}
	if(pid == 0) /// child process
	{
		int resp = execv(command[0], command);
		
		if(resp == -1)
		{
			perror("execv error");
			exit(-1);
		}
	}
	else
	{
		int waitStat;
		pid_t retPid = wait(&waitStat);
		
		if(retPid == -1)
		{
			perror("wait error");
		}
		else /// retPid == pid
		{
			if(WIFEXITED(waitStat))
			{
				printf("exit stat: %d\n", WEXITSTATUS(waitStat));
				if(WEXITSTATUS(waitStat) > 0)
				{
					perror("exit error");
				}
				else
				{
					retVal = true;
				}
			}
		}
	}
    va_end(args);
    return retVal;
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
    
    pid_t pid = 0;
    int wait_ret = 0;
    int wait_status = 0;
    bool retVal = true;
    int temp_fd = 0;
    
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    // command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/

	temp_fd = open(outputfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
	if (temp_fd < 0)
	{
		perror("open");
		retVal = false;
	}
	else
	{
		pid = fork();
		if (pid == -1)
		{
			perror("fork");
			retVal = false;
		}
		else if (pid == 0)
		{
			if (dup2(temp_fd, 1) < 0)
			{
				perror("dup2");
				retVal = false;
			}
			else
			{
				close(temp_fd);
				execv(command[0], (char *const *)command);
				exit(-1);
			}
		}
		else
		{
			//Parent process do nothing
		}
		
		wait_ret = waitpid(pid, &wait_status, 0);
		if (wait_ret == -1)
		{
			perror("wait");
			retVal = false;
		}
		else if (WIFEXITED(wait_status))
		{
			if (WEXITSTATUS(wait_status) > 0)
			{
				printf("Command failed\n");
				retVal = false;
			}
		}
		else
		{
			printf("Unexpected exit status %d\n",WEXITSTATUS(wait_status));
		}
	}
    va_end(args);
    return retVal;
}
