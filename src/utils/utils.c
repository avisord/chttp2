#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "utils.h"

const char *mime_from_ext(const char *filename) {
  const char *dot = strrchr(filename, '.');
  if (!dot)
    return "application/octet-stream";
  if (strcmp(dot, ".txt") == 0)
    return "text/plain";
  if (strcmp(dot, ".html") == 0)
    return "text/html";
  if (strcmp(dot, ".json") == 0)
    return "application/json";
  if (strcmp(dot, ".png") == 0)
    return "image/png";
  if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
    return "image/jpeg";
  if (strcmp(dot, ".gif") == 0)
    return "image/gif";
  if (strcmp(dot, ".pdf") == 0)
    return "application/pdf";
  if (strcmp(dot, ".csv") == 0)
    return "text/csv";
  return "application/octet-stream";
}

int safe_filename(const char *name) {
  return name && *name && !strchr(name, '/') && !strchr(name, '\\') &&
         !strstr(name, "..");
}

int safe_path(const char *path) {
  if (!path || !*path || path[0] == '/')
    return 0;
  if (strstr(path, ".."))
    return 0;
  return 1;
}

void fs_error(HttpResponse *res, int err) {
  int code;
  const char *msg;
  switch (err) {
    case ENOENT:    code = 404; msg = "Not found";           break;
    case EACCES:
    case EPERM:     code = 403; msg = "Permission denied";   break;
    case EEXIST:    code = 409; msg = "Already exists";      break;
    case ENOTDIR:   code = 400; msg = "Not a directory";     break;
    case EISDIR:    code = 400; msg = "Is a directory";      break;
    case ENOTEMPTY: code = 409; msg = "Directory not empty"; break;
    default:        code = 500; msg = "Internal error";      break;
  }
  chttp_set_status(res, code);
  char buf[128];
  snprintf(buf, sizeof(buf), "{\"error\":\"%s\",\"errno\":%d}", msg, err);
  chttp_send_json(res, buf);
}

int parse_multipart(HttpRequest *req, HttpResponse *res,
                    char **file_data_out, size_t *file_size_out,
                    char filename_out[256], char part_ct_out[128]) {
  const char *ct = chttp_header(req, "Content-Type");
  if (!ct || strncmp(ct, "multipart/form-data", 19) != 0) {
    chttp_set_status(res, 400);
    chttp_send_text(res, "Expected multipart/form-data");
    return -1;
  }

  const char *bp = strstr(ct, "boundary=");
  if (!bp) {
    chttp_set_status(res, 400);
    chttp_send_text(res, "Missing boundary");
    return -1;
  }
  bp += 9;

  char boundary[128] = {0};
  strncpy(boundary, bp, sizeof(boundary) - 1);
  if (boundary[0] == '"') {
    memmove(boundary, boundary + 1, strlen(boundary));
    char *q = strchr(boundary, '"');
    if (q) *q = '\0';
  }
  for (int i = (int)strlen(boundary) - 1;
       i >= 0 && (boundary[i] == ' ' || boundary[i] == '\r' || boundary[i] == '\n');
       i--)
    boundary[i] = '\0';

  if (!req->body || req->body_len == 0) {
    chttp_set_status(res, 400);
    chttp_send_text(res, "Empty body");
    return -1;
  }

  char delim[130] = "--";
  strncat(delim, boundary, sizeof(delim) - 3);

  char *part = strstr(req->body, delim);
  if (!part) {
    chttp_set_status(res, 400);
    chttp_send_text(res, "Boundary not found in body");
    return -1;
  }
  part += strlen(delim);
  if (part[0] == '\r' && part[1] == '\n')
    part += 2;

  char *body_start = strstr(part, "\r\n\r\n");
  if (!body_start) {
    chttp_set_status(res, 400);
    chttp_send_text(res, "Malformed multipart headers");
    return -1;
  }

  /* filename */
  memset(filename_out, 0, 256);
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
        if (len >= 255) len = 255;
        strncpy(filename_out, fn, len);
      }
    }
  }
  if (filename_out[0] == '\0')
    snprintf(filename_out, 256, "upload_%ld", (long)time(NULL));

  /* part content-type */
  strncpy(part_ct_out, "application/octet-stream", 128);
  char *pct = strstr(part, "Content-Type:");
  if (!pct) pct = strstr(part, "content-type:");
  if (pct && pct < body_start) {
    pct += 13;
    while (*pct == ' ') pct++;
    char *pct_end = strstr(pct, "\r\n");
    if (pct_end && pct_end < body_start) {
      int len = (int)(pct_end - pct);
      if (len >= 127) len = 127;
      strncpy(part_ct_out, pct, len);
      part_ct_out[len] = '\0';
    }
  }

  char *file_data = body_start + 4;
  char end_delim[134] = "\r\n--";
  strncat(end_delim, boundary, sizeof(end_delim) - 5);
  char *file_end = strstr(file_data, end_delim);
  if (!file_end) {
    chttp_set_status(res, 400);
    chttp_send_text(res, "Could not locate end boundary");
    return -1;
  }

  *file_data_out = file_data;
  *file_size_out = (size_t)(file_end - file_data);
  return 0;
}
