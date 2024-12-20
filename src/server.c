
#include "server.h"

struct Server* create_server(char *address, uint16_t port, uint32_t max_connections) {
    struct Server *server = calloc(1, sizeof(struct Server));

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        fprintf(stderr, "Error creating socket");
        exit(EXIT_FAILURE);
    }

    server->socket_fd = server_fd;
    server->config.sin_family = AF_INET;
    server->config.sin_port = htons(port);
    server->config.sin_addr.s_addr = inet_addr(address);
    server->max_connections = max_connections;

    if (bind(server_fd, (struct sockaddr*) &(server->config), sizeof(server->config)) == -1) {
        fprintf(stderr, "Bind failed");
        exit(EXIT_FAILURE);
    }

    printf("Server bind to %s\n", address);

    return server;
}

void start_server(struct Server *server) {
    if (listen(server->socket_fd, server->max_connections) == -1) {
        fprintf(stderr, "Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", ntohs(server->config.sin_port));

    while (1) {
        int client_fd = accept(server->socket_fd, NULL, NULL);
        if (client_fd == -1) {
            fprintf(stderr, "Accept failed");
            exit(EXIT_FAILURE);
        }

        printf("Wait for the client %d to send message\n", client_fd);

        int32_t status = contect(client_fd);
        if (status == -1) {
            fprintf(stderr, "Contect fail");
        } else if(status == 0)  {
            printf("Client disconnected\n");
        }
        close(client_fd);
    }
}

void stop_free_server(struct Server *server) {
    if (shutdown(server->socket_fd, SHUT_RDWR) == -1) {
        close(server->socket_fd);
        fprintf(stderr, "Error closing socket");
        exit(EXIT_FAILURE);
    }
    close(server->socket_fd);
    free(server);
}