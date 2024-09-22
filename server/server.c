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

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

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

#define BKLG_PND_CNCT   (5)
#define DEFAULT_FILE    ("/var/tmp/aesdsocketdata")
#define STRG_AVILBL     (64)

#define SYSTEM_ERROR    (-1)

/*
*/
void cleanup(int accept_fd, int socket_fd, int file_fd){

    remove(DEFAULT_FILE);

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
*/
int main(int argc, char** argv){

    
    int sock_fd, accpt_fd;
    int w_file_fd;
    int rc;

    struct addrinfo hints;
    struct addrinfo *server_info;
    // struct sockaddr bind_addr, client_addr;
    struct sockaddr client_addr;
    socklen_t client_addr_len;

    ssize_t rec_bytes, read_bytes, send_bytes; 
    ssize_t tot_rec_bytes = 0;
    ssize_t tot_read_bytes = 0;
    ssize_t write_bytes = 0;
    // ssize_t write_bytes;
    // int offset = 0;
    int buff_size;
    char *rec_buf;
    // char *send_buf;
    // char *token;

    client_addr_len = sizeof(client_addr);

    printf("opening syslog\n");
    openlog("aesdsocket: main", LOG_CONS|LOG_PERROR, LOG_USER);
    // FILE *w_file_fd = fopen(DEFAULT_FILE,'a+')
    w_file_fd = open(DEFAULT_FILE,O_RDWR|O_APPEND|O_CREAT, S_IRWXU|S_IRGRP|S_IROTH);

    if(w_file_fd == -1){

        syslog(LOG_ERR, "error opening %s: %s",DEFAULT_FILE,strerror(errno));
        return SYSTEM_ERROR;

    }


    // get socket file descriptor
    printf("getting socket file descriptor\n");
    sock_fd = socket(SOCK_DOMAIN, SOCK_STREAM, 0);
    if(sock_fd == -1){

        syslog(LOG_ERR,"socket error: %s", strerror(errno));

        cleanup(0,0,w_file_fd);
        // close(w_file_fd);
        // remove(DEFAULT_FILE);

        return SYSTEM_ERROR;

    }

    // set up hints addrinfo
    printf("setting up hints addrinfo struct\n");
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AI_FAM;
    hints.ai_socktype = SOCK_STREAM;

    // set up address info
    printf("getting address info\n");
    rc = getaddrinfo(NULL, SOCK_PORT, &hints, &server_info);
    if(rc != 0){

        // syslog(LOG_ERR, "getaddrinfor error: %s", gai_strerror(rc));
        syslog(LOG_ERR, "getaddrinfor error: %s", strerror(rc));
        
        cleanup(0,sock_fd,w_file_fd);
        // close(sock_fd);
        // close(w_file_fd);
        // remove(DEFAULT_FILE);

        return SYSTEM_ERROR;

    }

    // avoid address in use error
    printf("setting socket options\n");
    int option_val = 1;
    rc = setsockopt(sock_fd,SOL_SOCKET,SO_REUSEADDR,&option_val,sizeof(option_val));
    if(rc != 0) {

        syslog(LOG_ERR, "setsockopt error: %s", strerror(rc));
        
        // cleanup
        cleanup(0,sock_fd,w_file_fd);
        return SYSTEM_ERROR;
    } 

    // bind the address to socket
    printf("binding socket address\n");
    rc = bind(sock_fd, server_info->ai_addr, server_info->ai_addrlen);
    
    // free server info 
    freeaddrinfo(server_info);
    if(rc != 0){

        syslog(LOG_ERR, "bind error: %s", strerror(rc));
        
        // cleanup
        cleanup(0,sock_fd,w_file_fd);        
        return SYSTEM_ERROR;

    }    

    // continously check for connections
    while(1){

        // listen for connections
        rc = listen(sock_fd,BKLG_PND_CNCT);
        if(rc != 0){

            syslog(LOG_ERR, "listen error: %s", strerror(rc));
            
            // cleanup
            cleanup(0,sock_fd,w_file_fd);            
            return SYSTEM_ERROR;

        }  

        // accept connection
        accpt_fd = accept(sock_fd,&client_addr,&client_addr_len);

        if(accpt_fd == -1){

            syslog(LOG_ERR, "accept error: %s", strerror(errno));
            
            // cleanup
            cleanup(accpt_fd,sock_fd,w_file_fd);            
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
            cleanup(accpt_fd,sock_fd,w_file_fd);            
            return SYSTEM_ERROR;

        }
        // printf("%s\n",cip_str);
        syslog(LOG_NOTICE, "Accepted connection from %s",cip_str);


        // set up recieve buffer
        buff_size = STRG_AVILBL;
        rec_bytes = 0; 

        rec_buf = (char*)malloc(buff_size);
        // char *token;
        if(rec_buf == NULL){
            syslog(LOG_ERR,"failed to allocate %d to recieve buffer",buff_size);
            
            // cleanup
            cleanup(accpt_fd,sock_fd,w_file_fd);
            return SYSTEM_ERROR;
        }
        // make sure no weird data in buffer
        // memset(rec_buf,0,buff_size);
        // continously checking for messages
        bool done = false;
        while(!done){
            // // make sure no weird data in buffer
            memset(rec_buf,0,buff_size);
            // check for messages
            rec_bytes = recv(accpt_fd,rec_buf,buff_size, 0);
            tot_rec_bytes += rec_bytes;
            // printf("string %s will be saved\n",rec_buf);
            printf("recieved %ld bytes, recieved string: %s\n", rec_bytes,rec_buf);


            if(rec_bytes == -1){// error recieving
                syslog(LOG_ERR, "error recieving bytes: %s", strerror(errno));
                // cleanup
                cleanup(accpt_fd,sock_fd,w_file_fd);
                free(rec_buf);
                return SYSTEM_ERROR;
                
            }
            else if(rec_bytes == 0){// no data recieved, close connection
                printf("no data\n");
                syslog(LOG_NOTICE, "no data recieve, ending connection");
                // close accepted connection
                free(rec_buf);
                close(accpt_fd);
                continue;
                // break;
            }
            else{// data recieved, store to file
                if(rec_buf[rec_bytes-1] != (char)'\n'){
                // if(strch())
                    
                    printf("Packet not yet finished, will read next chunk\n");
                    done = false;
                }
                else{

                    // write_bytes += write(w_file_fd,"\n",strlen("\n"));
                    printf("packet fully received\n");
                    done = true;

                }
                write_bytes += write(w_file_fd,rec_buf,rec_bytes);

                printf("%ld bytes/ %s / written to %s\n", write_bytes,rec_buf,DEFAULT_FILE);

                if(write_bytes != tot_rec_bytes){
                    syslog(LOG_ERR, "error writing all bytes recieved to file");
                    // cleanup
                    cleanup(accpt_fd,sock_fd,w_file_fd);
                    // close(accpt_fd);
                    // close(sock_fd);
                    // close(w_file_fd);
                    // remove(DEFAULT_FILE);
                    free(rec_buf);

                    return SYSTEM_ERROR;
                }
                // break;
            
            }
        }
        free(rec_buf);

        
        // send contents of file back to client
        // char *read_buf;
        char rs_buf[buff_size]; // read and send buffer
        read_bytes = 0;
        send_bytes = 0;
        tot_read_bytes = 0;
        // memset(rs_buf,0,buff_size);
        // set file offset back to begining
        lseek(w_file_fd, 0, SEEK_SET);

        while(tot_read_bytes != write_bytes){
            // read
            memset(rs_buf,0,buff_size);
            read_bytes = read(w_file_fd,rs_buf,buff_size);
            tot_read_bytes += read_bytes;
            printf("%ld bytes read from file, %ld bytes total\n", read_bytes, tot_read_bytes);
            // offset += read_bytes;
            if(read_bytes == 0 ){// nothing read therfore nothing to send
                syslog(LOG_NOTICE, "no data read from file closing connection");
                close(accpt_fd);
                continue;
            }
            else if(read_bytes == -1){// error reading from file

                syslog(LOG_ERR, "ERROR reading file: %s", strerror(errno));
                // cleanup
                cleanup(accpt_fd,sock_fd,w_file_fd);
                return SYSTEM_ERROR;
            }
            else{// file read send data to client

                printf("data read: %s\n", rs_buf);

                send_bytes = send(accpt_fd, rs_buf, read_bytes,0);

                printf("bytes sent: %ld\n", send_bytes);

                if(send_bytes == -1){
                    syslog(LOG_ERR,"send error: %s",strerror(errno));

                    // cleanup
                    cleanup(accpt_fd,sock_fd,w_file_fd);
                    return SYSTEM_ERROR;
                }
                else if(send_bytes != read_bytes){
                    syslog(LOG_ERR,"failed to send all of the read bytes");

                    // cleanup
                    cleanup(accpt_fd,sock_fd,w_file_fd);
                    return SYSTEM_ERROR;
                }


            }
        }
        close(accpt_fd);
        syslog(LOG_NOTICE, "Closed connection from %s", cip_str);

    }

    closelog();
    // cleanup
    cleanup(accpt_fd,sock_fd,w_file_fd);
    return 0;
}