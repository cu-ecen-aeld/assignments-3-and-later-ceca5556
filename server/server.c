/*
*   @file server.c
*
*   @author Cedric Camacho
*   @date Sep 18, 2024
*
*   @brief socket server code
*/


#include "server.h"

// #define USE_PRINT_DBUG


#define BKLG_PND_CNCT   (5)
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
//     bool complete; 

//     char* rec_buf;
//     char* cip_str;

//     TAILQ_ENTRY(connection_params_s) next_connection;

// };

// typedef struct connection_params_s connection_params_t;

void connection_cleanup(connection_params_t *cnnct_params, bool thread_error){



    if(cnnct_params->accpt_fd){
        close(cnnct_params->accpt_fd);
        cnnct_params->accpt_fd = 0;
        syslog(LOG_NOTICE, "Closed connection from %s", cnnct_params->cip_str);
    }
    if(thread_error){
        cnnct_params->success = false;
    }
    else{
        cnnct_params->success = true;
    }

    cnnct_params->complete = true;

    // if(cnnct_params->rec_buf != NULL){

    //     free(cnnct_params->rec_buf);
    //     cnnct_params->rec_buf = NULL;

    // }

    if(cnnct_params->cip_str != NULL){

        free(cnnct_params->cip_str);
        cnnct_params->cip_str = NULL;

    }

    return;

}

