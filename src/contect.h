#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "const.h"
#include "http.h"
#include "contect.h"

int32_t contect(int32_t client_fd);
