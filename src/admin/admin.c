#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "admin.h"
#include "cJSON.h"

#define ROOT_GUARD(res)                                                        \
  do {                                                                         \
    if (getuid() != 0) {                                                       \
      chttp_set_status(res, 403);                                              \
      chttp_send_json(res, "{\"error\":\"Root privileges required\"}");        \
      return;                                                                  \
    }                                                                          \
  } while (0)

/* Run a command, return exit status (0 = success). */
static int run_command(char *const argv[]) {
  pid_t pid = fork();
  if (pid < 0) return -1;
  if (pid == 0) {
    /* Silence stdout/stderr from child commands */
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
    execvp(argv[0], argv);
    _exit(127);
  }
  int wstatus;
  waitpid(pid, &wstatus, 0);
  return WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : -1;
}

/* Validate username: alphanumeric + dash/underscore, 1-32 chars. */
static int valid_username(const char *name) {
  if (!name || !*name) return 0;
  size_t len = strlen(name);
  if (len > 32) return 0;
  for (size_t i = 0; i < len; i++) {
    char c = name[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '-' || c == '_'))
      return 0;
  }
  return 1;
}

/* Get supplementary groups for a user as a JSON array. */
static cJSON *get_user_groups(const char *username, gid_t gid) {
  cJSON *arr = cJSON_CreateArray();
  int ngroups = 64;
  gid_t *groups = malloc(sizeof(gid_t) * (size_t)ngroups);
  if (!groups) return arr;

  if (getgrouplist(username, gid, groups, &ngroups) < 0) {
    groups = realloc(groups, sizeof(gid_t) * (size_t)ngroups);
    if (!groups) return arr;
    getgrouplist(username, gid, groups, &ngroups);
  }

  for (int i = 0; i < ngroups; i++) {
    struct group *gr = getgrgid(groups[i]);
    if (gr) cJSON_AddItemToArray(arr, cJSON_CreateString(gr->gr_name));
  }
  free(groups);
  return arr;
}

/* GET /admin/users */
void handle_admin_list_users_impl(HttpRequest *req, HttpResponse *res) {
  (void)req;
  ROOT_GUARD(res);

  cJSON *arr = cJSON_CreateArray();
  struct passwd *pw;
  setpwent();
  while ((pw = getpwent()) != NULL) {
    if (pw->pw_uid < 1000 && pw->pw_uid != 0) continue;

    cJSON *u = cJSON_CreateObject();
    cJSON_AddStringToObject(u, "username", pw->pw_name);
    cJSON_AddNumberToObject(u, "uid", pw->pw_uid);
    cJSON_AddNumberToObject(u, "gid", pw->pw_gid);
    cJSON_AddStringToObject(u, "home", pw->pw_dir);
    cJSON_AddStringToObject(u, "shell", pw->pw_shell);
    cJSON_AddItemToObject(u, "groups", get_user_groups(pw->pw_name, pw->pw_gid));
    cJSON_AddItemToArray(arr, u);
  }
  endpwent();

  chttp_set_status(res, 200);
  char *str = cJSON_PrintUnformatted(arr);
  chttp_send_json(res, str ? str : "[]");
  free(str);
  cJSON_Delete(arr);
}

/* POST /admin/users — { username, password, shell? } */
void handle_admin_create_user_impl(HttpRequest *req, HttpResponse *res) {
  ROOT_GUARD(res);

  cJSON *body = cJSON_ParseWithLength(req->body, req->body_len);
  if (!body) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid JSON\"}");
    return;
  }

  const char *username = cJSON_GetStringValue(cJSON_GetObjectItem(body, "username"));
  const char *password = cJSON_GetStringValue(cJSON_GetObjectItem(body, "password"));
  cJSON *shell_item = cJSON_GetObjectItem(body, "shell");
  const char *shell = cJSON_GetStringValue(shell_item);
  if (!shell) shell = "/bin/bash";

  if (!valid_username(username)) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid username\"}");
    cJSON_Delete(body);
    return;
  }
  if (!password || strlen(password) < 1) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Password required\"}");
    cJSON_Delete(body);
    return;
  }

  /* Check if user already exists */
  struct passwd *existing = getpwnam(username);
  if (existing) {
    chttp_set_status(res, 409);
    chttp_send_json(res, "{\"error\":\"User already exists\"}");
    cJSON_Delete(body);
    return;
  }

  /* useradd -m -s <shell> <username> */
  char *useradd_argv[] = {"useradd", "-m", "-s", (char *)shell, (char *)username, NULL};
  int rc = run_command(useradd_argv);
  if (rc != 0) {
    chttp_set_status(res, 500);
    chttp_send_json(res, "{\"error\":\"Failed to create user\"}");
    cJSON_Delete(body);
    return;
  }

  /* Set password via chpasswd: echo "user:pass" | chpasswd */
  char passbuf[512];
  snprintf(passbuf, sizeof(passbuf), "%s:%s", username, password);

  int pp[2];
  if (pipe(pp) == 0) {
    pid_t cpid = fork();
    if (cpid == 0) {
      close(pp[1]);
      dup2(pp[0], STDIN_FILENO);
      close(pp[0]);
      freopen("/dev/null", "w", stdout);
      freopen("/dev/null", "w", stderr);
      execlp("chpasswd", "chpasswd", NULL);
      _exit(127);
    }
    close(pp[0]);
    write(pp[1], passbuf, strlen(passbuf));
    close(pp[1]);
    int ws;
    waitpid(cpid, &ws, 0);
  }

  /* Wipe password from memory */
  memset(passbuf, 0, sizeof(passbuf));

  chttp_set_status(res, 201);
  chttp_send_json(res, "{\"message\":\"User created\"}");
  cJSON_Delete(body);
}

