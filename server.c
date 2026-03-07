#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pthread.h>
#include <pwd.h>
#include <security/pam_appl.h>
#include <security/pam_misc.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "chttp.h"

#define SESSION_DIR "./sessions"
#define SESSION_ID_BYTES 32
#define SESSION_EXPIRY_SEC (24 * 3600)

static int generate_session_id(char out[65]) {
  uint8_t bytes[SESSION_ID_BYTES];
  int fd = open("/dev/urandom", O_RDONLY);
  if (fd < 0)
    return -1;
  ssize_t n = read(fd, bytes, sizeof(bytes));
  close(fd);
  if (n != SESSION_ID_BYTES)
    return -1;
  for (int i = 0; i < SESSION_ID_BYTES; i++)
    snprintf(out + i * 2, 3, "%02x", bytes[i]);
  out[64] = '\0';
  return 0;
}

struct pam_creds {
  const char *password;
};

static int pam_conv_func(int num_msg, const struct pam_message **msg,
                         struct pam_response **resp, void *appdata_ptr) {
  struct pam_creds *c = appdata_ptr;
  struct pam_response *r = calloc(num_msg, sizeof(*r));
  if (!r)
    return PAM_CONV_ERR;
  for (int i = 0; i < num_msg; i++) {
    if (msg[i]->msg_style == PAM_PROMPT_ECHO_OFF ||
        msg[i]->msg_style == PAM_PROMPT_ECHO_ON) {
      r[i].resp = strdup(c->password);
    }
  }
  *resp = r;
  return PAM_SUCCESS;
}

static int authenticate_pam(const char *username, const char *password) {
  struct pam_creds creds = {.password = password};
  struct pam_conv conv = {pam_conv_func, &creds};
  pam_handle_t *pamh = NULL;
  int ret = pam_start("login", username, &conv, &pamh);
  if (ret != PAM_SUCCESS)
    return -1;
  ret = pam_authenticate(pamh, 0);
  pam_end(pamh, ret);
  return (ret == PAM_SUCCESS) ? 0 : -1;
}

static int create_session(const char *username, char session_id_out[65]) {
  char id[65];
  if (generate_session_id(id) < 0)
    return -1;

  time_t now = time(NULL);
  time_t expires = now + SESSION_EXPIRY_SEC;

  char filepath[256];
  snprintf(filepath, sizeof(filepath), "%s/session_%s", SESSION_DIR, id);

  int session_file_perm = 0644;
  int fd = open(filepath, O_WRONLY | O_CREAT | O_EXCL, session_file_perm);
  if (fd < 0)
    return -1;

  char buf[256];
  int len = snprintf(buf, sizeof(buf),
                     "id=%s\nusername=%s\ncreated_at=%ld\nexpires_at=%ld\n", id,
                     username, (long)now, (long)expires);
  printf("create_session, buf=%s\n", buf);
  ssize_t written = write(fd, buf, len);
  printf("written %lu bytes\n", written);
  if (written != len) {
    close(fd);
    return -1;
  }

  close(fd);
  memcpy(session_id_out, id, 65);
  return 0;
}

static int parse_session_cookie(const char *cookie_hdr, char out[65]) {
  if (!cookie_hdr)
    return 0;
  const char *p = strstr(cookie_hdr, "session=");
  if (!p)
    return 0;
  p += 8;
  size_t i = 0;
  while (i < 64 && *p && *p != ';' && *p != ' ')
    out[i++] = *p++;
  out[i] = '\0';
  return i > 0;
}

static int validate_session(const char *session_id, char *username_out,
                            size_t out_size) {
  for (int i = 0; session_id[i]; i++) {
    if (!isxdigit((unsigned char)session_id[i]))
      return -1;
  }
  char filepath[256];
  snprintf(filepath, sizeof(filepath), "%s/session_%s", SESSION_DIR,
           session_id);

  int fd = open(filepath, O_RDONLY);
  if (fd < 0)
    return -1;

  char content[512];
  ssize_t n = read(fd, content, sizeof(content) - 1);
  close(fd);
  if (n <= 0)
    return -1;
  content[n] = '\0';

  char username[128] = "";
  time_t expires_at = 0;

  char *line = content;
  while (line && *line) {
    char *nl = strchr(line, '\n');
    if (nl)
      *nl = '\0';
    if (strncmp(line, "username=", 9) == 0)
      strncpy(username, line + 9, sizeof(username) - 1);
    else if (strncmp(line, "expires_at=", 11) == 0)
      expires_at = (time_t)atol(line + 11);
    line = nl ? nl + 1 : NULL;
  }

  if (!username[0] || expires_at == 0)
    return -1;
  if (time(NULL) > expires_at)
    return -1;

  if (username_out)
    strncpy(username_out, username, out_size - 1);
  return 0;
}

