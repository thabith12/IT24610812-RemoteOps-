#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <dirent.h>
#include <ctype.h>

#define PORT 9461
#define BUFFER_SIZE 4096
#define STORAGE_PATH "./agentfiles/IT24610812/"
#define MAX_FILE_SIZE 10485760
#define AUTH_TOKEN "OPS-0812"
#define SID "2180"


/* Send a response with the personalized SID */
void send_response(int client_fd, const char *message)
{
    char response[BUFFER_SIZE];

    snprintf(
        response,
        sizeof(response),
        "%s SID:%s\n",
        message,
        SID
    );

    send(
        client_fd,
        response,
        strlen(response),
        0
    );
}


/* Handle SYSINFO command */
void handle_sysinfo(int client_fd)
{
    struct utsname system_info;

    if (uname(&system_info) < 0)
    {
        send_response(
            client_fd,
            "ERR 500 SYSINFO_FAILED"
        );

        return;
    }

    char response[BUFFER_SIZE];

    snprintf(
        response,
        sizeof(response),
        "OK SYSINFO HOSTNAME:%s OS:%s KERNEL:%s ARCH:%s",
        system_info.nodename,
        system_info.sysname,
        system_info.release,
        system_info.machine
    );

    send_response(
        client_fd,
        response
    );
}


/* Handle LISTPROC command */
void handle_listproc(int client_fd)
{
    DIR *proc_dir;
    struct dirent *entry;

    char response[BUFFER_SIZE];
    int offset = 0;

    /* Start the response */
    offset += snprintf(
        response + offset,
        sizeof(response) - offset,
        "OK LISTPROC"
    );

    /* Open Linux /proc filesystem */
    proc_dir = opendir("/proc");

    if (proc_dir == NULL)
    {
        send_response(
            client_fd,
            "ERR 500 LISTPROC_FAILED"
        );

        return;
    }

    /* Read entries inside /proc */
    while ((entry = readdir(proc_dir)) != NULL)
    {
        int is_pid = 1;

        /* Check whether directory name contains only digits */
        for (int i = 0; entry->d_name[i] != '\0'; i++)
        {
            if (!isdigit((unsigned char)entry->d_name[i]))
            {
                is_pid = 0;
                break;
            }
        }

        /* Ignore non-PID entries */
        if (!is_pid)
        {
            continue;
        }

        char comm_path[512];
        char process_name[128];

        /* Build /proc/<PID>/comm path */
        snprintf(
            comm_path,
            sizeof(comm_path),
            "/proc/%s/comm",
            entry->d_name
        );

        /* Open process name file */
        FILE *file = fopen(
            comm_path,
            "r"
        );

        if (file == NULL)
        {
            continue;
        }

        /* Read process name */
        if (fgets(
                process_name,
                sizeof(process_name),
                file
            ) != NULL)
        {
            /* Remove newline */
            process_name[
                strcspn(process_name, "\r\n")
            ] = '\0';

            int written = snprintf(
                response + offset,
                sizeof(response) - offset,
                " PID:%s NAME:%s",
                entry->d_name,
                process_name
            );

            /* Stop if buffer is full */
            if (written < 0 ||
                written >= (int)(sizeof(response) - offset))
            {
                fclose(file);
                break;
            }

            offset += written;
        }

        fclose(file);
    }

    closedir(proc_dir);

    /* Add SID and newline */
    snprintf(
        response + offset,
        sizeof(response) - offset,
        " SID:%s\n",
        SID
    );

    /* Send complete response */
    send(
        client_fd,
        response,
        strlen(response),
        0
    );
}
/* Handle EXEC command with fixed whitelist */
void handle_exec(int client_fd, const char *command)
{
    char result[1024];
    FILE *pipe;

    /* DATE */
    if (strcmp(command, "DATE") == 0)
    {
        pipe = popen("date", "r");
    }

    /* UPTIME */
    else if (strcmp(command, "UPTIME") == 0)
    {
        pipe = popen("uptime", "r");
    }

    /* DISKFREE */
    else if (strcmp(command, "DISKFREE") == 0)
    {
        pipe = popen("df -h /", "r");
    }

    /* HOSTNAME */
    else if (strcmp(command, "HOSTNAME") == 0)
    {
        pipe = popen("hostname", "r");
    }

    /* WHOAMI */
    else if (strcmp(command, "WHOAMI") == 0)
    {
        pipe = popen("whoami", "r");
    }

    /* Anything else is rejected */
    else
    {
        send_response(
            client_fd,
            "ERR 002 COMMAND_NOT_ALLOWED"
        );

        return;
    }

    if (pipe == NULL)
    {
        send_response(
            client_fd,
            "ERR 500 EXEC_FAILED"
        );

        return;
    }

    memset(result, 0, sizeof(result));

    if (fgets(
            result,
            sizeof(result),
            pipe
        ) != NULL)
    {
        /* Remove newline */
        result[
            strcspn(result, "\r\n")
        ] = '\0';

        char response[BUFFER_SIZE];

        snprintf(
            response,
            sizeof(response),
            "OK EXEC_RESULT %s",
            result
        );

        send_response(
            client_fd,
            response
        );
    }
    else
    {
        send_response(
            client_fd,
            "ERR 500 EXEC_FAILED"
        );
    }

    pclose(pipe);
}

