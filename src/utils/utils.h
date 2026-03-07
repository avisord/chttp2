#pragma once
#include "chttp.h"

const char *mime_from_ext(const char *filename);
int safe_filename(const char *name);
int safe_path(const char *path);
void fs_error(HttpResponse *res, int err);
int parse_multipart(HttpRequest *req, HttpResponse *res,
                    char **file_data_out, size_t *file_size_out,
                    char filename_out[256], char part_ct_out[128]);
