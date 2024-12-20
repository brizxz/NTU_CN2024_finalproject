#include "http.h"

char* strdup(const char *s) {
    char *d = malloc(strlen(s) + 1);
    if (d == NULL) {
        return NULL;
    }
    strcpy(d, s);
    return d;
}


char* respond_http_to_string(RespondHttp *respond_http) {
    char *respond_http_string = (char*)calloc(BUFFER_SIZE, sizeof(char));
    sprintf(respond_http_string, "%s %s %s\r\n", respond_http->protocol, respond_http->status_code, respond_http->status_text);
    HttpHeader *header = respond_http->headers;
    while (header != NULL) {
        sprintf(respond_http_string, "%s%s: %s\r\n", respond_http_string, header->key, header->value);
        header = header->next;
    }
    if (respond_http->body != NULL) {
        sprintf(respond_http_string, "%s\r\n%s", respond_http_string, respond_http->body);
    }
    return respond_http_string;
}

char* request_http_to_string(RequestHttp *request_http) {
    char *request_http_string = (char*)calloc(BUFFER_SIZE, sizeof(char));
    sprintf(request_http_string, "%s %s %s\r\n", request_http->method, request_http->path, request_http->protocol);
    HttpHeader *header = request_http->headers;
    while (header != NULL) {
        sprintf(request_http_string, "%s%s: %s\r\n", request_http_string, header->key, header->value);
        header = header->next;
    }
    if (request_http->body != NULL) {
        sprintf(request_http_string, "%s\r\n%s", request_http_string, request_http->body);
    }
    sprintf(request_http_string, "%s\r\n%s", request_http_string, request_http->body);
    return request_http_string;
}

RespondHttp* respond_http_from_string(char *respond_http_string) {
    if (respond_http_string == NULL) {
        return NULL;
    }
    
    char *header_body_spilt = strstr(respond_http_string, "\r\n\r\n");
    char *header_token = respond_http_string;
    char *body_token = NULL;

    if (header_body_spilt != NULL) {
        for (size_t i = 0; i < 4; i++) {
            header_body_spilt[i] = '\0';
        }
        body_token = header_body_spilt + 4;
    }

    RespondHttp *respond_http = (RespondHttp*)calloc(1, sizeof(RespondHttp));

    char *token = strtok(header_token, " ");
    if (token == NULL) {
        free_respond_http(respond_http);
        return NULL;
    }
    respond_http->protocol = strdup(token);

    token = strtok(NULL, " ");
    if (token == NULL) {
        free_respond_http(respond_http);
        return NULL;
    }
    respond_http->status_code = strdup(token);

    token = strtok(NULL, "\r\n");
    if (token == NULL) {
        free_respond_http(respond_http);
        return NULL;
    }
    respond_http->status_text = strdup(token);

    while (1) {
        token = strtok(NULL, "\n");
        if (token == NULL) {
            break;
        }
        HttpHeader *header = (HttpHeader*)calloc(1, sizeof(HttpHeader));

        char *split = strstr(token, ": ");
        if (split == NULL) {
            free_respond_http(respond_http);
            return NULL;
        }
        split[0] = '\0';

        header->key = strdup(token);

        token += strlen(token) + 2;
        header->value = strdup(token);
        
        header->next = respond_http->headers;
        respond_http->headers = header;
    }

    if (body_token != NULL) {
        respond_http->body = strdup(body_token);
    }
    
    return respond_http;
}

RequestHttp* request_http_from_string(char *request_http_string) {
    if (request_http_string == NULL) {
        return NULL;
    }
    
    char *header_body_spilt = strstr(request_http_string, "\r\n\r\n");
    char *header_token = request_http_string;
    char *body_token = NULL;

    if (header_body_spilt != NULL) {
        for (size_t i = 0; i < 4; i++) {
            header_body_spilt[i] = '\0';
        }
        body_token = header_body_spilt + 4;
    }

    RequestHttp *request_http = (RequestHttp*)calloc(1, sizeof(RequestHttp));

    char *token = strtok(header_token, " ");
    if (token == NULL) {
        free_request_http(request_http);
        return NULL;
    }
    request_http->method = strdup(token);

    token = strtok(NULL, " ");
    if (token == NULL) {
        free_request_http(request_http);
        return NULL;
    }
    request_http->path = strdup(token);

    token = strtok(NULL, "\r\n");
    if (token == NULL) {
        free_request_http(request_http);
        return NULL;
    }
    request_http->protocol = strdup(token);

    while (1) {
        token = strtok(NULL, "\n");
        if (token == NULL) {
            break;
        }
        HttpHeader *header = (HttpHeader*)calloc(1, sizeof(HttpHeader));

        char *split = strstr(token, ": ");
        if (split == NULL) {
            free_request_http(request_http);
            return NULL;
        }
        split[0] = '\0';

        header->key = strdup(token);

        token += strlen(token) + 2;
        header->value = strdup(token);
        
        header->next = request_http->headers;
        request_http->headers = header;
    }

    if (body_token != NULL) {
        request_http->body = strdup(body_token);
    }
    
    return request_http;
}

void free_http_headers(HttpHeader *http_header) {
    while (http_header != NULL) {
        HttpHeader *next = http_header->next;
        free(http_header->key);
        free(http_header->value);
        free(http_header);
        http_header = next;
    }
    return;
}

void free_respond_http(RespondHttp *respond_http) {
    if (respond_http == NULL) {
        return;
    }
    if (respond_http->protocol != NULL) {
        free(respond_http->protocol);
    }
    if (respond_http->status_code != NULL) {
        free(respond_http->status_code);
    }
    if (respond_http->status_text != NULL) {
        free(respond_http->status_text);
    }
    if (respond_http->headers != NULL) {
        free_http_headers(respond_http->headers);
    }
    if (respond_http->body != NULL) {
        free(respond_http->body);
    }
    free(respond_http);
    return;
}

void free_request_http(RequestHttp *request_http) {
    if (request_http == NULL) {
        return;
    }
    if (request_http->method != NULL) {
        free(request_http->method);
    }
    if (request_http->path != NULL) {
        free(request_http->path);
    }
    if (request_http->protocol != NULL) {
        free(request_http->protocol);
    }
    if (request_http->headers != NULL) {
        free_http_headers(request_http->headers);
    }
    if (request_http->body != NULL) {
        free(request_http->body);
    }
    free(request_http);
    return;
}