#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "cJSON.h"
#include "fs.h"
#include "trash.h"
#include "sha256.h"
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
    cJSON_AddStringToObject(
        e,
        "mime",
        S_ISDIR(st.st_mode) ? "inode/directory" : mime_from_ext(full));
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

  struct stat st;
  if (stat(path, &st) < 0) { fs_error(res, errno); return; }
  if (!S_ISREG(st.st_mode)) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Not a regular file\",\"errno\":0}");
    return;
  }

  printf("Sending file %s\n", path);
  int file_fd = open(path, O_RDONLY);
  if (file_fd < 0) { fs_error(res, errno); return; }

  const char *slash = strrchr(path, '/');
  const char *fname = slash ? slash + 1 : path;
  printf("fname=%s\n", fname);

  /* Read file into response body */
  if (chttp_body_alloc(res, (size_t)st.st_size) < 0) {
    close(file_fd);
    chttp_set_status(res, 500);
    chttp_send_json(res, "{\"error\":\"Out of memory\",\"errno\":0}");
    return;
  }

  size_t total = 0;
  while (total < (size_t)st.st_size) {
    ssize_t rd = read(file_fd, res->body + total, (size_t)st.st_size - total);
    if (rd <= 0) break;
    total += (size_t)rd;
  }
  close(file_fd);
  res->body_len = total;

  /* Set headers via framework */
  char cd[320];
  const char *inline_param = chttp_query_param(req, "inline");
  if (inline_param && strcmp(inline_param, "1") == 0)
    snprintf(cd, sizeof(cd), "inline; filename=\"%s\"", fname);
  else
    snprintf(cd, sizeof(cd), "attachment; filename=\"%s\"", fname);

  chttp_set_status(res, 200);
  chttp_set_header(res, "Content-Type", mime_from_ext(fname));
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
  if (trash_item(path) != 0) { fs_error(res, errno); return; }
  chttp_send_json(res, "{\"message\":\"trashed\"}");
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
  if (trash_item(path) != 0) { fs_error(res, errno); return; }
  chttp_send_json(res, "{\"message\":\"trashed\"}");
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
  cJSON_AddStringToObject(
      obj,
      "mime",
      S_ISDIR(st.st_mode) ? "inode/directory" : mime_from_ext(path));

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

/* POST /fs/upload-stream?path=docs/file.txt
 *
 * Streaming upload: the handler runs in a forked child after privilege drop.
 * It writes the partial body already in req->body, then loop-recvs the
 * remainder in 64 KB chunks directly from req->fd.  TCP flow control provides
 * natural backpressure — blocking on fwrite() stops recv(), which fills the
 * socket buffer and signals the sender to slow down.
 *
 * Requires Content-Length header.  On success returns 201 + file stat JSON.
 * Partial files are unlinked on error mid-transfer.
 */
