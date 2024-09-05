/*
*  @file writer.c
*
*  @date Sep 3, 2024
*  @author Cedric Camacho
*
*  @brief writes input string to specified file
*
*/

// file defines
#define SYSTEM_ERROR (1)


// includes
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <string.h>


/*
*
*  @brief entry point of writer.c
*
*/
int main(int argc, const char** argv){

  // open log
  openlog(NULL,0,LOG_USER);

  // check amount of arguments (./call file_dir string)
  if(argc != 3){
  
    syslog(LOG_ERR,"\nERROR: incorrect number of input arguments\n"
                   "\rArguments are:\n"
                   "\rArgument 1: file location\n"
                   "\rArgument 2: string to write\n");
                   
    exit(SYSTEM_ERROR);
  
  }
  
  // save arguments to variables
  const char *file_dir = argv[1];
  const char *str = argv[2];
  
  // open the file specified
  //FILE *file = fopen(argv[1],w);
  FILE *file = fopen(file_dir,"w");
  
  
  // check if file was opened
  if(file == NULL){
  
    syslog(LOG_ERR, "ERROR: could not open file %s", file_dir);
    
    exit(SYSTEM_ERROR);
  
  }
  
  // find length of string
  //size_t str_size = strlen(argv[2]);
  size_t str_size = strlen(str);
  
  // write string to file
  size_t num_write = fwrite(str, sizeof(char), str_size, file);
  syslog(LOG_DEBUG,"Writing '%s' to %s",str,file_dir);
  
  // check bytes of input string to number of bytes written
  if(num_write != str_size){
  
    syslog(LOG_ERR, "ERROR: could not successfully write input string '%s' to file '%s'",str,file_dir);
    
    exit(SYSTEM_ERROR);
  
  }
  
  closelog();

}