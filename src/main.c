#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>


#include "server.h"
#include "const.h"

struct Server *server;

void stop_program() {
    stop_free_server(server);
}

int main() {

    signal(SIGINT, stop_program);

    server = create_server(HOST, PORT, MAX_CONNECTIONS);
    start_server(server);


    return EXIT_SUCCESS;
}