void handle_fs_stream_upload_impl(HttpRequest *req, HttpResponse *res) {
  const char *path = chttp_query_param(req, "path");
  if (!path || !safe_path(path)) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid path\",\"errno\":0}");
    return;
  }

  const char *cl_str = chttp_header(req, "Content-Length");
  if (!cl_str) {
    chttp_set_status(res, 411);
    chttp_send_json(res, "{\"error\":\"Content-Length required\"}");
    return;
  }
  size_t content_length = (size_t)strtoul(cl_str, NULL, 10);

  FILE *f = fopen(path, "wb");
  if (!f) { fs_error(res, errno); return; }

  /* Write partial body that arrived with the headers in the initial recv. */
  size_t written = 0;
  if (req->body && req->body_len > 0) {
    if (fwrite(req->body, 1, req->body_len, f) != req->body_len) {
      fclose(f); unlink(path); fs_error(res, errno); return;
    }
    written = req->body_len;
  }

  /* Stream remaining bytes in 64 KB chunks.  Blocking I/O gives us
   * backpressure for free: slow fwrite → stop recv → TCP window shrinks. */
  char chunk[65536];
  while (written < content_length) {
    size_t want = sizeof(chunk);
    if (content_length - written < want)
      want = content_length - written;

    int r = recv(req->fd, chunk, want, 0);
    if (r <= 0) {
      fclose(f); unlink(path);
      chttp_set_status(res, 400);
      chttp_send_json(res, "{\"error\":\"Upload interrupted\"}");
      return;
    }

    if (fwrite(chunk, 1, (size_t)r, f) != (size_t)r) {
      fclose(f); unlink(path); fs_error(res, errno); return;
    }
    written += (size_t)r;
  }
  fclose(f);

  struct stat st;
  if (stat(path, &st) != 0) { fs_error(res, errno); return; }

  char mtime_str[32];
  strftime(mtime_str, sizeof(mtime_str), "%Y-%m-%dT%H:%M:%SZ",
           gmtime(&st.st_mtime));

  const char *slash = strrchr(path, '/');
  const char *basename = slash ? slash + 1 : path;

  cJSON *obj = cJSON_CreateObject();
  cJSON_AddStringToObject(obj, "path",     path);
  cJSON_AddStringToObject(obj, "filename", basename);
  cJSON_AddNumberToObject(obj, "size_bytes", (double)st.st_size);
  cJSON_AddStringToObject(obj, "modified_at", mtime_str);

  chttp_set_status(res, 201);
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

/* ── Chunked manifest-based resumable upload ─────────────────────────────────
 *
 * Temp files in user's cwd (after privilege drop):
 *   .tmp-upload-<id>.meta   JSON manifest
 *   .tmp-upload-<id>.state  binary bitset  (chunk_count+7)/8 bytes
 *   .tmp-upload-<id>.data   preallocated file written via pwrite()
 *
 * On completion: .data renamed to dest; .meta and .state deleted.
 * On abort (DELETE): all three unlinked.
 * ──────────────────────────────────────────────────────────────────────────*/

/* Convert SHA-256 raw bytes to 64-char lowercase hex string. */
static void sha256_to_hex(const uint8_t h[32], char out[65]) {
  for (int i = 0; i < 32; i++) sprintf(out + i*2, "%02x", h[i]);
  out[64] = '\0';
}

