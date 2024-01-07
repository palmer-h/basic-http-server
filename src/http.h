#ifndef HTTP_H_
#define HTTP_H_

#define HTTP_HEADER_DATE_FORMAT "%a, %d %Y %b %X %Z"

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

#endif
