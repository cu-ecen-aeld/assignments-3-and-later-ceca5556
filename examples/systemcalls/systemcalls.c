#include "systemcalls.h"
#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

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
    
    if(cmd == NULL){
    
      printf("ERROR: input command is Null");
      syslog(LOG_ERR,"ERROR: input command is Null");
      return false;
    
    }
    
    
    int err_code = system(cmd);
    
    if(err_code == -1){
    
      printf("ERROR: system call failed: %s", strerror(errno));
      syslog(LOG_ERR,"ERROR: system call failed: %s", strerror(errno));
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
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/

    pid_t c_pid, w_pid;
    c_pid = fork();
    
    if(c_pid == -1){
    
      printf("ERROR: fork call failed: %s", strerror(errno));
      syslog(LOG_ERR,"ERROR: fork call failed: %s", strerror(errno));
      return false;        
    
    }
    
    char** arg = command+1;
    
    int err_code = execv(command[0],arg);
    
    
    if(err_code == -1){
    
      printf("ERROR: execv call failed: %s", strerror(errno));
      syslog(LOG_ERR,"ERROR: execv call failed: %s", strerror(errno));
      return false;  
    
    }
    
    int wstatus;
    
    w_pid = waitpid(c_pid,&wstatus,0);
    
    if(w_pid == -1){
    
      printf("ERROR: wait failed: %s", strerror(errno));
      syslog(LOG_ERR,"ERROR: wait failed: %s", strerror(errno));
      return false;  
    
    }
    //else if(w_pid != c_pid){
    
      
    
    //}
    
    
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

    // open file
    int fd = open(outputfile, O_WRONLY|O_TRUNC|O_CREAT,0644);
    
    if(fd == -1){
    
      printf("ERROR: file %s failed to open: %s",outputfile, strerror(errno));
      syslog(LOG_ERR,"ERROR: file %s failed to open: %s",outputfile, strerror(errno));
      return false;        
    
    }

    //create fork
    pid_t c_pid = fork();
    
    if(c_pid == -1){
    
      printf("ERROR: fork call failed: %s", strerror(errno));
      syslog(LOG_ERR,"ERROR: fork call failed: %s", strerror(errno));
      return false;       
    
    } 
    
    // redirect
    int new_fd = dup2(fd,1);
    
    if(new_fd == -1){
    
      printf("ERROR: redirect failed: %s", strerror(errno));
      syslog(LOG_ERR,"ERROR: redirect failed: %s", strerror(errno));
      return false;       
    
    }
    
    
    close(fd);
    
    
    // execute execv
    char** arg = command+1;
    
    int err_code = execv(command[0],arg);
    
    
    if(err_code == -1){
    
      printf("ERROR: execv call failed: %s", strerror(errno));
      syslog(LOG_ERR,"ERROR: execv call failed: %s", strerror(errno));
      return false;  
    
    }
    
    int wstatus;
    
    pid_t w_pid = waitpid(c_pid,&wstatus,0);
    
    if(w_pid == -1){
    
      printf("ERROR: wait failed: %s", strerror(errno));
      syslog(LOG_ERR,"ERROR: wait failed: %s", strerror(errno));
      return false;  
    
    }
    //else if(w_pid != c_pid){
    
      
    
    //}
     
  
    va_end(args);

    return true;
}