void handle_put(int client_fd, const char *filename, long file_size)
{
    char filepath[512];
    FILE *file;
    char buffer[4096];
    long total_received = 0;

    if (strstr(filename, "..") != NULL ||
        strchr(filename, '/') != NULL ||
        strchr(filename, '\\') != NULL)
    {
        send_response(
            client_fd,
            "ERR 400 INVALID_FILENAME"
        );
        return;
    }
    
    /* Build personalized storage path */
    snprintf(filepath, sizeof(filepath),
             "agentfiles/IT24610812/%s", filename);

    /* Open file for binary writing */
    file = fopen(filepath, "wb");

    if (file == NULL)
    {
        send_response(client_fd, "ERR 500 FILE_OPEN_FAILED");
        return;
    }

    /* Receive exactly file_size bytes */
    while (total_received < file_size)
    {
        long remaining = file_size - total_received;
        int chunk_size = sizeof(buffer);

        if (remaining < chunk_size)
            chunk_size = (int)remaining;

        int bytes_received = recv(
            client_fd,
            buffer,
            chunk_size,
            0
        );

        if (bytes_received <= 0)
        {
            fclose(file);
            remove(filepath);
            return;
        }

        fwrite(buffer, 1, bytes_received, file);

        total_received += bytes_received;
    }

    fclose(file);

    send_response(client_fd, "OK PUT");
}

void handle_get(int client_fd, const char *filename)
{
    char filepath[512];
    FILE *file;
    char buffer[4096];

    /* Prevent path traversal */
    if (strstr(filename, "..") != NULL ||
        strchr(filename, '/') != NULL ||
        strchr(filename, '\\') != NULL)
    {
        send_response(
            client_fd,
            "ERR 400 INVALID_FILENAME"
        );
        return;
    }

    /* Build personalized storage path */
    snprintf(
        filepath,
        sizeof(filepath),
        "%s%s",
        STORAGE_PATH,
        filename
    );

    /* Open requested file */
    file = fopen(filepath, "rb");

    if (file == NULL)
    {
        send_response(
            client_fd,
            "ERR 005 FILE_NOT_FOUND"
        );
        return;
    }

    /* Find file size */
    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);

        send_response(
            client_fd,
            "ERR 500 FILE_READ_FAILED"
        );

        return;
    }

    long file_size = ftell(file);

    if (file_size < 0)
    {
        fclose(file);

        send_response(
            client_fd,
            "ERR 500 FILE_READ_FAILED"
        );

        return;
    }

    rewind(file);

    /*
     * Send response line first.
     * The controller will then receive exactly
     * file_size raw bytes.
     */
    char response[BUFFER_SIZE];

    snprintf(
        response,
        sizeof(response),
        "OK FILE_SEND %ld",
        file_size
    );

    send_response(
        client_fd,
        response
    );

    /* Send exact file bytes */
    long total_sent = 0;

    while (total_sent < file_size)
    {
        size_t bytes_to_read = sizeof(buffer);

        if (file_size - total_sent < (long)bytes_to_read)
        {
            bytes_to_read =
                (size_t)(file_size - total_sent);
        }

        size_t bytes_read =
            fread(
                buffer,
                1,
                bytes_to_read,
                file
            );

        if (bytes_read == 0)
        {
            break;
        }

        size_t sent = 0;

        while (sent < bytes_read)
        {
            ssize_t n = send(
                client_fd,
                buffer + sent,
                bytes_read - sent,
                0
            );

            if (n <= 0)
            {
                fclose(file);
                return;
            }

            sent += (size_t)n;
        }

        total_sent += (long)bytes_read;
    }

    fclose(file);
}

