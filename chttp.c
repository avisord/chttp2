#include "chttp.h"
#include "cJSON.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ---- URL decode (ported from rpmi_server.c) ---- */

static char from_hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

void chttp_url_decode(const char *src, char *dst, size_t dst_size) {
    const char *p = src;
    char *q = dst;
    char *end = dst + dst_size - 1;

    while (*p && q < end) {
        if (*p == '%' && isxdigit((unsigned char)*(p+1)) && isxdigit((unsigned char)*(p+2))) {
            *q++ = (from_hex(*(p+1)) << 4) | from_hex(*(p+2));
            p += 3;
        } else if (*p == '+') {
            *q++ = ' ';
            p++;
        } else {
            *q++ = *p++;
        }
    }
    *q = '\0';
}

/* ---- Query string parsing ---- */

static void parse_query_string(char *qs, HttpRequest *req) {
    while (qs && *qs && req->query_count < CHTTP_MAX_QUERY_PARAMS) {
        char *next = strchr(qs, '&');
        if (next) *next++ = '\0';

        char *eq = strchr(qs, '=');
        if (eq) {
            *eq = '\0';
            HttpKV *kv = &req->query[req->query_count++];
            strncpy(kv->key, qs, sizeof(kv->key) - 1);
            kv->key[sizeof(kv->key) - 1] = '\0';
            chttp_url_decode(eq + 1, kv->value, sizeof(kv->value));
        }

        qs = next;
    }
}

/* ---- Request parsing ---- */

int chttp_parse_request(HttpRequest *req, const char *raw, int raw_len) {
    memset(req, 0, sizeof(*req));

    if (raw_len >= CHTTP_READ_BUFSIZE)
        raw_len = CHTTP_READ_BUFSIZE - 1;
    memcpy(req->raw_buf, raw, raw_len);
    req->raw_buf[raw_len] = '\0';

    /* Parse request line */
    char path_qs[1024] = {0};
    if (sscanf(req->raw_buf, "%7s %1023s %15s", req->method, path_qs, req->version) != 3)
        return -1;

    /* Split path and query string */
    char *q = strchr(path_qs, '?');
    if (q) {
        *q = '\0';
        strncpy(req->path, path_qs, sizeof(req->path) - 1);
        parse_query_string(q + 1, req);
    } else {
        strncpy(req->path, path_qs, sizeof(req->path) - 1);
    }
    req->path[sizeof(req->path) - 1] = '\0';

    /* Parse headers — walk line by line after the request line */
    char *line = strstr(req->raw_buf, "\r\n");
    if (!line) return -1;
    line += 2;

    while (req->header_count < CHTTP_MAX_HEADERS) {
        /* Empty line = end of headers */
        if (line[0] == '\r' && line[1] == '\n') break;
        if (line[0] == '\0') break;

        char *colon = strchr(line, ':');
        char *crlf  = strstr(line, "\r\n");
        if (!colon || !crlf || colon > crlf) {
            if (crlf) line = crlf + 2;
            else break;
            continue;
        }

        HttpKV *hdr = &req->headers[req->header_count++];

        /* Key: trim trailing whitespace */
        int key_len = (int)(colon - line);
        if (key_len >= (int)sizeof(hdr->key)) key_len = (int)sizeof(hdr->key) - 1;
        strncpy(hdr->key, line, key_len);
        hdr->key[key_len] = '\0';
        for (int k = key_len - 1; k >= 0 && hdr->key[k] == ' '; k--)
            hdr->key[k] = '\0';

        /* Value: skip leading whitespace */
        char *val_start = colon + 1;
        while (*val_start == ' ') val_start++;
        int val_len = (int)(crlf - val_start);
        if (val_len >= (int)sizeof(hdr->value)) val_len = (int)sizeof(hdr->value) - 1;
        strncpy(hdr->value, val_start, val_len);
        hdr->value[val_len] = '\0';

        line = crlf + 2;
    }

    /* Body: pointer into raw_buf past \r\n\r\n */
    char *body_start = strstr(req->raw_buf, "\r\n\r\n");
    if (body_start) {
        req->body = body_start + 4;
        ptrdiff_t offset = req->body - req->raw_buf;
        req->body_len = (offset < raw_len) ? (size_t)(raw_len - offset) : 0;
    }

    return 0;
}

