#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>

#define SERVER_IP "127.0.0.1"
#define PORT 9461
#define BUFFER_SIZE 4096
#define AUTH_TOKEN "OPS-0812"
#define AUTH_TOKEN "OPS-0812"
#define MONITOR_UDP_PORT 9462


int send_all(int sock_fd, const void *data, size_t length)
{
    size_t total_sent = 0;

    while (total_sent < length)
    {
        ssize_t n = send(
            sock_fd,
            (const char *)data + total_sent,
            length - total_sent,
            0
        );

        if (n <= 0)
            return -1;

        total_sent += (size_t)n;
    }

    return 0;
}

void receive_monitor_packets(int udp_fd, int count)
{
    char buffer[BUFFER_SIZE];

    struct sockaddr_in sender_addr;
    socklen_t sender_len = sizeof(sender_addr);

    for (int i = 0; i < count; i++)
    {
        ssize_t bytes_received = recvfrom(
            udp_fd,
            buffer,
            sizeof(buffer) - 1,
            0,
            (struct sockaddr *)&sender_addr,
            &sender_len
        );

        if (bytes_received < 0)
        {
            perror("recvfrom");
            return;
        }

        buffer[bytes_received] = '\0';

        printf(
            "UDP Monitor: %s\n",
            buffer
        );
    }
}

/* Receive one complete line */
int receive_line(int sock_fd, char *buffer, size_t size)
{
    size_t total = 0;

    while (total < size - 1)
    {
        char c;

        ssize_t n = recv(
            sock_fd,
            &c,
            1,
            0
        );

        if (n <= 0)
            return -1;

        buffer[total++] = c;

        if (c == '\n')
            break;
    }

    buffer[total] = '\0';

    return 0;
}

/* Receive exact number of bytes */
int receive_exact(
    int sock_fd,
    FILE *file,
    long file_size)
{
    char buffer[BUFFER_SIZE];
    long total_received = 0;

    while (total_received < file_size)
    {
        long remaining =
            file_size - total_received;

        size_t chunk_size = sizeof(buffer);

        if (remaining < (long)chunk_size)
        {
            chunk_size = (size_t)remaining;
        }

        ssize_t n = recv(
            sock_fd,
            buffer,
            chunk_size,
            0
        );

        if (n <= 0)
            return -1;

        fwrite(
            buffer,
            1,
            (size_t)n,
            file
        );

        total_received += n;
    }

    return 0;
}

int main()
{
    int sock_fd;
    struct sockaddr_in server_addr;

    char buffer[BUFFER_SIZE];

    /* Create socket */
    sock_fd = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (sock_fd < 0)
    {
        perror("socket");
        return 1;
    }

    /* Configure server */
    memset(
        &server_addr,
        0,
        sizeof(server_addr)
    );

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(
            AF_INET,
            SERVER_IP,
            &server_addr.sin_addr) <= 0)
    {
        perror("inet_pton");
        close(sock_fd);
        return 1;
    }

    /* Connect */
    if (connect(
            sock_fd,
            (struct sockaddr *)&server_addr,
            sizeof(server_addr)) < 0)
    {
        perror("connect");
        close(sock_fd);
        return 1;
    }

    printf("Connected to RemoteOps Agent.\n");

    /* ================= AUTH ================= */

    const char *auth =
        "AUTH OPS-0812\n";

    send_all(
        sock_fd,
        auth,
        strlen(auth)
    );

    if (receive_line(
            sock_fd,
            buffer,
            sizeof(buffer)) < 0)
    {
        printf("AUTH response failed.\n");
        close(sock_fd);
        return 1;
    }

    printf("Agent: %s", buffer);

    /* ================= GET ================= */

    const char *filename =
        "upload.txt";

    char get_command[BUFFER_SIZE];

    snprintf(
        get_command,
        sizeof(get_command),
        "GET %s\n",
        filename
    );

    printf("Requesting file: %s\n",
           filename);

    send_all(
        sock_fd,
        get_command,
        strlen(get_command)
    );

    /* Receive FILE_SEND response */
    if (receive_line(
            sock_fd,
            buffer,
            sizeof(buffer)) < 0)
    {
        printf("GET response failed.\n");
        close(sock_fd);
        return 1;
    }

    printf("Agent: %s", buffer);

    /* Check FILE_SEND */
    long file_size;

    if (sscanf(
            buffer,
            "OK FILE_SEND %ld",
            &file_size) != 1)
    {
        printf("GET failed.\n");
        close(sock_fd);
        return 1;
    }

    printf(
        "Downloading %s (%ld bytes)...\n",
        filename,
        file_size
    );

    /* Save downloaded file */
    FILE *file = fopen(
        "downloaded_upload.txt",
        "wb"
    );

    if (file == NULL)
    {
        perror("fopen");
        close(sock_fd);
        return 1;
    }

    /* Receive exact bytes */
    if (receive_exact(
            sock_fd,
            file,
            file_size) < 0)
    {
        printf("File download failed.\n");

        fclose(file);
        close(sock_fd);
        return 1;
    }

    fclose(file);

    printf(
        "Downloaded %ld bytes successfully.\n",
        file_size
    );

    /* ================= MONITOR ================= */

    int udp_fd;

    struct sockaddr_in udp_addr;

    udp_fd = socket(
        AF_INET,
        SOCK_DGRAM,
        0
    );

    if (udp_fd < 0)
    {
        perror("UDP socket");
        close(sock_fd);
        return 1;
    }

    memset(
        &udp_addr,
        0,
        sizeof(udp_addr)
    );

    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port = htons(MONITOR_UDP_PORT);

    if (bind(
            udp_fd,
            (struct sockaddr *)&udp_addr,
            sizeof(udp_addr)
        ) < 0)
    {
        perror("UDP bind");
        close(udp_fd);
        close(sock_fd);
        return 1;
    }

    /* Start monitoring */

    const char *monitor_start =
        "MONITOR START\n";

    send_all(
        sock_fd,
        monitor_start,
        strlen(monitor_start)
    );

    if (receive_line(
            sock_fd,
            buffer,
            sizeof(buffer)) < 0)
    {
        printf("MONITOR START response failed.\n");

        close(udp_fd);
        close(sock_fd);

        return 1;
    }

    printf(
        "Agent: %s",
        buffer
    );

    /* Receive 3 UDP monitoring reports */

    printf(
        "\nWaiting for UDP monitoring reports...\n"
    );

    receive_monitor_packets(
        udp_fd,
        3
    );

    /* Stop monitoring */

    const char *monitor_stop =
        "MONITOR STOP\n";

    send_all(
        sock_fd,
        monitor_stop,
        strlen(monitor_stop)
    );

    if (receive_line(
            sock_fd,
            buffer,
            sizeof(buffer)) < 0)
    {
        printf("MONITOR STOP response failed.\n");

        close(udp_fd);
        close(sock_fd);

        return 1;
    }

    printf(
        "Agent: %s",
        buffer
    );

    close(udp_fd);

    /* ================= QUIT ================= */

    const char *quit =
        "QUIT\n";

    send_all(
        sock_fd,
        quit,
        strlen(quit)
    );

    if (receive_line(
            sock_fd,
            buffer,
            sizeof(buffer)) == 0)
    {
        printf("Agent: %s", buffer);
    }

    close(sock_fd);

    printf(
        "Disconnected from Agent.\n"
    );

    return 0;
}
