/*
*   @file server.c
*
*   @author Cedric Camacho
*   @date Sep 18, 2024
*
*   @brief socket server code
*/



#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <syslog.h>

#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <pthread.h>

#include <time.h>

#include "server.h"
#include "queue.h"


// #define USE_PRINT_DBUG

#define IPV4            (0)
#define IPV6            (1)

#define SOCK_PORT       ("9000")
#define IP_FORMAT       (IPV4)

#if(IP_FORMAT)
#define SOCK_DOMAIN     (PF_INET6)
#define AI_FAM          (AF_INET6)

#else
#define SOCK_DOMAIN     (PF_INET)
#define AI_FAM          (AF_INET)

#endif

#define BACKLOG_CNCTS   (5)
#define DEFAULT_FILE    ("/var/tmp/aesdsocketdata")
#define DELETE_FILE     (true)
#define STRG_AVILBL     (64)

#define TSTAMP_LENGTH   (100) // stamp format: day(5 max) month(3) year(4) time(HH:MM:SS)(8)\n = 21 (not including any spaces,etc)
#define UPDATE_TIME     (10) // in seconds
// #define US_TO_S         (1e6) // micro seconds
// #define MS_TO_S         (1e3) // milliseconds

#define SYSTEM_ERROR    (-1)

// struct connection_params_s{

//     // int thread_ID;
//     pthread_t thread_ID;
    pthread_mutex_t file_mutex;
    // pthread_mutex_t complete_mutex;

//     int accpt_fd;
//     int file_fd;
//     // int sock_fd;

//     bool success; 

//     char* rec_buf;
//     char* cip_str;

//     TAILQ_ENTRY(connection_params_s) next_connection;

// };

// connection_params_t thread_list;

TAILQ_HEAD(connection_head, connection_params_s) thread_list;
// thread_list_t;



int accpt_fd, sock_fd, w_file_fd;
static sig_atomic_t sig_rec = false;

pthread_t cleanup_thread_ID = 0;
pthread_t timestamp_thread_ID = 0;

/*
*   @brief cleans up file descriptors and created file
*
*   @param[in] accept_fd
*       - file descriptor of the accepted connection with client
*
*   @param[in] socket_fd
*       - file descriptor of socket 
*
*   @param[in] file_fd
*       - file descriptor of file
*/
static void cleanup(bool file_delete, int accept_fd, int socket_fd, int file_fd){

    if(accept_fd){
        close(accept_fd);
    }

    if(socket_fd){
        close(socket_fd);
    }

    if(file_fd){
        close(file_fd);
    }

    if(file_delete){
        remove(DEFAULT_FILE);
    }

}


void signal_handler(){

    // syslog(LOG_NOTICE, "Caught signal, exiting");
    sig_rec = true;
    shutdown(sock_fd, SHUT_RDWR);

}

/*
*   @brief signal handler for SIGINT and SIGTERM
*/
void app_shutdown(){

    // syslog(LOG_NOTICE, "SHUTTING DOWN PROGRAM");
    // sig_rec = true;

    // pthread_kill(cleanup_thread_ID, SIGTERM);
    pthread_join(cleanup_thread_ID, NULL);
    pthread_kill(timestamp_thread_ID, SIGTERM);
    pthread_join(timestamp_thread_ID, NULL);

    // int rc = shutdown(sock_fd, SHUT_RDWR);
    int rc;
    // if(rc == -1){

    //     syslog(LOG_ERR, "ERROR: UNABLE TO GRACEFULLY SHUTDOWN SERVER: %s", strerror(errno));
    // }

    connection_params_t *cnnct_check = NULL;

    while(!TAILQ_EMPTY(&thread_list)){
        TAILQ_FOREACH(cnnct_check, &thread_list, next_connection){
        // TAILQ_FOREACH_SAFE(cnnct_check, &thread_list, next_connection,cnnct_check->next_connection.tqe_next){

            TAILQ_REMOVE(&thread_list, cnnct_check, next_connection);

            rc = pthread_kill(cnnct_check->thread_ID, SIGTERM);
            // pthread_kill(cnnct_check->thread_ID, SIGTERM);
            if(rc != 0){
                syslog(LOG_ERR,"ERROR SHUTTING DOWN, PTHREAD_KILL, ERROR NUM: %d",rc);
                exit(EXIT_FAILURE);
            }
            
            connection_cleanup(cnnct_check, NULL);

            free(cnnct_check);
            break;

        }
    }
    cleanup(DELETE_FILE,0, sock_fd, w_file_fd);

    // exit(EXIT_SUCCESS);

}

