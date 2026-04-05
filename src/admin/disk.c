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
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
    execvp(argv[0], argv);
    _exit(127);
  }
  int wstatus;
  waitpid(pid, &wstatus, 0);
  return WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : -1;
}

/* Run a command and capture its stdout. */
static int run_command_output(char *const argv[], char *buf, size_t bufsz) {
  int pipefd[2];
  if (pipe(pipefd) < 0) return -1;

  pid_t pid = fork();
  if (pid < 0) { close(pipefd[0]); close(pipefd[1]); return -1; }

  if (pid == 0) {
    close(pipefd[0]);
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[1]);
    freopen("/dev/null", "w", stderr);
    execvp(argv[0], argv);
    _exit(127);
  }

  close(pipefd[1]);
  size_t total = 0;
  while (total < bufsz - 1) {
    ssize_t n = read(pipefd[0], buf + total, bufsz - 1 - total);
    if (n <= 0) break;
    total += (size_t)n;
  }
  buf[total] = '\0';
  close(pipefd[0]);

  int wstatus;
  waitpid(pid, &wstatus, 0);
  return WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : -1;
}

/* Validate path: must start with / and have no .. */
static int valid_path(const char *path) {
  if (!path || path[0] != '/') return 0;
  if (strstr(path, "..")) return 0;
  return 1;
}

/* GET /admin/disks */
void handle_admin_list_disks_impl(HttpRequest *req, HttpResponse *res) {
  (void)req;
  ROOT_GUARD(res);

  /* lsblk outputs JSON directly */
  char *argv[] = {
    "lsblk", "-Jbo", "NAME,SIZE,TYPE,FSTYPE,MOUNTPOINT,LABEL,MODEL", NULL
  };
  char buf[65536];
  int rc = run_command_output(argv, buf, sizeof(buf));
  if (rc != 0) {
    chttp_set_status(res, 500);
    chttp_send_json(res, "{\"error\":\"Failed to list disks\"}");
    return;
  }

  /* lsblk returns { "blockdevices": [...] } — forward it directly */
  chttp_set_status(res, 200);
  chttp_send_json(res, buf);
}

/* POST /admin/disks/mount — { device, mountpoint, fstype? } */
void handle_admin_mount_impl(HttpRequest *req, HttpResponse *res) {
  ROOT_GUARD(res);

  cJSON *body = cJSON_ParseWithLength(req->body, req->body_len);
  if (!body) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid JSON\"}");
    return;
  }

  const char *device = cJSON_GetStringValue(cJSON_GetObjectItem(body, "device"));
  const char *mountpoint = cJSON_GetStringValue(cJSON_GetObjectItem(body, "mountpoint"));
  const char *fstype = cJSON_GetStringValue(cJSON_GetObjectItem(body, "fstype"));

  if (!device || !valid_path(device)) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid device path\"}");
    cJSON_Delete(body);
    return;
  }
  if (!mountpoint || !valid_path(mountpoint)) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid mountpoint\"}");
    cJSON_Delete(body);
    return;
  }

  int rc;
  if (fstype && *fstype) {
    char *argv[] = {"mount", "-t", (char *)fstype, (char *)device, (char *)mountpoint, NULL};
    rc = run_command(argv);
  } else {
    char *argv[] = {"mount", (char *)device, (char *)mountpoint, NULL};
    rc = run_command(argv);
  }

  if (rc != 0) {
    chttp_set_status(res, 500);
    chttp_send_json(res, "{\"error\":\"Mount failed\"}");
  } else {
    chttp_set_status(res, 200);
    chttp_send_json(res, "{\"message\":\"Mounted successfully\"}");
  }
  cJSON_Delete(body);
}

/* POST /admin/disks/unmount — { mountpoint } */
void handle_admin_unmount_impl(HttpRequest *req, HttpResponse *res) {
  ROOT_GUARD(res);

  cJSON *body = cJSON_ParseWithLength(req->body, req->body_len);
  if (!body) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid JSON\"}");
    return;
  }

  const char *mountpoint = cJSON_GetStringValue(cJSON_GetObjectItem(body, "mountpoint"));
  if (!mountpoint || !valid_path(mountpoint)) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid mountpoint\"}");
    cJSON_Delete(body);
    return;
  }

  char *argv[] = {"umount", (char *)mountpoint, NULL};
  int rc = run_command(argv);

  if (rc != 0) {
    chttp_set_status(res, 500);
    chttp_send_json(res, "{\"error\":\"Unmount failed\"}");
  } else {
    chttp_set_status(res, 200);
    chttp_send_json(res, "{\"message\":\"Unmounted successfully\"}");
  }
  cJSON_Delete(body);
}

/* POST /admin/disks/format — { device, fstype } */
void handle_admin_format_impl(HttpRequest *req, HttpResponse *res) {
  ROOT_GUARD(res);

  cJSON *body = cJSON_ParseWithLength(req->body, req->body_len);
  if (!body) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid JSON\"}");
    return;
  }

  const char *device = cJSON_GetStringValue(cJSON_GetObjectItem(body, "device"));
  const char *fstype = cJSON_GetStringValue(cJSON_GetObjectItem(body, "fstype"));

  if (!device || !valid_path(device)) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid device path\"}");
    cJSON_Delete(body);
    return;
  }
  if (!fstype || !*fstype) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Filesystem type required\"}");
    cJSON_Delete(body);
    return;
  }

  /* Build mkfs.<fstype> command */
  char cmd[64];
  snprintf(cmd, sizeof(cmd), "mkfs.%s", fstype);

  char *argv[] = {cmd, (char *)device, NULL};
  int rc = run_command(argv);

  if (rc != 0) {
    chttp_set_status(res, 500);
    chttp_send_json(res, "{\"error\":\"Format failed\"}");
  } else {
    chttp_set_status(res, 200);
    chttp_send_json(res, "{\"message\":\"Formatted successfully\"}");
  }
  cJSON_Delete(body);
}
