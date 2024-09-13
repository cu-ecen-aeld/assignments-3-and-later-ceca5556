#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
//#include <linux/delay.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

#define MS_TO_US (1000)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    thread_data_t *threadfunc_params = (thread_data_t *) thread_param;
    int rc;
    
    usleep(threadfunc_params->obtain_wait*MS_TO_US);
    rc = pthread_mutex_lock(threadfunc_params->mutex);
    //pthread_mutex_lock(threadfunc_params->mutex);
    
    if(rc != 0){
    
      ERROR_LOG("mutex lock function failed: %s", strerror(rc));
      threadfunc_params->thread_complete_success = false;
      return thread_param;
    
    }
    
    
    usleep(threadfunc_params->release_wait*MS_TO_US);
    rc = pthread_mutex_unlock(threadfunc_params->mutex);
    //pthread_mutex_unlock(threadfunc_params->mutex);
    
    if(rc != 0){
    
      ERROR_LOG("mutex unlock function failed: %s", strerror(rc));
      threadfunc_params->thread_complete_success = false;
      return thread_param;
    
    }
    
    threadfunc_params->thread_complete_success = true;
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    
    bool output = true;
    int rc;
    
    thread_data_t *thread_dat = malloc(sizeof(thread_data_t));
    
    if(thread_dat == NULL){
    
      ERROR_LOG("could not allocate memory for thread data");
      output = false;
      return output;
    
    }
    
    thread_dat->mutex = mutex;
    thread_dat->obtain_wait = wait_to_obtain_ms;
    thread_dat->release_wait = wait_to_release_ms;
    
    rc = pthread_create(thread,              // thread descriptor pointer
                        NULL,                // attributes -> null = default
                        threadfunc,          // thread function entry point
                        (void*)thread_dat);  // parameters to pass in
                   
    if(rc != 0){
    
      ERROR_LOG("pthread create: %s", strerror(rc));
      output = false;
    }
    //else{
      
    //  rc = pthread_join(thread,NULL);
      
    //  if(rc != 0){
      
    //    ERROR_LOG("pthread join: %s", strerror(rc));
    //    output = false;
      
    //  }
    //  else{
    //    output = 
    //  }
    
    //}
    
    //free(thread_dat);
    return output;
}

