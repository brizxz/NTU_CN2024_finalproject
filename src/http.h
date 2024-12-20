#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "const.h"

typedef struct HttpHeader {
    char *key;
    char *value;
    struct HttpHeader *next;
} HttpHeader;

typedef struct RespondHttp {
    char *protocol;
    char *status_code;
    char *status_text;
    HttpHeader *headers;
    char *body;
} RespondHttp;

typedef struct RequestHttp {
    char *method;
    char *path;
    char *protocol;
    HttpHeader *headers;
    char *body;
} RequestHttp;

char* respond_http_to_string(RespondHttp *respond_http);
char* request_http_to_string(RequestHttp *request_http);
RespondHttp* respond_http_from_string(char *respond_http_string);
RequestHttp* request_http_from_string(char *request_http_string);
void free_http_headers(HttpHeader *http_header);
void free_respond_http(RespondHttp *respond_http);
void free_request_http(RequestHttp *request_http);
