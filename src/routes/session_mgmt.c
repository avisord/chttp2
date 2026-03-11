#include <stdio.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "chttp.h"
#include "auth/auth.h"
#include "session_mgmt.h"

/* GET /sessions
 * Returns all sessions found in the Cookie header as a JSON array.
 * Scans for session_<user>=<token> cookies and reads their session files. */
void handle_get_sessions_impl(HttpRequest *req, HttpResponse *res) {
  const char *cookie_hdr = chttp_header(req, "Cookie");
  cJSON *arr = cJSON_CreateArray();
  printf("cookie_hdr: %s\n", cookie_hdr);

  if (cookie_hdr) {
    const char *p = cookie_hdr;
    while (*p) {
      while (*p == ' ') p++;
      /* Match session_<user>=<token> but NOT active_session= */
      if (strncmp(p, "session_", 8) == 0) {
        const char *eq = strchr(p, '=');
        if (eq) {
          const char *tok_start = eq + 1;
          char tok[65] = "";
          size_t i = 0;
          while (i < 64 && tok_start[i] && tok_start[i] != ';' && tok_start[i] != ' ') {
            tok[i] = tok_start[i];
            i++;
          }
          tok[i] = '\0';

          if (i == 64) {
            SessionInfo info;
            if (read_session_info(tok, &info) == 0 && info.expires_at > time(NULL)) {
              cJSON *obj = cJSON_CreateObject();
              cJSON_AddStringToObject(obj, "session_id", info.id);
              cJSON_AddStringToObject(obj, "username", info.username);
              cJSON_AddNumberToObject(obj, "expiry_time", (double)info.expires_at);
              cJSON_AddNumberToObject(obj, "last_access_time", (double)info.last_access_time);
              cJSON_AddItemToArray(arr, obj);
            }
          }
        }
      }
      const char *semi = strchr(p, ';');
      if (!semi) break;
      p = semi + 1;
    }
  }

  chttp_send_cjson(res, arr);
  cJSON_Delete(arr);
}

/* DELETE /sessions/:session_id
 * Deletes a session.  Auth: the caller must hold session_<user>=<target_sid>
 * in their cookies (no active_session required). */
void handle_delete_session_impl(HttpRequest *req, HttpResponse *res) {
  const char *target_sid = chttp_path_param(req, "session_id");
  if (!target_sid || !target_sid[0]) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"session_id required\"}");
    return;
  }

  SessionInfo info;
  if (read_session_info(target_sid, &info) < 0) {
    chttp_set_status(res, 404);
    chttp_send_json(res, "{\"error\":\"Session not found\"}");
    return;
  }

  /* Verify the requester holds the per-user cookie for this session. */
  const char *cookie_hdr = chttp_header(req, "Cookie");
  char owned_sid[65] = "";
  if (!find_user_session_id(cookie_hdr, info.username, owned_sid) ||
      strcmp(owned_sid, target_sid) != 0) {
    chttp_set_status(res, 403);
    chttp_send_json(res, "{\"error\":\"Forbidden\"}");
    return;
  }

  delete_session_file(target_sid);

  /* Always clear the per-user cookie for this session. */
  char cbuf[256];
  snprintf(cbuf, sizeof(cbuf),
      "session_%s=; Expires=Thu, 01 Jan 1970 00:00:00 GMT; HttpOnly; Path=/; SameSite=Lax",
      info.username);
  chttp_add_header(res, "Set-Cookie", cbuf);

  /* If this was also the active session, clear that cookie too. */
  char cur_sid[65] = "", cur_user[128] = "";
  if (parse_active_session_cookie(cookie_hdr, cur_sid, cur_user) &&
      strcmp(cur_sid, target_sid) == 0) {
    chttp_add_header(res, "Set-Cookie",
        "active_session=; Expires=Thu, 01 Jan 1970 00:00:00 GMT; HttpOnly; Path=/; SameSite=Lax");
  }

  chttp_send_json(res, "{\"message\":\"Session deleted\"}");
}

/* DELETE /logout
 * Deletes the active session and clears all related cookies. */
void handle_logout_impl(HttpRequest *req, HttpResponse *res) {
  const char *cookie_hdr = chttp_header(req, "Cookie");
  char cur_sid[65] = "", cur_user[128] = "";
  if (!parse_active_session_cookie(cookie_hdr, cur_sid, cur_user)) {
    chttp_set_status(res, 401);
    chttp_send_json(res, "{\"error\":\"Unauthorized\"}");
    return;
  }

  delete_session_file(cur_sid);

  chttp_add_header(res, "Set-Cookie",
      "active_session=; Expires=Thu, 01 Jan 1970 00:00:00 GMT; HttpOnly; Path=/; SameSite=Lax");

  char cbuf[256];
  snprintf(cbuf, sizeof(cbuf),
      "session_%s=; Expires=Thu, 01 Jan 1970 00:00:00 GMT; HttpOnly; Path=/; SameSite=Lax",
      cur_user);
  chttp_add_header(res, "Set-Cookie", cbuf);

  chttp_send_json(res, "{\"message\":\"Logged out\"}");
}

/* POST /sessions/switch/:session_id
 * Switches the active session.  Auth: the caller must hold
 * session_<user>=<target_sid> in their cookies. */
void handle_switch_session_impl(HttpRequest *req, HttpResponse *res) {
  const char *target_sid = chttp_path_param(req, "session_id");
  if (!target_sid || !target_sid[0]) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"session_id required\"}");
    return;
  }

  SessionInfo info;
  if (read_session_info(target_sid, &info) < 0 || info.expires_at <= time(NULL)) {
    chttp_set_status(res, 404);
    chttp_send_json(res, "{\"error\":\"Session not found or expired\"}");
    return;
  }

  /* Verify the requester holds the per-user cookie for this session. */
  const char *cookie_hdr = chttp_header(req, "Cookie");
  char owned_sid[65] = "";
  if (!find_user_session_id(cookie_hdr, info.username, owned_sid) ||
      strcmp(owned_sid, target_sid) != 0) {
    chttp_set_status(res, 403);
    chttp_send_json(res, "{\"error\":\"Forbidden\"}");
    return;
  }

  char abuf[256];
  snprintf(abuf, sizeof(abuf),
      "active_session=%s/%s; HttpOnly; Path=/; SameSite=Lax",
      info.id, info.username);
  chttp_add_header(res, "Set-Cookie", abuf);

  chttp_send_json(res, "{\"message\":\"Session switched\"}");
}
