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

#define SYSTEM_ERROR    (-1)

// struct connection_params_s{

//     // int thread_ID;
//     pthread_t thread_ID;
//     pthread_mutex_t *file_mutex;

//     int accpt_fd;
//     int file_fd;
//     // int sock_fd;

//     bool success; 

//     char* rec_buf;
//     char* cip_str;

//     TAILQ_ENTRY(connection_params_s) next_connection;

// };

// connection_params_t thread_list;

TAILQ_HEAD(connection_head, connection_params_t) thread_list;
// thread_list_t;



int accpt_fd, sock_fd, w_file_fd;

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

    if(file_delete){
        remove(DEFAULT_FILE);
    }

    if(accept_fd){
        close(accept_fd);
    }

    if(socket_fd){
        close(socket_fd);
    }

    if(file_fd){
        close(file_fd);
    }

}


/*
*   @brief signal handler for SIGINT and SIGTERM
*/
void app_shutdown(){

    syslog(LOG_NOTICE, "Caught signal, exiting");

    int rc = shutdown(sock_fd, SHUT_RDWR);
    
    if(rc == -1){

        syslog(LOG_ERR, "ERROR: UNABLE TO GRACEFULLY SHUTDOWN SERVER: %s", strerror(errno));
    }

    cleanup(DELETE_FILE,accpt_fd, sock_fd, w_file_fd);

    exit(EXIT_SUCCESS);



}


