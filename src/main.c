#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "admin/admin.h"
#include "auth/auth.h"
#include "fs/fs.h"
#include "fs/trash.h"
#include "routes/routes.h"
#include "routes/session_mgmt.h"
#include "routes/static.h"

int g_server_fd = -1;

/* Auth wrappers — each generates a static RouteHandler that validates the
 * active_session cookie then calls fork_and_run() with the _impl function. */
DEFINE_AUTH_ROUTE(handle_whoami,        handle_whoami_impl)

/* Logout needs active_session to know which session to remove. */
DEFINE_NOPRIV_AUTH_ROUTE(handle_logout, handle_logout_impl)

DEFINE_AUTH_ROUTE(handle_fs_list,        handle_fs_list_impl)
DEFINE_AUTH_ROUTE(handle_fs_upload,      handle_fs_upload_impl)
DEFINE_STREAM_AUTH_ROUTE(handle_fs_download, handle_fs_download_impl)
DEFINE_AUTH_ROUTE(handle_fs_delete_file, handle_fs_delete_file_impl)
DEFINE_AUTH_ROUTE(handle_fs_mkdir,       handle_fs_mkdir_impl)
DEFINE_AUTH_ROUTE(handle_fs_rmdir,       handle_fs_rmdir_impl)
DEFINE_AUTH_ROUTE(handle_fs_rename,      handle_fs_rename_impl)
DEFINE_AUTH_ROUTE(handle_fs_move,        handle_fs_move_impl)
DEFINE_AUTH_ROUTE(handle_fs_copy,        handle_fs_copy_impl)
DEFINE_AUTH_ROUTE(handle_fs_stat,        handle_fs_stat_impl)
DEFINE_AUTH_ROUTE(handle_fs_read,          handle_fs_read_impl)
DEFINE_AUTH_ROUTE(handle_fs_write,         handle_fs_write_impl)
DEFINE_STREAM_AUTH_ROUTE(handle_fs_stream_upload, handle_fs_stream_upload_impl)
DEFINE_AUTH_ROUTE(handle_fs_upload_session_create, handle_fs_upload_session_create_impl)
DEFINE_AUTH_ROUTE(handle_fs_upload_session_status, handle_fs_upload_session_status_impl)
DEFINE_STREAM_AUTH_ROUTE(handle_fs_upload_chunk,   handle_fs_upload_chunk_impl)
DEFINE_AUTH_ROUTE(handle_fs_upload_session_abort,  handle_fs_upload_session_abort_impl)

/* Trash management */
DEFINE_AUTH_ROUTE(handle_trash_list,    handle_trash_list_impl)
DEFINE_AUTH_ROUTE(handle_trash_restore, handle_trash_restore_impl)
DEFINE_AUTH_ROUTE(handle_trash_delete,  handle_trash_delete_impl)
DEFINE_AUTH_ROUTE(handle_trash_empty,   handle_trash_empty_impl)

