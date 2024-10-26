/*
*   @file server.c
*
*   @author Cedric Camacho
*   @date Oct 5, 2024
*
*   @brief connection thread code
*/


#include "server.h"

// #define USE_PRINT_DBUG
#define DEBUG_LOGS


#define BKLG_PND_CNCT   (5)

#ifdef USE_AESD_CHAR_DEVICE
#define DEFAULT_FILE    ("/dev/aesdchar")

#else
#define DEFAULT_FILE    ("/var/tmp/aesdsocketdata")
#endif

#define DELETE_FILE     (true)
#define STRG_AVILBL     (1024)

#define SYSTEM_ERROR    (-1)


void connection_cleanup(connection_params_t *cnnct_params, bool thread_error){



    if(cnnct_params->accpt_fd){
        close(cnnct_params->accpt_fd);
        cnnct_params->accpt_fd = 0;
        syslog(LOG_NOTICE, "NOTICE: Closed connection from %s", cnnct_params->cip_str);
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

    if(cnnct_params->file_fd){
        close(cnnct_params->file_fd);
        cnnct_params->file_fd = 0;
    }

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

    connection_p->file_fd = open(DEFAULT_FILE,O_RDWR|O_APPEND|O_CREAT, S_IRWXU|S_IRGRP|S_IROTH);

    if(connection_p->file_fd == -1){

        syslog(LOG_ERR, "error opening %s: %s",DEFAULT_FILE,strerror(errno));
        return NULL;

    }


    // set up recieve buffer
    // connection_p->rec_buf = (char*)malloc(buff_size);
    rec_buf = (char*)malloc(buff_size);

    // if(connection_p->rec_buf == NULL){
    if(rec_buf == NULL){
        syslog(LOG_ERR,"ERROR thread ID %ld: failed to allocate %d bytes to the recieve buffer",connection_p->thread_ID,buff_size);
        thread_error = true;
        connection_cleanup(connection_p, thread_error);
        return NULL;
    }

    // continously checking for messages
    bool done = false;
    // memset(connection_p->rec_buf,0,buff_size);
    memset(rec_buf,0,buff_size);
    while(!done){

        // check for messages
        // rec_bytes = recv(connection_p->accpt_fd,&connection_p->rec_buf[tot_rec_bytes],buff_size, 0);
        rec_bytes = recv(connection_p->accpt_fd,&rec_buf[tot_rec_bytes],buff_size, 0);
        tot_rec_bytes += rec_bytes;


        if(rec_bytes == -1){// error recieving
            syslog(LOG_ERR, "ERRROR thread ID %ld: recieve: %s", connection_p->thread_ID,strerror(errno));
            thread_error = true;
            break;
            
        }
        else if(rec_bytes == 0){// no data recieved, assumed done and continue
            syslog(LOG_NOTICE, "NOTICE: threadID %ld: no data recieved, assuming end of receive",connection_p->thread_ID);
            done = true;
            continue;
        }
        else{// data recieved, store to file
            // if(connection_p->rec_buf[tot_rec_bytes-1] != (char)'\n'){
            if(rec_buf[tot_rec_bytes-1] != (char)'\n'){

                // connection_p->rec_buf = realloc(connection_p->rec_buf,buff_size+tot_rec_bytes);
                rec_buf = realloc(rec_buf,buff_size+tot_rec_bytes);
                // if(connection_p->rec_buf == NULL){
                if(rec_buf == NULL){
                    syslog(LOG_ERR,"ERROR thread ID %ld: failed to re-allocate %d bytes to the recieve buffer",connection_p->thread_ID,buff_size);
                    thread_error = true;
                    break;
                }

            }
            else{

                done = true;

                #ifdef DEBUG_LOGS

                    syslog(LOG_INFO, "INFO: CONNECTION THREAD ID: %ld, %ld bytes recieved", connection_p->thread_ID, tot_rec_bytes);
                    syslog(LOG_INFO, "INFO: CONNECTION THREAD ID: %ld, string received: %s", connection_p->thread_ID, rec_buf);

                #endif


                rc = pthread_mutex_lock(connection_p->file_mutex);
                if(rc != 0){
                    syslog(LOG_ERR,"ERRROR thread ID %ld: mutex lock function failed: %s", connection_p->thread_ID,strerror(rc));
                    thread_error = true;
                    break;
                }
                
                // write_bytes += write(connection_p->file_fd,connection_p->rec_buf,tot_rec_bytes);
                write_bytes += write(connection_p->file_fd,rec_buf,tot_rec_bytes);
                

                rc = pthread_mutex_unlock(connection_p->file_mutex);
                if(rc != 0){
                    syslog(LOG_ERR,"ERRROR thread ID %ld: mutex unlock function failed: %s", connection_p->thread_ID,strerror(rc));
                    connection_cleanup(connection_p, thread_error);
                    exit(EXIT_FAILURE);
                }

                if(write_bytes != tot_rec_bytes){
                    syslog(LOG_ERR, "ERRROR thread ID %ld: not able to write all bytes recieved to file", connection_p->thread_ID);
                    thread_error = true;
                    break;
                }

                #ifdef DEBUG_LOGS

                    syslog(LOG_INFO, "INFO: CONNECTION THREAD ID: %ld, %ld of %ld bytes written to file", connection_p->thread_ID, write_bytes, tot_rec_bytes);
                    // syslog(LOG_INFO, "INFO: CONNECTION THREAD ID: %ld, string written: %s", connection_p->thread_ID, rec_buf);

                #endif

            }   
            // break;
        
        }
    }

    #ifdef DEBUG_LOGS

        syslog(LOG_INFO, "INFO: CONNECTION THREAD ID: %ld, recieved: %s", connection_p->thread_ID, rec_buf);

    #endif

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

        connection_cleanup(connection_p, thread_error);
        return NULL;
    }   
    // set file offset back to begining
    lseek(connection_p->file_fd, 0, SEEK_SET);


    // while(tot_read_bytes != write_bytes){
    while(!done){
        // read
        memset(rs_buf,0,buff_size);


        read_bytes = read(connection_p->file_fd,rs_buf,buff_size);


        tot_read_bytes += read_bytes;

        
        if(read_bytes == 0 ){// nothing read therfore nothing to send
            syslog(LOG_NOTICE, "NOTICE: all data read, closing connection");
            done = true;
            // close(connection_p->accpt_fd);
            continue;
        }
        else if(read_bytes == -1){// error reading from file

            syslog(LOG_ERR, "ERROR reading file: %s", strerror(errno));
            thread_error = true;
            break;
        }
        else{// file read send data to client

            #ifdef DEBUG_LOGS

                syslog(LOG_INFO, "INFO: CONNECTION THREAD ID: %ld, bytes read: %ld", connection_p->thread_ID, read_bytes);
                syslog(LOG_INFO, "INFO: CONNECTION THREAD ID: %ld, data read: %s", connection_p->thread_ID, rs_buf);

            #endif

            send_bytes = send(connection_p->accpt_fd, rs_buf, read_bytes,0);
            // send_bytes = send(connection_p->accpt_fd, "I AM NEWLINE\n", strlen("I AM NEWLINE\n"),0);


            #ifdef DEBUG_LOGS

                syslog(LOG_INFO, "INFO: CONNECTION THREAD ID: %ld, bytes sent: %ld", connection_p->thread_ID, send_bytes);
                syslog(LOG_INFO, "INFO: CONNECTION THREAD ID: %ld, string sent: %s", connection_p->thread_ID, rs_buf);

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
        thread_error = true;
        connection_cleanup(connection_p, thread_error);
        exit(EXIT_FAILURE);
    }

    
    connection_cleanup(connection_p, thread_error);
    return NULL;
}