/* Listening socket fd — closed in child processes to prevent inheritance. */
static int g_server_fd = -1;

/*
 * fork_and_run — authenticate, fork, drop privileges, execute handler.
 *
 * Forks a child process that:
 *   1. Closes the listening socket.
 *   2. Drops supplementary groups, GID, and UID to those of `username`.
 *   3. Changes the working directory to the user's home.
 *   4. Calls `handler(req, &child_res)` and writes the response to the
 *      client fd directly.
 *   5. Exits via _exit(0).
 *
 * The parent waits up to FORK_HANDLER_TIMEOUT_SEC seconds.  If the child
 * times out it is SIGKILL'd and the parent sends 504 to the client.
 *
 * On success  → res->status is set to 0 (sentinel: "response already sent").
 * On pre-fork error → res is filled with the error and -1 is returned so the
 *                     caller's normal response path writes it.
 */
#define FORK_HANDLER_TIMEOUT_SEC 30

static int fork_and_run(HttpRequest *req, HttpResponse *res,
                        RouteHandler handler, const char *username) {
  /* Look up user info before forking (use reentrant variant). */
  struct passwd pw_buf, *pw;
  char pw_strbuf[1024];
  if (getpwnam_r(username, &pw_buf, pw_strbuf, sizeof(pw_strbuf), &pw) != 0 ||
      !pw) {
    chttp_set_status(res, 500);
    chttp_send_json(res, "{\"error\":\"System user account not found\"}");
    return -1;
  }

  /*
   * Use a pipe so the parent can detect child exit without SIGALRM/signals.
   * The write-end is inherited by the child; when _exit() closes it, the
   * parent's select() sees EOF on the read-end.
   */
  int pipefd[2];
  if (pipe(pipefd) < 0) {
    chttp_set_status(res, 503);
    chttp_send_json(res, "{\"error\":\"Service temporarily unavailable\"}");
    return -1;
  }

  int client_fd = req->fd;

  pid_t pid = fork();
  if (pid < 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    chttp_set_status(res, 503);
    chttp_send_json(res, "{\"error\":\"Service temporarily unavailable\"}");
    return -1;
  }

  /* ------------------------------------------------------------------ */
  /* Child process                                                        */
  /* ------------------------------------------------------------------ */
  if (pid == 0) {
    close(pipefd[0]); /* child does not read from pipe */

    /* Release the listening socket — child must not accept connections. */
    if (g_server_fd >= 0)
      close(g_server_fd);

/* Convenience: send an error response from the child and exit. */
#define CHILD_ERR(http_code, json_msg)                                         \
  do {                                                                         \
    HttpResponse _er;                                                          \
    memset(&_er, 0, sizeof(_er));                                              \
    _er.status = (http_code);                                                  \
    chttp_send_json(&_er, (json_msg));                                         \
    chttp_write_response(client_fd, &_er);                                     \
    close(client_fd);                                                          \
    close(pipefd[1]);                                                          \
    _exit(1);                                                                  \
  } while (0)

    /* Drop supplementary groups first. */
    if (initgroups(username, pw->pw_gid) < 0)
      CHILD_ERR(500,
                "{\"error\":\"Failed to initialise supplementary groups\"}");

    /* Set GID before UID — once root is surrendered GID cannot be changed. */
    if (setgid(pw->pw_gid) < 0)
      CHILD_ERR(500, "{\"error\":\"Failed to set process group\"}");

    if (setuid(pw->pw_uid) < 0)
      CHILD_ERR(500, "{\"error\":\"Failed to set process user\"}");

    /* Verify privilege drop is irreversible. */
    if (setuid(0) == 0)
      CHILD_ERR(500, "{\"error\":\"Privilege drop verification failed\"}");

    /* Change to user's home directory; fall back to /tmp on failure. */
    if (chdir(pw->pw_dir) < 0)
      chdir("/tmp");

    /* Run the actual request handler. */
    HttpResponse child_res;
    memset(&child_res, 0, sizeof(child_res));
    child_res.status = 200;
    handler(req, &child_res);

    /* Write response to client then tear down. */
    chttp_write_response(client_fd, &child_res);
    close(client_fd);
    close(pipefd[1]); /* EOF on pipe — signals parent we are done */
    _exit(0);

#undef CHILD_ERR
  }

  /* ------------------------------------------------------------------ */
  /* Parent process                                                       */
  /* ------------------------------------------------------------------ */
  close(pipefd[1]); /* parent does not write to pipe */

  /* Wait for child to finish (pipe read-end gets EOF on child exit). */
  fd_set rfds;
  FD_ZERO(&rfds);
  FD_SET(pipefd[0], &rfds);
  struct timeval tv = {.tv_sec = FORK_HANDLER_TIMEOUT_SEC, .tv_usec = 0};

  int sel;
  do {
    sel = select(pipefd[0] + 1, &rfds, NULL, NULL, &tv);
  } while (sel < 0 && errno == EINTR);

  close(pipefd[0]);

  int timed_out = (sel <= 0);
  if (timed_out) {
    /* Child is hung — kill it unconditionally. */
    kill(pid, SIGKILL);
  }

  /* Always reap to prevent zombies. */
  int wstatus;
  waitpid(pid, &wstatus, 0);

  if (timed_out) {
    /* Child never wrote a response; write the gateway-timeout ourselves. */
    HttpResponse err_res;
    memset(&err_res, 0, sizeof(err_res));
    err_res.status = 504;
    chttp_send_json(&err_res, "{\"error\":\"Request handler timed out\"}");
    chttp_write_response(client_fd, &err_res);
  }

  /*
   * Set status to 0 — the sentinel that tells connection_thread the response
   * has already been written to the socket and chttp_write_response must be
   * skipped.
   */
  res->status = 0;
  return 0;
}

