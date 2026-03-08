#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "cJSON.h"
#include "fs.h"
#include "utils/utils.h"

#define FS_CONTENT_MAX (64 * 1024)

/* GET /fs/list?path=. */
void handle_fs_list_impl(HttpRequest *req, HttpResponse *res) {
  const char *path = chttp_query_param(req, "path");
  printf("handle_fs_list_impl, path=%s\n", path);
  if (!path) path = ".";
  if (!safe_path(path) && strcmp(path, ".") != 0) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid path\",\"errno\":0}");
    return;
  }

  DIR *dp = opendir(path);
  if (!dp) { fs_error(res, errno); return; }

  cJSON *obj = cJSON_CreateObject();
  cJSON_AddStringToObject(obj, "path", path);
  cJSON *entries = cJSON_AddArrayToObject(obj, "entries");

  struct dirent *de;
  while ((de = readdir(dp)) != NULL) {
    if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
      continue;

    char full[1024];
    snprintf(full, sizeof(full), "%s/%s", path, de->d_name);

    struct stat st;
    if (stat(full, &st) != 0) continue;

    char mtime_str[32];
    strftime(mtime_str, sizeof(mtime_str), "%Y-%m-%dT%H:%M:%SZ",
             gmtime(&st.st_mtime));

    cJSON *e = cJSON_CreateObject();
    cJSON_AddStringToObject(e, "name", de->d_name);
    cJSON_AddStringToObject(e, "type", S_ISDIR(st.st_mode) ? "dir" : "file");
    cJSON_AddNumberToObject(e, "size", S_ISDIR(st.st_mode) ? 0 : (double)st.st_size);
    cJSON_AddStringToObject(e, "modified", mtime_str);
    cJSON_AddItemToArray(entries, e);
  }
  closedir(dp);

  chttp_send_cjson(res, obj);
  cJSON_Delete(obj);
}

/* POST /fs/upload?path=. + multipart body */
void handle_fs_upload_impl(HttpRequest *req, HttpResponse *res) {
  const char *dir = chttp_query_param(req, "path");
  if (!dir) dir = ".";
  if (!safe_path(dir) && strcmp(dir, ".") != 0) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid path\",\"errno\":0}");
    return;
  }

  char *file_data;
  size_t file_size;
  char filename[256];
  char part_ct[128];
  if (parse_multipart(req, res, &file_data, &file_size, filename, part_ct) < 0)
    return;

  for (char *c = filename; *c; c++)
    if (*c == '/' || *c == '\\') *c = '_';

  char filepath[1024];
  snprintf(filepath, sizeof(filepath), "%s/%s", dir, filename);

  FILE *f = fopen(filepath, "wb");
  if (!f) { fs_error(res, errno); return; }
  fwrite(file_data, 1, file_size, f);
  fclose(f);

  char buf[1152];
  snprintf(buf, sizeof(buf),
           "{\"message\":\"uploaded\",\"path\":\"%s\",\"size\":%zu}",
           filepath, file_size);
  chttp_set_status(res, 201);
  chttp_send_json(res, buf);
}

/* GET /fs/download?path=docs/file.txt */
void handle_fs_download_impl(HttpRequest *req, HttpResponse *res) {
  const char *path = chttp_query_param(req, "path");
  if (!path || !safe_path(path)) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid path\",\"errno\":0}");
    return;
  }

  FILE *f = fopen(path, "rb");
  if (!f) { fs_error(res, errno); return; }

  fseek(f, 0, SEEK_END);
  long fsize = ftell(f);
  fseek(f, 0, SEEK_SET);

  if (fsize < 0) {
    fclose(f);
    chttp_set_status(res, 500);
    chttp_send_json(res, "{\"error\":\"Failed to read file size\",\"errno\":0}");
    return;
  }

  if (chttp_body_alloc(res, (size_t)fsize) < 0) {
    fclose(f);
    chttp_set_status(res, 500);
    chttp_send_json(res, "{\"error\":\"Out of memory\",\"errno\":0}");
    return;
  }
  res->body_len = fread(res->body, 1, (size_t)fsize, f);
  fclose(f);

  const char *slash = strrchr(path, '/');
  const char *basename = slash ? slash + 1 : path;
  chttp_set_header(res, "Content-Type", mime_from_ext(basename));

  char cd[512];
  snprintf(cd, sizeof(cd), "attachment; filename=\"%s\"", basename);
  chttp_set_header(res, "Content-Disposition", cd);
}

/* DELETE /fs/file?path=docs/file.txt */
void handle_fs_delete_file_impl(HttpRequest *req, HttpResponse *res) {
  const char *path = chttp_query_param(req, "path");
  if (!path || !safe_path(path)) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid path\",\"errno\":0}");
    return;
  }
  if (unlink(path) != 0) { fs_error(res, errno); return; }
  chttp_send_json(res, "{\"message\":\"deleted\"}");
}

