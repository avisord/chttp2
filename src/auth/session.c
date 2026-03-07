#include <ctype.h>
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

int parse_session_cookie(const char *cookie_hdr, char out[65]) {
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

int validate_session(const char *session_id, char *username_out,
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
