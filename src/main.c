#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "auth/auth.h"
#include "fs/fs.h"
#include "routes/routes.h"

int g_server_fd = -1;

/* Auth wrappers — each generates a static RouteHandler that validates the
 * session cookie then calls fork_and_run() with the _impl function. */
DEFINE_AUTH_ROUTE(handle_whoami,        handle_whoami_impl)
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
  if (getuid() != 0) {
    fprintf(stderr, "error: server must be run as root (use sudo)\n");
    return 1;
  }

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