/* Admin management — runs as authenticated user; handlers guard on uid==0 */
DEFINE_AUTH_ROUTE(handle_admin_list_users,  handle_admin_list_users_impl)
DEFINE_AUTH_ROUTE(handle_admin_create_user, handle_admin_create_user_impl)
DEFINE_AUTH_ROUTE(handle_admin_edit_user,   handle_admin_edit_user_impl)
DEFINE_AUTH_ROUTE(handle_admin_delete_user, handle_admin_delete_user_impl)
DEFINE_AUTH_ROUTE(handle_admin_list_disks,  handle_admin_list_disks_impl)
DEFINE_AUTH_ROUTE(handle_admin_mount,       handle_admin_mount_impl)
DEFINE_AUTH_ROUTE(handle_admin_unmount,     handle_admin_unmount_impl)
DEFINE_AUTH_ROUTE(handle_admin_format,      handle_admin_format_impl)

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

  /* Test Routes - not needed anymore */
  //CHTTP_GET(&srv, "/", handle_root);
  //CHTTP_GET(&srv, "/hello", handle_hello);
  //CHTTP_GET(&srv, "/users/:id", handle_get_user);
  //CHTTP_POST(&srv, "/users", handle_create_user);
  //CHTTP_GET(&srv, "/echo", handle_echo);
  //CHTTP_POST(&srv, "/upload", handle_upload);
  //CHTTP_WS(&srv, "/socket", handle_socket);
  //CHTTP_SSE(&srv, "/sse", handle_sse);
  //CHTTP_GET(&srv, "/fdownload/:filename", handle_download);
  //CHTTP_GET(&srv, "/fmetadata/:filename", handle_fmetadata);

  /* Authenticated — runs handler in a forked child under user privileges */
  CHTTP_POST(&srv,   "/login",                          handle_login);
  CHTTP_GET(&srv,    "/whoami",                         handle_whoami);

  /* Session management */
  CHTTP_GET(&srv,    "/sessions",                       handle_get_sessions_impl);
  CHTTP_DELETE(&srv, "/sessions/:session_id",           handle_delete_session_impl);
  CHTTP_DELETE(&srv, "/logout",                         handle_logout);           /* needs active_session */
  CHTTP_POST(&srv,   "/sessions/switch/:session_id",    handle_switch_session_impl);

  /* NAS File Management API — all authenticated */
  CHTTP_GET(&srv,    "/fs/list",     handle_fs_list);
  CHTTP_POST(&srv,   "/fs/upload",   handle_fs_upload);
  CHTTP_STREAM_GET(&srv, "/fs/download", handle_fs_download);
  CHTTP_DELETE(&srv, "/fs/file",     handle_fs_delete_file);
  CHTTP_POST(&srv,   "/fs/mkdir",    handle_fs_mkdir);
  CHTTP_DELETE(&srv, "/fs/dir",      handle_fs_rmdir);
  CHTTP_POST(&srv,   "/fs/rename",   handle_fs_rename);
  CHTTP_POST(&srv,   "/fs/move",     handle_fs_move);
  CHTTP_POST(&srv,   "/fs/copy",     handle_fs_copy);
  CHTTP_GET(&srv,    "/fs/stat",     handle_fs_stat);
  CHTTP_GET(&srv,    "/fs/content",       handle_fs_read);
  CHTTP_PUT(&srv,    "/fs/content",       handle_fs_write);
  CHTTP_STREAM_POST(&srv, "/fs/upload-stream",                 handle_fs_stream_upload);
  CHTTP_POST(&srv,        "/fs/upload-session",               handle_fs_upload_session_create);
  CHTTP_GET(&srv,         "/fs/upload-session/:upload_id",    handle_fs_upload_session_status);
  CHTTP_STREAM_POST(&srv, "/fs/upload-chunk/:upload_id",      handle_fs_upload_chunk);
  CHTTP_DELETE(&srv,      "/fs/upload-session/:upload_id",    handle_fs_upload_session_abort);

  /* Trash management — authenticated */
  CHTTP_GET(&srv,    "/trash/list",     handle_trash_list);
  CHTTP_POST(&srv,   "/trash/restore",  handle_trash_restore);
  CHTTP_DELETE(&srv, "/trash/:name",    handle_trash_delete);
  CHTTP_DELETE(&srv, "/trash",          handle_trash_empty);

  /* Admin management — requires root session */
  CHTTP_GET(&srv,    "/admin/users",            handle_admin_list_users);
  CHTTP_POST(&srv,   "/admin/users",            handle_admin_create_user);
  CHTTP_PUT(&srv,    "/admin/users/:username",   handle_admin_edit_user);
  CHTTP_DELETE(&srv, "/admin/users/:username",   handle_admin_delete_user);
  CHTTP_GET(&srv,    "/admin/disks",            handle_admin_list_disks);
  CHTTP_POST(&srv,   "/admin/disks/mount",      handle_admin_mount);
  CHTTP_POST(&srv,   "/admin/disks/unmount",    handle_admin_unmount);
  CHTTP_POST(&srv,   "/admin/disks/format",     handle_admin_format);

  /* Static file server — public, no auth */
  CHTTP_STREAM_GET(&srv, "/static/*", handle_static);
  CHTTP_HEAD(&srv,       "/static/*", handle_static);
  /* Catch-all: serve www/ for any unmatched GET/HEAD (registered last) */
  CHTTP_STREAM_GET(&srv, "/*", handle_static);
  CHTTP_HEAD(&srv,       "/*", handle_static);

  chttp_server_run(&srv);

  chttp_server_destroy(&srv);
  return 0;
}
