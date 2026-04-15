#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "auth.h"

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

int create_session(const char *username, char session_id_out[65]) {
  char id[65];
  if (generate_session_id(id) < 0) {
    perror("create_session: generate_session_id failed");
    return -1;
  }

  time_t now = time(NULL);
  time_t expires = now + SESSION_EXPIRY_SEC;

  char filepath[256];
  snprintf(filepath, sizeof(filepath), "%s/session_%s", SESSION_DIR, id);

  int fd = open(filepath, O_WRONLY | O_CREAT | O_EXCL, 0644);
  if (fd < 0) {
    fprintf(stderr, "create_session: open(%s) failed: %s\n", filepath, strerror(errno));
    return -1;
  }

  char buf[512];
  int len = snprintf(buf, sizeof(buf),
                     "id=%s\nusername=%s\ncreated_at=%ld\nexpires_at=%ld\nlast_access_time=%ld\n",
                     id, username, (long)now, (long)expires, (long)now);
  ssize_t written = write(fd, buf, len);
  close(fd);
  if (written != len)
    return -1;

  memcpy(session_id_out, id, 65);
  return 0;
}

/* Parse active_session=<token>/<user> from the Cookie header.
 * Returns 1 on success, 0 if not found or malformed. */
int parse_active_session_cookie(const char *cookie_hdr,
                                 char sid_out[65], char user_out[128]) {
  if (!cookie_hdr)
    return 0;
  const char *p = cookie_hdr;
  while (*p) {
    while (*p == ' ') p++;
    if (strncmp(p, "active_session=", 15) == 0) {
      p += 15;
      /* Format: <64-hex-token>/<username> */
      const char *slash = NULL;
      /* The token must be exactly 64 hex chars; look for '/' after them */
      for (int i = 0; i < 64; i++) {
        if (!p[i] || p[i] == ';') return 0;
      }
      if (p[64] != '/') return 0;
      slash = p + 64;
      /* Validate token chars */
      for (int i = 0; i < 64; i++) {
        if (!isxdigit((unsigned char)p[i])) return 0;
      }
      memcpy(sid_out, p, 64);
      sid_out[64] = '\0';
      /* Extract username */
      const char *ustart = slash + 1;
      size_t ulen = 0;
      while (ustart[ulen] && ustart[ulen] != ';' && ustart[ulen] != ' ')
        ulen++;
      if (ulen == 0 || ulen >= 128) return 0;
      memcpy(user_out, ustart, ulen);
      user_out[ulen] = '\0';
      return 1;
    }
    const char *semi = strchr(p, ';');
    if (!semi) break;
    p = semi + 1;
  }
  return 0;
}

int validate_session(const char *session_id, char *username_out,
                     size_t out_size) {
  for (int i = 0; session_id[i]; i++) {
    if (!isxdigit((unsigned char)session_id[i]))
      return -1;
  }
  char filepath[256];
  snprintf(filepath, sizeof(filepath), "%s/session_%s", SESSION_DIR, session_id);

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
  time_t created_at = 0, expires_at = 0;

  char *line = content;
  while (line && *line) {
    char *nl = strchr(line, '\n');
    if (nl) *nl = '\0';
    if (strncmp(line, "username=", 9) == 0)
      strncpy(username, line + 9, sizeof(username) - 1);
    else if (strncmp(line, "created_at=", 11) == 0)
      created_at = (time_t)atol(line + 11);
    else if (strncmp(line, "expires_at=", 11) == 0)
      expires_at = (time_t)atol(line + 11);
    line = nl ? nl + 1 : NULL;
  }

  if (!username[0] || expires_at == 0)
    return -1;
  if (time(NULL) > expires_at)
    return -1;

  /* Update last_access_time by rewriting the session file. */
  time_t now = time(NULL);
  int wfd = open(filepath, O_WRONLY | O_TRUNC);
  if (wfd >= 0) {
    char nbuf[512];
    int nlen = snprintf(nbuf, sizeof(nbuf),
        "id=%s\nusername=%s\ncreated_at=%ld\nexpires_at=%ld\nlast_access_time=%ld\n",
        session_id, username, (long)created_at, (long)expires_at, (long)now);
    write(wfd, nbuf, nlen);
    close(wfd);
  }

  if (username_out)
    strncpy(username_out, username, out_size - 1);
  return 0;
}

int read_session_info(const char *session_id, SessionInfo *info_out) {
  for (int i = 0; session_id[i]; i++) {
    if (!isxdigit((unsigned char)session_id[i]))
      return -1;
  }
  char filepath[256];
  snprintf(filepath, sizeof(filepath), "%s/session_%s", SESSION_DIR, session_id);

  int fd = open(filepath, O_RDONLY);
  if (fd < 0)
    return -1;

  char content[512];
  ssize_t n = read(fd, content, sizeof(content) - 1);
  close(fd);
  if (n <= 0)
    return -1;
  content[n] = '\0';

  memset(info_out, 0, sizeof(*info_out));

  char *line = content;
  while (line && *line) {
    char *nl = strchr(line, '\n');
    if (nl) *nl = '\0';
    if (strncmp(line, "id=", 3) == 0)
      strncpy(info_out->id, line + 3, sizeof(info_out->id) - 1);
    else if (strncmp(line, "username=", 9) == 0)
      strncpy(info_out->username, line + 9, sizeof(info_out->username) - 1);
    else if (strncmp(line, "created_at=", 11) == 0)
      info_out->created_at = (time_t)atol(line + 11);
    else if (strncmp(line, "expires_at=", 11) == 0)
      info_out->expires_at = (time_t)atol(line + 11);
    else if (strncmp(line, "last_access_time=", 17) == 0)
      info_out->last_access_time = (time_t)atol(line + 17);
    line = nl ? nl + 1 : NULL;
  }

  if (!info_out->username[0] || info_out->expires_at == 0)
    return -1;
  return 0;
}

int delete_session_file(const char *session_id) {
  for (int i = 0; session_id[i]; i++) {
    if (!isxdigit((unsigned char)session_id[i]))
      return -1;
  }
  char filepath[256];
  snprintf(filepath, sizeof(filepath), "%s/session_%s", SESSION_DIR, session_id);
  return unlink(filepath);
}

/* Find the session token stored in session_<username>=<token> cookie. */
int find_user_session_id(const char *cookie_hdr, const char *username,
                          char sid_out[65]) {
  if (!cookie_hdr || !username)
    return 0;
  char key[192];
  snprintf(key, sizeof(key), "session_%s=", username);
  const char *p = strstr(cookie_hdr, key);
  if (!p)
    return 0;
  p += strlen(key);
  size_t i = 0;
  while (i < 64 && *p && *p != ';' && *p != ' ')
    sid_out[i++] = *p++;
  sid_out[i] = '\0';
  return (i > 0);
}
