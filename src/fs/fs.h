#pragma once
#include "chttp.h"

void handle_fs_list_impl(HttpRequest *req, HttpResponse *res);
void handle_fs_upload_impl(HttpRequest *req, HttpResponse *res);
void handle_fs_download_impl(HttpRequest *req, HttpResponse *res);
void handle_fs_delete_file_impl(HttpRequest *req, HttpResponse *res);
void handle_fs_mkdir_impl(HttpRequest *req, HttpResponse *res);
void handle_fs_rmdir_impl(HttpRequest *req, HttpResponse *res);
void handle_fs_rename_impl(HttpRequest *req, HttpResponse *res);
void handle_fs_move_impl(HttpRequest *req, HttpResponse *res);
void handle_fs_copy_impl(HttpRequest *req, HttpResponse *res);
void handle_fs_stat_impl(HttpRequest *req, HttpResponse *res);
void handle_fs_read_impl(HttpRequest *req, HttpResponse *res);
void handle_fs_write_impl(HttpRequest *req, HttpResponse *res);
void handle_fs_stream_upload_impl(HttpRequest *req, HttpResponse *res);
