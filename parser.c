#include <stdlib.h>
#include <string.h>

#include "parser.h"

struct HttpRequest *parse_request(const char *raw) {
    struct HttpRequest *req = NULL;
    req = malloc(sizeof(struct HttpRequest));
    if (!req) {
        return NULL;
    }
    memset(req, 0, sizeof(struct HttpRequest));

    /* Method */
    size_t meth_len = strcspn(raw, " ");
    if (memcmp(raw, "GET", strlen("GET")) == 0) {
        req->method = GET;
    } else if (memcmp(raw, "POST", strlen("POST")) == 0) {
        req->method = POST;
    } else {
        req->method = UNSUPPORTED;
    }
    raw += meth_len + 1;

    /* Request-URI */
    size_t url_len = strcspn(raw, " ");
    req->url = malloc(url_len + 1);
    if (!req->url) {
        free_request(req);
        return NULL;
    }
    memcpy(req->url, raw, url_len);
    req->url[url_len] = '\0';
    raw += url_len + 1;

    /* HTTP-Version */
    size_t ver_len = strcspn(raw, "\r\n");
    req->version = malloc(ver_len + 1);
    if (!req->version) {
        free_request(req);
        return NULL;
    }
    memcpy(req->version, raw, ver_len);
    req->version[ver_len] = '\0';
    raw += ver_len + 2; /* move past <CR><LF> */

    struct HttpRequestHeader *header = NULL, *last = NULL;
    while (raw[0]!='\r' || raw[1]!='\n') {
        last = header;
        header = malloc(sizeof(HttpRequestHeader));
        if (!header) {
            free_request(req);
            return NULL;
        }

        /* name */
        size_t name_len = strcspn(raw, ":");
        header->name = malloc(name_len + 1);
        if (!header->name) {
            free_request(req);
            return NULL;
        }
        memcpy(header->name, raw, name_len);
        header->name[name_len] = '\0';
        raw += name_len + 1;
        while (*raw == ' ') {
            raw++;
        }

        /* value */
        size_t value_len = strcspn(raw, "\r\n");
        header->value = malloc(value_len + 1);
        if (!header->value) {
            free_request(req);
            return NULL;
        }
        memcpy(header->value, raw, value_len);
        header->value[value_len] = '\0';
        raw += value_len + 2; /* move past <CR><LF> */

        /* next */
        header->next = last;
    }
    req->headers = header;
    raw += 2; /* move past <CR><LF> */

    size_t body_len = strlen(raw);
    req->body = malloc(body_len + 1);
    if (!req->body) {
        free_request(req);
        return NULL;
    }
    memcpy(req->body, raw, body_len);
    req->body[body_len] = '\0';

    return req;
}

void free_header(struct HttpRequestHeader *h) {
    if (h) {
        free(h->name);
        free(h->value);
        free_header(h->next);
        free(h);
    }
}

void free_request(struct HttpRequest *req) {
    free(req->url);
    free(req->version);
    free_header(req->headers);
    free(req->body);
    free(req);
}
