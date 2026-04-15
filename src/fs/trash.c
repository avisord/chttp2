#define _XOPEN_SOURCE 500 /* nftw */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "cJSON.h"
#include "trash.h"
#include "utils/utils.h"

#define TRASH_BASE    ".imaginary/trash"
#define TRASH_FILES   TRASH_BASE "/files"
#define TRASH_INFO    TRASH_BASE "/info"

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                   */
/* ------------------------------------------------------------------ */

/* mkdir -p for a path relative to cwd. */
static int mkdirp(const char *path, mode_t mode) {
  char tmp[1024];
  snprintf(tmp, sizeof(tmp), "%s", path);
  for (char *p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -1;
      *p = '/';
    }
  }
  return mkdir(tmp, mode) != 0 && errno != EEXIST ? -1 : 0;
}

/* Ensure the trash directory structure exists. */
static int ensure_trash_dirs(void) {
  if (mkdirp(TRASH_FILES, 0700) != 0) return -1;
  if (mkdirp(TRASH_INFO, 0700) != 0) return -1;
  return 0;
}

/* Extract basename from a path (pointer into the original string). */
static const char *path_basename(const char *path) {
  const char *slash = strrchr(path, '/');
  return slash ? slash + 1 : path;
}

/* Write the .info sidecar file. */
static int write_info_file(const char *trash_name, const char *original_path) {
  char info_path[1024];
  snprintf(info_path, sizeof(info_path), "%s/%s.info", TRASH_INFO, trash_name);

  FILE *f = fopen(info_path, "w");
  if (!f) return -1;

  time_t now = time(NULL);
  struct tm tm;
  gmtime_r(&now, &tm);
  char timebuf[32];
  strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%SZ", &tm);

  fprintf(f, "path=%s\ndeleted_at=%s\n", original_path, timebuf);
  fclose(f);
  return 0;
}

/* Read the .info sidecar file. Returns 0 on success. */
static int read_info_file(const char *trash_name,
                          char *original_path, size_t path_size,
                          char *deleted_at, size_t time_size) {
  char info_path[1024];
  snprintf(info_path, sizeof(info_path), "%s/%s.info", TRASH_INFO, trash_name);

  FILE *f = fopen(info_path, "r");
  if (!f) return -1;

  original_path[0] = '\0';
  deleted_at[0] = '\0';

  char line[1024];
  while (fgets(line, sizeof(line), f)) {
    /* strip newline */
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';

    if (strncmp(line, "path=", 5) == 0) {
      snprintf(original_path, path_size, "%s", line + 5);
    } else if (strncmp(line, "deleted_at=", 11) == 0) {
      snprintf(deleted_at, time_size, "%s", line + 11);
    }
  }
  fclose(f);
  return (original_path[0] && deleted_at[0]) ? 0 : -1;
}

/* nftw callback for recursive removal. */
static int nftw_remove_cb(const char *fpath, const struct stat *sb,
                          int typeflag, struct FTW *ftwbuf) {
  (void)sb; (void)ftwbuf;
  if (typeflag == FTW_DP || typeflag == FTW_D)
    return rmdir(fpath);
  return unlink(fpath);
}

/* Recursively remove a file or directory. */
static int remove_recursive(const char *path) {
  struct stat st;
  if (lstat(path, &st) != 0) return -1;
  if (S_ISDIR(st.st_mode))
    return nftw(path, nftw_remove_cb, 64, FTW_DEPTH | FTW_PHYS);
  return unlink(path);
}

/* ------------------------------------------------------------------ */
/*  trash_item() — public helper called from fs.c delete handlers      */
/* ------------------------------------------------------------------ */

int trash_item(const char *path) {
  if (ensure_trash_dirs() != 0) return -1;

  const char *base = path_basename(path);
  if (!base || !*base) { errno = EINVAL; return -1; }

  /* Build destination path, handling name collisions. */
  char dest[1024];
  snprintf(dest, sizeof(dest), "%s/%s", TRASH_FILES, base);

  char trash_name[512];
  snprintf(trash_name, sizeof(trash_name), "%s", base);

  struct stat st;
  if (lstat(dest, &st) == 0) {
    /* Collision — try .1, .2, ... */
    for (int i = 1; i < 10000; i++) {
      snprintf(trash_name, sizeof(trash_name), "%s.%d", base, i);
      snprintf(dest, sizeof(dest), "%s/%s", TRASH_FILES, trash_name);
      if (lstat(dest, &st) != 0 && errno == ENOENT) break;
    }
  }

  if (rename(path, dest) != 0) return -1;
  if (write_info_file(trash_name, path) != 0) {
    /* Best-effort: try to undo the rename if info write fails. */
    rename(dest, path);
    return -1;
  }
  return 0;
}

/* ------------------------------------------------------------------ */
/*  GET /trash/list                                                    */
/* ------------------------------------------------------------------ */

