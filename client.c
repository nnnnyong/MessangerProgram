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
// #define STDIN 0

int main() {
    int sockfd, maxfd;
    struct sockaddr_in serv_addr;
    char msg[1024];
    char msg_recv[1024];

    if ( (sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "failed create socket\n");
        exit(1);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_aton("127.0.0.1", &(serv_addr.sin_addr));
    // memset(&serv_addr.sin_zero, '\0', sizeof serv_addr.sin_zero);

    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(struct sockaddr)) < 0) {
        fprintf(stderr, "failed connect to server\n");
        exit(1);
    }

    
    fd_set readfds, activefds;

    FD_ZERO(&activefds);
    FD_ZERO(&readfds);

    FD_SET(sockfd, &activefds);
    FD_SET(STDIN_FILENO, &activefds);

    maxfd = sockfd;

    while (1) {
        readfds = activefds;
        int n;
        if ((n = select(maxfd + 1, &readfds, NULL, NULL, NULL)) < 0) {
            perror("select error");
            exit(1);
        }

        // if (n > 0) 
            // printf("\n>>> ");
        if (FD_ISSET(STDIN_FILENO, &readfds)){
            // 사용자의 입력이 있을 때
            scanf(" %[^\n]", msg);
            if (msg[0] != '\0')
                send(sockfd, msg, strlen(msg), 0);
            else
                continue;
        }
        else if (FD_ISSET(sockfd, &readfds)) {
            // 서버로부터의 송신이 있을 때
            int n;
            if ((n = recv(sockfd, msg_recv, 1024, 0)) < 0) {
                perror("recieve error");
                exit(1);
            }
            if (n == 0) {
                printf("server closed\n");
                break;
            }
            msg_recv[n] = '\0';
            printf("%s\n ", msg_recv);
        }
    }
    close(sockfd);
    return 0;
}
