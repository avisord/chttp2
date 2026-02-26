#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "chttp.h"

/* GET / */
static void handle_root(HttpRequest *req, HttpResponse *res) {
    (void)req;
    chttp_send_text(res, "Welcome to chttp!");
}

/* GET /hello */
static void handle_hello(HttpRequest *req, HttpResponse *res) {
    (void)req;
    chttp_send_text(res, "Hello, World!");
}

/* GET /users/:id */
static void handle_get_user(HttpRequest *req, HttpResponse *res) {
    const char *id = chttp_path_param(req, "id");
    if (!id) {
        chttp_set_status(res, 400);
        chttp_send_text(res, "Missing user id");
        return;
    }

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "id",   id);
    cJSON_AddStringToObject(obj, "name", "Alice");
    chttp_send_cjson(res, obj);
    cJSON_Delete(obj);
}

/* POST /users — expects JSON body {"name":"..."} */
static void handle_create_user(HttpRequest *req, HttpResponse *res) {
    if (!req->body || req->body_len == 0) {
        chttp_set_status(res, 400);
        chttp_send_text(res, "Empty body");
        return;
    }

    cJSON *body = cJSON_ParseWithLength(req->body, req->body_len);
    if (!body) {
        chttp_set_status(res, 400);
        chttp_send_text(res, "Invalid JSON");
        return;
    }

    cJSON *name_item = cJSON_GetObjectItem(body, "name");
    const char *name = (name_item && cJSON_IsString(name_item))
                       ? name_item->valuestring : "Unknown";

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "created");
    cJSON_AddStringToObject(resp, "name",   name);

    chttp_set_status(res, 201);
    chttp_send_cjson(res, resp);

    cJSON_Delete(body);
    cJSON_Delete(resp);
}

/* GET /echo?msg=... */
static void handle_echo(HttpRequest *req, HttpResponse *res) {
    const char *msg = chttp_query_param(req, "msg");
    if (!msg) {
        chttp_send_text(res, "(no msg query param)");
        return;
    }
    chttp_send_text(res, msg);
}

/* Detect MIME type from file extension */
static const char *mime_from_ext(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot) return "application/octet-stream";
    if (strcmp(dot, ".txt")  == 0) return "text/plain";
    if (strcmp(dot, ".html") == 0) return "text/html";
    if (strcmp(dot, ".json") == 0) return "application/json";
    if (strcmp(dot, ".png")  == 0) return "image/png";
    if (strcmp(dot, ".jpg")  == 0 ||
        strcmp(dot, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(dot, ".gif")  == 0) return "image/gif";
    if (strcmp(dot, ".pdf")  == 0) return "application/pdf";
    if (strcmp(dot, ".csv")  == 0) return "text/csv";
    return "application/octet-stream";
}

/* Validate filename: no path traversal, no slashes */
static int safe_filename(const char *name) {
    return name && *name
        && !strchr(name, '/')
        && !strchr(name, '\\')
        && !strstr(name, "..");
}

/* GET /fdownload/:filename */
static void handle_download(HttpRequest *req, HttpResponse *res) {
    const char *filename = chttp_path_param(req, "filename");
    if (!safe_filename(filename)) {
        chttp_set_status(res, 400);
        chttp_send_text(res, "Invalid filename");
        return;
    }

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "uploads/%s", filename);

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        chttp_set_status(res, 404);
        chttp_send_text(res, "File not found");
        return;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize < 0 || fsize >= (long)CHTTP_RESP_BUFSIZE) {
        fclose(f);
        chttp_set_status(res, 500);
        chttp_send_text(res, "File too large to serve");
        return;
    }

    res->body_len = fread(res->body, 1, (size_t)fsize, f);
    fclose(f);

    chttp_set_header(res, "Content-Type", mime_from_ext(filename));

    char cd[320];
    snprintf(cd, sizeof(cd), "attachment; filename=\"%s\"", filename);
    chttp_set_header(res, "Content-Disposition", cd);
}

/* GET /fmetadata/:filename */
static void handle_fmetadata(HttpRequest *req, HttpResponse *res) {
    const char *filename = chttp_path_param(req, "filename");
    if (!safe_filename(filename)) {
        chttp_set_status(res, 400);
        chttp_send_text(res, "Invalid filename");
        return;
    }

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "uploads/%s", filename);

    struct stat st;
    if (stat(filepath, &st) != 0) {
        chttp_set_status(res, 404);
        chttp_send_text(res, "File not found");
        return;
    }

    char mtime_str[32];
    struct tm *tm_info = gmtime(&st.st_mtime);
    strftime(mtime_str, sizeof(mtime_str), "%Y-%m-%dT%H:%M:%SZ", tm_info);

    cJSON *meta = cJSON_CreateObject();
    cJSON_AddStringToObject(meta, "filename",     filename);
    cJSON_AddStringToObject(meta, "path",         filepath);
    cJSON_AddStringToObject(meta, "content_type", mime_from_ext(filename));
    cJSON_AddNumberToObject(meta, "size_bytes",   (double)st.st_size);
    cJSON_AddStringToObject(meta, "modified_at",  mtime_str);

    chttp_send_cjson(res, meta);
    cJSON_Delete(meta);
}

