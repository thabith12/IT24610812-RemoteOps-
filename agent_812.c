#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 9461
#define BUFFER_SIZE 1024

#define AUTH_TOKEN "OPS-0812"
#define SID "2180"

int main() {

    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    char buffer[BUFFER_SIZE];

    /* 1. Create TCP socket */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    /* 2. Configure server address */
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    /* 3. Bind socket to port */
    if (bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {

        perror("bind");
        close(server_fd);
        return 1;
    }

    /* 4. Start listening */
    if (listen(server_fd, 5) < 0) {

        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("RemoteOps Agent started.\n");
    printf("Listening on TCP port %d...\n", PORT);

    /* 5. Accept Controller connections */
    while (1) {

        client_len = sizeof(client_addr);

        client_fd = accept(
            server_fd,
            (struct sockaddr *)&client_addr,
            &client_len
        );

        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        printf("Controller connected.\n");

        int authenticated = 0;

        /* 6. Receive command */
        memset(buffer, 0, BUFFER_SIZE);

        int bytes_received = recv(
            client_fd,
            buffer,
            BUFFER_SIZE - 1,
            0
        );

        if (bytes_received <= 0) {
            close(client_fd);
            printf("Controller disconnected.\n");
            continue;
        }

        buffer[bytes_received] = '\0';

        /* Remove newline */
        buffer[strcspn(buffer, "\r\n")] = '\0';

        printf("Received: %s\n", buffer);

        /* 7. Check AUTH command */
        if (strcmp(buffer, "AUTH OPS-0812") == 0) {

            authenticated = 1;

            const char *response =
                "OK AUTHENTICATED SID:2180\n";

            send(
                client_fd,
                response,
                strlen(response),
                0
            );

            printf("Controller authenticated.\n");
        }
        else {

            const char *response =
                "ERR 001 AUTH_FAILED SID:2180\n";

            send(
                client_fd,
                response,
                strlen(response),
                0
            );

            printf("Authentication failed.\n");
        }

        /* 8. Show authentication state */
        if (authenticated) {
            printf("Authentication state: AUTHENTICATED\n");
        } else {
            printf("Authentication state: NOT AUTHENTICATED\n");
        }

        /* 9. Close connection for this basic authentication test */
        close(client_fd);

        printf("Controller disconnected.\n");
    }

    close(server_fd);

    return 0;
}