/* ---- Lookup helpers ---- */

const char *chttp_query_param(HttpRequest *req, const char *key) {
    for (int i = 0; i < req->query_count; i++)
        if (strcmp(req->query[i].key, key) == 0)
            return req->query[i].value;
    return NULL;
}

const char *chttp_header(HttpRequest *req, const char *key) {
    for (int i = 0; i < req->header_count; i++)
        if (strcasecmp(req->headers[i].key, key) == 0)
            return req->headers[i].value;
    return NULL;
}

const char *chttp_path_param(HttpRequest *req, const char *name) {
    for (int i = 0; i < req->path_param_count; i++)
        if (strcmp(req->path_params[i].key, name) == 0)
            return req->path_params[i].value;
    return NULL;
}

/* ---- Path parameter matching ---- */

int chttp_match_route(const char *pattern, const char *path,
                      HttpKV *params, int *count) {
    *count = 0;
    const char *p = pattern;
    const char *u = path;

    while (*p || *u) {
        /* Consume leading slash */
        if (*p == '/' && *u == '/') { p++; u++; continue; }
        if (*p == '/' || *u == '/') return 0;

        /* Find end of each segment */
        const char *p_end = strchr(p, '/');
        if (!p_end) p_end = p + strlen(p);

        const char *u_end = strchr(u, '/');
        if (!u_end) u_end = u + strlen(u);

        if (*p == ':') {
            /* Capture wildcard segment */
            if (*count < CHTTP_MAX_PATH_PARAMS) {
                HttpKV *kv = &params[*count];

                int name_len = (int)(p_end - p) - 1; /* skip ':' */
                if (name_len >= (int)sizeof(kv->key)) name_len = (int)sizeof(kv->key) - 1;
                strncpy(kv->key, p + 1, name_len);
                kv->key[name_len] = '\0';

                int val_len = (int)(u_end - u);
                if (val_len >= (int)sizeof(kv->value)) val_len = (int)sizeof(kv->value) - 1;
                strncpy(kv->value, u, val_len);
                kv->value[val_len] = '\0';

                (*count)++;
            }
        } else {
            /* Exact segment match */
            int p_len = (int)(p_end - p);
            int u_len = (int)(u_end - u);
            if (p_len != u_len || strncmp(p, u, p_len) != 0)
                return 0;
        }

        p = p_end;
        u = u_end;
    }

    return (*p == '\0' && *u == '\0') ? 1 : 0;
}

/* ---- Routing ---- */

int chttp_route(HttpServer *srv, const char *method,
                const char *pattern, RouteHandler h) {
    if (srv->route_count >= CHTTP_MAX_ROUTES) {
        fprintf(stderr, "chttp: route table full, cannot add %s %s\n", method, pattern);
        return -1;
    }
    Route *r = &srv->routes[srv->route_count++];
    strncpy(r->method,  method,  sizeof(r->method)  - 1);
    strncpy(r->pattern, pattern, sizeof(r->pattern) - 1);
    r->handler = h;
    return 0;
}

int chttp_dispatch(HttpServer *srv, HttpRequest *req, HttpResponse *res) {
    for (int i = 0; i < srv->route_count; i++) {
        Route *r = &srv->routes[i];
        if (strcmp(r->method, req->method) != 0) continue;

        HttpKV params[CHTTP_MAX_PATH_PARAMS];
        int count = 0;
        if (chttp_match_route(r->pattern, req->path, params, &count)) {
            req->path_param_count = count;
            for (int j = 0; j < count; j++)
                req->path_params[j] = params[j];
            r->handler(req, res);
            return 1;
        }
    }
    return 0;
}

/* ---- Response building ---- */

static const char *status_text(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default:  return "Unknown";
    }
}

void chttp_set_status(HttpResponse *res, int code) {
    res->status = code;
}

void chttp_set_header(HttpResponse *res, const char *key, const char *val) {
    for (int i = 0; i < res->header_count; i++) {
        if (strcasecmp(res->headers[i].key, key) == 0) {
            strncpy(res->headers[i].value, val, sizeof(res->headers[i].value) - 1);
            return;
        }
    }
    if (res->header_count >= CHTTP_MAX_HEADERS) return;
    HttpKV *h = &res->headers[res->header_count++];
    strncpy(h->key,   key, sizeof(h->key)   - 1);
    strncpy(h->value, val, sizeof(h->value) - 1);
}

