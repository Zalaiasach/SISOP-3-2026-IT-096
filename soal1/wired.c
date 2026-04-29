#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include "protocol.h"

typedef struct{
    int sockfd;
    char name[50];
    int is_active;
    int is_admin;
} Client;

Client clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
time_t start_time;

void log_write(const char *entity, const char *action){
    pthread_mutex_lock(&log_mutex);
    FILE *fp = fopen("history.log","a");
    if(fp != NULL){
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        fprintf(fp, "[%04d-%02d-%02d %02d:%02d:%02d] [%s] [%s]\n",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                tm.tm_hour, tm.tm_min, tm.tm_sec,
                entity, action);
        fclose(fp);
    }
    pthread_mutex_unlock(&log_mutex);
}

void broadcast_mutex(const char *message, int sender_fd){
    pthread_mutex_lock(&clients_mutex);
    for(int i =0;i<MAX_CLIENTS;i++){
        if(clients[i].is_active && clients[i].sockfd != sender_fd && !clients[i].is_admin){
            send(clients[i].sockfd, message, strlen(message),0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg){
    int cli_sock = *((int *)arg);
    free(arg);
    char buffer[BUFFER_SIZE];
    char name[50];
    int is_admin = 0;
    while(1){
        memset(buffer,0,BUFFER_SIZE);
        if(recv(cli_sock, buffer, BUFFER_SIZE,0)<=0){
            close(cli_sock);
            return NULL;
        }
        buffer[strcspn(buffer, "\n")]=0;
        strncpy(name,buffer,sizeof(name));

        if(strcmp(name, "The Knights")==0){
            char pass_prompt[] ="AUTH_PASS";
            send(cli_sock,pass_prompt,strlen(pass_prompt),0);
            memset(buffer,0,BUFFER_SIZE);
            recv(cli_sock,buffer,BUFFER_SIZE,0);

            is_admin=1;
            char success_msg[]="AUTH_ADMIN_OK\n=== THE KNIGHTS CONSOLE ===\n1. Check Active Entities (Users)\n2. Check Server Uptime\n3. Execute Emergency Shutdown\n4. Disconnect\n";
            send(cli_sock,success_msg,strlen(success_msg),0);
            log_write("System", "User 'The Knights' connected");
            break;
        }else{
            int name_exists=0;
            pthread_mutex_lock(&clients_mutex);
            for(int i=0;i<MAX_CLIENTS;i++){
                if(clients[i].is_active && strcmp(clients[i].name,name)==0){
                    name_exists=1;
                    break;
                }
            }
            pthread_mutex_unlock(&clients_mutex);

            if(name_exists){
                char err_msg[BUFFER_SIZE];
                sprintf(err_msg, "AUTH_ERR [System] The identity '%s' is already synchronized in The Wired.\n", name );
                send(cli_sock, err_msg,strlen(err_msg), 0);
            }else{
                char ok_msg[BUFFER_SIZE];
                sprintf(ok_msg, "AUTH_OK --- Welcome to The Wired, %s ---\n", name);
                send(cli_sock,ok_msg,strlen(ok_msg),0);

                char log_msg[BUFFER_SIZE];
                sprintf(log_msg,"User '%s' connected",name);
                log_write("System", log_msg);
                break;
            }
        }
    }

    pthread_mutex_lock(&clients_mutex);
    for(int i =0; i<MAX_CLIENTS;i++){
        if(!clients[i].is_active){
            clients[i].sockfd=cli_sock;
            strncpy(clients[i].name, name,sizeof(clients[i].name));
            clients[i].is_active=1;
            clients[i].is_admin=is_admin;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    while(1){
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(cli_sock,buffer,BUFFER_SIZE,0);
        if(bytes_received<=0)break;
        buffer[strcspn(buffer,"\n")]=0;

        if(strcmp(buffer,"/exit")==0 || (is_admin && strcmp(buffer,"4")==0))break;

        if(is_admin){
            if(strcmp(buffer,"1")==0){
                log_write("Admin", "RPC_GER_USERS");
                int count=0;
                pthread_mutex_lock(&clients_mutex);
                for(int i=0;i<MAX_CLIENTS;i++){
                    if(clients[i].is_active && !clients[i].is_admin) count++;
                }
                pthread_mutex_unlock(&clients_mutex);
                char res[BUFFER_SIZE];
                sprintf(res,"[System] Active Entities: %d\n",count);
                send(cli_sock,res,strlen(res),0);
            }else if (strcmp(buffer,"2")==0){
                log_write("Admin", "RPC_GET_UPTIME");
                time_t current_time = time(NULL);
                double seconds = difftime(current_time,start_time);
                char res[BUFFER_SIZE];
                sprintf(res, "[System] Server Uptime: %.0f seconds\n", seconds);
                send(cli_sock,res,strlen(res),0);
            }else if (strcmp(buffer, "3") == 0) {
                log_write("Admin", "RPC_SHUTDOWN");
                log_write("System", "EMERGENCY SHUTDOWN INITIATED");
                printf("Emergency Shutdown initiated by Admin.\n");
                exit(0);
            }
        }else{
            char chat_log[BUFFER_SIZE];
            sprintf(chat_log, "[%s]: %s", name, buffer);
            log_write("User",chat_log);

            char broadcast_buf[BUFFER_SIZE];
            sprintf(broadcast_buf, "[%s]: %s\n", name, buffer);
            broadcast_mutex(broadcast_buf, cli_sock);
        }
    }

    char log_msg[BUFFER_SIZE];
    sprintf (log_msg, "User '%s' disconnected", name );
    log_write("System", log_msg);
    pthread_mutex_lock(&clients_mutex);
    for(int i=0;i<MAX_CLIENTS;i++){
        if(clients[i].sockfd ==cli_sock){
            clients[i].is_active=0;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    close(cli_sock);
    return NULL;
}

int main(){
    start_time = time(NULL);
    log_write("System", "SERVER ONLINE");

    for (int i = 0; i < MAX_CLIENTS; i++) clients[i].is_active = 0;

    int server_sock, *new_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT); 
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP); 

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind gagal"); exit(EXIT_FAILURE);
    }

    if (listen(server_sock, 10) == 0) {
        printf("The Wired is listening on %s:%d...\n", SERVER_IP, SERVER_PORT);
    } else {
        printf("Listen error\n"); exit(EXIT_FAILURE);
    }

    while (1) {
        new_sock = malloc(sizeof(int));
        *new_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);
        if (*new_sock < 0) {
            perror("Accept gagal"); free(new_sock); continue;
        }
        
        pthread_t client_thread;
        pthread_create(&client_thread, NULL, handle_client, (void*)new_sock);
        pthread_detach(client_thread);
    }

    close(server_sock);
    return 0;
}