void *thread_complete_cleanup(){
// static void thread_complete_cleanup(){

    // connection_params_t *thread_params = sip->

    // printf("thread id: %ld");
    connection_params_t *cnnct_check = NULL;
    // connection_params_t *to_free = NULL;
    // int k = 5;
    while(!sig_rec){
        TAILQ_FOREACH(cnnct_check, &thread_list, next_connection){
        // TAILQ_FOREACH_SAFE(cnnct_check, &thread_list, next_connection,cnnct_check->next_connection.tqe_next){

            // if(to_free != NULL){
            //     printf("to_free points to %p while cnnct_check to %p", to_free, cnnct_check);
            //     free(to_free);
            // }

            // printf("thread ID%ld\n",cnnct_check->thread_ID);
            if(cnnct_check->complete){
                // cnnct_check->complete = false;
                pthread_join(cnnct_check->thread_ID,NULL);
                // printf("thread ID %ld, oID: %d, has completed %s\n",cnnct_check->thread_ID, cnnct_check->other_id,cnnct_check->success ? "successfully" : "in failure");
                syslog(LOG_NOTICE,"thread ID %ld has completed %s\n",cnnct_check->thread_ID,cnnct_check->success ? "successfully" : "in failure");
                TAILQ_REMOVE(&thread_list, cnnct_check, next_connection);
                // usleep(10);
                // pthread_mutex_unlock(cnnct_check->complete_mutex);
                // to_free = cnnct_check;
                free(cnnct_check);
                break;
                // cnnct_check = NULL;
            }

        }
        // k--;
        // usleep(100);
    }
    return NULL;
    // return;
}

void *timestamp_thread(){

    int rc;
    ssize_t write_bytes;
    // bool thread_error;
    char timestamp_buf[TSTAMP_LENGTH] = "timestamp:";
    // memset(timestamp_buf,0,TSTAMP_LENGTH);
    int stamp_start = strlen(timestamp_buf);

    time_t tme;
    struct tm *loc_time;
    // printf("start\n");
    while(!sig_rec){
        sleep(UPDATE_TIME);
        tme = time(NULL);
        loc_time = localtime(&tme);
        if(loc_time == NULL){
            syslog(LOG_ERR,"ERRROR timestamp thread, ID %ld: localtime function failed", timestamp_thread_ID);
            // thread_error = true;
            raise(SIGINT);
            // exit(EXIT_FAILURE);

        }
        // printf("got local time\n");

        rc = strftime(&timestamp_buf[stamp_start], TSTAMP_LENGTH, "%a, %d %b %Y %T %z", loc_time);
        if(rc == 0){

            syslog(LOG_ERR,"ERRROR timestamp thread, ID %ld: strftime returned 0", timestamp_thread_ID);
            // thread_error = true;
            raise(SIGINT);
            // exit(EXIT_FAILURE);

        }

        timestamp_buf[strlen(timestamp_buf)] = '\n';

        // printf("timestamp compplete\n");

        rc = pthread_mutex_lock(&file_mutex);
        if(rc != 0){
            syslog(LOG_ERR,"ERRROR timestamp thread, ID %ld: mutex lock function failed: %s", timestamp_thread_ID,strerror(rc));
            // thread_error = true;
            raise(SIGINT);
            // exit(EXIT_FAILURE);
        }

        write_bytes = write(w_file_fd,timestamp_buf,strlen(timestamp_buf));

        rc = pthread_mutex_unlock(&file_mutex);
        if(rc != 0){
            syslog(LOG_ERR,"ERRROR timestamp thread, ID %ld: mutex unlock function failed: %s", timestamp_thread_ID,strerror(rc));
            raise(SIGINT);
            // exit(EXIT_FAILURE);
        }

        // printf(" compplete");
        if(write_bytes != strlen(timestamp_buf)){

            syslog(LOG_ERR, "ERRROR timestamp thread, ID %ld: not able to write all bytes recieved to file", timestamp_thread_ID);
            raise(SIGINT);
            // exit(EXIT_FAILURE);

        }
        // printf("write compplete\n");

        // if(!sig_rec){
            // sleep(UPDATE_TIME);
        // }
    }

    return NULL;

}