/* PUT /admin/users/:username — { password?, shell?, groups? } */
void handle_admin_edit_user_impl(HttpRequest *req, HttpResponse *res) {
  ROOT_GUARD(res);

  const char *username = chttp_path_param(req, "username");
  if (!valid_username(username)) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid username\"}");
    return;
  }

  /* Verify user exists */
  struct passwd *pw = getpwnam(username);
  if (!pw) {
    chttp_set_status(res, 404);
    chttp_send_json(res, "{\"error\":\"User not found\"}");
    return;
  }

  cJSON *body = cJSON_ParseWithLength(req->body, req->body_len);
  if (!body) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid JSON\"}");
    return;
  }

  int changed = 0;

  /* Change shell */
  const char *shell = cJSON_GetStringValue(cJSON_GetObjectItem(body, "shell"));
  if (shell) {
    char *argv[] = {"usermod", "-s", (char *)shell, (char *)username, NULL};
    if (run_command(argv) != 0) {
      chttp_set_status(res, 500);
      chttp_send_json(res, "{\"error\":\"Failed to change shell\"}");
      cJSON_Delete(body);
      return;
    }
    changed = 1;
  }

  /* Change groups (supplementary) */
  const char *groups = cJSON_GetStringValue(cJSON_GetObjectItem(body, "groups"));
  if (groups) {
    char *argv[] = {"usermod", "-G", (char *)groups, (char *)username, NULL};
    if (run_command(argv) != 0) {
      chttp_set_status(res, 500);
      chttp_send_json(res, "{\"error\":\"Failed to change groups\"}");
      cJSON_Delete(body);
      return;
    }
    changed = 1;
  }

  /* Change password */
  const char *password = cJSON_GetStringValue(cJSON_GetObjectItem(body, "password"));
  if (password && strlen(password) > 0) {
    char passbuf[512];
    snprintf(passbuf, sizeof(passbuf), "%s:%s", username, password);

    int pp[2];
    if (pipe(pp) == 0) {
      pid_t cpid = fork();
      if (cpid == 0) {
        close(pp[1]);
        dup2(pp[0], STDIN_FILENO);
        close(pp[0]);
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execlp("chpasswd", "chpasswd", NULL);
        _exit(127);
      }
      close(pp[0]);
      write(pp[1], passbuf, strlen(passbuf));
      close(pp[1]);
      int ws;
      waitpid(cpid, &ws, 0);
    }
    memset(passbuf, 0, sizeof(passbuf));
    changed = 1;
  }

  if (!changed) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"No changes specified\"}");
  } else {
    chttp_set_status(res, 200);
    chttp_send_json(res, "{\"message\":\"User updated\"}");
  }
  cJSON_Delete(body);
}

/* DELETE /admin/users/:username */
void handle_admin_delete_user_impl(HttpRequest *req, HttpResponse *res) {
  ROOT_GUARD(res);

  const char *username = chttp_path_param(req, "username");
  if (!valid_username(username)) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid username\"}");
    return;
  }

  /* Prevent deleting root */
  if (strcmp(username, "root") == 0) {
    chttp_set_status(res, 403);
    chttp_send_json(res, "{\"error\":\"Cannot delete root user\"}");
    return;
  }

  struct passwd *pw = getpwnam(username);
  if (!pw) {
    chttp_set_status(res, 404);
    chttp_send_json(res, "{\"error\":\"User not found\"}");
    return;
  }

  /* userdel -r <username> */
  char *argv[] = {"userdel", "-r", (char *)username, NULL};
  int rc = run_command(argv);
  if (rc != 0) {
    chttp_set_status(res, 500);
    chttp_send_json(res, "{\"error\":\"Failed to delete user\"}");
    return;
  }

  chttp_set_status(res, 200);
  chttp_send_json(res, "{\"message\":\"User deleted\"}");
}
