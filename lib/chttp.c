#include "chttp.h"
#include "cJSON.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <pthread.h>
#include <stdint.h>
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
    r->handler     = h;
    r->ws_handler  = NULL;
    r->sse_handler = NULL;
    return 0;
}

int chttp_ws_route(HttpServer *srv, const char *pattern, WsHandler h) {
    if (srv->route_count >= CHTTP_MAX_ROUTES) {
        fprintf(stderr, "chttp: route table full, cannot add WS %s\n", pattern);
        return -1;
    }
    Route *r = &srv->routes[srv->route_count++];
    strncpy(r->method,  "GET",   sizeof(r->method)  - 1);
    strncpy(r->pattern, pattern, sizeof(r->pattern) - 1);
    r->handler     = NULL;
    r->ws_handler  = h;
    r->sse_handler = NULL;
    return 0;
}

int chttp_sse_route(HttpServer *srv, const char *pattern, SseHandler h) {
    if (srv->route_count >= CHTTP_MAX_ROUTES) {
        fprintf(stderr, "chttp: route table full, cannot add SSE %s\n", pattern);
        return -1;
    }
    Route *r = &srv->routes[srv->route_count++];
    strncpy(r->method,  "GET",   sizeof(r->method)  - 1);
    strncpy(r->pattern, pattern, sizeof(r->pattern) - 1);
    r->handler     = NULL;
    r->ws_handler  = NULL;
    r->sse_handler = h;
    return 0;
}

/* ---- WebSocket: SHA-1 (RFC 3174) ---- */

#define SHA1_ROL(x,n) (((x)<<(n))|((uint32_t)(x)>>(32-(n))))

static void sha1(const unsigned char *data, size_t len, unsigned char out[20]) {
    uint32_t h0=0x67452301u, h1=0xEFCDAB89u, h2=0x98BADCFEu,
             h3=0x10325476u, h4=0xC3D2E1F0u;
    uint64_t bit_len  = (uint64_t)len * 8;
    size_t   pad_len  = ((len + 8) / 64 + 1) * 64;

    unsigned char *msg = calloc(pad_len, 1);
    if (!msg) return;
    memcpy(msg, data, len);
    msg[len] = 0x80;
    for (int i = 0; i < 8; i++)
        msg[pad_len - 8 + i] = (unsigned char)(bit_len >> (56 - i * 8));

    for (size_t bi = 0; bi < pad_len; bi += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++)
            w[i] = ((uint32_t)msg[bi+i*4]   << 24) | ((uint32_t)msg[bi+i*4+1] << 16)
                 | ((uint32_t)msg[bi+i*4+2] <<  8) |  (uint32_t)msg[bi+i*4+3];
        for (int i = 16; i < 80; i++)
            w[i] = SHA1_ROL(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);

        uint32_t a=h0, b=h1, c=h2, d=h3, e=h4;
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if      (i < 20) { f = (b & c) | (~b & d);          k = 0x5A827999u; }
            else if (i < 40) { f = b ^ c ^ d;                    k = 0x6ED9EBA1u; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
            else              { f = b ^ c ^ d;                    k = 0xCA62C1D6u; }
            uint32_t t = SHA1_ROL(a, 5) + f + e + k + w[i];
            e=d; d=c; c=SHA1_ROL(b,30); b=a; a=t;
        }
        h0+=a; h1+=b; h2+=c; h3+=d; h4+=e;
    }
    free(msg);

    uint32_t hh[5] = {h0, h1, h2, h3, h4};
    for (int i = 0; i < 5; i++) {
        out[i*4]   = (hh[i] >> 24) & 0xFF;
        out[i*4+1] = (hh[i] >> 16) & 0xFF;
        out[i*4+2] = (hh[i] >>  8) & 0xFF;
        out[i*4+3] =  hh[i]        & 0xFF;
    }
}

/* ---- WebSocket: base64 encode ---- */

