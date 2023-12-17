#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080
#define MAX_CONNECTIONS 20
#define FILE_BUFFER_SIZE 1024

typedef struct usernames_ls usernames_ls;
typedef struct clients_info clients_info;
typedef enum CLIENT_STATUS CLIENT_STATUS;

enum CLIENT_STATUS {
    IN_CHAT,
    WAITING_USERNAME
};

struct clients_info {
    int sockfd;
    char name[20];
    char IP[INET_ADDRSTRLEN];
    CLIENT_STATUS status;
    clients_info* next;
    clients_info* prev;
};

int find_max(clients_info *cl) {
    if (cl == NULL) 
        return -1;
    int ans = find_max(cl->next);
    return (ans < cl->sockfd) ? cl->sockfd : ans; 
}

char *parse_filename(char *filepath) {
    char *filename = strrchr(filepath, '/');

    if (filename != NULL) {
        filename++;
    }
    else {
        filename = filepath;
    }

    return filename;
}
void receive_file(int sockfd, const char *filepath) {
    // 파일 경로에서 파일 이름 파싱
    char *filename = parse_filename(filepath);

    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        perror("file open error");
        return;
    }

    char buffer[FILE_BUFFER_SIZE];
    ssize_t bytesRead;

    while ((bytesRead = recv(sockfd, buffer, sizeof(buffer), 0)) > 0) {
        if (strcmp(buffer, "<EOF>") == 0)
            // 파일의 끝인 경우 종료
            break;
        fwrite(buffer, 1, bytesRead, file);
    }

    fclose(file);
}

int main(void) {
    int sockfd, maxfd;
    struct sockaddr_in server_addr;
    char msg [1024];
    fd_set readfds, activefds;
    clients_info *clt, *clt2, *clients = NULL;

    if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    // memset(&server_addr.sin_zero, '\0', sizeof server_addr.sin_zero);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof (server_addr)) < 0) {
        perror("bind failed");
        exit(1);
    }

    if (listen(sockfd, MAX_CONNECTIONS) == -1) {
        fprintf(stderr, "listen failed port number %d\n", PORT);
        exit(1);
    }

    FD_ZERO(&activefds);
    FD_ZERO(&readfds);

    FD_SET(sockfd, &activefds);
    maxfd = sockfd;

    printf("Server up and running\n");
    while (1) {
        readfds = activefds;
        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select error");
            exit(1);
        }

        if (FD_ISSET(sockfd, &readfds)) {
            // 새로운 접속이 있을 때
            struct sockaddr_in cl;
            int len = sizeof(cl);
            int cl_fd;
            if ( (cl_fd = accept(sockfd,(struct sockaddr*) &cl, &len)) < 0) {
                fprintf(stderr, "Couldn't accept\n");
                exit(1);
            }
            FD_SET(cl_fd, &activefds);
            if (cl_fd > maxfd) 
                maxfd = cl_fd;

            clients_info* new_clnt = (clients_info *)malloc(sizeof(clients_info));
            new_clnt->name[0]='\0';
            inet_ntop(cl.sin_family, &cl.sin_addr,new_clnt->IP,sizeof(new_clnt->IP));
            new_clnt->sockfd = cl_fd;
            new_clnt->status = WAITING_USERNAME;

            new_clnt->next = clients;
            new_clnt->prev = NULL;

            clients = new_clnt;

            printf("connected from %s\n", new_clnt->IP);
            strcpy(msg, "Enter username : ");
            send(cl_fd,msg, strlen(msg),0);
        }
        else {
            // 클라이언트를 순회
            for (clt = clients; clt != NULL; clt = clt->next) {
                if (FD_ISSET(clt->sockfd,&readfds)) {
                    // 클라이언트로부터 전송이 있다면
                    memset(msg, 0, strlen(msg)*sizeof(char));
                    int n = recv(clt->sockfd, msg, 1024, 0);

                    // 받은 데이터가 없다면
                    if (n == 0) {
                        FD_CLR(clt->sockfd,&activefds);
                        if (clt->prev != NULL)
                            clt->prev->next = clt->next;
                        else {
                            clients = clt->next;
                        }

                        close(clt->sockfd);
                        if (clt->sockfd == maxfd) 
                            maxfd = find_max(clients);

                        printf("Client disconnected %s\n", clt->name);
                        free(clt);
                        break;
                    }
                    else {
                        switch(clt->status) {
                            case IN_CHAT:{
                                if (strncmp(msg, "<file>", 6) == 0) {
                                    // 파일 전송인 경우
                                    char filepath[256];
                                    sscanf(msg, "<file>%256s", filepath);

                                    printf("Receiving file %s from %s\n", filepath, clt->name);
                                    receive_file(clt->sockfd, filepath);
                                    
                                    break;
                                }
                                char send_message[50];
                                send_message[0] = '\0';
                                strcat(send_message,"<");
                                strcat(send_message,clt->name);
                                strcat(send_message,"> : ");
                                strcat(send_message, msg);
                                printf("message from  %s\n", send_message);

                                // 사용자들에게 수신받은 메시지 전달(송신자 제외)
                                for (clt2 = clients; clt2 != NULL; clt2 = clt2->next) {
                                    if (clt2 == clt) 
                                        continue;
                                    send(clt2->sockfd, send_message,strlen(send_message), 0);
                                }
                                break;
                            }
                            case WAITING_USERNAME: {
                                int found = 0;

                                // 사용자 이름이 중복되는 지 확인
                                for (clt2 = clients; clt2 != NULL && !found; clt2 = clt2->next) {
                                    if (clt2 == clt) 
                                        continue;
                                    if (strcmp(msg, clt2->name) == 0) {
                                        found = 1;
                                        break;
                                    }
                                }
                                
                                // 사용자 이름이 중복되는 경우 다시 입력
                                if (found) {
                                    memset(msg, 0, strlen(msg));
                                    strcpy(msg, "<Server> duplicate username. enter username again\n");
                                }
                                else {
                                    printf("<username> from  %s\n", msg);
                                    strcpy(clt->name,msg);
                                    strcpy(msg, "<server> Hello ");
                                    strcat(msg, clt->name);
                                    clt->status = IN_CHAT;
                                }
                                send(clt->sockfd, msg, strlen(msg), 0);
                                break;
                            }
                            default : 
                                 break;
                        }
                    }
                }
            }
        }
    }
    
    close(sockfd);
    return 0;
}