/*
*   @brief app entry point
*/
int main(int argc, char** argv){

    signal(SIGINT, app_shutdown);
    signal(SIGTERM, app_shutdown);

    
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

    // thread_list.tqh_first->accpt_fd = 1;
    // printf("accept fd set to %d",thread_list.accpt_fd);
    // test(&thread_list.tqh_first);
    // exit(1);

    // create mutex
    pthread_mutex_t file_mutex;

    #ifdef USE_PRINT_DBUG
        printf("opening syslog\n");
    #endif
    openlog("aesdsocket: main", LOG_CONS|LOG_PERROR, LOG_USER);


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

    // create daemon if specified
    #ifdef USE_PRINT_DBUG
        printf("creating daemon\n");
    #endif
    if(daemon_flag){

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

    // continously check for connections
    #ifdef USE_PRINT_DBUG
        printf("listening for connections\n");
    #endif
    while(1){

        // listen for connections
        rc = listen(sock_fd,BACKLOG_CNCTS);
        if(rc != 0){

            syslog(LOG_ERR, "listen error: %s", strerror(rc));
            
            // cleanup
            cleanup(0,0,sock_fd,w_file_fd);            
            return SYSTEM_ERROR;

        }  

        // accept connection
        accpt_fd = accept(sock_fd,&client_addr,&client_addr_len);

        if(accpt_fd == -1){

            syslog(LOG_ERR, "accept error: %s", strerror(errno));
            
            // cleanup
            cleanup(0,accpt_fd,sock_fd,w_file_fd);            
            return SYSTEM_ERROR;

        }

        // translate client addr to readable ip
        char cip_str[client_addr_len];
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


        // // set up recieve buffer
        // buff_size = STRG_AVILBL;
        // rec_bytes = 0; 

        // rec_buf = (char*)malloc(buff_size);
        // // char *token;
        // if(rec_buf == NULL){
        //     syslog(LOG_ERR,"failed to allocate %d to recieve buffer",buff_size);
            
        //     // cleanup
        //     cleanup(0,accpt_fd,sock_fd,w_file_fd);
        //     return SYSTEM_ERROR;
        // }
        
        // // continously checking for messages
        // bool done = false;
        // while(!done){

        //     // // make sure no weird data in buffer
        //     memset(rec_buf,0,buff_size);

        //     // check for messages
        //     rec_bytes = recv(accpt_fd,rec_buf,buff_size, 0);
        //     tot_rec_bytes += rec_bytes;

        //     #ifdef USE_PRINT_DBUG
        //         printf("recieved %ld bytes, recieved string: %s\n", rec_bytes,rec_buf);
        //     #endif

        //     if(rec_bytes == -1){// error recieving
        //         syslog(LOG_ERR, "error recieving bytes: %s", strerror(errno));
        //         // cleanup
        //         cleanup(0,accpt_fd,sock_fd,w_file_fd);
        //         free(rec_buf);
        //         return SYSTEM_ERROR;
                
        //     }
        //     else if(rec_bytes == 0){// no data recieved, close connection
        //         syslog(LOG_NOTICE, "no data recieve, ending connection");

        //         // close accepted connection and free recieve buffer
        //         free(rec_buf);
        //         close(accpt_fd);
        //         continue;
        //     }
        //     else{// data recieved, store to file
        //         if(rec_buf[rec_bytes-1] != (char)'\n'){
        //         // if(strch())
        //             #ifdef USE_PRINT_DBUG
        //                 printf("Packet not yet finished, will read next chunk\n");
        //             #endif
        //             done = false;
        //         }
        //         else{

        //             #ifdef USE_PRINT_DBUG
        //                 printf("packet fully received\n");
        //             #endif
        //             done = true;

        //         }
        //         write_bytes += write(w_file_fd,rec_buf,rec_bytes);

        //         #ifdef USE_PRINT_DBUG
        //             printf("%ld bytes/ %s / written to %s\n", write_bytes,rec_buf,DEFAULT_FILE);
        //         #endif

        //         if(write_bytes != tot_rec_bytes){
        //             syslog(LOG_ERR, "error writing all bytes recieved to file");
        //             // cleanup
        //             cleanup(0,accpt_fd,sock_fd,w_file_fd);
        //             free(rec_buf);

        //             return SYSTEM_ERROR;
        //         }
        //         // break;
            
        //     }
        // }
        // // free recieve buffer
        // free(rec_buf);


        // // send contents of file back to client
        // char rs_buf[buff_size]; // read and send buffer
        // read_bytes = 0;
        // send_bytes = 0;
        // tot_read_bytes = 0;

        // // set file offset back to begining
        // lseek(w_file_fd, 0, SEEK_SET);

        // while(tot_read_bytes != write_bytes){
        //     // read
        //     memset(rs_buf,0,buff_size);
        //     read_bytes = read(w_file_fd,rs_buf,buff_size);
        //     tot_read_bytes += read_bytes;

        //      #ifdef USE_PRINT_DBUG
        //         printf("%ld bytes read from file, %ld bytes total\n", read_bytes, tot_read_bytes);
        //     #endif
            
        //     if(read_bytes == 0 ){// nothing read therfore nothing to send
        //         syslog(LOG_NOTICE, "no data read from file closing connection");
        //         close(accpt_fd);
        //         continue;
        //     }
        //     else if(read_bytes == -1){// error reading from file

        //         syslog(LOG_ERR, "ERROR reading file: %s", strerror(errno));
        //         // cleanup
        //         cleanup(0,accpt_fd,sock_fd,w_file_fd);
        //         return SYSTEM_ERROR;
        //     }
        //     else{// file read send data to client

        //         #ifdef USE_PRINT_DBUG
        //             printf("data read: %s\n", rs_buf);
        //         #endif

        //         send_bytes = send(accpt_fd, rs_buf, read_bytes,0);

        //         #ifdef USE_PRINT_DBUG
        //             printf("bytes sent: %ld\n", send_bytes);
        //         #endif

        //         if(send_bytes == -1){
        //             syslog(LOG_ERR,"send error: %s",strerror(errno));

        //             // cleanup
        //             cleanup(0,accpt_fd,sock_fd,w_file_fd);
        //             return SYSTEM_ERROR;
        //         }
        //         else if(send_bytes != read_bytes){
        //             syslog(LOG_ERR,"failed to send all of the read bytes");

        //             // cleanup
        //             cleanup(0,accpt_fd,sock_fd,w_file_fd);
        //             return SYSTEM_ERROR;
        //         }


        //     }
        // }
        // close(accpt_fd);
        // syslog(LOG_NOTICE, "Closed connection from %s", cip_str);

    }

    // cleanup
    closelog();
    cleanup(DELETE_FILE,accpt_fd,sock_fd,w_file_fd);
    return 0;
}