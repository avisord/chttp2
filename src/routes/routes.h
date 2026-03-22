#pragma once
#include "chttp.h"

void handle_root(HttpRequest *req, HttpResponse *res);
void handle_hello(HttpRequest *req, HttpResponse *res);
void handle_echo(HttpRequest *req, HttpResponse *res);
void handle_get_user(HttpRequest *req, HttpResponse *res);
void handle_create_user(HttpRequest *req, HttpResponse *res);
void handle_login(HttpRequest *req, HttpResponse *res);
void handle_upload(HttpRequest *req, HttpResponse *res);
void handle_download(HttpRequest *req, HttpResponse *res);
void handle_fmetadata(HttpRequest *req, HttpResponse *res);
void handle_socket(CConn *conn);
void handle_sse(CConn *conn);
void handle_whoami_impl(HttpRequest *req, HttpResponse *res);
