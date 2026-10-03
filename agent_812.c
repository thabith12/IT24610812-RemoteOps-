#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/utsname.h>

#define PORT 9461
#define BUFFER_SIZE 4096

#define AUTH_TOKEN "OPS-0812"
#define SID "2180"


void send_response(int client_fd, const char *message)
{
    char response[BUFFER_SIZE];

    snprintf(response, sizeof(response),
             "%s SID:%s\n", message, SID);

    send(client_fd, response, strlen(response), 0);
}


void handle_sysinfo(int client_fd)
{
    struct utsname system_info;

    if (uname(&system_info) < 0) {
        send_response(client_fd, "ERR 500 SYSINFO_FAILED");
        return;
    }

    char response[BUFFER_SIZE];

    snprintf(response, sizeof(response),
             "OK SYSINFO HOSTNAME:%s OS:%s KERNEL:%s ARCH:%s",
             system_info.nodename,
             system_info.sysname,
             system_info.release,
             system_info.machine);

    send_response(client_fd, response);
}


int main()
{
    int server_fd, client_fd;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    socklen_t client_len;

    char buffer[BUFFER_SIZE];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("socket");
        return 1;
    }


    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);


    if (bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {

        perror("bind");
        close(server_fd);
        return 1;
    }


    if (listen(server_fd, 5) < 0) {

        perror("listen");
        close(server_fd);
        return 1;
    }


    printf("RemoteOps Agent started.\n");
    printf("Listening on TCP port %d...\n", PORT);


    while (1)
    {
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


        while (1)
        {
            memset(buffer, 0, sizeof(buffer));

            int bytes_received = recv(
                client_fd,
                buffer,
                sizeof(buffer) - 1,
                0
            );


            if (bytes_received <= 0) {
                break;
            }


            buffer[bytes_received] = '\0';

            buffer[strcspn(buffer, "\r\n")] = '\0';

            printf("Received: %s\n", buffer);


            /* AUTH */
            if (strncmp(buffer, "AUTH ", 5) == 0)
            {
                if (strcmp(buffer + 5, AUTH_TOKEN) == 0)
                {
                    authenticated = 1;

                    send_response(
                        client_fd,
                        "OK AUTHENTICATED"
                    );

                    printf("Controller authenticated.\n");
                }
                else
                {
                    authenticated = 0;

                    send_response(
                        client_fd,
                        "ERR 001 AUTH_FAILED"
                    );

                    printf("Authentication failed.\n");
                }
            }


            /* SYSINFO */
            else if (strcmp(buffer, "SYSINFO") == 0)
            {
                if (!authenticated)
                {
                    send_response(
                        client_fd,
                        "ERR 002 NOT_AUTHENTICATED"
                    );
                }
                else
                {
                    handle_sysinfo(client_fd);
                }
            }


            /* QUIT */
            else if (strcmp(buffer, "QUIT") == 0)
            {
                send_response(
                    client_fd,
                    "OK BYE"
                );

                break;
            }


            /* Unknown command */
            else
            {
                send_response(
                    client_fd,
                    "ERR 003 UNKNOWN_COMMAND"
                );
            }
        }


        close(client_fd);

        printf("Controller disconnected.\n");
    }


    close(server_fd);

    return 0;
}
