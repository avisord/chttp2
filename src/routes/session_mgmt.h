#ifndef SESSION_MGMT_H
#define SESSION_MGMT_H
#include "chttp.h"

void handle_get_sessions_impl(HttpRequest *req, HttpResponse *res);
void handle_delete_session_impl(HttpRequest *req, HttpResponse *res);
void handle_logout_impl(HttpRequest *req, HttpResponse *res);
void handle_switch_session_impl(HttpRequest *req, HttpResponse *res);

#endif
