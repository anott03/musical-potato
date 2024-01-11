#ifndef C11CODEREVIEW_LIB_H
#define C11CODEREVIEW_LIB_H

typedef enum Method {UNSUPPORTED, GET, POST} Method;

typedef struct HttpRequestHeader {
    char *name;
    char *value;
    struct HttpRequestHeader *next;
} HttpRequestHeader;

typedef struct HttpRequest {
    enum Method method;
    char *url;
    char *version;
    struct HttpRequestHeader *headers;
    char *body;
} HttpRequest;


struct HttpRequest *parse_request(const char *raw);
void free_header(struct HttpRequestHeader *h);
void free_request(struct HttpRequest *req);

#endif
