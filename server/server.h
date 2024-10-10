#ifndef SERVER_H
#define SERVER_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <errno.h>
#include <unistd.h>
// #include <fcntl.h>

#include <syslog.h>

#include <signal.h>
// #include <sys/types.h>
// #include <sys/socket.h>
// #include <netdb.h>
#include <arpa/inet.h>

#include <pthread.h>

#include "queue.h"

typedef struct connection_params_s{

    // int thread_ID;
    pthread_t thread_ID;
    // int other_id;
    pthread_mutex_t *file_mutex;
    // pthread_mutex_t *complete_mutex;

    int accpt_fd;
    int file_fd;
    // int sock_fd;

    bool success;
    bool complete; 

    char* rec_buf;
    char* cip_str;

    TAILQ_ENTRY(connection_params_s) next_connection;

}connection_params_t;

// typedef struct connection_params_s connection_params_t;

void connection_cleanup(connection_params_t *cnnct_params, bool thread_error);

// bool connection_thread_init();
void *connection_thread(void* thread_params);

#endif