/*
 * DEFINE_AUTH_ROUTE(wrapper_name, inner_handler)
 *
 * Generates a RouteHandler `wrapper_name` that:
 *   1. Validates the session cookie → extracts username.
 *   2. On invalid session: responds 401 immediately (no fork).
 *   3. On valid session: calls fork_and_run() to execute `inner_handler`
 *      under the authenticated user's OS privileges.
 */
#define DEFINE_AUTH_ROUTE(wrapper_name, inner_handler)                         \
  static void wrapper_name(HttpRequest *req, HttpResponse *res) {              \
    const char *_cookie = chttp_header(req, "Cookie");                         \
    char _sid[65] = "";                                                        \
    char _user[128] = "";                                                      \
    if (!parse_session_cookie(_cookie, _sid) ||                                \
        validate_session(_sid, _user, sizeof(_user)) < 0) {                    \
      chttp_set_status(res, 401);                                              \
      chttp_send_json(res, "{\"error\":\"Unauthorized\"}");                    \
      return;                                                                  \
    }                                                                          \
    fork_and_run(req, res, (inner_handler), _user);                            \
  }

static int require_auth(HttpRequest *req, HttpResponse *res)
    __attribute__((unused));
static int require_auth(HttpRequest *req, HttpResponse *res) {
  const char *cookie = chttp_header(req, "Cookie");
  char session_id[65] = "";
  if (!parse_session_cookie(cookie, session_id) ||
      validate_session(session_id, NULL, 0) < 0) {
    chttp_set_status(res, 401);
    chttp_send_json(res, "{\"error\":\"Unauthorized\"}");
    return -1;
  }
  return 0;
}

