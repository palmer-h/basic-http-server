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
#include <ctype.h>
#include <time.h>

#define PORT "3000"
#define BACKLOG 10

typedef enum HttpMethod { GET, POST, PUT, DELETE } HttpMethod;

typedef struct HttpRequestHeader {
    char *name;
    char *value;
    struct HttpRequestHeader *next;
} HttpRequestHeader;

typedef struct HttpRequest {
    enum HttpMethod method;
    char *path;
    char *version;
    struct HttpRequestHeader *headers;
    char *body;
} HttpRequest;

typedef struct HttpResponse {
    char *status;
    char *reason;
    struct HttpRequestHeader *headers;
    char *body;
} HttpResponse;

const char *HttpMethods[] = { "GET", "POST", "PUT", "DELETE" };

/**
 * Return HTTP specification compliant current datetime string (01, Jan 2000 23:59:59 GMT)
*/
void get_current_date_time(char *s) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(s, 30, "%a, %d %Y %b %X %Z", tm);
}

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
void free_request(struct HttpRequest *request) {
    free(request->path);
    free(request->version);
    free_header(request->headers);
    free(request->body);
    free(request);
}

/**
 * Accepts a raw http request buffer and returns a request struct
*/
struct HttpRequest *parse_request(const char *raw) {
    struct HttpRequest *request = NULL;
    struct HttpRequestHeaders *headers = NULL;
    size_t len = strcspn(raw, " "); // Store length of each part (method, path, etc.)

    // Allocate memory for request struct
    request = malloc(sizeof(struct HttpRequest));

    if (!request) {
        free_request(request);
        return NULL;
    }

    memset(request, 0, sizeof(struct HttpRequest));

    // If valid method, copy method to struct (already includes null terminator)
    if (memcmp(raw, "GET", 3) == 0) {
        request->method = GET;
    } else if (memcmp(raw, "POST", 4) == 0) {
        request->method = POST;
    } else if (memcmp(raw, "PUT", 3) == 0) {
        request->method = PUT;
    } else if (memcmp(raw, "DELETE", 6) == 0) {
        request->method = DELETE;
    } else {
        free_request(request);
        return NULL;
    }

    // Move pointer to start of path
    raw += len + 1;

    // Path length
    len = strcspn(raw, " ");

    // Allocate correct amount of memory based on path length (+ 1 for null terminating char)
    request->path = malloc(len + 1);

    if (!request->path) {
        free_request(request);
        return NULL;
    }

    // Copy path to struct member
    memcpy(request->path, raw, len);

    // Add null terminating char to end of path
    request->path[len] = '\0';

    // Move pointer to start of HTTP version
    raw += len + 1;

    // Length of HTTP version
    len = strcspn(raw, "\n");

    // If second to last char is \r, is CLRF so reduce length by 1
    if (raw[len -1] == '\r') {
        --len;
    }

    // Allocate memory for HTTP version based on length of HTTP version (+ 1 for null terminating char)
    request->version = malloc(len + 1);

    if (!request->version) {
        free_request(request);
        return NULL;
    }

    // Copy version to struct member
    memcpy(request->version, raw, len);

    // Add null terminating char to end of version
    request->version[len] = '\0';

    // Move pointer to start of first line of headers (passed <CR> or <LF>)
    ++raw;

    // If pointing at \n then is CRLF and we only moved passed the \r
    if (raw[0] == '\n') {
        ++raw;
    }

    struct HttpRequestHeader *header = NULL, *last = NULL;

    // While next line does not start with \r or \n (blank line indicates end of headers and start of body)
    while (raw[0] != '\n' && raw[0] != '\r') {
        last = header;

        header = malloc(sizeof(HttpRequestHeader));

        // Length of header name
        len = strcspn(raw, ":");

        // Allocate memory based on length of header name
        header->name = malloc(len + 1);

        if (!header->name) {
            free_request(request);
            return NULL;
        }

        memcpy(header->name, raw, len);

        // Add null terminating char to end of header name
        header->name[len] = '\0';

        // Move pointer passed colon
        raw += len + 1;

        // Ignore any spaces between ":" and value (e.g: Header-Name:   header-value)
        while (*raw == ' ') {
            ++raw;
        }

        // TODO: correct length (could be "\r" instead of "\n"!)

        // Length of header value
        len = strcspn(raw, "\n");

        // Allocate memory based on length of header value (+ 1 for null terminator)
        header->value = malloc(len + 1);

        if (!header->value) {
            free_request(request);
            return NULL;
        }

        memcpy(header->value, raw, len);

        // Add null terminating char to end of header value
        header->value[len] = '\0';

        // Move to next header (passed <CR> or <LF>)
        raw += len + 1;

        // Set pointer to prev header
        header->next = last;
    }

    // Set headers
    request->headers = header;

    /**
     * TODO: Combine path and hostname header to form full URL?
    */

    /**
     * TODO: Check content-size header - Make mandatory?
    */

    // If GET request then body is redundant so return request as is
    if (request->method == GET) {
        return request;
    }

    // Move to start of body
    ++raw;

    // If pointing at \n then is CRLF and we only moved passed the \r
    if (raw[0] == '\n') {
        ++raw;
    }

    // Length of body
    len = strlen(raw);

    // Allocate memory based on length of body (+ 1 for null terminator)
    request->body = malloc(len + 1);

    if (!request->body) {
        free_request(request);
        return NULL;
    }

    memcpy(request->body, raw, len);

    // Add null terminating char to end of body
    request->body[len] = '\0';

    return request;
}

struct HttpResponse *handle_request(struct HttpRequest *req) {
    struct HttpResponse *res = NULL;
    struct HttpRequestHeader *header = NULL;
    char date[30];

    // Allocate memory for response struct
    res = malloc(sizeof(struct HttpResponse));

    if (!res) {
        return NULL;
        // TODO: Free res memory
    }

    // Allocate 4 bytes for status - 3 digit code + null terminating char
    res->status = malloc(4);

    get_current_date_time(date);

    printf("Status: %s \n", res->status);
    printf("Date: %s\n", date);

    printf("Headers: \n");
    for (header = req->headers; header; header = header->next) {
        printf("%s: %s\n", header->name, header->value);
    }

    return res;
}

int handle_conn(int sockfd) {
    ssize_t bytes_recv, total_recv = 0;
    char buffer[2048];
    char *method[8], *path[2048];

    while((bytes_recv = recv(sockfd, buffer + total_recv, sizeof buffer - total_recv, 0)) > 0) {
        if (bytes_recv == -1) {
            return -1;
        }

        // Try again
        if (bytes_recv < 0 && errno == EAGAIN) {
            continue;
        }

        // Client closed connection
        if (bytes_recv == 0) {
            return 0;
        }
        
        // Add bytes of latest chunk to total received bytes
        total_recv += bytes_recv;
    }

    struct HttpRequest *request = parse_request(buffer);

    if (request == NULL) {
        free_request(request);
        return -1;
    }

    handle_request(request);

    free_request(request);

    return 0;
}

int main() {
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
                if (send(new_fd, "Error handling connection", 25, 0) == -1) {
                    printf("Error sending to %s\n", s);
                }
            }

            close(new_fd);            
            _exit(0);
        }
    }

    return 0;
}
