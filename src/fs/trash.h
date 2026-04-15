#pragma once
#include "chttp.h"

/* Move a file or directory to ~/.imaginary/trash/.
 * path is relative to the user's home (cwd).
 * Returns 0 on success, -1 on error (errno is set). */
int trash_item(const char *path);

void handle_trash_list_impl(HttpRequest *req, HttpResponse *res);
void handle_trash_restore_impl(HttpRequest *req, HttpResponse *res);
void handle_trash_delete_impl(HttpRequest *req, HttpResponse *res);
void handle_trash_empty_impl(HttpRequest *req, HttpResponse *res);
