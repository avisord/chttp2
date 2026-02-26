#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chttp.h"

/* GET / */
static void handle_root(HttpRequest *req, HttpResponse *res) {
    (void)req;
    chttp_send_text(res, "Welcome to chttp!");
}

/* GET /hello */
static void handle_hello(HttpRequest *req, HttpResponse *res) {
    (void)req;
    chttp_send_text(res, "Hello, World!");
}

/* GET /users/:id */
static void handle_get_user(HttpRequest *req, HttpResponse *res) {
    const char *id = chttp_path_param(req, "id");
    if (!id) {
        chttp_set_status(res, 400);
        chttp_send_text(res, "Missing user id");
        return;
    }

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "id",   id);
    cJSON_AddStringToObject(obj, "name", "Alice");
    chttp_send_cjson(res, obj);
    cJSON_Delete(obj);
}

/* POST /users — expects JSON body {"name":"..."} */
static void handle_create_user(HttpRequest *req, HttpResponse *res) {
    if (!req->body || req->body_len == 0) {
        chttp_set_status(res, 400);
        chttp_send_text(res, "Empty body");
        return;
    }

    cJSON *body = cJSON_ParseWithLength(req->body, req->body_len);
    if (!body) {
        chttp_set_status(res, 400);
        chttp_send_text(res, "Invalid JSON");
        return;
    }

    cJSON *name_item = cJSON_GetObjectItem(body, "name");
    const char *name = (name_item && cJSON_IsString(name_item))
                       ? name_item->valuestring : "Unknown";

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "created");
    cJSON_AddStringToObject(resp, "name",   name);

    chttp_set_status(res, 201);
    chttp_send_cjson(res, resp);

    cJSON_Delete(body);
    cJSON_Delete(resp);
}

/* GET /echo?msg=... */
static void handle_echo(HttpRequest *req, HttpResponse *res) {
    const char *msg = chttp_query_param(req, "msg");
    if (!msg) {
        chttp_send_text(res, "(no msg query param)");
        return;
    }
    chttp_send_text(res, msg);
}

int main(void) {
    HttpServer srv;

    if (chttp_server_init(&srv, 8000) < 0)
        return 1;

    CHTTP_GET(&srv,  "/",          handle_root);
    CHTTP_GET(&srv,  "/hello",     handle_hello);
    CHTTP_GET(&srv,  "/users/:id", handle_get_user);
    CHTTP_POST(&srv, "/users",     handle_create_user);
    CHTTP_GET(&srv,  "/echo",      handle_echo);

    chttp_server_run(&srv);

    chttp_server_destroy(&srv);
    return 0;
}
