#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "cJSON.h"
#include "routes.h"
#include "auth/auth.h"
#include "utils/utils.h"

/* GET / */
void handle_root(HttpRequest *req, HttpResponse *res) {
  (void)req;
  chttp_send_text(res, "Welcome to chttp!");
}

/* GET /hello */
void handle_hello(HttpRequest *req, HttpResponse *res) {
  (void)req;
  chttp_send_text(res, "Hello, World!");
}

/* GET /users/:id */
void handle_get_user(HttpRequest *req, HttpResponse *res) {
  const char *id = chttp_path_param(req, "id");
  if (!id) {
    chttp_set_status(res, 400);
    chttp_send_text(res, "Missing user id");
    return;
  }

  cJSON *obj = cJSON_CreateObject();
  cJSON_AddStringToObject(obj, "id", id);
  cJSON_AddStringToObject(obj, "name", "Alice");
  chttp_send_cjson(res, obj);
  cJSON_Delete(obj);
}

/* POST /users — expects JSON body {"name":"..."} */
void handle_create_user(HttpRequest *req, HttpResponse *res) {
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
                         ? name_item->valuestring
                         : "Unknown";

  cJSON *resp = cJSON_CreateObject();
  cJSON_AddStringToObject(resp, "status", "created");
  cJSON_AddStringToObject(resp, "name", name);

  chttp_set_status(res, 201);
  chttp_send_cjson(res, resp);

  cJSON_Delete(body);
  cJSON_Delete(resp);
}

/* GET /echo?msg=... */
void handle_echo(HttpRequest *req, HttpResponse *res) {
  const char *msg = chttp_query_param(req, "msg");
  if (!msg) {
    chttp_send_text(res, "(no msg query param)");
    return;
  }
  chttp_send_text(res, msg);
}

/* POST /login */
void handle_login(HttpRequest *req, HttpResponse *res) {
  if (!req->body || req->body_len == 0) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Request body required\"}");
    return;
  }
  cJSON *json = cJSON_ParseWithLength(req->body, req->body_len);
  if (!json) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid JSON\"}");
    return;
  }
  cJSON *j_user = cJSON_GetObjectItem(json, "username");
  cJSON *j_pass = cJSON_GetObjectItem(json, "password");
  if (!cJSON_IsString(j_user) || !cJSON_IsString(j_pass)) {
    cJSON_Delete(json);
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"username and password required\"}");
    return;
  }

  printf("authenticate_pam, j_user->valuestring=%s, j_pass->valuestring=%s\n",
         j_user->valuestring, j_pass->valuestring);
  if (authenticate_pam(j_user->valuestring, j_pass->valuestring) < 0) {
    cJSON_Delete(json);
    chttp_set_status(res, 401);
    chttp_send_json(res, "{\"error\":\"Invalid credentials\"}");
    return;
  }

  char session_id[65];
  if (create_session(j_user->valuestring, session_id) < 0) {
    cJSON_Delete(json);
    chttp_set_status(res, 500);
    chttp_send_json(res, "{\"error\":\"Failed to create session\"}");
    return;
  }
  cJSON_Delete(json);

  char cookie_val[128];
  snprintf(cookie_val, sizeof(cookie_val),
           "session=%s; HttpOnly; Path=/; SameSite=Lax", session_id);
  chttp_set_header(res, "Set-Cookie", cookie_val);

  chttp_send_json(res, "{\"message\":\"Login successful\"}");
}

/* GET /socket — WebSocket endpoint */
void handle_socket(int fd) {
  char buf[4096];
  time_t last_tick = time(NULL);

  while (1) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    long wait = 10 - (long)(time(NULL) - last_tick);
    if (wait <= 0)
      wait = 0;
    struct timeval tv = {.tv_sec = wait, .tv_usec = 0};

    int sel = select(fd + 1, &rfds, NULL, NULL, &tv);
    if (sel < 0)
      break;

    /* Periodic tick */
    if ((long)(time(NULL) - last_tick) >= 10) {
      char ts[32];
      time_t t = time(NULL);
      strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", gmtime(&t));

      cJSON *ev = cJSON_CreateObject();
      cJSON_AddStringToObject(ev, "event", "tick");
      cJSON_AddStringToObject(ev, "timestamp", ts);
      char *msg = cJSON_PrintUnformatted(ev);
      cJSON_Delete(ev);
      if (chttp_ws_send(fd, msg, strlen(msg)) < 0) {
        free(msg);
        break;
      }
      free(msg);
      last_tick = time(NULL);
    }

    if (sel == 0)
      continue;

    int n = chttp_ws_recv(fd, buf, sizeof(buf) - 1);
    if (n < 0)
      break;
    if (n == 0)
      continue;

    char ts[32];
    time_t t = time(NULL);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", gmtime(&t));

    cJSON *ev = cJSON_CreateObject();
    cJSON_AddStringToObject(ev, "event", "echo");
    cJSON_AddStringToObject(ev, "data", buf);
    cJSON_AddStringToObject(ev, "timestamp", ts);
    char *msg = cJSON_PrintUnformatted(ev);
    cJSON_Delete(ev);
    if (chttp_ws_send(fd, msg, strlen(msg)) < 0) {
      free(msg);
      break;
    }
    free(msg);
  }

  close(fd);
}