/* POST /fs/mkdir  body: {"path":"docs/new"} */
void handle_fs_mkdir_impl(HttpRequest *req, HttpResponse *res) {
  if (!req->body || req->body_len == 0) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Body required\",\"errno\":0}");
    return;
  }
  cJSON *json = cJSON_ParseWithLength(req->body, req->body_len);
  if (!json) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid JSON\",\"errno\":0}");
    return;
  }
  cJSON *j = cJSON_GetObjectItem(json, "path");
  if (!cJSON_IsString(j)) {
    cJSON_Delete(json);
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"path required\",\"errno\":0}");
    return;
  }
  const char *path = j->valuestring;
  if (!safe_path(path)) {
    cJSON_Delete(json);
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid path\",\"errno\":0}");
    return;
  }

  /* mkdir -p: iterate over path components */
  char tmp[1024];
  strncpy(tmp, path, sizeof(tmp) - 1);
  tmp[sizeof(tmp) - 1] = '\0';
  for (char *p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      mkdir(tmp, 0755);
      *p = '/';
    }
  }
  if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
    int e = errno;
    cJSON_Delete(json);
    fs_error(res, e);
    return;
  }
  cJSON_Delete(json);
  chttp_set_status(res, 201);
  chttp_send_json(res, "{\"message\":\"created\"}");
}

/* DELETE /fs/dir?path=docs/empty */
void handle_fs_rmdir_impl(HttpRequest *req, HttpResponse *res) {
  const char *path = chttp_query_param(req, "path");
  if (!path || !safe_path(path)) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid path\",\"errno\":0}");
    return;
  }
  if (rmdir(path) != 0) { fs_error(res, errno); return; }
  chttp_send_json(res, "{\"message\":\"removed\"}");
}

/* POST /fs/rename  body: {"path":"a.txt","name":"b.txt"} */
void handle_fs_rename_impl(HttpRequest *req, HttpResponse *res) {
  if (!req->body || req->body_len == 0) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Body required\",\"errno\":0}");
    return;
  }
  cJSON *json = cJSON_ParseWithLength(req->body, req->body_len);
  if (!json) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid JSON\",\"errno\":0}");
    return;
  }
  cJSON *j_path = cJSON_GetObjectItem(json, "path");
  cJSON *j_name = cJSON_GetObjectItem(json, "name");
  if (!cJSON_IsString(j_path) || !cJSON_IsString(j_name)) {
    cJSON_Delete(json);
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"path and name required\",\"errno\":0}");
    return;
  }
  if (!safe_path(j_path->valuestring) || !safe_filename(j_name->valuestring)) {
    cJSON_Delete(json);
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid path or name\",\"errno\":0}");
    return;
  }

  char dst[1024];
  const char *src = j_path->valuestring;
  const char *slash = strrchr(src, '/');
  if (slash) {
    int dirlen = (int)(slash - src);
    snprintf(dst, sizeof(dst), "%.*s/%s", dirlen, src, j_name->valuestring);
  } else {
    snprintf(dst, sizeof(dst), "%s", j_name->valuestring);
  }

  if (rename(src, dst) != 0) {
    int e = errno;
    cJSON_Delete(json);
    fs_error(res, e);
    return;
  }
  cJSON_Delete(json);
  chttp_send_json(res, "{\"message\":\"renamed\"}");
}

/* POST /fs/move  body: {"from":"src/a.txt","to":"dst/a.txt"} */
void handle_fs_move_impl(HttpRequest *req, HttpResponse *res) {
  if (!req->body || req->body_len == 0) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Body required\",\"errno\":0}");
    return;
  }
  cJSON *json = cJSON_ParseWithLength(req->body, req->body_len);
  if (!json) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid JSON\",\"errno\":0}");
    return;
  }
  cJSON *j_from = cJSON_GetObjectItem(json, "from");
  cJSON *j_to   = cJSON_GetObjectItem(json, "to");
  if (!cJSON_IsString(j_from) || !cJSON_IsString(j_to)) {
    cJSON_Delete(json);
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"from and to required\",\"errno\":0}");
    return;
  }
  if (!safe_path(j_from->valuestring) || !safe_path(j_to->valuestring)) {
    cJSON_Delete(json);
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid path\",\"errno\":0}");
    return;
  }
  if (rename(j_from->valuestring, j_to->valuestring) != 0) {
    int e = errno;
    cJSON_Delete(json);
    fs_error(res, e);
    return;
  }
  cJSON_Delete(json);
  chttp_send_json(res, "{\"message\":\"moved\"}");
}

