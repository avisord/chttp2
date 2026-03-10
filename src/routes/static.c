#define _GNU_SOURCE
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "chttp.h"
#include "utils/utils.h"

#define STATIC_ROOT "www"

/* Limit decoded relative path so full_path + "/index.html" always fits in PATH_MAX */
#define REL_PATH_MAX  2048
/* PATH_MAX minus space for "/index.html\0" so idx snprintf never truncates */
#define FULL_PATH_MAX (PATH_MAX - 16)

static const char CORS[] =
    "Access-Control-Allow-Origin: http://localhost:8081\r\n"
    "Access-Control-Allow-Credentials: true\r\n"
    "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, PATCH, OPTIONS\r\n"
    "Access-Control-Allow-Headers: Content-Type, Cookie, X-Chunk-Index\r\n";

static void send_error(int fd, int code, const char *text) {
    char buf[512];
    int n = snprintf(buf, sizeof(buf),
        "HTTP/1.1 %d %s\r\n"
        "Content-Length: 0\r\n"
        "%s"
        "\r\n",
        code, text, CORS);
    write(fd, buf, n);
}

static void send_304(int fd, const char *etag) {
    char buf[512];
    int n = snprintf(buf, sizeof(buf),
        "HTTP/1.1 304 Not Modified\r\n"
        "ETag: %s\r\n"
        "%s"
        "\r\n",
        etag, CORS);
    write(fd, buf, n);
}

