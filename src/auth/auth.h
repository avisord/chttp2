#pragma once
#include "chttp.h"

#define SESSION_DIR           "./sessions"
#define SESSION_ID_BYTES       32
#define SESSION_EXPIRY_SEC    (24 * 3600)
#define FORK_HANDLER_TIMEOUT_SEC 30

extern int g_server_fd;

int authenticate_pam(const char *username, const char *password);
int create_session(const char *username, char session_id_out[65]);
int parse_session_cookie(const char *cookie_hdr, char out[65]);
int validate_session(const char *session_id, char *username_out, size_t out_size);
int fork_and_run(HttpRequest *req, HttpResponse *res, RouteHandler handler,
                 const char *username);

/*
 * DEFINE_AUTH_ROUTE(wrapper_name, inner_handler)
 *
 * Generates a static RouteHandler `wrapper_name` that validates the session
 * cookie and, on success, calls fork_and_run() to execute `inner_handler`
 * under the authenticated user's OS privileges.
 *
 * All 13 invocations live in src/main.c — easy to audit the auth boundary.
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
