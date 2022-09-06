#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>

bool terminalRcd;


void updateTermFlag( int signum )
{
    if ( signum == SIGINT || signum == SIGTERM ) 
    {
	    terminalRcd = true;	
    }
}

int main(int argc, char** argv)
{
    openlog(NULL,0,LOG_USER);
    




    // initialize termination flag
    terminalRcd = false;



   // register hander for SIGINT and SIGTERM
    struct sigaction termReceiver;
    memset(&termReceiver, 0, sizeof(struct sigaction));
    termReceiver.sa_handler = updateTermFlag;
    if ( sigaction(SIGTERM, &termReceiver, NULL) != 0 )
    {
        syslog(LOG_ERR,"Failed SIGTERM  registration with error %s",strerror(errno));
	    return -1;
    }
    
    if ( sigaction(SIGINT, &termReceiver, NULL) != 0 )
    {
        syslog(LOG_ERR, "Failed SIGINT  registration with error %s",strerror(errno));
        return -1;
    }
    syslog(LOG_DEBUG, "Registered signal handlers");


    bool daemonMode = false;
    if ( argc == 2 && ( strcmp(argv[1], "-d") == 0 ) ) 
    {
        daemonMode = true;
	    syslog(LOG_DEBUG, "Running server as daemon");
    }
    else if ( argc > 1 ) {
        syslog(LOG_ERR, "Invalid input use aesdsocket [-d]");
    }    

    // setup server
    int listnerSocket = socket(PF_INET,SOCK_STREAM,0);
    if ( listnerSocket == -1 ) {
        syslog(LOG_ERR, "Failed initializing socket, exiting : %s",strerror(errno));
	    return -1;
    }
    
    struct addrinfo hints;
    struct addrinfo *srvInfo;  

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM; 
    hints.ai_flags = AI_PASSIVE;  

    int retValue = getaddrinfo(NULL, "9000", &hints, &srvInfo);
    if (retValue != 0)
    {
        syslog(LOG_ERR, "getaddrinfo: %s\n", gai_strerror(retValue));
        return -1;
    }

    if ( bind(listnerSocket,srvInfo->ai_addr,sizeof(struct sockaddr)) != 0 ) {
        syslog(LOG_ERR, "Failed socket bind to address, exiting : %s", strerror(errno));
	    freeaddrinfo(srvInfo);
	    close(listnerSocket);
	    return -1;
    }

    // free server address info
    freeaddrinfo(srvInfo);

    // bind successful, if daemon mode, fork here   
    pid_t fpid;
    int nullfd;

    if (daemonMode)
    {
         fpid = fork(); 
    }

    if (daemonMode)
    {
        if ( fpid == -1 ) 
        {
            syslog(LOG_ERR, "Fork failed exiting %s",strerror(errno));
            close(listnerSocket);
            return -1;
        }
	
        if ( fpid == 0 ) 
        {
            // i am a child  
            // set up itself as a daemon
            if (setsid() == -1 )
            {
                syslog(LOG_ERR, "Daemon failed starting own session %s", strerror(errno));
                close(listnerSocket);
                return -1;
            }
            if (chdir("/") == -1 ) 
            {
                    syslog(LOG_ERR, "Daemon failed to do chdir : %s", strerror(errno));
                    close(listnerSocket);
                    return -1;
            }
            if ((nullfd = open("/dev/null", O_RDWR) ) == -1 ) 
            {
                    syslog(LOG_ERR, "Daemon failed opening /dev/null with %s", strerror(errno));
                    close(listnerSocket);
                    return -1;
            }
            if ( dup2(nullfd,1) < 0 ) // for the rest of this process, stdout goes to null, not tty
            {
                syslog(LOG_ERR, "Daemon redirecting stdout to /dev/null failed with %s", strerror(errno));
                close(listnerSocket);
                close(nullfd);
                return -1;
            }
            if ( dup2(nullfd,2) < 0 ) // for the rest of this process, stderr goes to null, not tty
            {
                syslog(LOG_ERR, "Daemon failed redirecting stderr to /dev/null : %s", strerror(errno));
                close(listnerSocket);
                close(nullfd);
                return -1;
            }
            if ( dup2(nullfd,0) < 0 ) // for the rest of this process, stdin comes from null, not tty
            {
                syslog(LOG_ERR, "Daemon failed redirecting stdin to /dev/null : %s", strerror(errno));
                close(listnerSocket);
                close(nullfd);
                return -1;
            }
            // all std streams point to null, can close fd from open to null
            close(nullfd);
        }
        else
        {
            // parent
            // close fd on listener socket and exit successfully
            close(listnerSocket);
            return 0;
        }

    }

    // after this either the child has dropped through to opening the server, or this is not daemon


    if ( listen(listnerSocket,2) != 0 )
    {
        syslog(LOG_ERR, "Failed socket listen, exiting : %s", strerror(errno));
	    close(listnerSocket);
	    return -1;
    }
   




    // client file handle
    FILE *fHandle = fopen("/var/tmp/aesdsocketdata","a+");
    if ( fHandle == NULL ) {
        syslog(LOG_ERR,"Failed in opening /var/tmp/aesdsocketdata with error %s", strerror(errno));
        close(listnerSocket);
        return -1;
    }


    // initialize, start with 1 kB
    size_t buffInc = 1024; 
    size_t buffSize = buffInc;
    size_t sizeToRead = buffSize;
    void *allocBase = malloc(buffSize*sizeof(char)); // start with 1 kB
    void *buff = allocBase;
    void *incStart = buff; // increment start, use char*
    memset(buff, 0, buffSize*sizeof(char));
    ssize_t numRead;
    ssize_t numWritten;
    bool dropped = false;    


    while ( true ) 
    {
        
        struct sockaddr_in clientAddress;
        memset(&clientAddress,0,sizeof(clientAddress));
        size_t claddrsz = sizeof(clientAddress);
        int acceptedSocket = accept(listnerSocket, (struct sockaddr *)&clientAddress, (socklen_t *)&claddrsz);
        if ( acceptedSocket == -1 ) 
        {
            if ( errno == EINTR )
            {
                syslog(LOG_INFO, "Caught signal, exiting");
                free(allocBase);
                if ( fclose(fHandle) != 0 ) {
                    syslog(LOG_ERR, "Failed closing client file, %s",strerror(errno));
                }
                if ( close(listnerSocket) == -1 ) {
                    syslog(LOG_ERR, "Failed closing listener socket, %s",strerror(errno));
                }
                if ( remove("/var/tmp/aesdsocketdata") == -1 ) {
                    syslog(LOG_ERR, "Failed removing client file, %s",strerror(errno));
                }

                return 0;

            }
            else
            {
                    syslog(LOG_ERR, "Failed connection accept, exiting : %s", strerror(errno));
                    free(allocBase);
                    if ( fclose(fHandle) != 0 ) {
                        syslog(LOG_ERR, "Failed closing client file, %s",strerror(errno));
                    }
                    if ( close(listnerSocket) == -1 ) {
                        syslog(LOG_ERR, "Failed closing listener socket, %s",strerror(errno));
                    }
            // leave temp file as an artifact of unclean exit
                    return -1;
            }
        }

        // connection established, need to log, writing address in capital hex
        syslog(LOG_DEBUG,"Accepted connection from %X",clientAddress.sin_addr.s_addr);

        while ((numRead = recv(acceptedSocket, incStart,  sizeToRead, 0 ))  > 0 )
        {
            // check the last byte of this increment
            char *last = (char*)incStart + numRead - 1;
            if ( *last == '\n' )
            {
                // packet is complete
                size_t packet_length = buffSize - sizeToRead + numRead;
                syslog(LOG_DEBUG, "Received packet of size %ld",packet_length);
        
            // catch corner case where we perfectly filled the buffer increment
            if ( numRead == sizeToRead )
            {
                syslog(LOG_INFO, "Wrote packet exactly to boundary");
                allocBase = realloc(buff,(buffSize+1)*sizeof(char));
                if (allocBase == NULL)
                {
                    syslog(LOG_ERR,"Buffer realloc failed! Requested %ld. Dropping packet.", buffSize+1);
                    // reset buffer, unnecessary, we will fall through to the end of the packet end case
                    dropped = true;
                }
                else
                {
                    buff = allocBase;
                    buffSize+=1;
                    // add null terminator
                    *( (char*)buff + buffSize ) = '\0';
                }
        
            }
            if ( !dropped )
            {
                size_t leftToWrite = packet_length;
                char *writestart = (char*)buff;
                    // write new string to temp file	   
                
                do {
                    numWritten = fprintf(fHandle, "%s", writestart); 
                    if ( numWritten < 0 )
                    {
                                syslog(LOG_ERR,"Failure writing to client file : %s", strerror(errno));
                    }
                    else
                    {
                        leftToWrite -= numWritten;
                        if ( leftToWrite > 0 ) 
                        {
                            syslog(LOG_DEBUG,"Only wrote %ld bytes to file, %ld still left to write", numWritten, leftToWrite);
                            writestart += numWritten;
                        }
                    }
                } while ( leftToWrite > 0 && numWritten > 0 );

            }
            // write back all file contents (echo full packet history) one packet at a time
            // seek to file start
            rewind(fHandle);
            // print line by line, recycle socket buffer
            while ((numRead = getline((char**)&buff, &buffSize, fHandle)) != -1) 
            {
                if ( ( numWritten = send(acceptedSocket, buff, numRead, 0) ) < numRead )
                {
                    syslog(LOG_ERR,"Wrote fewer bytes (%ld) to socket than were read from file (%ld) for this line", numWritten, numRead);
                }
            }

            // seek to file end for next write
            if ( fseek(fHandle, 0, SEEK_END) == -1 ) 
            {

                syslog(LOG_ERR, "Failed seek after returning temp file to client : %s", strerror(errno));
            }

            // clear buffer
            incStart = buff;
            sizeToRead = buffSize;
            memset(buff, 0, buffSize*sizeof(char));

            // reset dropped flag
            dropped = false;
            }
            else
            { // realloc buf and put inc start at the right location to read next kB input of the current packet
                syslog(LOG_DEBUG, "Current buffer of size %ld to be realloc'ed to size %ld",buffSize, buffSize + buffInc);
                allocBase = realloc(buff,(buffSize + buffInc)*sizeof(char));
                if (allocBase == NULL)
                {
                    syslog(LOG_ERR,"Buffer realloc failed! Requested %ld. Dropping packet.", buffSize+1);
                    dropped = true;
                }
                else
                {
                    buff = allocBase;
                    incStart = (void *)( (char*)buff + buffSize );
                    buffSize += buffInc;
                    sizeToRead = buffInc;
                    syslog(LOG_DEBUG, " == Realloc == Buffer size: %ld, Next read max: %ld, Next write: %p, Buffer loc: %p", buffSize, sizeToRead, incStart, buff);
                }
        } //while numread

        }
        
        // check if connection close or error
        if ( numRead == -1 )
        {
            syslog(LOG_ERR, "revc returned failure code, exiting : %s", strerror(errno));
            free(allocBase);
            if ( fclose(fHandle) != 0 ) 
            {
                syslog(LOG_ERR, "Failed closing client file, %s",strerror(errno));
            }
            if ( close(listnerSocket) == -1 ) 
            {
                syslog(LOG_ERR, "Failed closing listener socket, %s",strerror(errno));
            }

            if ( close(acceptedSocket) == -1 ) 
            {
                syslog(LOG_ERR, "Failed closing connection socket, %s",strerror(errno));
            }
            // leave temp file as artifact of unclean exit
            return -1;
        }
        // else, close accepted_sock to accept a new one
        close(acceptedSocket);

        // cleanly exit by closing the listener socket, client file, and freeing buffer
        // also delete client file and then exit the program
        if ( terminalRcd )
        {
            syslog(LOG_INFO, "Caught signal, exiting");
            free(allocBase);
            if ( fclose(fHandle) != 0 )
            {
            syslog(LOG_ERR, "Failed closing client file, %s",strerror(errno)); 	    
            }
            if ( close(listnerSocket) == -1 )
            {
                syslog(LOG_ERR, "Failed closing listener socket, %s",strerror(errno));
            }
            if ( remove("/var/tmp/aesdsocketdata") == -1 ) 
            {
                syslog(LOG_ERR, "Failed removing client file, %s",strerror(errno));		
            }

            return 0;
        }

    } //while true

    //return 0;
}
