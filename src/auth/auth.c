#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <security/pam_appl.h>
#include <security/pam_misc.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "auth.h"

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

int authenticate_pam(const char *username, const char *password) {
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

int fork_and_run(HttpRequest *req, HttpResponse *res,
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

    /* Verify privilege drop is irreversible (skip if user is root). */
    if (pw->pw_uid != 0 && setuid(0) == 0)
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
    chttp_response_free(&err_res);
  }

  /*
   * Set status to 0 — the sentinel that tells connection_thread the response
   * has already been written to the socket and chttp_write_response must be
   * skipped.
   */
  res->status = 0;
  return 0;
}
