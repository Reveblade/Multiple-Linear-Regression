#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT_NUMBER 60000
#define BUFFER_SIZE 1024

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    char buffer[BUFFER_SIZE];
    int opt = 1;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT_NUMBER);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 1) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    while (1) {
        client_len = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept failed");
            continue;
        }

        if (send(client_fd, "WELCOME TO PRICE PREDICTION SERVER\r\n", 37, 0) < 0) {
            perror("send failed");
            close(client_fd);
            continue;
        }

        if (send(client_fd, "Enter CSV file name to load:\r\n", 32, 0) < 0) {
            perror("send failed");
            close(client_fd);
            continue;
        }

        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received < 0) {
            perror("recv failed");
            close(client_fd);
            continue;
        }

        buffer[bytes_received] = '\0';
        char *newline = strchr(buffer, '\n');
        if (newline) {
            *newline = '\0';
        }
        char *carriage = strchr(buffer, '\r');
        if (carriage) {
            *carriage = '\0';
        }

        printf("%s\n", buffer);

        handle_client(client_fd, buffer);

        if (send(client_fd, "Closing connection...\n", 22, 0) < 0) {
            perror("send failed");
            close(client_fd);
            continue;
        }

        close(client_fd);
    }

    close(server_fd);
    return 0;
}
