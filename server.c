#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define PORT "3000"
#define BACKLOG 10

int handle_conn(int sockfd)
{
    if (send(sockfd, "Hello, world!", 13, 0) == -1) {
        return -1;
    }

    return 0;
}

int main()
{
    int sockfd, new_fd;
    struct sockaddr_storage conn_addr;
    struct addrinfo hints, *servinfo;
    char s[INET6_ADDRSTRLEN];
    pid_t pid;
    socklen_t sin_size;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, PORT, &hints, &servinfo) != 0) {
        perror("Error getting address information \n");
        exit(1);
    }

    sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

    if (sockfd == -1) {
        perror("Error creating socket");
        exit(1);
    }

    if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        perror("Error binding socket \n");
        close(sockfd);
        exit(1);
    }

    freeaddrinfo(servinfo);

    if (listen(sockfd, BACKLOG) == -1) {
        perror("Error listening");
        close(sockfd);
        exit(1);
    }

    printf("Waiting for connections... \n");

    for(;;) {
        sin_size = sizeof conn_addr;
        new_fd = accept(sockfd, (struct sockaddr*) &conn_addr, &sin_size);

        if (new_fd == -1) {
            perror("Error accepting");
            continue;
        }

        inet_ntop(conn_addr.ss_family,
            (struct sockaddr *)&conn_addr,
            s, sizeof s);
        printf("server: got connection from %s\n", s);

        // Create child process so parent can continue listening
        pid = fork();

        if (pid == -1) {
            perror("Error creating fork");
            exit(1);
        }

        // Is parent process - close conn to socket
        if (pid == 1) {
            close(new_fd);
            continue;
        }

        // Is child process - handle new conn
        if (pid == 0) {
            close(sockfd); // Child does not need this

            if (handle_conn(new_fd) == -1) {
                printf("Error handling request from %s\n", s);
            }

            printf("Hello!");

            close(new_fd);
            _exit(0);
        }
    }

    return 0;
}
