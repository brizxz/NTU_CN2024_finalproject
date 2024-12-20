#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "const.h"
#include "contect.h"
typedef struct Server{
    struct sockaddr_in config;
    uint32_t socket_fd;
    uint32_t max_connections;
} Server;

struct Server* create_server(char *address, uint16_t port, uint32_t max_connections);
void start_server(struct Server *server);
void stop_free_server(struct Server *server);