/* GET /sse — Server-Sent Events endpoint */
void handle_sse(int fd) {
  while (1) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    struct timeval tv = {.tv_sec = 10, .tv_usec = 0};

    int sel = select(fd + 1, &rfds, NULL, NULL, &tv);
    if (sel < 0)
      break;

    if (sel > 0) {
      char tmp[16];
      if (recv(fd, tmp, sizeof(tmp), MSG_DONTWAIT) <= 0)
        break;
    }

    char ts[32];
    time_t t = time(NULL);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", gmtime(&t));

    char ping_data[64];
    snprintf(ping_data, sizeof(ping_data), "{\"timestamp\":\"%s\"}", ts);
    if (chttp_sse_send(fd, "ping", ping_data) < 0)
      break;

    char msg_data[128];
    snprintf(msg_data, sizeof(msg_data),
             "{\"text\":\"hello from server\",\"timestamp\":\"%s\"}", ts);
    if (chttp_sse_send(fd, "message", msg_data) < 0)
      break;
  }

  close(fd);
}

/* GET /fdownload/:filename */
void handle_download(HttpRequest *req, HttpResponse *res) {
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
void handle_fmetadata(HttpRequest *req, HttpResponse *res) {
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
  cJSON_AddStringToObject(meta, "filename", filename);
  cJSON_AddStringToObject(meta, "path", filepath);
  cJSON_AddStringToObject(meta, "content_type", mime_from_ext(filename));
  cJSON_AddNumberToObject(meta, "size_bytes", (double)st.st_size);
  cJSON_AddStringToObject(meta, "modified_at", mtime_str);

  chttp_send_cjson(res, meta);
  cJSON_Delete(meta);
}

/* POST /upload — multipart/form-data file upload */
void handle_upload(HttpRequest *req, HttpResponse *res) {
  char *file_data;
  size_t file_size;
  char filename[256];
  char part_ct[128];

  if (parse_multipart(req, res, &file_data, &file_size, filename, part_ct) < 0)
    return;

  for (char *c = filename; *c; c++)
    if (*c == '/' || *c == '\\')
      *c = '_';

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

  cJSON *meta = cJSON_CreateObject();
  cJSON_AddStringToObject(meta, "filename", filename);
  cJSON_AddStringToObject(meta, "path", filepath);
  cJSON_AddStringToObject(meta, "content_type", part_ct);
  cJSON_AddNumberToObject(meta, "size_bytes", (double)file_size);

  chttp_set_status(res, 201);
  chttp_send_cjson(res, meta);
  cJSON_Delete(meta);
}

/* GET /whoami — runs under authenticated user's OS privileges (via fork) */
void handle_whoami_impl(HttpRequest *req, HttpResponse *res) {
  (void)req;

  uid_t uid = getuid();
  gid_t gid = getgid();

  struct passwd pw_buf, *pw;
  char pw_strbuf[1024];
  getpwuid_r(uid, &pw_buf, pw_strbuf, sizeof(pw_strbuf), &pw);

  char cwd[512];
  if (!getcwd(cwd, sizeof(cwd)))
    strncpy(cwd, "(unknown)", sizeof(cwd));

  cJSON *obj = cJSON_CreateObject();
  cJSON_AddNumberToObject(obj, "uid", (double)uid);
  cJSON_AddNumberToObject(obj, "gid", (double)gid);
  if (pw) {
    cJSON_AddStringToObject(obj, "username", pw->pw_name);
    cJSON_AddStringToObject(obj, "home", pw->pw_dir);
    cJSON_AddStringToObject(obj, "shell", pw->pw_shell);
  }
  cJSON_AddStringToObject(obj, "cwd", cwd);

  chttp_send_cjson(res, obj);
  cJSON_Delete(obj);
}
