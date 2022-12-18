#include<sys/types.h>
#include<sys/socket.h>
#include<sys/syslog.h>
#include<errno.h>
#include<signal.h>
#include<sys/syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include<stdio.h>
#include<stdlib.h>
#include <netdb.h>
#include<string.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include <sys/queue.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>



#define PORT "9000"
#define BACKLOG (5)
#define BUFFER_SIZE (1024)
#define FILE_PATH "/var/tmp/aesdsocketdata"

pthread_mutex_t mutex;
int sockfd,new_sockfd,file_fd;
bool interrupted=false;

struct addrinfo *server_info=NULL;
int total_connections=0;
bool timer_alarm=false;

struct thread_data{
    pthread_t thread_id;
    int count;
    int socketfd;
    struct sockaddr *client;
    bool connection_complete_success;
    SLIST_ENTRY(thread_data) entries;
};

SLIST_HEAD(slisthead,thread_data) head=SLIST_HEAD_INITIALIZER(&head);

int set_time(){
    int ret;
    struct itimerval timer_1;
    getitimer(ITIMER_REAL,&timer_1);
    timer_1.it_value.tv_sec=10;
    timer_1.it_value.tv_usec=0;
    timer_1.it_interval.tv_sec=10;
    timer_1.it_interval.tv_usec=0;
    ret=setitimer(ITIMER_REAL,&timer_1,NULL);
    return(ret);
}
int reset_time(){
    int ret;
    struct itimerval timer_1;
    getitimer(ITIMER_REAL,&timer_1);
    timer_1.it_value.tv_sec=0;
    timer_1.it_value.tv_usec=0;
    timer_1.it_interval.tv_sec=0;
    timer_1.it_interval.tv_usec=0;
    ret=setitimer(ITIMER_REAL,&timer_1,NULL);
    return(ret);
}

int write_time_to_file(int fd){
    char outstr[100]={};
    char buffer[100];
    time_t t;
    struct tm *tmp;
    const char* fmt= "%a, %d %b %Y %T %z";
    t = time(NULL);
    tmp = localtime(&t);
    if (tmp == NULL) {
        perror("localtime");
        return(-1);
    }

    if (strftime(outstr, sizeof(outstr), fmt, tmp) == 0) {
        return(-1);
    }
    strcpy(buffer,"timestamp:");
    strcat(buffer,outstr);
    strcat(buffer,"\n");
    write(fd,buffer,strlen(buffer)*sizeof(char));
    return(0);
}


void* thread_function(void* thread_param)
{
    int nr,ret;
    int size_recived=0,s_recv,s_send,current_size=0;
    char clientIP[INET6_ADDRSTRLEN];
    struct thread_data* thread_info = (struct thread_data *)thread_param;
    inet_ntop(AF_INET,thread_info->client,clientIP,sizeof(clientIP));
    bool recv_complete=false;
    char* buff = malloc(BUFFER_SIZE*sizeof(char*));
    while(!recv_complete)
    {
        buff=(char*)(realloc(buff,current_size+BUFFER_SIZE));
        s_recv=recv(thread_info->socketfd,(buff+current_size),BUFFER_SIZE,0);
        if(s_recv==-1){
            syslog(LOG_USER, "Error while recieving");
            perror("recv");
            exit(-1);
        }
        size_recived=current_size+s_recv;
        if(size_recived>0 && buff[size_recived-1]=='\n'){
            recv_complete=true;
            buff[size_recived-1]='\n';
        }
        current_size+=BUFFER_SIZE;
    }
    if(recv_complete)
    {
        ret=pthread_mutex_lock(&mutex); 
        if(ret!=0)
        {
            syslog(LOG_USER,"Error while unlocking mutex");
            exit(-1);
        } 
        file_fd=open(FILE_PATH, O_RDWR | O_CREAT | O_APPEND ,0644);
        if(file_fd == -1)
        {
            syslog(LOG_USER, "Unable to open file to read, Check the permission");
            perror("open");
            exit(-1);
        }     
        if(timer_alarm)
        {
            write_time_to_file(file_fd);
            timer_alarm=false;    
        }
        nr=write(file_fd, buff, size_recived);
        if(nr!=size_recived)
        {
            syslog(LOG_USER, "Writing to file not Successfull"); 
            perror("write");
            exit(-1);
        }   
           
    }
    char message[BUFFER_SIZE];
    lseek(file_fd,0,SEEK_SET);
    while((nr=read(file_fd,message,BUFFER_SIZE))!=0)
    {
        if(nr ==-1)
        {
            syslog(LOG_USER, "Reading from file not Successfull");   
            exit(-1);
        } 
        s_send=send(thread_info->socketfd,message,(nr/sizeof(char)),0);
        if(s_send<0)
        {
           syslog(LOG_USER, "Sending failed"); 
        }        
    }
    close(file_fd);  
    ret=pthread_mutex_unlock(&mutex);   
    if(ret!=0){
        syslog(LOG_USER,"Error while unlocking mutex");
        exit(-1);
    }
    thread_info->connection_complete_success =true;
    if(buff != NULL)
    {
        free(buff);
    }
    return 0;            
}