/* Validate upload_id: exactly 16 lowercase hex chars. Returns 0 ok, -1 bad. */
static int validate_upload_id(const char *id) {
  if (!id) return -1;
  for (int i = 0; i < 16; i++) {
    char c = id[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return -1;
  }
  return id[16] == '\0' ? 0 : -1;
}

#define UPLOAD_TMP_DIR ".imaginary/uploads"

/* Ensure ~/.imaginary/uploads/ exists (called once per session create). */
static int ensure_upload_tmp_dir(void) {
  mkdir(".imaginary", 0700);
  return mkdir(UPLOAD_TMP_DIR, 0700) == 0 || errno == EEXIST ? 0 : -1;
}

/* Build .imaginary/uploads/<id>.<ext> path. */
static void upload_tmp_path(const char *id, const char *ext,
                             char *out, size_t n) {
  snprintf(out, n, UPLOAD_TMP_DIR "/%s.%s", id, ext);
}

/* Bitset helpers */
static void bitset_set(uint8_t *bs, size_t i) {
  bs[i / 8] |= (uint8_t)(1u << (i % 8));
}
static int bitset_get(const uint8_t *bs, size_t i) {
  return (bs[i / 8] >> (i % 8)) & 1;
}
static int bitset_all_set(const uint8_t *bs, size_t count) {
  size_t full = count / 8;
  for (size_t i = 0; i < full; i++)
    if (bs[i] != 0xff) return 0;
  size_t rem = count % 8;
  if (rem) {
    uint8_t mask = (uint8_t)((1u << rem) - 1);
    if ((bs[full] & mask) != mask) return 0;
  }
  return 1;
}

/* Shared tail: stat the completed file and send 201 JSON. */
static void send_upload_complete(HttpResponse *res, const char *path) {
  struct stat st;
  if (stat(path, &st) != 0) { fs_error(res, errno); return; }
  char mtime[32];
  strftime(mtime, sizeof(mtime), "%Y-%m-%dT%H:%M:%SZ", gmtime(&st.st_mtime));
  const char *slash = strrchr(path, '/');
  const char *base  = slash ? slash + 1 : path;
  cJSON *obj = cJSON_CreateObject();
  cJSON_AddStringToObject(obj, "path",        path);
  cJSON_AddStringToObject(obj, "filename",    base);
  cJSON_AddNumberToObject(obj, "size_bytes",  (double)st.st_size);
  cJSON_AddStringToObject(obj, "modified_at", mtime);
  chttp_set_status(res, 201);
  chttp_send_cjson(res, obj);
  cJSON_Delete(obj);
}

/* POST /fs/upload-session — create session from JSON manifest */
void handle_fs_upload_session_create_impl(HttpRequest *req, HttpResponse *res) {
  if (!req->body || req->body_len == 0) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Body required\"}");
    return;
  }

  cJSON *json = cJSON_ParseWithLength(req->body, req->body_len);
  if (!json) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid JSON\"}");
    return;
  }

  cJSON *j_dest        = cJSON_GetObjectItem(json, "dest");
  cJSON *j_filename    = cJSON_GetObjectItem(json, "filename");
  cJSON *j_file_size   = cJSON_GetObjectItem(json, "file_size");
  cJSON *j_chunk_size  = cJSON_GetObjectItem(json, "chunk_size");
  cJSON *j_chunk_count = cJSON_GetObjectItem(json, "chunk_count");
  cJSON *j_hashes      = cJSON_GetObjectItem(json, "chunk_hashes");

  if (!cJSON_IsString(j_dest) || !cJSON_IsString(j_filename) ||
      !cJSON_IsNumber(j_file_size) || !cJSON_IsNumber(j_chunk_size) ||
      !cJSON_IsNumber(j_chunk_count) || !cJSON_IsArray(j_hashes)) {
    cJSON_Delete(json);
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Missing or invalid manifest fields\"}");
    return;
  }

  size_t file_size   = (size_t)j_file_size->valuedouble;
  size_t chunk_size  = (size_t)j_chunk_size->valuedouble;
  size_t chunk_count = (size_t)j_chunk_count->valuedouble;

  /* Validate chunk_size bounds */
  if (chunk_size < 4096 || chunk_size > 256 * 1024 * 1024) {
    cJSON_Delete(json);
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"chunk_size out of range (4KB..256MB)\"}");
    return;
  }
  /* Validate chunk_count */
  if (chunk_count == 0 || chunk_count > 65536) {
    cJSON_Delete(json);
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"chunk_count out of range (1..65536)\"}");
    return;
  }
  /* Validate chunk_count == ceil(file_size / chunk_size) */
  size_t expected = (file_size + chunk_size - 1) / chunk_size;
  if (chunk_count != expected) {
    cJSON_Delete(json);
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"chunk_count does not match ceil(file_size/chunk_size)\"}");
    return;
  }
  /* Validate hash array length */
  if ((size_t)cJSON_GetArraySize(j_hashes) != chunk_count) {
    cJSON_Delete(json);
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"chunk_hashes length != chunk_count\"}");
    return;
  }
  /* Validate each hash is 64 lowercase hex chars */
  for (int i = 0; i < (int)chunk_count; i++) {
    cJSON *h = cJSON_GetArrayItem(j_hashes, i);
    if (!cJSON_IsString(h) || strlen(h->valuestring) != 64) {
      cJSON_Delete(json);
      chttp_set_status(res, 400);
      chttp_send_json(res, "{\"error\":\"Each chunk hash must be 64 hex characters\"}");
      return;
    }
    for (int k = 0; k < 64; k++) {
      char c = h->valuestring[k];
      if (!((c>='0'&&c<='9')||(c>='a'&&c<='f'))) {
        cJSON_Delete(json);
        chttp_set_status(res, 400);
        chttp_send_json(res, "{\"error\":\"Chunk hash must be lowercase hex\"}");
        return;
      }
    }
  }
  /* Validate dest path */
  if (!safe_path(j_dest->valuestring)) {
    cJSON_Delete(json);
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid dest path\"}");
    return;
  }

  /* Check that the destination directory is writable by this user.
   * This runs inside fork_and_run() after privilege drop, so access()
   * reflects the authenticated user's real permissions — not root's.
   * Catching this here avoids uploading gigabytes only to fail at rename. */
  {
    char dest_copy[512];
    strncpy(dest_copy, j_dest->valuestring, sizeof(dest_copy) - 1);
    dest_copy[sizeof(dest_copy) - 1] = '\0';

    char *slash = strrchr(dest_copy, '/');
    const char *dest_dir;
    if (slash) {
      *slash = '\0';
      dest_dir = dest_copy[0] != '\0' ? dest_copy : "/";
    } else {
      dest_dir = ".";
    }

    if (access(dest_dir, W_OK | X_OK) != 0) {
      int e = errno;
      cJSON_Delete(json);
      fs_error(res, e);
      return;
    }
  }

  /* Ensure temp dir exists */
  if (ensure_upload_tmp_dir() != 0) {
    cJSON_Delete(json);
    fs_error(res, errno);
    return;
  }

  /* Generate upload_id: 8 random bytes → 16 hex chars */
  char upload_id[17];
  char meta_path[128], state_path[128], data_path[128];
  int attempts = 0;
  int meta_fd = -1;

  while (attempts++ < 5) {
    uint8_t rnd[8];
    int urnd = open("/dev/urandom", O_RDONLY);
    if (urnd < 0) { cJSON_Delete(json); fs_error(res, errno); return; }
    if (read(urnd, rnd, 8) != 8) {
      close(urnd); cJSON_Delete(json);
      chttp_set_status(res, 500);
      chttp_send_json(res, "{\"error\":\"Failed to read random bytes\"}");
      return;
    }
    close(urnd);
    for (int i = 0; i < 8; i++) sprintf(upload_id + i*2, "%02x", rnd[i]);
    upload_id[16] = '\0';

    upload_tmp_path(upload_id, "meta", meta_path, sizeof(meta_path));
    meta_fd = open(meta_path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (meta_fd >= 0) break;
    if (errno != EEXIST) {
      cJSON_Delete(json);
      fs_error(res, errno);
      return;
    }
  }
  if (meta_fd < 0) {
    cJSON_Delete(json);
    chttp_set_status(res, 500);
    chttp_send_json(res, "{\"error\":\"Failed to generate unique upload_id\"}");
    return;
  }

  /* Write .meta file */
  char *meta_str = cJSON_PrintUnformatted(json);
  if (!meta_str || write(meta_fd, meta_str, strlen(meta_str)) < 0) {
    int e = errno;
    close(meta_fd); unlink(meta_path);
    free(meta_str); cJSON_Delete(json);
    fs_error(res, e);
    return;
  }
  free(meta_str);
  close(meta_fd);
  cJSON_Delete(json);

  /* Write .state file: (chunk_count+7)/8 zero bytes */
  upload_tmp_path(upload_id, "state", state_path, sizeof(state_path));
  size_t state_bytes = (chunk_count + 7) / 8;
  int sfd = open(state_path, O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (sfd < 0) {
    unlink(meta_path); fs_error(res, errno); return;
  }
  {
    uint8_t zero = 0;
    for (size_t i = 0; i < state_bytes; i++) write(sfd, &zero, 1);
  }
  close(sfd);

  /* Create .data file: preallocate file_size bytes */
  upload_tmp_path(upload_id, "data", data_path, sizeof(data_path));
  int dfd = open(data_path, O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (dfd < 0) {
    unlink(meta_path); unlink(state_path); fs_error(res, errno); return;
  }
  if (ftruncate(dfd, (off_t)file_size) != 0) {
    int e = errno;
    close(dfd); unlink(meta_path); unlink(state_path); unlink(data_path);
    fs_error(res, e);
    return;
  }
  close(dfd);

  char resp[64];
  snprintf(resp, sizeof(resp), "{\"upload_id\":\"%s\"}", upload_id);
  chttp_set_status(res, 201);
  chttp_send_json(res, resp);
}

/* GET /fs/upload-session/:upload_id — query status */
void handle_fs_upload_session_status_impl(HttpRequest *req, HttpResponse *res) {
  const char *upload_id = chttp_path_param(req, "upload_id");
  if (validate_upload_id(upload_id) != 0) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid upload_id\"}");
    return;
  }

  char meta_path[128], state_path[128];
  upload_tmp_path(upload_id, "meta",  meta_path,  sizeof(meta_path));
  upload_tmp_path(upload_id, "state", state_path, sizeof(state_path));

  /* Read meta */
  FILE *mf = fopen(meta_path, "r");
  if (!mf) {
    chttp_set_status(res, 404);
    chttp_send_json(res, "{\"error\":\"Upload session not found\"}");
    return;
  }
  fseek(mf, 0, SEEK_END);
  long msz = ftell(mf);
  fseek(mf, 0, SEEK_SET);
  char *mbuf = malloc((size_t)msz + 1);
  if (!mbuf) { fclose(mf); chttp_set_status(res, 500); chttp_send_json(res, "{\"error\":\"OOM\"}"); return; }
  fread(mbuf, 1, (size_t)msz, mf);
  fclose(mf);
  mbuf[msz] = '\0';

  cJSON *meta = cJSON_Parse(mbuf);
  free(mbuf);
  if (!meta) {
    chttp_set_status(res, 500);
    chttp_send_json(res, "{\"error\":\"Corrupt meta file\"}");
    return;
  }

  size_t chunk_count = (size_t)cJSON_GetObjectItem(meta, "chunk_count")->valuedouble;

  /* Read state bitset */
  size_t state_bytes = (chunk_count + 7) / 8;
  uint8_t *bs = calloc(state_bytes, 1);
  if (!bs) { cJSON_Delete(meta); chttp_set_status(res, 500); chttp_send_json(res, "{\"error\":\"OOM\"}"); return; }

  FILE *sf = fopen(state_path, "rb");
  if (sf) {
    fread(bs, 1, state_bytes, sf);
    fclose(sf);
  }

  cJSON *obj = cJSON_CreateObject();
  cJSON_AddStringToObject(obj, "upload_id",   upload_id);
  cJSON_AddItemToObject(obj,   "filename",    cJSON_Duplicate(cJSON_GetObjectItem(meta, "filename"), 0));
  cJSON_AddItemToObject(obj,   "dest",        cJSON_Duplicate(cJSON_GetObjectItem(meta, "dest"), 0));
  cJSON_AddItemToObject(obj,   "file_size",   cJSON_Duplicate(cJSON_GetObjectItem(meta, "file_size"), 0));
  cJSON_AddItemToObject(obj,   "chunk_size",  cJSON_Duplicate(cJSON_GetObjectItem(meta, "chunk_size"), 0));
  cJSON_AddItemToObject(obj,   "chunk_count", cJSON_Duplicate(cJSON_GetObjectItem(meta, "chunk_count"), 0));

  cJSON *recv_arr = cJSON_AddArrayToObject(obj, "received_chunks");
  for (size_t i = 0; i < chunk_count; i++) {
    if (bitset_get(bs, i))
      cJSON_AddItemToArray(recv_arr, cJSON_CreateNumber((double)i));
  }
  free(bs);
  cJSON_Delete(meta);

  chttp_send_cjson(res, obj);
  cJSON_Delete(obj);
}

