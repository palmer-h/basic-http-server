#ifndef HTTP_H_
#define HTTP_H_

#define PORT "3000"
#define HTTP_VERSION "HTTP/1.0"
#define HTTP_HEADER_DATE_FORMAT "%a, %d %Y %b %X %Z"
#define HTTP_HEADER_DATE_LENGTH 30
#define SERVER_NAME "Palmers Basic HTTP"

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
    struct HttpRequestHeader *headers;
    char *body;
} HttpResponse;

#endif
