#ifndef HTTP_H
#define HTTP_H

#include <sys/types.h>

typedef enum HTTP_METHOD{GET, POST} HttpMethod;

typedef struct {
    const char* key;
    const char* value;
} HttpHeader;

typedef struct {
    HttpMethod method;
    char* path;
    HttpHeader* headers;
    unsigned int numHeaders;
    unsigned int headerSpace;
    char* body;
    size_t bodySize;
} HttpRequest;

typedef struct {
    int responseCode;
    HttpHeader* headers;
    unsigned int numHeaders;
    unsigned int headerSpace;
    char* body;
    size_t bodySize;
} HttpResponse;

HttpResponse* send_request(int, HttpRequest*);

HttpRequest* create_request(HttpMethod);
void add_header(HttpRequest*, HttpHeader);
void free_request(HttpRequest*);
void free_response(HttpResponse*);

// Utility methods
HttpHeader* http_find_header(HttpResponse*, const char*);

#endif