/* POST /fs/upload-chunk/:upload_id — stream one chunk (STREAM route) */
void handle_fs_upload_chunk_impl(HttpRequest *req, HttpResponse *res) {
  const char *upload_id = chttp_path_param(req, "upload_id");
  if (validate_upload_id(upload_id) != 0) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid upload_id\"}");
    return;
  }

  const char *idx_str = chttp_header(req, "X-Chunk-Index");
  const char *cl_str  = chttp_header(req, "Content-Length");
  if (!idx_str || !cl_str) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"X-Chunk-Index and Content-Length required\"}");
    return;
  }

  size_t chunk_index = (size_t)strtoul(idx_str, NULL, 10);
  size_t chunk_bytes = (size_t)strtoull(cl_str, NULL, 10);

  char meta_path[128], state_path[128], data_path[128];
  upload_tmp_path(upload_id, "meta",  meta_path,  sizeof(meta_path));
  upload_tmp_path(upload_id, "state", state_path, sizeof(state_path));
  upload_tmp_path(upload_id, "data",  data_path,  sizeof(data_path));

  /* Read meta */
  FILE *mf = fopen(meta_path, "r");
  if (!mf) {
    chttp_set_status(res, 404);
    chttp_send_json(res, "{\"error\":\"Upload session not found\"}");
    return;
  }
  fseek(mf, 0, SEEK_END);
  long msz = ftell(mf);
  fseek(mf, 0, SEEK_SET);
  char *mbuf = malloc((size_t)msz + 1);
  if (!mbuf) { fclose(mf); chttp_set_status(res, 500); chttp_send_json(res, "{\"error\":\"OOM\"}"); return; }
  fread(mbuf, 1, (size_t)msz, mf);
  fclose(mf);
  mbuf[msz] = '\0';

  cJSON *meta = cJSON_Parse(mbuf);
  free(mbuf);
  if (!meta) {
    chttp_set_status(res, 500);
    chttp_send_json(res, "{\"error\":\"Corrupt meta file\"}");
    return;
  }

  size_t chunk_count = (size_t)cJSON_GetObjectItem(meta, "chunk_count")->valuedouble;
  size_t chunk_size  = (size_t)cJSON_GetObjectItem(meta, "chunk_size")->valuedouble;
  cJSON *j_hashes    = cJSON_GetObjectItem(meta, "chunk_hashes");

  if (chunk_index >= chunk_count) {
    cJSON_Delete(meta);
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"chunk_index out of range\"}");
    return;
  }
  if (chunk_bytes > chunk_size) {
    cJSON_Delete(meta);
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"chunk too large\"}");
    return;
  }

  const char *expected_hash = cJSON_GetArrayItem(j_hashes, (int)chunk_index)->valuestring;

  /* Read state bitset */
  size_t state_bytes = (chunk_count + 7) / 8;
  uint8_t *bs = calloc(state_bytes, 1);
  if (!bs) { cJSON_Delete(meta); chttp_set_status(res, 500); chttp_send_json(res, "{\"error\":\"OOM\"}"); return; }

  int sfd = open(state_path, O_RDWR);
  if (sfd < 0) {
    free(bs); cJSON_Delete(meta); fs_error(res, errno); return;
  }
  read(sfd, bs, state_bytes);

  /* Already received? */
  if (bitset_get(bs, chunk_index)) {
    close(sfd);
    free(bs);
    cJSON_Delete(meta);
    /* Drain incoming body */
    char drain[4096];
    size_t drained = req->body_len; /* already in buffer */
    while (drained < chunk_bytes) {
      size_t want = sizeof(drain);
      if (chunk_bytes - drained < want) want = chunk_bytes - drained;
      int r = recv(req->fd, drain, want, 0);
      if (r <= 0) break;
      drained += (size_t)r;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"status\":\"already_done\",\"chunk\":%zu}", chunk_index);
    chttp_send_json(res, buf);
    return;
  }
  close(sfd);

  /* Allocate buffer and read chunk */
  uint8_t *buf = malloc(chunk_bytes);
  if (!buf) {
    free(bs); cJSON_Delete(meta);
    chttp_set_status(res, 500);
    chttp_send_json(res, "{\"error\":\"OOM\"}");
    return;
  }

  SHA256_CTX sha;
  sha256_init(&sha);

  size_t received = 0;

  /* Copy partial body already buffered with headers */
  if (req->body && req->body_len > 0) {
    size_t take = req->body_len < chunk_bytes ? req->body_len : chunk_bytes;
    memcpy(buf, req->body, take);
    sha256_update(&sha, (const uint8_t *)req->body, take);
    received = take;
  }

  /* Recv remaining bytes from socket */
  while (received < chunk_bytes) {
    size_t want = chunk_bytes - received;
    int r = recv(req->fd, (char *)buf + received, want, 0);
    if (r <= 0) {
      free(buf); free(bs); cJSON_Delete(meta);
      chttp_set_status(res, 400);
      chttp_send_json(res, "{\"error\":\"Upload interrupted\"}");
      return;
    }
    sha256_update(&sha, (const uint8_t *)buf + received, (size_t)r);
    received += (size_t)r;
  }

  /* Verify SHA-256 */
  uint8_t hash_bytes[32];
  sha256_final(&sha, hash_bytes);
  char hash_hex[65];
  sha256_to_hex(hash_bytes, hash_hex);

  if (strcmp(hash_hex, expected_hash) != 0) {
    free(buf); free(bs); cJSON_Delete(meta);
    char errbuf[128];
    snprintf(errbuf, sizeof(errbuf),
             "{\"error\":\"hash_mismatch\",\"chunk\":%zu}", chunk_index);
    chttp_set_status(res, 422);
    chttp_send_json(res, errbuf);
    return;
  }

  /* Write chunk to .data via pwrite */
  int dfd = open(data_path, O_WRONLY);
  if (dfd < 0) {
    free(buf); free(bs); cJSON_Delete(meta); fs_error(res, errno); return;
  }
  off_t offset = (off_t)(chunk_index * chunk_size);
  ssize_t written = pwrite(dfd, buf, chunk_bytes, offset);
  close(dfd);
  free(buf);

  if (written < 0 || (size_t)written != chunk_bytes) {
    free(bs); cJSON_Delete(meta); fs_error(res, errno); return;
  }

  /* Update bitset with file lock */
  sfd = open(state_path, O_RDWR);
  if (sfd < 0) { free(bs); cJSON_Delete(meta); fs_error(res, errno); return; }

  struct flock fl;
  memset(&fl, 0, sizeof(fl));
  fl.l_type   = F_WRLCK;
  fl.l_whence = SEEK_SET;
  fcntl(sfd, F_SETLKW, &fl);

  /* Re-read under lock */
  pread(sfd, bs, state_bytes, 0);
  bitset_set(bs, chunk_index);
  pwrite(sfd, bs, state_bytes, 0);

  fl.l_type = F_UNLCK;
  fcntl(sfd, F_SETLK, &fl);
  close(sfd);

  int all_done = bitset_all_set(bs, chunk_count);
  free(bs);

  /* Grab dest before freeing meta */
  char dest_buf[512];
  strncpy(dest_buf, cJSON_GetObjectItem(meta, "dest")->valuestring, sizeof(dest_buf) - 1);
  dest_buf[sizeof(dest_buf) - 1] = '\0';
  cJSON_Delete(meta);

  if (all_done) {
    /* Rename .data to final dest, clean up .meta and .state */
    if (rename(data_path, dest_buf) != 0) { fs_error(res, errno); return; }
    unlink(meta_path);
    unlink(state_path);
    send_upload_complete(res, dest_buf);
    return;
  }

  char okbuf[64];
  snprintf(okbuf, sizeof(okbuf), "{\"status\":\"ok\",\"chunk\":%zu}", chunk_index);
  chttp_send_json(res, okbuf);
}

/* DELETE /fs/upload-session/:upload_id — abort and clean up */
void handle_fs_upload_session_abort_impl(HttpRequest *req, HttpResponse *res) {
  const char *upload_id = chttp_path_param(req, "upload_id");
  if (validate_upload_id(upload_id) != 0) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid upload_id\"}");
    return;
  }

  char meta_path[128], state_path[128], data_path[128];
  upload_tmp_path(upload_id, "meta",  meta_path,  sizeof(meta_path));
  upload_tmp_path(upload_id, "state", state_path, sizeof(state_path));
  upload_tmp_path(upload_id, "data",  data_path,  sizeof(data_path));

  unlink(meta_path);
  unlink(state_path);
  unlink(data_path);

  chttp_set_status(res, 204);
  chttp_send_json(res, "");
}