static void handle_login(HttpRequest *req, HttpResponse *res) {
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
  cJSON_AddStringToObject(obj, "id", id);
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
static void handle_echo(HttpRequest *req, HttpResponse *res) {
  const char *msg = chttp_query_param(req, "msg");
  if (!msg) {
    chttp_send_text(res, "(no msg query param)");
    return;
  }
  chttp_send_text(res, msg);
}

/* GET /socket — WebSocket endpoint
 *   client → server : any text, echoed back with timestamp
 *   server → client : echo reply
 * {"event":"echo","data":"...","timestamp":"..."} periodic
 * {"event":"tick","timestamp":"..."}  every 10 s
 */
static void handle_socket(int fd) {
  char buf[4096];
  time_t last_tick = time(NULL);

  while (1) {
    /* Use select so we can fire the periodic tick even with no client data. */
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
      continue; /* timeout only — no data */

    /* Client frame */
    int n = chttp_ws_recv(fd, buf, sizeof(buf) - 1);
    if (n < 0)
      break; /* connection closed */
    if (n == 0)
      continue; /* ping — handled inside chttp_ws_recv */

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

/* GET /sse — Server-Sent Events endpoint
 *   Every 10 s the server pushes two named events:
 *     event: ping   — keepalive with current timestamp
 *     event: message — demo payload
 *   select() on fd detects client disconnect between ticks.
 */
static void handle_sse(int fd) {
  while (1) {
    /* Wait up to 10 s; readable fd means client closed the connection. */
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    struct timeval tv = {.tv_sec = 10, .tv_usec = 0};

    int sel = select(fd + 1, &rfds, NULL, NULL, &tv);
    if (sel < 0)
      break;

    if (sel > 0) {
      /* SSE clients don't send data; readable means EOF / disconnect. */
      char tmp[16];
      if (recv(fd, tmp, sizeof(tmp), MSG_DONTWAIT) <= 0)
        break;
    }

    /* Build timestamp */
    char ts[32];
    time_t t = time(NULL);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", gmtime(&t));

    /* ping — keepalive */
    char ping_data[64];
    snprintf(ping_data, sizeof(ping_data), "{\"timestamp\":\"%s\"}", ts);
    if (chttp_sse_send(fd, "ping", ping_data) < 0)
      break;

    /* message — demo event */
    char msg_data[128];
    snprintf(msg_data, sizeof(msg_data),
             "{\"text\":\"hello from server\",\"timestamp\":\"%s\"}", ts);
    if (chttp_sse_send(fd, "message", msg_data) < 0)
      break;
  }

  close(fd);
}

/* Detect MIME type from file extension */
static const char *mime_from_ext(const char *filename) {
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

/* Validate filename: no path traversal, no slashes */
static int safe_filename(const char *name) {
  return name && *name && !strchr(name, '/') && !strchr(name, '\\') &&
         !strstr(name, "..");
}

/* Validate relative path: allows '/' segments, blocks '..', absolute, empty */
static int safe_path(const char *path) {
  if (!path || !*path || path[0] == '/')
    return 0;
  if (strstr(path, ".."))
    return 0;
  return 1;
}

/* Map errno to HTTP status and write JSON error body */
static void fs_error(HttpResponse *res, int err) {
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

/*
 * parse_multipart — parse the first part of a multipart/form-data body.
 *
 * On success returns 0 and sets:
 *   *file_data_out  — pointer into req->body where file bytes start
 *   *file_size_out  — number of file bytes
 *   filename_out    — NUL-terminated filename (256 bytes)
 *   part_ct_out     — NUL-terminated part Content-Type (128 bytes)
 *
 * On error writes an error response and returns -1.
 */
static int parse_multipart(HttpRequest *req, HttpResponse *res,
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
  cJSON_AddStringToObject(meta, "filename", filename);
  cJSON_AddStringToObject(meta, "path", filepath);
  cJSON_AddStringToObject(meta, "content_type", mime_from_ext(filename));
  cJSON_AddNumberToObject(meta, "size_bytes", (double)st.st_size);
  cJSON_AddStringToObject(meta, "modified_at", mtime_str);

  chttp_send_cjson(res, meta);
  cJSON_Delete(meta);
}

/* POST /upload — multipart/form-data file upload */
static void handle_upload(HttpRequest *req, HttpResponse *res) {
  char *file_data;
  size_t file_size;
  char filename[256];
  char part_ct[128];

  if (parse_multipart(req, res, &file_data, &file_size, filename, part_ct) < 0)
    return;

  /* Sanitize filename: reject path traversal */
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

/* ------------------------------------------------------------------ */
/* Authenticated route: GET /whoami                                    */
/*                                                                     */
/* Runs inside a forked child process under the authenticated user's   */
/* OS privileges.  Returns the effective UID/GID, username, home dir,  */
/* and current working directory as seen from that process.            */
/* ------------------------------------------------------------------ */
static void handle_whoami_impl(HttpRequest *req, HttpResponse *res) {
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

/* Generates: void handle_whoami(HttpRequest*, HttpResponse*) */
DEFINE_AUTH_ROUTE(handle_whoami, handle_whoami_impl)

/* ================================================================== */
/* NAS File Management API — all run under authenticated user's UID   */
/* ================================================================== */

/* GET /fs/list?path=. */
static void handle_fs_list_impl(HttpRequest *req, HttpResponse *res) {
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
static void handle_fs_upload_impl(HttpRequest *req, HttpResponse *res) {
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

  /* Sanitize bare filename */
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
static void handle_fs_download_impl(HttpRequest *req, HttpResponse *res) {
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

  if (fsize < 0 || fsize >= (long)CHTTP_RESP_BUFSIZE) {
    fclose(f);
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"File too large\",\"errno\":0}");
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
static void handle_fs_delete_file_impl(HttpRequest *req, HttpResponse *res) {
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
static void handle_fs_mkdir_impl(HttpRequest *req, HttpResponse *res) {
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
static void handle_fs_rmdir_impl(HttpRequest *req, HttpResponse *res) {
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
static void handle_fs_rename_impl(HttpRequest *req, HttpResponse *res) {
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

  /* Build destination: same directory as source, new name */
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
static void handle_fs_move_impl(HttpRequest *req, HttpResponse *res) {
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
static void handle_fs_copy_impl(HttpRequest *req, HttpResponse *res) {
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
static void handle_fs_stat_impl(HttpRequest *req, HttpResponse *res) {
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

#define FS_CONTENT_MAX (64 * 1024)

/* GET /fs/content?path=notes.txt */
static void handle_fs_read_impl(HttpRequest *req, HttpResponse *res) {
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
static void handle_fs_write_impl(HttpRequest *req, HttpResponse *res) {
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

/* Auth wrappers for all 12 fs routes */
DEFINE_AUTH_ROUTE(handle_fs_list,        handle_fs_list_impl)
DEFINE_AUTH_ROUTE(handle_fs_upload,      handle_fs_upload_impl)
DEFINE_AUTH_ROUTE(handle_fs_download,    handle_fs_download_impl)
DEFINE_AUTH_ROUTE(handle_fs_delete_file, handle_fs_delete_file_impl)
DEFINE_AUTH_ROUTE(handle_fs_mkdir,       handle_fs_mkdir_impl)
DEFINE_AUTH_ROUTE(handle_fs_rmdir,       handle_fs_rmdir_impl)
DEFINE_AUTH_ROUTE(handle_fs_rename,      handle_fs_rename_impl)
DEFINE_AUTH_ROUTE(handle_fs_move,        handle_fs_move_impl)
DEFINE_AUTH_ROUTE(handle_fs_copy,        handle_fs_copy_impl)
DEFINE_AUTH_ROUTE(handle_fs_stat,        handle_fs_stat_impl)
DEFINE_AUTH_ROUTE(handle_fs_read,        handle_fs_read_impl)
DEFINE_AUTH_ROUTE(handle_fs_write,       handle_fs_write_impl)

int main(void) {
  mkdir(SESSION_DIR, 0700);

  HttpServer srv;

  if (chttp_server_init(&srv, 8080) < 0)
    return 1;

  /* Expose the listening fd so forked children can close it. */
  g_server_fd = srv.server_fd;

  CHTTP_POST(&srv, "/login", handle_login);
  CHTTP_GET(&srv, "/", handle_root);
  CHTTP_GET(&srv, "/hello", handle_hello);
  CHTTP_GET(&srv, "/users/:id", handle_get_user);
  CHTTP_POST(&srv, "/users", handle_create_user);
  CHTTP_GET(&srv, "/echo", handle_echo);
  CHTTP_POST(&srv, "/upload", handle_upload);
  CHTTP_WS(&srv, "/socket", handle_socket);
  CHTTP_SSE(&srv, "/sse", handle_sse);
  CHTTP_GET(&srv, "/fdownload/:filename", handle_download);
  CHTTP_GET(&srv, "/fmetadata/:filename", handle_fmetadata);
  /* Authenticated — runs handler in a forked child under user privileges */
  CHTTP_GET(&srv, "/whoami", handle_whoami);

  /* NAS File Management API — all authenticated */
  CHTTP_GET(&srv,    "/fs/list",     handle_fs_list);
  CHTTP_POST(&srv,   "/fs/upload",   handle_fs_upload);
  CHTTP_GET(&srv,    "/fs/download", handle_fs_download);
  CHTTP_DELETE(&srv, "/fs/file",     handle_fs_delete_file);
  CHTTP_POST(&srv,   "/fs/mkdir",    handle_fs_mkdir);
  CHTTP_DELETE(&srv, "/fs/dir",      handle_fs_rmdir);
  CHTTP_POST(&srv,   "/fs/rename",   handle_fs_rename);
  CHTTP_POST(&srv,   "/fs/move",     handle_fs_move);
  CHTTP_POST(&srv,   "/fs/copy",     handle_fs_copy);
  CHTTP_GET(&srv,    "/fs/stat",     handle_fs_stat);
  CHTTP_GET(&srv,    "/fs/content",  handle_fs_read);
  CHTTP_PUT(&srv,    "/fs/content",  handle_fs_write);

  chttp_server_run(&srv);

  chttp_server_destroy(&srv);
  return 0;
}