void handle_trash_list_impl(HttpRequest *req, HttpResponse *res) {
  (void)req;
  cJSON *arr = cJSON_CreateArray();

  DIR *d = opendir(TRASH_FILES);
  if (!d) {
    /* Trash doesn't exist yet — return empty list. */
    chttp_send_cjson(res, arr);
    cJSON_Delete(arr);
    return;
  }

  struct dirent *ent;
  while ((ent = readdir(d)) != NULL) {
    if (ent->d_name[0] == '.' &&
        (ent->d_name[1] == '\0' ||
         (ent->d_name[1] == '.' && ent->d_name[2] == '\0')))
      continue;

    char original_path[1024], deleted_at[64];
    if (read_info_file(ent->d_name, original_path, sizeof(original_path),
                       deleted_at, sizeof(deleted_at)) != 0)
      continue; /* skip entries without valid .info */

    /* stat the trashed file/dir */
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s/%s", TRASH_FILES, ent->d_name);
    struct stat st;
    if (lstat(full_path, &st) != 0) continue;

    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "name", ent->d_name);
    cJSON_AddStringToObject(item, "original_path", original_path);
    cJSON_AddStringToObject(item, "deleted_at", deleted_at);
    cJSON_AddStringToObject(item, "type", S_ISDIR(st.st_mode) ? "dir" : "file");
    cJSON_AddNumberToObject(item, "size", (double)st.st_size);
    cJSON_AddItemToArray(arr, item);
  }
  closedir(d);

  chttp_send_cjson(res, arr);
  cJSON_Delete(arr);
}

/* ------------------------------------------------------------------ */
/*  POST /trash/restore   body: {"name":"filename"}                    */
/* ------------------------------------------------------------------ */

void handle_trash_restore_impl(HttpRequest *req, HttpResponse *res) {
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
  cJSON *j = cJSON_GetObjectItem(json, "name");
  if (!cJSON_IsString(j) || !safe_filename(j->valuestring)) {
    cJSON_Delete(json);
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid name\",\"errno\":0}");
    return;
  }
  const char *name = j->valuestring;

  /* Read original path from .info */
  char original_path[1024], deleted_at[64];
  if (read_info_file(name, original_path, sizeof(original_path),
                     deleted_at, sizeof(deleted_at)) != 0) {
    cJSON_Delete(json);
    chttp_set_status(res, 404);
    chttp_send_json(res, "{\"error\":\"Trash entry not found\",\"errno\":0}");
    return;
  }

  /* Ensure parent directory exists (mkdir -p). */
  char parent[1024];
  snprintf(parent, sizeof(parent), "%s", original_path);
  char *last_slash = strrchr(parent, '/');
  if (last_slash && last_slash != parent) {
    *last_slash = '\0';
    mkdirp(parent, 0755);
  }

  /* Move back from trash. */
  char trash_path[1024];
  snprintf(trash_path, sizeof(trash_path), "%s/%s", TRASH_FILES, name);
  if (rename(trash_path, original_path) != 0) {
    int e = errno;
    cJSON_Delete(json);
    fs_error(res, e);
    return;
  }

  /* Remove .info file. */
  char info_path[1024];
  snprintf(info_path, sizeof(info_path), "%s/%s.info", TRASH_INFO, name);
  unlink(info_path);

  cJSON_Delete(json);
  chttp_send_json(res, "{\"message\":\"restored\"}");
}

/* ------------------------------------------------------------------ */
/*  DELETE /trash/:name                                                */
/* ------------------------------------------------------------------ */

void handle_trash_delete_impl(HttpRequest *req, HttpResponse *res) {
  const char *name = chttp_path_param(req, "name");
  if (!name || !safe_filename(name)) {
    chttp_set_status(res, 400);
    chttp_send_json(res, "{\"error\":\"Invalid name\",\"errno\":0}");
    return;
  }

  char trash_path[1024];
  snprintf(trash_path, sizeof(trash_path), "%s/%s", TRASH_FILES, name);
  if (remove_recursive(trash_path) != 0) {
    fs_error(res, errno);
    return;
  }

  /* Remove .info file. */
  char info_path[1024];
  snprintf(info_path, sizeof(info_path), "%s/%s.info", TRASH_INFO, name);
  unlink(info_path);

  chttp_send_json(res, "{\"message\":\"deleted\"}");
}

/* ------------------------------------------------------------------ */
/*  DELETE /trash                                                      */
/* ------------------------------------------------------------------ */

void handle_trash_empty_impl(HttpRequest *req, HttpResponse *res) {
  (void)req;
  int errors = 0;

  /* Remove all entries in trash/files/ */
  DIR *d = opendir(TRASH_FILES);
  if (d) {
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
      if (ent->d_name[0] == '.' &&
          (ent->d_name[1] == '\0' ||
           (ent->d_name[1] == '.' && ent->d_name[2] == '\0')))
        continue;
      char path[1024];
      snprintf(path, sizeof(path), "%s/%s", TRASH_FILES, ent->d_name);
      if (remove_recursive(path) != 0) errors++;
    }
    closedir(d);
  }

  /* Remove all .info files */
  d = opendir(TRASH_INFO);
  if (d) {
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
      if (ent->d_name[0] == '.' &&
          (ent->d_name[1] == '\0' ||
           (ent->d_name[1] == '.' && ent->d_name[2] == '\0')))
        continue;
      char path[1024];
      snprintf(path, sizeof(path), "%s/%s", TRASH_INFO, ent->d_name);
      unlink(path);
    }
    closedir(d);
  }

  if (errors) {
    chttp_set_status(res, 500);
    chttp_send_json(res, "{\"error\":\"Some items could not be deleted\",\"errno\":0}");
    return;
  }
  chttp_send_json(res, "{\"message\":\"trash emptied\"}");
}
