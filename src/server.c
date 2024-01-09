#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <ctype.h>

#include "http.h"
#include "socket.h"
#include "mime.h"
#include "date_utils.h"

/**
 * Free memory allocated for the request struct header
*/
void free_header(struct HttpRequestHeader *h) {
    if (h) {
        free(h->name);
        free(h->value);
        free_header(h->next);
        free(h);
    }
}

/**
 * Free memory allocated for the request struct
*/
void free_request(struct HttpRequest *req) {
    free(req->path);
    free(req->version);
    free_header(req->headers);
    free(req->body);
    free(req);
}

/**
 * Free memory allocated for the response struct
*/
void free_response(struct HttpResponse *res) {
    if (res->body) {
        free(res->body);
    }
    if (res->headers) {
        free(res->headers);
    }
    free(res);
}

/**
 * Adds a header to a response
*/
int add_response_header(char *name, char *value, struct HttpResponse *res) {
    struct HttpRequestHeader *header = NULL;
    struct HttpRequestHeader *lastHeader = NULL;

    header = malloc(sizeof(HttpRequestHeader));
    lastHeader = res->headers;

    header->name = malloc(strlen(name) + 1);
    strcpy(header->name, name);

    header->value = malloc(strlen(value) + 1);
    strcpy(header->value, value);

    header->next = lastHeader;

    res->headers = header;

    return 0;
}

/**
 * Builds the response send to the client
 * 
 * Accepts pointer to pre-malloced destination and HttpResponse struct
 * 
*/
int send_response(int sockfd, struct HttpResponse *res, char *status, char *reason) {
    char *resStr;
    size_t length;
    char date[HTTP_HEADER_DATE_LENGTH];
    struct HttpRequestHeader *header = NULL;
    struct HttpRequestHeader *lastHeader = NULL;

    length = strlen(HTTP_VERSION) + strlen(status) + strlen(reason) + 4;

    resStr = malloc(length);

    // Initial response line
    snprintf(resStr, length, "HTTP/1.0 %s %s\n", status, reason);

    /**
     * Default headers (TODO: Refactor?)
    */
    length = length + strlen("Server") + strlen(SERVER_NAME) + 3;
    resStr = realloc(resStr, length);
    strcat(resStr, "Server");
    strcat(resStr, ": ");
    strcat(resStr, SERVER_NAME);
    strcat(resStr, "\n");

    current_date_time(date);
    length = length + strlen("Date") + strlen(date) + 3;
    resStr = realloc(resStr, length);
    strcat(resStr, "Date");
    strcat(resStr, ": ");
    strcat(resStr, date);
    strcat(resStr, "\n");

    // Add headers from res struct
    if (res && res->headers) {
        for (header = res->headers; header; header = header->next) {
            length = length + strlen(header->name) + strlen(header->value) + 3;
            resStr = realloc(resStr, length);
            strcat(resStr, header->name);
            strcat(resStr, ": ");
            strcat(resStr, header->value);
            strcat(resStr, "\n");
        }
    }

    ++length;
    resStr = realloc(resStr, length);
    strcat(resStr, "\n");

    // Body
    if (res && res->body) {
        length += strlen(res->body) + 1;
        resStr = realloc(resStr, length);
        strcat(resStr, res->body);
    } else {
        length += strlen(reason) + 1;
        resStr = realloc(resStr, length);
        strcat(resStr, reason);
    }

    if (send(sockfd, resStr, length, 0) == -1) {
        free(resStr);
        return -1;
    };

    free(resStr);

    return 0;
}

/**
 * Handles child process on new connection
*/
int handle_conn(int sockfd) {
    int bufSize = 1024;
    ssize_t bytesRecv, totalRecv = 0;
    struct HttpResponse *res = NULL;
    char *rawReq = malloc(bufSize);

    while((bytesRecv = recv(sockfd, rawReq + totalRecv, bufSize, 0)) > 0) {
        int newBufSize;

        if (bytesRecv == -1) {
            return -1;
        }

        // Try again
        if (bytesRecv < 0 && errno == EAGAIN) {
            continue;
        }

        // Client closed connection
        if (bytesRecv == 0) {
            return 0;
        }

        // Double buffer size if newly received chunk bytes + total received bytes is bigger than current buffer size
        if (bytesRecv + totalRecv > bufSize) {
            newBufSize = bufSize * 2;
            rawReq = realloc(rawReq, newBufSize);
            bufSize = newBufSize;
        }

        // Add bytes of latest chunk to total received bytes
        totalRecv += bytesRecv;
    }

    res = malloc(sizeof(struct HttpResponse));

    return -1;

    if (send_response(sockfd, res, "200", "OK") == -1) {
        return -1;
    }

    free_response(res);

    return 0;
}

int main() {
    int listenSockfd, newSockfd;
    struct sockaddr_storage connAddr;
    char ip[INET6_ADDRSTRLEN];
    pid_t pid;
    socklen_t sin_size;

    if ((listenSockfd = create_listening_socket()) < 0) {
        perror("Error creating listening socket");
        exit(1);
    }

    printf("Waiting for connections... \n\n");

    for(;;) {
        sin_size = sizeof connAddr;
        newSockfd = accept(listenSockfd, (struct sockaddr*) &connAddr, &sin_size);

        if (newSockfd == -1) {
            perror("Error accepting");
            continue;
        }

        inet_ntop(connAddr.ss_family,
            (struct sockaddr *)&connAddr,
            ip, sizeof(ip));
        printf("-------------------------------\n");
        printf("Accepted connection from %s\n\n", ip);

        // Create child process so parent can continue listening
        pid = fork();

        if (pid == -1) {
            perror("Error creating fork");
            close(newSockfd);
            exit(1);
        }

        // Is parent process - close conn to socket
        if (pid == 1) {
            close(newSockfd);
            continue;
        }

        // Is child process - handle new conn
        if (pid == 0) {
            close(listenSockfd); // Child does not need this

            if (handle_conn(newSockfd) == -1) {
                printf("Error handling connection from %s\n", ip);
                if (send_response(newSockfd, NULL, "500", "Internal Server Error") == -1) {
                    printf("Error sending to %s\n", ip);
                }
            }

            printf("Closing connection %s\n", ip);
            printf("-------------------------------\n");
            close(newSockfd);
            _exit(0);
        }
    }

    return 0;
}