void chttp_send_text(HttpResponse *res, const char *text) {
    chttp_set_header(res, "Content-Type", "text/plain");
    size_t len = strlen(text);
    if (len >= CHTTP_RESP_BUFSIZE) len = CHTTP_RESP_BUFSIZE - 1;
    memcpy(res->body, text, len);
    res->body[len] = '\0';
    res->body_len = len;
}

void chttp_send_json(HttpResponse *res, const char *json_str) {
    chttp_set_header(res, "Content-Type", "application/json");
    size_t len = strlen(json_str);
    if (len >= CHTTP_RESP_BUFSIZE) len = CHTTP_RESP_BUFSIZE - 1;
    memcpy(res->body, json_str, len);
    res->body[len] = '\0';
    res->body_len = len;
}

void chttp_send_cjson(HttpResponse *res, cJSON *obj) {
    char *str = cJSON_PrintUnformatted(obj);
    if (!str) { chttp_send_json(res, "{}"); return; }
    chttp_send_json(res, str);
    free(str);
}

int chttp_write_response(int fd, HttpResponse *res) {
    char hbuf[4096];
    int n = snprintf(hbuf, sizeof(hbuf),
                     "HTTP/1.1 %d %s\r\n", res->status, status_text(res->status));

    for (int i = 0; i < res->header_count; i++)
        n += snprintf(hbuf + n, sizeof(hbuf) - n,
                      "%s: %s\r\n", res->headers[i].key, res->headers[i].value);

    n += snprintf(hbuf + n, sizeof(hbuf) - n,
                  "Content-Length: %zu\r\n\r\n", res->body_len);

    write(fd, hbuf, n);
    if (res->body_len > 0)
        write(fd, res->body, res->body_len);

    return 0;
}

/* ---- Server lifecycle ---- */

typedef struct {
    HttpServer *srv;
    int client_fd;
} ConnectionArgs;

static void *connection_thread(void *arg) {
    ConnectionArgs *ca = (ConnectionArgs *)arg;
    HttpServer *srv = ca->srv;
    int fd = ca->client_fd;
    free(ca);

    char buf[CHTTP_READ_BUFSIZE];
    int n = recv(fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) { close(fd); return NULL; }
    buf[n] = '\0';

    HttpRequest  req;
    HttpResponse res;
    memset(&res, 0, sizeof(res));
    res.status = 200;

    if (chttp_parse_request(&req, buf, n) < 0) {
        const char *bad = "HTTP/1.1 400 Bad Request\r\nContent-Length: 11\r\n\r\nBad Request";
        write(fd, bad, strlen(bad));
        close(fd);
        return NULL;
    }

    if (!chttp_dispatch(srv, &req, &res)) {
        res.status = 404;
        chttp_send_text(&res, "Not Found");
    }

    chttp_write_response(fd, &res);
    close(fd);
    return NULL;
}

int chttp_server_init(HttpServer *srv, int port) {
    memset(srv, 0, sizeof(*srv));
    srv->port = port;

    srv->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (srv->server_fd < 0) { perror("socket"); return -1; }

    int opt = 1;
    setsockopt(srv->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(srv->server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); return -1;
    }
    if (listen(srv->server_fd, 10) < 0) {
        perror("listen"); return -1;
    }
    return 0;
}

void chttp_server_run(HttpServer *srv) {
    printf("Listening on port %d\n", srv->port);
    while (1) {
        int client_fd = accept(srv->server_fd, NULL, NULL);
        if (client_fd < 0) { perror("accept"); continue; }

        ConnectionArgs *ca = malloc(sizeof(*ca));
        if (!ca) { close(client_fd); continue; }
        ca->srv = srv;
        ca->client_fd = client_fd;

        pthread_t tid;
        pthread_create(&tid, NULL, connection_thread, ca);
        pthread_detach(tid);
    }
}

void chttp_server_destroy(HttpServer *srv) {
    close(srv->server_fd);
}