/*
*   @brief app entry point
*/
int main(int argc, char** argv){

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    // signal(SIGUSR1,thread_complete_cleanup);

    
    // int sock_fd, accpt_fd, w_file_fd;
    int rc;

    struct addrinfo hints;
    struct addrinfo *server_info;
    struct sockaddr client_addr;
    socklen_t client_addr_len;

    client_addr_len = sizeof(client_addr);

    // ssize_t rec_bytes, read_bytes, send_bytes; 
    // ssize_t tot_rec_bytes = 0;
    // ssize_t tot_read_bytes = 0;
    // ssize_t write_bytes = 0;

    bool daemon_flag = false;

    // initialize thread link list
    TAILQ_INIT(&thread_list);


    #ifdef USE_PRINT_DBUG
        printf("opening syslog\n");
        openlog("aesdsocket", LOG_CONS|LOG_PERROR, LOG_USER);
    #else
        openlog("aesdsocket", 0, LOG_USER);
        // openlog("aesdsocket", LOG_CONS|LOG_PERROR, LOG_USER);
    #endif


    // check arguments
    int arg = 0;
    while(arg < argc){
        if(!(strcmp(argv[arg],"-d"))){
            daemon_flag = true;

            syslog(LOG_NOTICE,"daemon arg detected");
        }

        arg++;
    }

    // FILE *w_file_fd = fopen(DEFAULT_FILE,'a+')
    w_file_fd = open(DEFAULT_FILE,O_RDWR|O_APPEND|O_CREAT, S_IRWXU|S_IRGRP|S_IROTH);

    if(w_file_fd == -1){

        syslog(LOG_ERR, "error opening %s: %s",DEFAULT_FILE,strerror(errno));
        return SYSTEM_ERROR;

    }


    // get socket file descriptor
    #ifdef USE_PRINT_DBUG
        printf("getting socket file descriptor\n");
    #endif
    sock_fd = socket(SOCK_DOMAIN, SOCK_STREAM, 0);
    if(sock_fd == -1){

        syslog(LOG_ERR,"socket error: %s", strerror(errno));

        cleanup(0,0,0,w_file_fd);

        return SYSTEM_ERROR;

    }

    // set up hints addrinfo
    #ifdef USE_PRINT_DBUG
        printf("setting up hints addrinfo struct\n");
    #endif
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AI_FAM;
    hints.ai_socktype = SOCK_STREAM;

    // set up address info
    #ifdef USE_PRINT_DBUG
        printf("getting address info\n");
    #endif
    rc = getaddrinfo(NULL, SOCK_PORT, &hints, &server_info);
    if(rc != 0){

        // syslog(LOG_ERR, "getaddrinfor error: %s", gai_strerror(rc));
        syslog(LOG_ERR, "getaddrinfor error: %s", strerror(rc));
        
        cleanup(0,0,sock_fd,w_file_fd);
        return SYSTEM_ERROR;

    }

    // avoid address in use error
    #ifdef USE_PRINT_DBUG
        printf("setting socket options\n");
    #endif
    int option_val = 1;
    rc = setsockopt(sock_fd,SOL_SOCKET,SO_REUSEADDR,&option_val,sizeof(option_val));
    if(rc != 0) {

        syslog(LOG_ERR, "setsockopt error: %s", strerror(rc));
        
        // cleanup
        cleanup(0,0,sock_fd,w_file_fd);
        freeaddrinfo(server_info);
        return SYSTEM_ERROR;
    } 

    // bind the address to socket
    #ifdef USE_PRINT_DBUG
        printf("binding socket address\n");
    #endif
    rc = bind(sock_fd, server_info->ai_addr, server_info->ai_addrlen);
    
    // free server info 
    freeaddrinfo(server_info);
    if(rc != 0){

        syslog(LOG_ERR, "bind error: %s", strerror(rc));
        
        // cleanup
        cleanup(0,0,sock_fd,w_file_fd);        
        return SYSTEM_ERROR;

    }    

    if(daemon_flag){

        // create daemon if specified
        #ifdef USE_PRINT_DBUG
            printf("creating daemon\n");
        #endif

        rc = daemon(0,0);
        if(rc != 0){

            syslog(LOG_ERR,"ERROR: daemon creation: %s", strerror(errno));
            cleanup(0,0,sock_fd,w_file_fd); 
            return SYSTEM_ERROR;

        }
        
        syslog(LOG_NOTICE,"daemon created");


    }

    // initializing mutex
    rc = pthread_mutex_init(&file_mutex,NULL);
    if(rc != 0){

        syslog(LOG_ERR, "ERROR: could not succesfully initialize mutex, error number: %d", rc);
        return SYSTEM_ERROR;

    }

    #ifdef USE_PRINT_DBUG
        printf("creating cleanup thread\n");
    #endif

    // create cleanup thread
    rc = pthread_create(&cleanup_thread_ID,
                        NULL,
                        thread_complete_cleanup,
                        NULL);
    if(rc != 0){
        syslog(LOG_ERR, "ERROR: pthread create error, thread cleanup,with error number: %d", rc);
        return SYSTEM_ERROR;
    }

    // create timestamp thread
    rc = pthread_create(&timestamp_thread_ID,
                        NULL,
                        timestamp_thread,
                        NULL);
    if(rc != 0){
        syslog(LOG_ERR, "ERROR: pthread create error, time stamp cleanup,with error number: %d", rc);
        return SYSTEM_ERROR;
    }
    // cleanup_thread_ID = thread;

    // listen for connections
        rc = listen(sock_fd,BACKLOG_CNCTS);
        if(rc != 0){

            syslog(LOG_ERR, "listen error: %s", strerror(rc));
            
            // cleanup
            cleanup(0,0,sock_fd,w_file_fd);            
            return SYSTEM_ERROR;

        }

    pthread_t thread;

    // continously check for connections
    #ifdef USE_PRINT_DBUG
        printf("listening for connections\n");
    #endif
    while(1){

        // printf("sig_rec is %s\n",sig_rec ? "true" : "false");  

        // accept connection
        accpt_fd = accept(sock_fd,&client_addr,&client_addr_len);

        // printf("sig_rec is %s\n",sig_rec ? "true" : "false");

        if(sig_rec){
            break;
        }

        if(accpt_fd == -1){

            syslog(LOG_ERR, "accept error: %s", strerror(errno));
            
            // cleanup
            cleanup(0,accpt_fd,sock_fd,w_file_fd);            
            return SYSTEM_ERROR;

        }

        // translate client addr to readable ip
        char cip_str[client_addr_len];
        memset(cip_str,0,client_addr_len);
        #if(IP_FORMAT)// if ipv6
            const char* inet_ret = inet_ntop(client_addr.sa_family,(struct sockaddr_in6*)&client_addr,cip_str,client_addr_len);
        #else // if ipv4
            const char* inet_ret = inet_ntop(client_addr.sa_family,(struct sockaddr_in*)&client_addr,cip_str,client_addr_len);
        #endif
        if(inet_ret == NULL){

            syslog(LOG_ERR, "inet error: %s", strerror(errno));
            
            // cleanup
            cleanup(0,accpt_fd,sock_fd,w_file_fd);            
            return SYSTEM_ERROR;

        }
        // printf("%s\n",cip_str);
        syslog(LOG_NOTICE, "Accepted connection from %s",cip_str);

        connection_params_t *cnct_thr_params = (connection_params_t*)malloc(sizeof(connection_params_t));

        if(cnct_thr_params == NULL){
            syslog(LOG_ERR,"ERROR allocating enough memory for new thread parameters");
            cleanup(0,accpt_fd,sock_fd,w_file_fd);
            continue;
        }

        cnct_thr_params->file_mutex = &file_mutex;
        cnct_thr_params->accpt_fd = accpt_fd;
        cnct_thr_params->file_fd = w_file_fd;
        cnct_thr_params->success = false;
        cnct_thr_params->complete = false;
        cnct_thr_params->cip_str = (char*)malloc(client_addr_len);
        strcpy(cnct_thr_params->cip_str,cip_str);
        // memcpy(cnct_thr_params->cip_str,scip_str,client_addr_len);

        // cnct_thr_params->cip_str = (const char*)&cip_str;

        rc = pthread_create(&thread,
                            NULL,
                            connection_thread,
                            (void*) cnct_thr_params);

        if(rc != 0){
            syslog(LOG_ERR, "ERROR: connection pthread create error (error num: %d), closing connection", rc);
            cleanup(0,accpt_fd,0,0);    
            continue;
            // return SYSTEM_ERROR;
        }
        
        cnct_thr_params->thread_ID=thread;
        TAILQ_INSERT_TAIL(&thread_list, cnct_thr_params,next_connection);


    }

    // cleanup
    // printf("SHUTTING DOWN\n");
    syslog(LOG_NOTICE, "Caught signal, exiting");
    app_shutdown();
    closelog();
    // cleanup(DELETE_FILE,accpt_fd,sock_fd,w_file_fd);
    exit(EXIT_SUCCESS);
    return 0;
}