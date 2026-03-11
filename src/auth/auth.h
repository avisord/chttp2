#pragma once
#include <string.h>
#include <time.h>
#include "chttp.h"

#define SESSION_DIR           "./sessions"
#define SESSION_ID_BYTES       32
#define SESSION_EXPIRY_SEC    (24 * 3600)
#define FORK_HANDLER_TIMEOUT_SEC  30
#define FORK_STREAM_TIMEOUT_SEC   3600  /* 1 hour — for large streaming uploads */

extern int g_server_fd;

typedef struct {
  char   id[65];
  char   username[128];
  time_t created_at;
  time_t expires_at;
  time_t last_access_time;
} SessionInfo;

int authenticate_pam(const char *username, const char *password);
int create_session(const char *username, char session_id_out[65]);

/* Parse active_session=<token>/<user> from Cookie header. */
int parse_active_session_cookie(const char *cookie_hdr,
                                 char sid_out[65], char user_out[128]);

int validate_session(const char *session_id, char *username_out, size_t out_size);
int read_session_info(const char *session_id, SessionInfo *info_out);
int delete_session_file(const char *session_id);
int find_user_session_id(const char *cookie_hdr, const char *username,
                          char sid_out[65]);

int fork_and_run(HttpRequest *req, HttpResponse *res, RouteHandler handler,
                 const char *username);
int fork_and_stream(HttpRequest *req, HttpResponse *res, RouteHandler handler,
                    const char *username);

/*
 * DEFINE_AUTH_ROUTE(wrapper_name, inner_handler)
 *
 * Validates the active_session cookie (token + username cross-check), then
 * calls fork_and_run() to execute inner_handler under user OS privileges.
 */
#define DEFINE_AUTH_ROUTE(wrapper_name, inner_handler)                         \
  static void wrapper_name(HttpRequest *req, HttpResponse *res) {              \
    const char *_cookie = chttp_header(req, "Cookie");                         \
    char _sid[65] = "";                                                        \
    char _claimed[128] = "";                                                   \
    char _user[128] = "";                                                      \
    if (!parse_active_session_cookie(_cookie, _sid, _claimed) ||               \
        validate_session(_sid, _user, sizeof(_user)) < 0 ||                    \
        strcmp(_claimed, _user) != 0) {                                        \
      chttp_set_status(res, 401);                                              \
      chttp_send_json(res, "{\"error\":\"Unauthorized\"}");                    \
      return;                                                                  \
    }                                                                          \
    fork_and_run(req, res, (inner_handler), _user);                            \
  }

/*
 * DEFINE_STREAM_AUTH_ROUTE(wrapper_name, inner_handler)
 *
 * Like DEFINE_AUTH_ROUTE but calls fork_and_stream() (1-hour timeout).
 */
#define DEFINE_STREAM_AUTH_ROUTE(wrapper_name, inner_handler)                  \
  static void wrapper_name(HttpRequest *req, HttpResponse *res) {              \
    const char *_cookie = chttp_header(req, "Cookie");                         \
    char _sid[65] = "";                                                        \
    char _claimed[128] = "";                                                   \
    char _user[128] = "";                                                      \
    if (!parse_active_session_cookie(_cookie, _sid, _claimed) ||               \
        validate_session(_sid, _user, sizeof(_user)) < 0 ||                    \
        strcmp(_claimed, _user) != 0) {                                        \
      chttp_set_status(res, 401);                                              \
      chttp_send_json(res, "{\"error\":\"Unauthorized\"}");                    \
      return;                                                                  \
    }                                                                          \
    fork_and_stream(req, res, (inner_handler), _user);                         \
  }

/*
 * DEFINE_NOPRIV_AUTH_ROUTE(wrapper_name, inner_handler)
 *
 * Validates the active_session cookie, then calls inner_handler directly in
 * the connection thread (as root, no fork/setuid).  Used for session
 * management routes that need to create/delete session files.
 */
#define DEFINE_NOPRIV_AUTH_ROUTE(wrapper_name, inner_handler)                  \
  static void wrapper_name(HttpRequest *req, HttpResponse *res) {              \
    const char *_cookie = chttp_header(req, "Cookie");                         \
    char _sid[65] = "";                                                        \
    char _claimed[128] = "";                                                   \
    char _user[128] = "";                                                      \
    if (!parse_active_session_cookie(_cookie, _sid, _claimed) ||               \
        validate_session(_sid, _user, sizeof(_user)) < 0 ||                    \
        strcmp(_claimed, _user) != 0) {                                        \
      chttp_set_status(res, 401);                                              \
      chttp_send_json(res, "{\"error\":\"Unauthorized\"}");                    \
      return;                                                                  \
    }                                                                          \
    (inner_handler)(req, res);                                                 \
  }