int main()
{
    int server_fd;
    int client_fd;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    socklen_t client_len;

    char buffer[BUFFER_SIZE];


    /* 1. Create TCP socket */
    server_fd = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }


    /* 2. Configure server address */
    memset(
        &server_addr,
        0,
        sizeof(server_addr)
    );

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);


    /* 3. Bind socket to port 9461 */
    if (bind(
            server_fd,
            (struct sockaddr *)&server_addr,
            sizeof(server_addr)
        ) < 0)
    {
        perror("bind");

        close(server_fd);

        return 1;
    }


    /* 4. Start listening */
    if (listen(server_fd, 5) < 0)
    {
        perror("listen");

        close(server_fd);

        return 1;
    }


    printf("RemoteOps Agent started.\n");
    printf("Listening on TCP port %d...\n", PORT);


    /* 5. Accept Controller connections */
    while (1)
    {
        client_len = sizeof(client_addr);

        client_fd = accept(
            server_fd,
            (struct sockaddr *)&client_addr,
            &client_len
        );

        if (client_fd < 0)
        {
            perror("accept");
            continue;
        }


        printf("Controller connected.\n");


        /* Authentication state */
        int authenticated = 0;


        /* Handle multiple commands on same connection */
        while (1)
        {
            memset(
                buffer,
                0,
                sizeof(buffer)
            );


            int bytes_received = recv(
                client_fd,
                buffer,
                sizeof(buffer) - 1,
                0
            );


            /* Controller disconnected */
            if (bytes_received <= 0)
            {
                break;
            }


            buffer[bytes_received] = '\0';


            /* Remove CR/LF */
            buffer[
                strcspn(buffer, "\r\n")
            ] = '\0';


            printf(
                "Received: %s\n",
                buffer
            );


            /* ================================= */
            /* AUTH                              */
            /* ================================= */

            if (strncmp(buffer, "AUTH ", 5) == 0)
            {
                if (strcmp(
                        buffer + 5,
                        AUTH_TOKEN
                    ) == 0)
                {
                    authenticated = 1;

                    send_response(
                        client_fd,
                        "OK AUTHENTICATED"
                    );

                    printf(
                        "Controller authenticated.\n"
                    );
                }
                else
                {
                    authenticated = 0;

                    send_response(
                        client_fd,
                        "ERR 001 AUTH_FAILED"
                    );

                    printf(
                        "Authentication failed.\n"
                    );
                }
            }


            /* ================================= */
            /* SYSINFO                           */
            /* ================================= */

            else if (strcmp(
                        buffer,
                        "SYSINFO"
                    ) == 0)
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
                    handle_sysinfo(
                        client_fd
                    );
                }
            }


            /* ================================= */
            /* LISTPROC                          */
            /* ================================= */

            else if (strcmp(
                        buffer,
                        "LISTPROC"
                    ) == 0)
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
                    handle_listproc(
                        client_fd
                    );
                }
            }
            
           /* EXEC */
else if (strncmp(buffer, "EXEC ", 5) == 0)
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
        handle_exec(
            client_fd,
            buffer + 5
        );
    }
}

else if (strncmp(buffer, "PUT ", 4) == 0)
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
        char filename[256];
        long file_size;

        if (sscanf(buffer + 4, "%255s %ld",
                   filename, &file_size) != 2)
        {
            send_response(
                client_fd,
                "ERR 400 INVALID_PUT"
            );
        }
        else if (file_size < 0)
        {
            send_response(
                client_fd,
                "ERR 400 INVALID_FILE_SIZE"
            );
        }
        else
        {
            handle_put(
                client_fd,
                filename,
                file_size
            );
        }
    }
}

else if (strncmp(buffer, "GET ", 4) == 0)
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
        char filename[256];

        if (sscanf(
                buffer + 4,
                "%255s",
                filename) != 1)
        {
            send_response(
                client_fd,
                "ERR 400 INVALID_GET"
            );
        }
        else
        {
            handle_get(
                client_fd,
                filename
            );
        }
    }
} 

            /* ================================= */
            /* QUIT                              */
            /* ================================= */

            else if (strcmp(
                        buffer,
                        "QUIT"
                    ) == 0)
            {
                send_response(
                    client_fd,
                    "OK BYE"
                );

                break;
            }


            /* ================================= */
            /* UNKNOWN COMMAND                   */
            /* ================================= */

            else
            {
                send_response(
                    client_fd,
                    "ERR 003 UNKNOWN_COMMAND"
                );
            }
        }


        /* Close Controller connection */
        close(client_fd);

        printf(
            "Controller disconnected.\n"
        );
    }


    /* Close server socket */
    close(server_fd);

    return 0;
}