void *connection_thread(void* thread_params){

    connection_params_t *connection_p = (connection_params_t *)thread_params;

    // unique to thread
    int rc;
    int buff_size = STRG_AVILBL;
    ssize_t rec_bytes, read_bytes, send_bytes; 
    ssize_t tot_rec_bytes = 0;
    ssize_t tot_read_bytes = 0;
    ssize_t write_bytes = 0;
    char *rec_buf;
    bool thread_error = false;

    // from thread_params
    // pthread_mutex_t *file_mutex = connection_p->file_mutex;
    // pthread_mutex_t *complete_mutex = connection_p->complete_mutex;

    // bool write_start = false;
    // bool write_finish = false;
    // bool read_start = false;
    // bool read_finish = false;

    // openlog("aesdsocket: thread function", LOG_CONS|LOG_PERROR, LOG_USER);

    // set up recieve buffer
    // connection_p->rec_buf = (char*)malloc(buff_size);
    rec_buf = (char*)malloc(buff_size);

    // char *token;
    // if(connection_p->rec_buf == NULL){
    if(rec_buf == NULL){
        syslog(LOG_ERR,"ERROR thread ID %ld: failed to allocate %d bytes to the recieve buffer",connection_p->thread_ID,buff_size);
        thread_error = true;
        connection_cleanup(connection_p, thread_error);
        // raise(SIGUSR1);
        return NULL;
    }

    // continously checking for messages
    bool done = false;
    // memset(connection_p->rec_buf,0,buff_size);
    memset(rec_buf,0,buff_size);
    while(!done){

        // // make sure no weird data in buffer

        // check for messages
        // rec_bytes = recv(connection_p->accpt_fd,&connection_p->rec_buf[tot_rec_bytes],buff_size, 0);
        rec_bytes = recv(connection_p->accpt_fd,&rec_buf[tot_rec_bytes],buff_size, 0);
        tot_rec_bytes += rec_bytes;

        #ifdef USE_PRINT_DBUG
            // printf("recieved %ld bytes now have %ld total bytes, recieved string: %s\n", rec_bytes,tot_rec_bytes,connection_p->rec_buf);
            printf("recieved %ld bytes now have %ld total bytes, recieved string: %s\n", rec_bytes,tot_rec_bytes,rec_buf);
        #endif

        if(rec_bytes == -1){// error recieving
            syslog(LOG_ERR, "ERRROR thread ID %ld: recieve: %s", connection_p->thread_ID,strerror(errno));
            thread_error = true;
            break;
            
        }
        else if(rec_bytes == 0){// no data recieved, assumed done and continue
            syslog(LOG_NOTICE, "NOTICE threadID %ld: no data recieved, assuming end of receive",connection_p->thread_ID);
            done = true;
            continue;
        }
        else{// data recieved, store to file
            // if(connection_p->rec_buf[tot_rec_bytes-1] != (char)'\n'){
            if(rec_buf[tot_rec_bytes-1] != (char)'\n'){
            // if(strch())
                #ifdef USE_PRINT_DBUG
                    printf("Packet not yet finished, will re-connect with bigger buffer\n");
                #endif

                // buff_size += rec_bytes;
                // connection_p->rec_buf = realloc(connection_p->rec_buf,buff_size+tot_rec_bytes);
                rec_buf = realloc(rec_buf,buff_size+tot_rec_bytes);
                // if(connection_p->rec_buf == NULL){
                if(rec_buf == NULL){
                    syslog(LOG_ERR,"ERROR thread ID %ld: failed to re-allocate %d bytes to the recieve buffer",connection_p->thread_ID,buff_size);
                    
                    // cleanup
                    // cleanup(accpt_fd,sock_fd,w_file_fd);
                    // cleanup(0,connection_p->accpt_fd,0,0);
                    // free(connection_p->rec_buf);
                    // connection_p->success = false;
                    thread_error = true;
                    // return thread_params;
                    break;
                }

            }
            else{

                #ifdef USE_PRINT_DBUG
                    printf("packet fully received\n");
                #endif

                done = true;


                rc = pthread_mutex_lock(connection_p->file_mutex);
                if(rc != 0){
                    syslog(LOG_ERR,"ERRROR thread ID %ld: mutex lock function failed: %s", connection_p->thread_ID,strerror(rc));
                    // cleanup(0,connection_p->accpt_fd,0,0);
                    // free(connection_p->rec_buf);
                    // connection_p->success = false;
                    thread_error = true;
                    // return thread_params;
                    break;
                }
                
                // write_bytes += write(connection_p->file_fd,connection_p->rec_buf,tot_rec_bytes);
                write_bytes += write(connection_p->file_fd,rec_buf,tot_rec_bytes);
                

                rc = pthread_mutex_unlock(connection_p->file_mutex);
                if(rc != 0){
                    syslog(LOG_ERR,"ERRROR thread ID %ld: mutex unlock function failed: %s", connection_p->thread_ID,strerror(rc));
                    // cleanup(0,connection_p->accpt_fd,0,0);
                    // free(connection_p->rec_buf);
                    // connection_p->success = false;
                    // thread_error = true;
                    // return thread_params;
                    connection_cleanup(connection_p, thread_error);
                    exit(EXIT_FAILURE);
                }


                #ifdef USE_PRINT_DBUG
                    // printf("%ld bytes/ %s / written to %s\n", write_bytes,connection_p->rec_buf,DEFAULT_FILE);
                    printf("%ld bytes/ %s / written to %s\n", write_bytes,rec_buf,DEFAULT_FILE);
                #endif

                if(write_bytes != tot_rec_bytes){
                    syslog(LOG_ERR, "ERRROR thread ID %ld: not able to write all bytes recieved to file", connection_p->thread_ID);
                    // cleanup
                    // cleanup(accpt_fd,sock_fd,w_file_fd);
                    // cleanup(0,connection_p->accpt_fd,0,0);
                    // free(connection_p->rec_buf);
                    thread_error = true;
                    // return thread_params;
                    break;
                }

            }   
            // break;
        
        }
    }

    // free recieve buffer
    // free(connection_p->rec_buf);
    free(rec_buf);

    // connection_p->rec_buf = NULL;
    rec_buf = NULL;

    if(thread_error){
        connection_cleanup(connection_p, thread_error);
        return NULL;

    }


    // send contents of file back to client
    buff_size = STRG_AVILBL;
    char rs_buf[buff_size]; // read and send buffer
    read_bytes = 0;
    send_bytes = 0;
    tot_read_bytes = 0;
    done = false;


    rc = pthread_mutex_lock(connection_p->file_mutex);
    if(rc != 0){
        syslog(LOG_ERR,"ERRROR thread ID %ld: mutex lock function failed: %s", connection_p->thread_ID,strerror(rc));
        // cleanup(0,connection_p->accpt_fd,0,0);
        // free(connection_p->rec_buf);
        // close(connection_p->accpt_fd);
        // syslog(LOG_NOTICE, "Closed connection from %s", connection_p->cip_str);
        // connection_p->success = false;
        // connection_p->complete = true;
        // return thread_params;
        connection_cleanup(connection_p, thread_error);
        // raise(SIGUSR1);
        return NULL;
    }   
    // set file offset back to begining
    lseek(connection_p->file_fd, 0, SEEK_SET);

    // rc = pthread_mutex_unlock(connection_p->file_mutex);
    // if(rc != 0){
    //     syslog(LOG_ERR,"ERRROR thread ID %ld: mutex unlock function failed: %s", connection_p->thread_ID,strerror(rc));
    //     cleanup(0,connection_p->accpt_fd,0,0);
    //     free(connection_p->rec_buf);
    //     connection_p->success = false;
    //     return thread_params;
    // }
    // while(tot_read_bytes != write_bytes){
    while(!done){
        // read
        memset(rs_buf,0,buff_size);

        // rc = pthread_mutex_lock(connection_p->file_mutex);
        // if(rc != 0){
        //     syslog(LOG_ERR,"ERRROR thread ID %ld: mutex lock function failed: %s", connection_p->thread_ID,strerror(rc));
        //     cleanup(0,connection_p->accpt_fd,0,0);
        //     free(connection_p->rec_buf);
        //     connection_p->success = false;
        //     return thread_params;
        // }   

        read_bytes = read(connection_p->file_fd,rs_buf,buff_size);

        // rc  = pthread_mutex_unlock(connection_p->file_mutex);
        // if(rc != 0){
        //     syslog(LOG_ERR,"ERRROR thread ID %ld: mutex unlock function failed: %s", connection_p->thread_ID,strerror(rc));
        //     cleanup(0,connection_p->accpt_fd,0,0);
        //     free(connection_p->rec_buf);
        //     connection_p->success = false;
        //     return thread_params;
        // }

        tot_read_bytes += read_bytes;

        #ifdef USE_PRINT_DBUG
            printf("%ld bytes read from file, %ld bytes total\n", read_bytes, tot_read_bytes);
        #endif
        
        if(read_bytes == 0 ){// nothing read therfore nothing to send
            syslog(LOG_NOTICE, "all data read, closing connection");
            done = true;
            // close(connection_p->accpt_fd);
            continue;
        }
        else if(read_bytes == -1){// error reading from file

            syslog(LOG_ERR, "ERROR reading file: %s", strerror(errno));
            thread_error = true;
            // cleanup
            // cleanup(0,connection_p->accpt_fd,sock_fd,w_file_fd);
            // rc  = pthread_mutex_unlock(connection_p->file_mutex);
            // cleanup(0,connection_p->accpt_fd,0,0);
            // connection_p->success = false;
            // return thread_params;
            break;
        }
        else{// file read send data to client

            #ifdef USE_PRINT_DBUG
                printf("data read: %s\n", rs_buf);
            #endif

            send_bytes = send(connection_p->accpt_fd, rs_buf, read_bytes,0);

            #ifdef USE_PRINT_DBUG
                printf("bytes sent: %ld\n", send_bytes);
            #endif

            if(send_bytes == -1){
                syslog(LOG_ERR,"send error: %s",strerror(errno));
                thread_error = true;
                break;
            }
            else if(send_bytes != read_bytes){
                syslog(LOG_ERR,"failed to send all of the read bytes");
                thread_error = true;
                break;
            }


        }
    }



    rc  = pthread_mutex_unlock(connection_p->file_mutex);
    if(rc != 0){
        syslog(LOG_ERR,"ERRROR thread ID %ld: mutex unlock function failed: %s", connection_p->thread_ID,strerror(rc));
        connection_cleanup(connection_p, thread_error);
        exit(EXIT_FAILURE);
    }

    
    connection_cleanup(connection_p, thread_error);

    // raise(SIGUSR1);
    // return thread_params;
    return NULL;
}