/* POST /upload — multipart/form-data file upload */
static void handle_upload(HttpRequest *req, HttpResponse *res) {
    /* Validate Content-Type */
    const char *ct = chttp_header(req, "Content-Type");
    if (!ct || strncmp(ct, "multipart/form-data", 19) != 0) {
        chttp_set_status(res, 400);
        chttp_send_text(res, "Expected multipart/form-data");
        return;
    }

    /* Extract boundary */
    const char *bp = strstr(ct, "boundary=");
    if (!bp) {
        chttp_set_status(res, 400);
        chttp_send_text(res, "Missing boundary");
        return;
    }
    bp += 9; /* skip "boundary=" */

    char boundary[128] = {0};
    strncpy(boundary, bp, sizeof(boundary) - 1);
    /* Strip optional quotes */
    if (boundary[0] == '"') {
        memmove(boundary, boundary + 1, strlen(boundary));
        char *q = strchr(boundary, '"');
        if (q) *q = '\0';
    }
    /* Trim trailing whitespace */
    for (int i = (int)strlen(boundary) - 1;
         i >= 0 && (boundary[i] == ' ' || boundary[i] == '\r' || boundary[i] == '\n');
         i--)
        boundary[i] = '\0';

    if (!req->body || req->body_len == 0) {
        chttp_set_status(res, 400);
        chttp_send_text(res, "Empty body");
        return;
    }

    /* Locate first boundary line */
    char delim[130] = "--";
    strncat(delim, boundary, sizeof(delim) - 3);

    char *part = strstr(req->body, delim);
    if (!part) {
        chttp_set_status(res, 400);
        chttp_send_text(res, "Boundary not found in body");
        return;
    }
    /* Advance past boundary + CRLF */
    part += strlen(delim);
    if (part[0] == '\r' && part[1] == '\n') part += 2;

    /* Find end of part headers (blank line) */
    char *body_start = strstr(part, "\r\n\r\n");
    if (!body_start) {
        chttp_set_status(res, 400);
        chttp_send_text(res, "Malformed multipart headers");
        return;
    }

    /* Extract filename from Content-Disposition */
    char filename[256] = {0};
    char *cd = strstr(part, "Content-Disposition:");
    if (!cd || cd > body_start)
        cd = strstr(part, "content-disposition:");
    if (cd && cd < body_start) {
        char *fn = strstr(cd, "filename=\"");
        if (fn && fn < body_start) {
            fn += 10;
            char *fn_end = strchr(fn, '"');
            if (fn_end && fn_end < body_start) {
                int len = (int)(fn_end - fn);
                if (len >= (int)sizeof(filename)) len = (int)sizeof(filename) - 1;
                strncpy(filename, fn, len);
            }
        }
    }
    if (filename[0] == '\0')
        snprintf(filename, sizeof(filename), "upload_%ld", (long)time(NULL));

    /* Extract part Content-Type */
    char part_ct[128] = "application/octet-stream";
    char *pct = strstr(part, "Content-Type:");
    if (!pct) pct = strstr(part, "content-type:");
    if (pct && pct < body_start) {
        pct += 13;
        while (*pct == ' ') pct++;
        char *pct_end = strstr(pct, "\r\n");
        if (pct_end && pct_end < body_start) {
            int len = (int)(pct_end - pct);
            if (len >= (int)sizeof(part_ct)) len = (int)sizeof(part_ct) - 1;
            strncpy(part_ct, pct, len);
            part_ct[len] = '\0';
        }
    }

    /* File data starts after the blank line */
    char *file_data = body_start + 4;

    /* File data ends before \r\n--boundary */
    char end_delim[134] = "\r\n--";
    strncat(end_delim, boundary, sizeof(end_delim) - 5);

    char *file_end = strstr(file_data, end_delim);
    if (!file_end) {
        chttp_set_status(res, 400);
        chttp_send_text(res, "Could not locate end boundary");
        return;
    }
    size_t file_size = (size_t)(file_end - file_data);

    /* Sanitize filename: reject path traversal */
    for (char *c = filename; *c; c++)
        if (*c == '/' || *c == '\\') *c = '_';

    /* Ensure uploads/ directory exists */
    mkdir("uploads", 0755);

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "uploads/%s", filename);

    FILE *f = fopen(filepath, "wb");
    if (!f) {
        chttp_set_status(res, 500);
        chttp_send_text(res, "Failed to save file");
        return;
    }
    fwrite(file_data, 1, file_size, f);
    fclose(f);

    /* Respond with file metadata */
    cJSON *meta = cJSON_CreateObject();
    cJSON_AddStringToObject(meta, "filename",     filename);
    cJSON_AddStringToObject(meta, "path",         filepath);
    cJSON_AddStringToObject(meta, "content_type", part_ct);
    cJSON_AddNumberToObject(meta, "size_bytes",   (double)file_size);

    chttp_set_status(res, 201);
    chttp_send_cjson(res, meta);
    cJSON_Delete(meta);
}

int main(void) {
    HttpServer srv;

    if (chttp_server_init(&srv, 8000) < 0)
        return 1;

    CHTTP_GET(&srv,  "/",          handle_root);
    CHTTP_GET(&srv,  "/hello",     handle_hello);
    CHTTP_GET(&srv,  "/users/:id", handle_get_user);
    CHTTP_POST(&srv, "/users",     handle_create_user);
    CHTTP_GET(&srv,  "/echo",      handle_echo);
    CHTTP_POST(&srv, "/upload",              handle_upload);
    CHTTP_GET(&srv,  "/fdownload/:filename", handle_download);
    CHTTP_GET(&srv,  "/fmetadata/:filename", handle_fmetadata);

    chttp_server_run(&srv);

    chttp_server_destroy(&srv);
    return 0;
}
