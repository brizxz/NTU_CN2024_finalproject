#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "../const.h"

int main()
{
    while (1) {
        int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_fd == -1) {
            perror("Socket creation error");
            exit(1);
        }

        struct sockaddr_in serv_name;
        serv_name.sin_family = AF_INET;
        inet_aton(HOST, &serv_name.sin_addr);
        serv_name.sin_port = htons(PORT);

        int status = connect(sock_fd, (struct sockaddr *)&serv_name, sizeof(serv_name));
        if (status == -1) {
            perror("Connection error");
            exit(1);
        }

    
        char *message = calloc(BUFFER_SIZE, sizeof(char));
        char *read_buffer = calloc(BUFFER_SIZE, sizeof(char));
        printf("please input message: ");
        fgets(message, BUFFER_SIZE, stdin);

        if(!strcmp(message, "exit\n")) {
            close(sock_fd);
            printf("EXIT.\n");
            exit(EXIT_SUCCESS);
        }

        char *send_buffer = calloc(BUFFER_SIZE, sizeof(char));
        sprintf(send_buffer, "POST / HTTP/1.1\r\nHost: localhost:%d\r\n\r\n%s", PORT, message);
        free(message);

        send(sock_fd, send_buffer, BUFFER_SIZE, 0);
        free(send_buffer);
        
        if (recv(sock_fd, read_buffer, BUFFER_SIZE, 0) == -1) {
            close(sock_fd);
            free(read_buffer);  
            printf("server closed connection.\n");
            break;
        }
        close(sock_fd);
        printf("Received:\n%s\n", read_buffer);
        free(read_buffer);  
        
    }

    return 0;
}