/* POST /fs/copy  body: {"from":"a.txt","to":"copy.txt"} */
void handle_fs_copy_impl(HttpRequest *req, HttpResponse *res) {
  if (!req->body || req->body_len == 0) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Body required\",\"errno\":0}");
    return;
  }
  cJSON *json = cJSON_ParseWithLength(req->body, req->body_len);
  if (!json) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid JSON\",\"errno\":0}");
    return;
  }
  cJSON *j_from = cJSON_GetObjectItem(json, "from");
  cJSON *j_to   = cJSON_GetObjectItem(json, "to");
  if (!cJSON_IsString(j_from) || !cJSON_IsString(j_to)) {
    cJSON_Delete(json);
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"from and to required\",\"errno\":0}");
    return;
  }
  if (!safe_path(j_from->valuestring) || !safe_path(j_to->valuestring)) {
    cJSON_Delete(json);
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid path\",\"errno\":0}");
    return;
  }

  FILE *src = fopen(j_from->valuestring, "rb");
  if (!src) { int e = errno; cJSON_Delete(json); fs_error(res, e); return; }

  FILE *dst = fopen(j_to->valuestring, "wb");
  if (!dst) {
    int e = errno;
    fclose(src);
    cJSON_Delete(json);
    fs_error(res, e);
    return;
  }

  char cbuf[4096];
  size_t n;
  while ((n = fread(cbuf, 1, sizeof(cbuf), src)) > 0)
    fwrite(cbuf, 1, n, dst);

  fclose(src);
  fclose(dst);
  cJSON_Delete(json);
  chttp_set_status(res, 201);
  chttp_send_json(res, "{\"message\":\"copied\"}");
}

/* GET /fs/stat?path=docs/file.txt */
void handle_fs_stat_impl(HttpRequest *req, HttpResponse *res) {
  const char *path = chttp_query_param(req, "path");
  if (!path || (!safe_path(path) && strcmp(path, ".") != 0)) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid path\",\"errno\":0}");
    return;
  }

  struct stat st;
  if (stat(path, &st) != 0) { fs_error(res, errno); return; }

  char mtime_str[32];
  strftime(mtime_str, sizeof(mtime_str), "%Y-%m-%dT%H:%M:%SZ",
           gmtime(&st.st_mtime));

  char mode_str[8];
  snprintf(mode_str, sizeof(mode_str), "%04o", (unsigned)(st.st_mode & 07777));

  const char *slash = strrchr(path, '/');
  const char *name = slash ? slash + 1 : path;

  cJSON *obj = cJSON_CreateObject();
  cJSON_AddStringToObject(obj, "name", name);
  cJSON_AddStringToObject(obj, "path", path);
  cJSON_AddStringToObject(obj, "type", S_ISDIR(st.st_mode) ? "dir" : "file");
  cJSON_AddNumberToObject(obj, "size", (double)st.st_size);
  cJSON_AddStringToObject(obj, "mode", mode_str);
  cJSON_AddNumberToObject(obj, "uid", (double)st.st_uid);
  cJSON_AddStringToObject(obj, "modified", mtime_str);

  chttp_send_cjson(res, obj);
  cJSON_Delete(obj);
}

/* GET /fs/content?path=notes.txt */
void handle_fs_read_impl(HttpRequest *req, HttpResponse *res) {
  const char *path = chttp_query_param(req, "path");
  if (!path || !safe_path(path)) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid path\",\"errno\":0}");
    return;
  }

  FILE *f = fopen(path, "r");
  if (!f) { fs_error(res, errno); return; }

  char *content = malloc(FS_CONTENT_MAX + 1);
  if (!content) {
    fclose(f);
    chttp_set_status(res, 500);
    chttp_send_json(res, "{\"error\":\"Internal error\",\"errno\":0}");
    return;
  }

  size_t n = fread(content, 1, FS_CONTENT_MAX, f);
  fclose(f);
  content[n] = '\0';

  cJSON *obj = cJSON_CreateObject();
  cJSON_AddStringToObject(obj, "path", path);
  cJSON_AddStringToObject(obj, "content", content);
  free(content);

  chttp_send_cjson(res, obj);
  cJSON_Delete(obj);
}

/* PUT /fs/content?path=notes.txt  body: raw text */
void handle_fs_write_impl(HttpRequest *req, HttpResponse *res) {
  const char *path = chttp_query_param(req, "path");
  if (!path || !safe_path(path)) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid path\",\"errno\":0}");
    return;
  }

  FILE *f = fopen(path, "w");
  if (!f) { fs_error(res, errno); return; }

  size_t written = 0;
  if (req->body && req->body_len > 0)
    written = fwrite(req->body, 1, req->body_len, f);
  fclose(f);

  char buf[256];
  snprintf(buf, sizeof(buf),
           "{\"message\":\"saved\",\"path\":\"%s\",\"size\":%zu}",
           path, written);
  chttp_send_json(res, buf);
}
