#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include "protocol.h"

int sock;
int is_admin=0;

void sigint_handler(int sig) {
    printf("\n[System] Disconnecting from The Wired...\n");
    send(sock, "/exit\n", 6, 0);
    close(sock);
    exit(0);
}

void *receive_messages(void *socket_desc) {
    int sock = *(int*)socket_desc;
    char buffer[BUFFER_SIZE];
    
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            printf("\n[System] Connection lost to The Wired.\n");
            exit(0);
        }
        printf("\r%s", buffer);
        
        if (is_admin) printf("Command >> ");
        else printf("> ");
        fflush(stdout);
    }
    return NULL;
}

int main() {
    signal(SIGINT, sigint_handler);

    struct sockaddr_in server_addr;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT); -
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP); 

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed"); exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    char input[BUFFER_SIZE];

    while (1) {
        printf("Enter your name: ");
        fgets(input, BUFFER_SIZE, stdin);
        input[strcspn(input, "\n")] = 0;
        send(sock, input, strlen(input), 0);

        memset(buffer, 0, BUFFER_SIZE);
        recv(sock, buffer, BUFFER_SIZE, 0);

        if (strncmp(buffer, "AUTH_ERR", 8) == 0) {
            printf("%s", buffer + 9);
        } 
        else if (strncmp(buffer, "AUTH_PASS", 9) == 0) {
            printf("Enter Password: ");
            fgets(input, BUFFER_SIZE, stdin);
            input[strcspn(input, "\n")] = 0;
            send(sock, input, strlen(input), 0);
            
            memset(buffer, 0, BUFFER_SIZE);
            recv(sock, buffer, BUFFER_SIZE, 0);
            if (strncmp(buffer, "AUTH_ADMIN_OK", 13) == 0) {
                is_admin = 1;
                printf("[System] Authentication Successful. Granted Admin privileges.\n\n");
                printf("%s", buffer + 14);
                break;
            }
        }
        else if (strncmp(buffer, "AUTH_OK", 7) == 0) {
            printf("%s", buffer + 8);
            break;
        }
    }

    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receive_messages, (void*)&sock);
    while (1) {
        if (is_admin) printf("Command >> ");
        else printf("> ");
        
        fgets(input, BUFFER_SIZE, stdin);
        
        if (strcmp(input, "\n") == 0) continue;

        send(sock, input, strlen(input), 0);

        if (strncmp(input, "/exit", 5) == 0 || (is_admin && strncmp(input, "4", 1) == 0)) {
            printf("[System] Disconnecting from The Wired...\n");
            break;
        }
        
        usleep(100000); 
    }

    close(sock);
    return 0;
}