void handle_static(HttpRequest *req, HttpResponse *res) {
    /* Strip known prefix: "/static/" or fall back to skipping leading "/" */
    const char *rel_raw;
    if (strncmp(req->path, "/static/", 8) == 0)
        rel_raw = req->path + 8;
    else
        rel_raw = req->path + 1; /* skip leading slash for catch-all routes */

    /* URL-decode; bounded to keep full_path and idx within PATH_MAX */
    char rel_decoded[REL_PATH_MAX];
    chttp_url_decode(rel_raw, rel_decoded, sizeof(rel_decoded));

    /* Reject path traversal */
    if (strstr(rel_decoded, "..")) {
        send_error(req->fd, 400, "Bad Request");
        res->status = 0;
        return;
    }

    /* Build full path — at most strlen("www/") + REL_PATH_MAX - 1 < FULL_PATH_MAX */
    char full_path[FULL_PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", STATIC_ROOT, rel_decoded);

    struct stat st;
    if (stat(full_path, &st) < 0) {
        send_error(req->fd, 404, "Not Found");
        res->status = 0;
        return;
    }

    /* Directory without trailing slash: redirect so relative URLs work */
    if (S_ISDIR(st.st_mode) && req->path[strlen(req->path) - 1] != '/') {
        char rbuf[512];
        int n = snprintf(rbuf, sizeof(rbuf),
            "HTTP/1.1 301 Moved Permanently\r\n"
            "Location: %s/\r\n"
            "Content-Length: 0\r\n"
            "%s"
            "\r\n",
            req->path, CORS);
        write(req->fd, rbuf, n);
        res->status = 0;
        return;
    }

    /* Directory: look for index.html then index.htm */
    if (S_ISDIR(st.st_mode)) {
        char idx[PATH_MAX];
        snprintf(idx, sizeof(idx), "%s/index.html", full_path);
        if (stat(idx, &st) == 0 && S_ISREG(st.st_mode)) {
            memcpy(full_path, idx, sizeof(full_path));
        } else {
            snprintf(idx, sizeof(idx), "%s/index.htm", full_path);
            if (stat(idx, &st) == 0 && S_ISREG(st.st_mode)) {
                memcpy(full_path, idx, sizeof(full_path));
            } else {
                send_error(req->fd, 403, "Forbidden");
                res->status = 0;
                return;
            }
        }
    }

    /* Must be a regular file */
    if (!S_ISREG(st.st_mode)) {
        send_error(req->fd, 404, "Not Found");
        res->status = 0;
        return;
    }

    /* realpath check: prevent symlink escape outside www/ */
    char root_rp[PATH_MAX], file_rp[PATH_MAX];
    if (!realpath(STATIC_ROOT, root_rp) || !realpath(full_path, file_rp)) {
        send_error(req->fd, 404, "Not Found");
        res->status = 0;
        return;
    }
    size_t root_len = strlen(root_rp);
    if (strncmp(file_rp, root_rp, root_len) != 0 ||
        (file_rp[root_len] != '/' && file_rp[root_len] != '\0')) {
        send_error(req->fd, 403, "Forbidden");
        res->status = 0;
        return;
    }

    /* ETag: mtime + size hex */
    char etag[64];
    snprintf(etag, sizeof(etag), "\"%llx-%llx\"",
             (long long)st.st_mtime, (long long)st.st_size);

    /* If-None-Match */
    const char *inm = chttp_header(req, "If-None-Match");
    if (inm && strcmp(inm, etag) == 0) {
        send_304(req->fd, etag);
        res->status = 0;
        return;
    }

    /* If-Modified-Since */
    const char *ims_str = chttp_header(req, "If-Modified-Since");
    if (ims_str) {
        struct tm tm_ims = {0};
        if (strptime(ims_str, "%a, %d %b %Y %H:%M:%S GMT", &tm_ims)) {
            time_t ims_time = timegm(&tm_ims);
            if (st.st_mtime <= ims_time) {
                send_304(req->fd, etag);
                res->status = 0;
                return;
            }
        }
    }

    /* Range parsing */
    off_t range_start = 0;
    off_t range_end   = st.st_size - 1;
    int   has_range   = 0;

    const char *range_hdr = chttp_header(req, "Range");
    if (range_hdr && strncmp(range_hdr, "bytes=", 6) == 0) {
        const char *spec = range_hdr + 6;
        const char *dash = strchr(spec, '-');
        if (dash) {
            long long s = -1, e = -1;
            if (dash > spec)   s = strtoll(spec,     NULL, 10);
            if (*(dash + 1))   e = strtoll(dash + 1, NULL, 10);

            if (s < 0 && e >= 0) {
                /* suffix range: -N */
                range_start = st.st_size - e;
                range_end   = st.st_size - 1;
            } else if (s >= 0 && e < 0) {
                /* open-ended: N- */
                range_start = s;
                range_end   = st.st_size - 1;
            } else if (s >= 0 && e >= 0) {
                range_start = s;
                range_end   = e;
            }

            /* Clamp and validate */
            if (range_start < 0) range_start = 0;
            if (range_end >= st.st_size) range_end = st.st_size - 1;

            if (range_start > range_end || range_start >= st.st_size) {
                char buf[512];
                int n = snprintf(buf, sizeof(buf),
                    "HTTP/1.1 416 Range Not Satisfiable\r\n"
                    "Content-Range: bytes */%lld\r\n"
                    "Content-Length: 0\r\n"
                    "%s"
                    "\r\n",
                    (long long)st.st_size, CORS);
                write(req->fd, buf, n);
                res->status = 0;
                return;
            }
            has_range = 1;
        }
    }

    off_t content_length = range_end - range_start + 1;
    int   status_code    = has_range ? 206 : 200;
    const char *status_text = has_range ? "Partial Content" : "OK";

    /* Open file */
    int file_fd = open(full_path, O_RDONLY);
    if (file_fd < 0) {
        send_error(req->fd, 500, "Internal Server Error");
        res->status = 0;
        return;
    }

    /* Last-Modified */
    char last_modified[64];
    struct tm tm_gm;
    gmtime_r(&st.st_mtime, &tm_gm);
    strftime(last_modified, sizeof(last_modified),
             "%a, %d %b %Y %H:%M:%S GMT", &tm_gm);

    /* MIME type */
    const char *mime = mime_from_ext(full_path);
    if (!mime) mime = "application/octet-stream";

    /* Write response headers */
    char hbuf[1024];
    int  n;
    if (has_range) {
        n = snprintf(hbuf, sizeof(hbuf),
            "HTTP/1.1 %d %s\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %lld\r\n"
            "ETag: %s\r\n"
            "Last-Modified: %s\r\n"
            "Accept-Ranges: bytes\r\n"
            "Content-Range: bytes %lld-%lld/%lld\r\n"
            "%s"
            "\r\n",
            status_code, status_text,
            mime,
            (long long)content_length,
            etag,
            last_modified,
            (long long)range_start, (long long)range_end, (long long)st.st_size,
            CORS);
    } else {
        n = snprintf(hbuf, sizeof(hbuf),
            "HTTP/1.1 %d %s\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %lld\r\n"
            "ETag: %s\r\n"
            "Last-Modified: %s\r\n"
            "Accept-Ranges: bytes\r\n"
            "%s"
            "\r\n",
            status_code, status_text,
            mime,
            (long long)content_length,
            etag,
            last_modified,
            CORS);
    }
    write(req->fd, hbuf, n);

    /* Send body for GET only (HEAD omits body) */
    if (strcmp(req->method, "GET") == 0) {
        off_t offset    = range_start;
        off_t remaining = content_length;
        while (remaining > 0) {
            ssize_t sent = sendfile(req->fd, file_fd, &offset, (size_t)remaining);
            if (sent <= 0) break;
            remaining -= sent;
        }
    }

    close(file_fd);
    res->status = 0;
}
