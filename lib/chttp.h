#ifndef CHTTP_H
#define CHTTP_H

#include <stddef.h>
#include <sys/types.h>
#include "cJSON.h"

#define CHTTP_MAX_HEADERS      32
#define CHTTP_MAX_QUERY_PARAMS 16
#define CHTTP_MAX_PATH_PARAMS  8
#define CHTTP_MAX_ROUTES       64
#define CHTTP_READ_BUFSIZE     8192

typedef struct {
    char key[128];
    char value[512];
} HttpKV;

/* Opaque connection handle — wraps a raw fd and, when built with -DCHTTP_TLS,
 * an OpenSSL SSL* so all I/O automatically goes through the right layer. */
typedef struct CConn CConn;

typedef struct {
    CConn *conn;              /* client connection (TLS-aware) */
    char method[8];
    char path[1024];
    char version[16];
    HttpKV headers[CHTTP_MAX_HEADERS];    int header_count;
    HttpKV query[CHTTP_MAX_QUERY_PARAMS]; int query_count;
    HttpKV path_params[CHTTP_MAX_PATH_PARAMS]; int path_param_count;
    char *body; size_t body_len;
    char *body_heap;                  /* non-NULL when body was heap-allocated */
    char raw_buf[CHTTP_READ_BUFSIZE]; /* owns raw bytes (headers + partial body) */
} HttpRequest;

typedef struct {
    int status;
    HttpKV headers[CHTTP_MAX_HEADERS]; int header_count;
    char  *body;
    size_t body_len;
    size_t body_cap;
} HttpResponse;

typedef void (*RouteHandler)(HttpRequest *req, HttpResponse *res);
typedef void (*WsHandler)(CConn *conn);  /* called after WS handshake */
typedef void (*SseHandler)(CConn *conn); /* called after SSE headers flushed */

typedef struct {
    char method[8];
    char pattern[256];
    RouteHandler handler;     /* NULL for WS/SSE routes */
    WsHandler    ws_handler;  /* NULL for HTTP/SSE routes */
    SseHandler   sse_handler; /* NULL for HTTP/WS routes */
    int  is_streaming;        /* 1 = skip body buffering; handler reads conn directly */
} Route;

typedef struct {
    int    port;
    int    server_fd;
    size_t max_body_size; /* max request body bytes; 0 = unlimited */
    Route  routes[CHTTP_MAX_ROUTES]; int route_count;
#ifdef CHTTP_TLS
    void  *ssl_ctx;       /* SSL_CTX* — NULL when TLS disabled */
#endif
} HttpServer;

/* ---- Server lifecycle ---- */
int  chttp_server_init(HttpServer *srv, int port);
void chttp_server_run(HttpServer *srv);
void chttp_server_destroy(HttpServer *srv);

/* Enable TLS on the server.  Must be called after chttp_server_init() and
 * before chttp_server_run().  cert_path and key_path are PEM files.
 * Only available when built with -DCHTTP_TLS. */
int  chttp_server_enable_tls(HttpServer *srv,
                              const char *cert_path,
                              const char *key_path);

/* ---- Routing ---- */
int  chttp_route(HttpServer *srv, const char *method, const char *pattern, RouteHandler h);
int  chttp_stream_route(HttpServer *srv, const char *method, const char *pattern, RouteHandler h);
int  chttp_ws_route(HttpServer *srv, const char *pattern, WsHandler h);
int  chttp_sse_route(HttpServer *srv, const char *pattern, SseHandler h);
int  chttp_match_route(const char *pattern, const char *path, HttpKV *params, int *count);
int  chttp_dispatch(HttpServer *srv, HttpRequest *req, HttpResponse *res, CConn *conn);

#define CHTTP_GET(s,p,h)    chttp_route(s, "GET",    p, h)
#define CHTTP_POST(s,p,h)   chttp_route(s, "POST",   p, h)
#define CHTTP_PUT(s,p,h)    chttp_route(s, "PUT",    p, h)
#define CHTTP_DELETE(s,p,h) chttp_route(s, "DELETE", p, h)
#define CHTTP_PATCH(s,p,h)  chttp_route(s, "PATCH",  p, h)
#define CHTTP_WS(s,p,h)          chttp_ws_route(s,  p, h)
#define CHTTP_SSE(s,p,h)         chttp_sse_route(s, p, h)
#define CHTTP_HEAD(s,p,h)        chttp_route(s, "HEAD", p, h)
#define CHTTP_STREAM_POST(s,p,h) chttp_stream_route(s, "POST", p, h)
#define CHTTP_STREAM_GET(s,p,h)  chttp_stream_route(s, "GET",  p, h)

/* ---- CConn accessors ---- */

/* Raw file descriptor — use for dup2/close in forked children, NOT for I/O. */
int     chttp_conn_fd(CConn *conn);

/* TLS-aware I/O wrappers — use these instead of write()/recv() on the client. */
ssize_t chttp_conn_write(CConn *conn, const void *buf, size_t n);
ssize_t chttp_conn_recv(CConn *conn, void *buf, size_t n, int flags);

/* Send `count` bytes from an already-open file fd.  Uses sendfile(2) for
 * plain connections; falls back to a read+write loop through TLS. */
int     chttp_conn_sendfile(CConn *conn, int file_fd, size_t count);

/* Close the connection cleanly (TLS shutdown if applicable) and free the
 * CConn struct.  Call this from connection owners (connection_thread, child). */
void    chttp_conn_close(CConn *conn);

/* Release the CConn struct WITHOUT sending a TLS close_notify.  Use this in
 * the forked parent after fork() so that only the child closes the TLS session. */
void    chttp_conn_free(CConn *conn);

/* ---- WebSocket I/O (usable in WsHandler callbacks) ---- */
int chttp_ws_send(CConn *conn, const char *payload, size_t len);
int chttp_ws_recv(CConn *conn, char *buf, size_t bufsize);

/* ---- SSE I/O (usable in SseHandler callbacks) ---- */
int chttp_sse_send(CConn *conn, const char *event, const char *data);

/* ---- Parsing ---- */
int         chttp_parse_request(HttpRequest *req, const char *raw, int raw_len);
const char *chttp_query_param(HttpRequest *req, const char *key);
const char *chttp_header(HttpRequest *req, const char *key);
const char *chttp_path_param(HttpRequest *req, const char *name);
void        chttp_url_decode(const char *src, char *dst, size_t dst_size);

/* ---- Response building ---- */
void chttp_set_status(HttpResponse *res, int code);
void chttp_set_header(HttpResponse *res, const char *key, const char *val);
void chttp_add_header(HttpResponse *res, const char *key, const char *val);
void chttp_send_text(HttpResponse *res, const char *text);
void chttp_send_json(HttpResponse *res, const char *json_str);
void chttp_send_cjson(HttpResponse *res, cJSON *obj);
int  chttp_write_response(CConn *conn, HttpResponse *res);

/* Pre-allocate `size` bytes in res->body for direct writing (e.g. fread). */
int  chttp_body_alloc(HttpResponse *res, size_t size);

/* Free res->body and zero body fields. Safe on a zero-init'd res. */
void chttp_response_free(HttpResponse *res);

#endif /* CHTTP_H */
