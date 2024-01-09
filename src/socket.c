#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "socket.h"

int create_listening_socket() {
    int sockfd;
    struct addrinfo hints, *servinfo;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, PORT, &hints, &servinfo) != 0) {
        perror("Error getting address information \n");
        return -1;
    }

    sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

    if (sockfd == -1) {
        perror("Error creating socket");
        return -1;
    }

    if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        perror("Error binding socket \n");
        close(sockfd);
        return -1;
    }

    freeaddrinfo(servinfo);

    if (listen(sockfd, BACKLOG) == -1) {
        perror("Error listening");
        close(sockfd);
        return -1;
    }

    return sockfd;
}
