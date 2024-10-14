#ifndef SERVER_H
#define SERVER_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <errno.h>
#include <unistd.h>

#include <syslog.h>

#include <signal.h>
#include <arpa/inet.h>

#include <pthread.h>

#include "queue.h"

typedef struct connection_params_s{

    pthread_t thread_ID;
    pthread_mutex_t *file_mutex;

    int accpt_fd;
    int file_fd;

    bool success;
    bool complete; 

    // char* rec_buf;
    char* cip_str;

    TAILQ_ENTRY(connection_params_s) next_connection;

}connection_params_t;

// typedef struct connection_params_s connection_params_t;

// Function prototypes;
void connection_cleanup(connection_params_t *cnnct_params, bool thread_error);
void *connection_thread(void* thread_params);

#endif