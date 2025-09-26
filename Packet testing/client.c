#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define PORT 8080
#define MAX  1024

int main() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); exit(1); }

    int n;
    char buff[15] = "127.0.0.1\0";
    struct sockaddr_in servaddr = {0};
    servaddr.sin_family = AF_INET;
    servaddr.sin_port   = htons(PORT);
    servaddr.sin_addr.s_addr = inet_addr(buff);
    while (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        printf("Can't connect to host %s\n", buff);
        bzero(buff, sizeof(buff));
        printf("Enter the server address : ");
        n = 0;
        while ((buff[n++] = getchar()) != '\n')
            ;
        buff[n] = '\0';
        if (strncmp(buff, "exit", 4) == 0) {
            exit(0);
        }
        servaddr.sin_addr.s_addr = inet_addr(buff);
    }
    printf("Connected to server on port %d.\n", PORT);
    printf("Type a message and press Enter; 'exit' closes the client.\n");

    fd_set master, read_fds;
    FD_ZERO(&master);
    FD_SET(STDIN_FILENO, &master);
    FD_SET(sockfd, &master);
    int fdmax = (sockfd > STDIN_FILENO) ? sockfd : STDIN_FILENO;

    for (;;) {
        read_fds = master;
        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        // keyboard input
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            char line[MAX];
            if (!fgets(line, sizeof line, stdin)) continue;
            write(sockfd, line, strlen(line));
            if (strncmp(line, "exit", 4) == 0) {
                printf("Client exiting.\n");
                break;
            }
        }

        // message from server
        if (FD_ISSET(sockfd, &read_fds)) {
            char buf[MAX];
            int n = read(sockfd, buf, sizeof buf - 1);
            if (n <= 0) {
                printf("Server closed connection.\n");
                break;
            }
            buf[n] = '\0';
            if (buf[0] != '\n') {
                printf("From server: %s\n", buf);
            }
            if (strncmp(buf, "exit", 4) == 0) {
                printf("Server requested exit. Closing.\n");
                break;
            }
        }
    }

    close(sockfd);
    return 0;
}
