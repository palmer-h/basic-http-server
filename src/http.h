#ifndef HTTP_H_
#define HTTP_H_

#define PORT "3000"
#define HTTP_VERSION "HTTP/1.0"
#define HTTP_HEADER_DATE_FORMAT "%a, %d %Y %b %X %Z"
#define HTTP_HEADER_DATE_LENGTH 30
#define SERVER_NAME "Palmers Basic HTTP"
#define HTTP_STATUS_REASON_MAX_SIZE 32
#define HTTP_MAX_BODY_SIZE 1000000

/**
 * HTTP methods
*/
#define HTTP_METHOD_GET "GET"
#define HTTP_METHOD_POST "POST"
#define HTTP_METHOD_PUT "PUT"
#define HTTP_METHOD_DELETE "DELETE"

/**
 * HTTP header names
*/
#define HTTP_HEADER_CONTENT_LENGTH "Content-Length"
#define HTTP_HEADER_CONTENT_TYPE "Content-Type"

/**
 * HTTP status codes
*/
#define HTTP_STATUS_CONTINUE 100
#define HTTP_STATUS_SWITCHING_PROTOCOLS 101
#define HTTP_STATUS_OK 200
#define HTTP_STATUS_CREATED 201
#define HTTP_STATUS_ACCEPTED 202
#define HTTP_STATUS_NON_AUTHORITATIVE_INFORMATION 203
#define HTTP_STATUS_NO_CONTENT 204
#define HTTP_STATUS_RESET_CONTENT 205
#define HTTP_STATUS_PARTIAL_CONTENT 206
#define HTTP_STATUS_MULTIPLE_CHOICES 300
#define HTTP_STATUS_MOVED_PERMANENTLY 301
#define HTTP_STATUS_MOVED_TEMPORARILY 302
#define HTTP_STATUS_SEE_OTHER 303
#define HTTP_STATUS_NOT_MODIFIED 304
#define HTTP_STATUS_USE_PROXY 305
#define HTTP_STATUS_BAD_REQUEST 400
#define HTTP_STATUS_UNAUTHORIZED 401
#define HTTP_STATUS_PAYMENT_REQUIRED 402
#define HTTP_STATUS_FORBIDDEN 403
#define HTTP_STATUS_NOT_FOUND 404
#define HTTP_STATUS_METHOD_NOT_ALLOWED 405
#define HTTP_STATUS_NOT_ACCEPTABLE 406
#define HTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED 407
#define HTTP_STATUS_REQUEST_TIME_OUT 408
#define HTTP_STATUS_CONFLICT 409
#define HTTP_STATUS_GONE 410
#define HTTP_STATUS_LENGTH_REQUIRED 411
#define HTTP_STATUS_PRECONDITION_FAILED 412
#define HTTP_STATUS_REQUEST_ENTITY_TOO_LARGE 413
#define HTTP_STATUS_REQUEST_URI_TOO_LARGE 414
#define HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE 415
#define HTTP_STATUS_INTERNAL_SERVER_ERROR 500
#define HTTP_STATUS_NOT_IMPLEMENTED 501
#define HTTP_STATUS_BAD_GATEWAY 502
#define HTTP_STATUS_SERVICE_UNAVAILABLE 503
#define HTTP_STATUS_GATEWAY_TIME_OUT 504
#define HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED 505

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

typedef struct HttpQueryParam {
    char *key;
    char *value;
    struct HttpQueryParam *next;
} HttpQueryParam;

const char *reason_from_status_code(int status);

#endif