static void signal_handler (int signo)
{
    int ret,fd;
    struct thread_data* ele;
    if(signo == SIGINT || signo == SIGTERM)
    {   
        syslog (LOG_USER,"Caught Signal Exiting!");
        close(file_fd);
        ret=remove(FILE_PATH);
        if(ret==-1)
        {
            perror("remove");
            syslog(LOG_USER, "Error while removing file");
        }
        while(!SLIST_EMPTY(&head))
        {
            ele=SLIST_FIRST(&head);
            syslog (LOG_USER,"Freeing thread_id {%d}", (int)ele->count);
            pthread_join(ele->thread_id,NULL);
            SLIST_REMOVE_HEAD(&head,entries);
            free(ele);
        }
        interrupted=true;   
        exit (EXIT_SUCCESS);  
    }
    
    if(signo == SIGALRM)
    {
        if(total_connections==0)
        {
            fd=open(FILE_PATH, O_RDWR | O_CREAT | O_APPEND ,0644);
            write_time_to_file(fd);
        }
        else
        {
            timer_alarm=true;   
        }
    }
}


int main(int argc,char* argv[])
{

    openlog("aesdsocket",LOG_PID|LOG_ERR,LOG_USER); 
    setlogmask(LOG_UPTO(LOG_DEBUG));  
    struct addrinfo hints;
    bool deamon=false;
    interrupted=false;
    struct sockaddr client_addr;
    
    socklen_t addr_size=sizeof(client_addr);
    int status,ret,tr=1;
    if(argc>=2)
    {
        if(strcmp(argv[1],"-d")==0){
            deamon=true;
        }
    }


    syslog(LOG_USER, "*******************************************************************");
    syslog(LOG_USER, "Setting up signal handlers");
    syslog(LOG_USER, "*******************************************************************");
    signal (SIGTERM, signal_handler);
    signal (SIGINT, signal_handler);
    signal (SIGALRM, signal_handler);
    
    sockfd=socket(PF_INET,SOCK_STREAM,0);
    if(sockfd==-1)
    {
        syslog(LOG_USER, "Unable to create socket");
        perror("socket");
        return(-1);
    }
    tr=1;
    if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&tr,sizeof(int)) == -1) {
        perror("setsockopt");
        return(-1);
    }
    memset(&hints,0,sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol=0;

    status=getaddrinfo(NULL,PORT,&hints,&server_info);
    if(status!=0)
    {
        syslog(LOG_USER, "Error while getting address");
        return(-1);
    }

    ret=bind(sockfd,server_info->ai_addr,sizeof(struct sockaddr));
    if(ret<0)
    {
        syslog(LOG_USER, "Binding not done.");
        perror("bind");
        return(-1);

    }

    freeaddrinfo(server_info);
    if(deamon)
    {
        int ret=daemon(-1,-1);
        if(ret==-1){
            perror("daemon");
        }
    }


    
    ret=pthread_mutex_init(&mutex,NULL);
    SLIST_INIT(&head);
    listen(sockfd,BACKLOG);
    ret=set_time();
    if(ret!=0){
        perror("setitimer");
    }
    int counter = 0;
    while(!interrupted){ 
        new_sockfd=accept(sockfd,&client_addr,&addr_size);
        counter++;
        if(new_sockfd==-1){
            syslog(LOG_USER, "Error while accepting");
            perror("accept");
            close(sockfd);
            return(-1);
        }
        struct thread_data* client_data=malloc(sizeof(struct thread_data));
        client_data->socketfd=new_sockfd;
        client_data->client=&client_addr;
        client_data->connection_complete_success=false;
        client_data->count = counter;
        syslog(LOG_USER, "Accepted Connection");
        ret=pthread_create(&(client_data->thread_id),NULL,thread_function,client_data);
        syslog (LOG_USER,"Creating thread_id {%d}", (int)client_data->count);
        //Check if creation of thread was successfull
        if(ret != 0)
        {
            return (-1);
        }
        SLIST_INSERT_HEAD(&head,client_data,entries);
        if(client_data->connection_complete_success)
        {
            syslog (LOG_USER,"Completed connection for thread_id {%d}", (int)client_data->count);  
            close(client_data->socketfd);
            pthread_join(client_data->thread_id,NULL);
            SLIST_REMOVE(&head,client_data, thread_data, entries);

        }
    }
    close(file_fd);
    remove(FILE_PATH);
    syslog(LOG_USER, "File removed");
    closelog();
return(0);
}