static void base64_encode(const unsigned char *in, size_t len, char *out) {
    static const char tbl[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t i = 0, j = 0;
    for (; i + 2 < len; i += 3) {
        out[j++] = tbl[in[i]   >> 2];
        out[j++] = tbl[((in[i]   & 0x03) << 4) | (in[i+1] >> 4)];
        out[j++] = tbl[((in[i+1] & 0x0F) << 2) | (in[i+2] >> 6)];
        out[j++] = tbl[  in[i+2] & 0x3F];
    }
    if (i < len) {
        out[j++] = tbl[in[i] >> 2];
        if (i + 1 < len) {
            out[j++] = tbl[((in[i] & 0x03) << 4) | (in[i+1] >> 4)];
            out[j++] = tbl[ (in[i+1] & 0x0F) << 2];
        } else {
            out[j++] = tbl[(in[i] & 0x03) << 4];
            out[j++] = '=';
        }
        out[j++] = '=';
    }
    out[j] = '\0';
}

/* ---- WebSocket: HTTP 101 handshake ---- */

#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

static int ws_handshake(int fd, HttpRequest *req) {
    const char *key = chttp_header(req, "Sec-WebSocket-Key");
    if (!key) return -1;

    char combined[256];
    snprintf(combined, sizeof(combined), "%s" WS_GUID, key);

    unsigned char sha[20];
    sha1((unsigned char *)combined, strlen(combined), sha);

    char accept[32]; /* base64(20 bytes) = 28 chars + '\0' */
    base64_encode(sha, 20, accept);

    char resp[256];
    int n = snprintf(resp, sizeof(resp),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n\r\n", accept);
    write(fd, resp, n);
    return 0;
}

/* ---- WebSocket: frame I/O ---- */

static int recv_exact(int fd, void *buf, size_t n) {
    size_t got = 0;
    while (got < n) {
        int r = recv(fd, (char *)buf + got, n - got, 0);
        if (r <= 0) return -1;
        got += (size_t)r;
    }
    return 0;
}

/* Send an unmasked text frame (server → client). */
int chttp_ws_send(int fd, const char *payload, size_t len) {
    unsigned char hdr[10];
    int hlen = 0;
    hdr[hlen++] = 0x81; /* FIN + opcode text */
    if (len < 126) {
        hdr[hlen++] = (unsigned char)len;
    } else if (len < 65536) {
        hdr[hlen++] = 126;
        hdr[hlen++] = (unsigned char)((len >> 8) & 0xFF);
        hdr[hlen++] = (unsigned char)( len       & 0xFF);
    } else {
        hdr[hlen++] = 127;
        for (int i = 7; i >= 0; i--)
            hdr[hlen++] = (unsigned char)((len >> (i * 8)) & 0xFF);
    }
    if (write(fd, hdr, hlen) < 0) return -1;
    if (write(fd, payload, len) < 0) return -1;
    return 0;
}

/* Read one masked frame from client. Handles ping/close internally.
 * Returns: number of payload bytes (≥1), 0 if ping was handled, -1 on close/error. */
int chttp_ws_recv(int fd, char *buf, size_t bufsize) {
    unsigned char hdr[2];
    if (recv_exact(fd, hdr, 2) < 0) return -1;

    int opcode = hdr[0] & 0x0F;
    int masked  = (hdr[1] >> 7) & 1;
    size_t plen = hdr[1] & 0x7F;

    if (plen == 126) {
        unsigned char ext[2];
        if (recv_exact(fd, ext, 2) < 0) return -1;
        plen = ((size_t)ext[0] << 8) | ext[1];
    } else if (plen == 127) {
        unsigned char ext[8];
        if (recv_exact(fd, ext, 8) < 0) return -1;
        plen = 0;
        for (int i = 0; i < 8; i++) plen = (plen << 8) | ext[i];
    }

    unsigned char mask[4] = {0};
    if (masked && recv_exact(fd, mask, 4) < 0) return -1;

    if (opcode == 0x8) { /* Close — echo close frame */
        unsigned char close_frame[2] = {0x88, 0x00};
        write(fd, close_frame, 2);
        return -1;
    }

    if (plen >= bufsize) return -1; /* frame too large */

    if (plen > 0 && recv_exact(fd, buf, plen) < 0) return -1;
    if (masked)
        for (size_t i = 0; i < plen; i++) buf[i] ^= mask[i % 4];
    buf[plen] = '\0';

    if (opcode == 0x9) { /* Ping — send pong with same payload */
        unsigned char pong_hdr[2] = {0x8A, (unsigned char)plen};
        write(fd, pong_hdr, 2);
        if (plen > 0) write(fd, buf, plen);
        return 0;
    }

    return (int)plen;
}

/* ---- SSE helpers ---- */

static int sse_start(int fd) {
    static const char hdrs[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n";
    return write(fd, hdrs, sizeof(hdrs) - 1) < 0 ? -1 : 0;
}

/* Send one SSE event. event may be NULL for an anonymous data line.
 * Returns 0 on success, -1 if the client has disconnected. */
int chttp_sse_send(int fd, const char *event, const char *data) {
    char buf[4096];
    int n;
    if (event)
        n = snprintf(buf, sizeof(buf), "event: %s\ndata: %s\n\n", event, data);
    else
        n = snprintf(buf, sizeof(buf), "data: %s\n\n", data);
    return write(fd, buf, n) < 0 ? -1 : 0;
}

int chttp_dispatch(HttpServer *srv, HttpRequest *req, HttpResponse *res, int fd) {
    for (int i = 0; i < srv->route_count; i++) {
        Route *r = &srv->routes[i];
        if (strcmp(r->method, req->method) != 0) continue;

        HttpKV params[CHTTP_MAX_PATH_PARAMS];
        int count = 0;
        if (!chttp_match_route(r->pattern, req->path, params, &count)) continue;

        req->path_param_count = count;
        for (int j = 0; j < count; j++)
            req->path_params[j] = params[j];

        if (r->ws_handler) {
            const char *upgrade = chttp_header(req, "Upgrade");
            if (!upgrade || strcasecmp(upgrade, "websocket") != 0) {
                chttp_set_status(res, 426);
                chttp_send_text(res, "WebSocket upgrade required");
                return 1;
            }
            if (ws_handshake(fd, req) < 0) {
                chttp_set_status(res, 400);
                chttp_send_text(res, "WebSocket handshake failed");
                return 1;
            }
            r->ws_handler(fd);
            return -1; /* fd owned by ws handler */
        }

        if (r->sse_handler) {
            if (sse_start(fd) < 0) return -1;
            r->sse_handler(fd);
            return -1; /* fd owned by sse handler */
        }

        r->handler(req, res);
        return 1;
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

    req.fd = fd; /* expose client fd to handlers (used by fork-based auth) */

    int dispatched = chttp_dispatch(srv, &req, &res, fd);
    if (dispatched == -1) return NULL; /* WebSocket/SSE: handler owns fd */
    if (!dispatched) {
        res.status = 404;
        chttp_send_text(&res, "Not Found");
    }

    /* status == 0 is a sentinel meaning a forked child already sent the
     * response directly on fd; skip writing and just close our copy. */
    if (res.status != 0)
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
