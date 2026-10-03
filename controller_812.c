#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "127.0.0.1"
#define PORT 9461
#define BUFFER_SIZE 1024

int main() {

    int sock_fd;
    struct sockaddr_in server_addr;

    char buffer[BUFFER_SIZE];

    /* 1. Create TCP socket */
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (sock_fd < 0) {
        perror("socket");
        return 1;
    }

    /* 2. Configure server address */
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    /* 3. Convert IP address */
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {

        perror("inet_pton");
        close(sock_fd);
        return 1;
    }

    /* 4. Connect to Agent */
    if (connect(sock_fd,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0) {

        perror("connect");
        close(sock_fd);
        return 1;
    }

    printf("Connected to RemoteOps Agent.\n");
    printf("Agent: %s:%d\n", SERVER_IP, PORT);

    /* 5. Send test message */
    const char *message = "hello\n";

    if (send(sock_fd,
             message,
             strlen(message),
             0) < 0) {

        perror("send");
        close(sock_fd);
        return 1;
    }

    /* 6. Receive response */
    memset(buffer, 0, BUFFER_SIZE);

    int bytes_received = recv(
        sock_fd,
        buffer,
        BUFFER_SIZE - 1,
        0
    );

    if (bytes_received < 0) {

        perror("recv");
        close(sock_fd);
        return 1;
    }

    buffer[bytes_received] = '\0';

    printf("Agent response: %s", buffer);

    /* 7. Close connection */
    close(sock_fd);

    printf("Disconnected from Agent.\n");

    return 0;
}
