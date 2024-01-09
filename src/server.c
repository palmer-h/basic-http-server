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
#include <math.h>

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
int send_response(int sockfd, struct HttpResponse *res, int status) {
    char *resStr;
    char *statusStr;
    char reason[HTTP_STATUS_REASON_MAX_SIZE];
    size_t length;
    char date[HTTP_HEADER_DATE_LENGTH];
    struct HttpRequestHeader *header = NULL;
    struct HttpRequestHeader *lastHeader = NULL;

    statusStr = malloc(sizeof(char)*(int)log10(status));

    sprintf(statusStr, "%d", status);

    strcpy(reason, reason_from_status_code(status));

    length = strlen(HTTP_VERSION) + strlen(statusStr) + strlen(reason) + 4;

    resStr = malloc(length);

    // Initial response line
    snprintf(resStr, length, "HTTP/1.0 %s %s\n", statusStr, reason);

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
 * Parses raw request into HttpRequest struct
*/
struct HttpRequest *parse_request(int sockfd, const char *raw) {
    struct HttpRequest *req = NULL;
    struct HttpRequestHeaders *headers = NULL;
    struct HttpRequestHeader *header = NULL, *last = NULL;
    size_t len = strcspn(raw, " "); // Store length of each part (method, path, etc.)
    size_t bodyLen = 0;

    req = malloc(sizeof(struct HttpRequest));

    if (!req || !len || len < strlen(HTTP_METHOD_GET) || len > strlen(HTTP_METHOD_DELETE)) {
        free_request(req);
        send_response(sockfd, NULL, !req ? HTTP_STATUS_INTERNAL_SERVER_ERROR : HTTP_STATUS_BAD_REQUEST);
        return NULL;
    }

    /**
     * TODO: Figure what the below is doing and is it neccessary?
    */
    // memset(req, 0, sizeof(struct HttpRequest));

    // If valid method, copy method to struct (already includes null terminator)
    if (len == strlen(HTTP_METHOD_GET) && memcmp(raw, HTTP_METHOD_GET, len) == 0) {
        req->method = GET;
    } else if (len == strlen(HTTP_METHOD_POST) && memcmp(raw, HTTP_METHOD_POST, len) == 0) {
        req->method = POST;
    } else if (len == strlen(HTTP_METHOD_PUT) && memcmp(raw, HTTP_METHOD_PUT, len) == 0) {
        req->method = PUT;
    } else if (len == strlen(HTTP_METHOD_DELETE) && memcmp(raw, HTTP_METHOD_DELETE, len) == 0) {
        req->method = DELETE;
    } else {
        free_request(req);
        send_response(sockfd, NULL, HTTP_STATUS_NOT_IMPLEMENTED);
        return NULL;
    }

    // Move pointer to start of path and determine path length
    raw += len + 1;
    len = strcspn(raw, " ");

    // No path - 400 Bad Request
    if (!len) {
        free_request(req);
        send_response(sockfd, NULL, HTTP_STATUS_BAD_REQUEST);
        return NULL;
    }

    // Allocate correct amount of memory based on path length (+ 1 for `.` at start of path, + 1 for null terminating char)
    req->path = malloc(len + 2);

    if (!req->path) {
        free_request(req);
        send_response(sockfd, NULL, HTTP_STATUS_INTERNAL_SERVER_ERROR);
        return NULL;
    }

    // Add `.` to start of path struct member, append path from raw request and add null terminating char
    req->path[0] = '.';
    memcpy(&req->path[1], raw, len);
    req->path[len + 1] = '\0';

    // Move pointer to start of HTTP version
    raw += len + 1;

    // Length of HTTP version
    len = strcspn(raw, "\n");

    // If second to last char is \r, is CLRF so reduce length by 1
    if (raw[len -1] == '\r') {
        --len;
    }

    // Check version is supported
    if (memcmp(raw, HTTP_VERSION, len) != 0) {
        free_request(req);
        send_response(sockfd, NULL, HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED);
        return NULL;
    }

    // Allocate memory for HTTP version based on length of HTTP version (+ 1 for null terminating char)
    req->version = malloc(len + 1);

    if (!req->version) {
        free_request(req);
        send_response(sockfd, NULL, HTTP_STATUS_INTERNAL_SERVER_ERROR);
        return NULL;
    }

    // Copy version to struct member and add null terminating char
    memcpy(req->version, raw, len);
    req->version[len + 1] = '\0';

    // Move pointer to start of first line of headers (passed <CR> or <LF>)
    raw += len + 1;

    // If pointing at \n then is CRLF and we only moved passed the \r
    if (raw[0] == '\n') {
        ++raw;
    }

    // While next line exists and does not start with \r or \n (blank line indicates end of headers and start of body)
    while (raw[0] && raw[0] != '\n' && raw[0] != '\r') {
        last = header;

        header = malloc(sizeof(HttpRequestHeader));

        // Length of header name
        len = strcspn(raw, ":");

        // No header value - 400 Bad Request
        if (!len) {
            free_request(req);
            send_response(sockfd, NULL, HTTP_STATUS_BAD_REQUEST);
            return NULL;
        }

        // Allocate memory based on length of header name (+ 1 for null terminator)
        header->name = malloc(len + 1);

        if (!header->name) {
            free_request(req);
            send_response(sockfd, NULL, HTTP_STATUS_INTERNAL_SERVER_ERROR);
            return NULL;
        }

        memcpy(header->name, raw, len);

        // Add null terminating char to end of header name
        header->name[len] = '\0';

        // Move raw req buffer passed colon
        raw += len + 1;

        // Ignore any spaces between ":" and value (e.g: Header-Name:   header-value)
        while (*raw == ' ') {
            ++raw;
        }

        // Length of header value
        len = strcspn(raw, "\n");

        // If second to last char is CR then request uses CRLF so reduce length by 1
        if (raw[len - 1] == '\r') {
            --len;
        }

        // Allocate memory based on length of header value (+ 1 for null terminator)
        header->value = malloc(len + 1);

        if (!header->value) {
            free_request(req);
            send_response(sockfd, NULL, HTTP_STATUS_INTERNAL_SERVER_ERROR);
            return NULL;
        }

        memcpy(header->value, raw, len);

        // Add null terminating char to end of header value
        header->value[len] = '\0';

        // Move to next header (passed <CR> or <LF>)
        if (raw[len] == '\r') {
            raw += len + 2;
        } else {
            raw += len + 1;
        }

        // Set next header pointer to prev header
        header->next = last;
    }

    // Move passed CR if request uses CRLF
    if (raw[0] == '\r') {
        ++raw;
    }

    // Set headers
    req->headers = header;

    // If GET request then body is redundant so return request as is
    if (req->method == GET) {
        return req;
    }

    return NULL;
}

/**
 * Handles child process on new connection
*/
int handle_conn(int sockfd) {
    int bufSize = 1024;
    ssize_t bytesRecv, totalRecv = 0;
    struct HttpRequest *req = NULL;
    struct HttpResponse *res = NULL;
    char *rawReq = malloc(bufSize); // TODO: Need to free this memory!

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

    req = parse_request(sockfd, rawReq);

    if (!req) {
        // TODO: Do something? Log?
        return 0;
    }

    res = malloc(sizeof(struct HttpResponse));

    free_request(req);

    if (send_response(sockfd, res, HTTP_STATUS_OK) == -1) {
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
                if (send_response(newSockfd, NULL, HTTP_STATUS_INTERNAL_SERVER_ERROR) == -1) {
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