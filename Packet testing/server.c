#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define PORT 8080
#define MAX  1024

typedef struct {
    int id;      // simple numeric ID
    int fd;      // socket descriptor
    int active;
} client_t;

int main() {
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) { perror("socket"); exit(1); }

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in servaddr = {0};
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

    if (bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind"); exit(1);
    }
    if (listen(listenfd, 10) < 0) { perror("listen"); exit(1); }

    fd_set master, read_fds;
    FD_ZERO(&master);
    FD_SET(listenfd, &master);
    FD_SET(STDIN_FILENO, &master);
    int fdmax = (listenfd > STDIN_FILENO) ? listenfd : STDIN_FILENO;

    client_t clients[FD_SETSIZE];
    for (int i = 0; i < FD_SETSIZE; i++) clients[i].active = 0;

    int next_id = 1;

    printf("Server listening on port %d\n", PORT);
    printf("Commands from server console:\n");
    printf("   <id> <message>   send message to a client\n");
    printf("   exit             shut down server (sends exit to all)\n");

    for (;;) {
        read_fds = master;
        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        // --- server console input ---
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            char line[MAX];
            if (!fgets(line, sizeof line, stdin)) continue;

            if (strncmp(line, "exit", 4) == 0) {
                // tell all clients to exit and close
                for (int i = 0; i < FD_SETSIZE; i++) {
                    if (clients[i].active) {
                        write(clients[i].fd, "exit\n", 5);
                        close(clients[i].fd);
                        clients[i].active = 0;
                    }
                }
                printf("Server shutting down.\n");
                break;
            } else {
                // expected format:  <id> <message>
                int id;
                char msg[MAX];
                if (sscanf(line, "%d %[^\n]", &id, msg) == 2) {
                    int sent = 0;
                    for (int i = 0; i < FD_SETSIZE; i++) {
                        if (clients[i].active && clients[i].id == id) {
                            write(clients[i].fd, msg, strlen(msg));
                            write(clients[i].fd, "\n", 1);
                            printf("Sent to client %d: %s\n", id, msg);
                            sent = 1;
                            break;
                        }
                    }
                    if (!sent) printf("No such client id: %d\n", id);
                } else {
                    printf("Usage: <id> <message>\n");
                }
            }
        }

        // --- existing client activity ---
        for (int i = 0; i <= fdmax; i++) {
            if (i == listenfd || i == STDIN_FILENO) continue;
            if (!FD_ISSET(i, &read_fds)) continue;

            char buf[MAX];
            int n = read(i, buf, sizeof buf - 1);
            if (n <= 0) {
                // client closed
                close(i);
                FD_CLR(i, &master);
                for (int j = 0; j < FD_SETSIZE; j++)
                    if (clients[j].active && clients[j].fd == i) clients[j].active = 0;
                printf("Client fd %d disconnected\n", i);
            } else {
                buf[n] = '\0';
                // find client id
                int cid = -1;
                for (int j = 0; j < FD_SETSIZE; j++)
                    if (clients[j].active && clients[j].fd == i) { cid = clients[j].id; break; }

                if (strncmp(buf, "exit", 4) == 0) {
                    // drop only this client
                    write(i, "exit\n", 5);
                    close(i);
                    FD_CLR(i, &master);
                    for (int j = 0; j < FD_SETSIZE; j++)
                        if (clients[j].fd == i) clients[j].active = 0;
                    printf("Client %d requested exit\n", cid);
                } else {
                    printf("Client %d: %s", cid, buf);
                    // NOTE: we no longer echo back to the client
                }
            }
        }

        // --- new connection? ---
        if (FD_ISSET(listenfd, &read_fds)) {
            int newfd = accept(listenfd, NULL, NULL);
            if (newfd >= 0) {
                FD_SET(newfd, &master);
                if (newfd > fdmax) fdmax = newfd;

                for (int i = 0; i < FD_SETSIZE; i++) {
                    if (!clients[i].active) {
                        clients[i].active = 1;
                        clients[i].fd = newfd;
                        clients[i].id = next_id++;
                        break;
                    }
                }
                printf("New client connected with id %d (fd=%d)\n",
                       next_id - 1, newfd);
            }
        }
    }

    close(listenfd);
    return 0;
}
