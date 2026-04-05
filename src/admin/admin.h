#pragma once
#include "chttp.h"

/* User management */
void handle_admin_list_users_impl(HttpRequest *req, HttpResponse *res);
void handle_admin_create_user_impl(HttpRequest *req, HttpResponse *res);
void handle_admin_edit_user_impl(HttpRequest *req, HttpResponse *res);
void handle_admin_delete_user_impl(HttpRequest *req, HttpResponse *res);

/* Disk management */
void handle_admin_list_disks_impl(HttpRequest *req, HttpResponse *res);
void handle_admin_mount_impl(HttpRequest *req, HttpResponse *res);
void handle_admin_unmount_impl(HttpRequest *req, HttpResponse *res);
void handle_admin_format_impl(HttpRequest *req, HttpResponse *res);
