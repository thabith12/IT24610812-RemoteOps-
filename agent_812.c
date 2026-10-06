#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <dirent.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/time.h>
#include <netinet/in.h>


#define PORT 9461
#define BUFFER_SIZE 4096
#define STORAGE_PATH "./agentfiles/IT24610812/"
#define MAX_FILE_SIZE 10485760
#define AUTH_TOKEN "OPS-0812"
#define SID "2180"
#define LOG_FILE "remoteops_IT24610812.log"

/* UDP Monitor configuration */
#define MONITOR_UDP_PORT 9462
#define MONITOR_INTERVAL 5

volatile int monitor_running = 0;
pthread_t monitor_thread;

struct MonitorConfig
{
    struct sockaddr_in controller_addr;
    int udp_socket;
    volatile int running;
};
struct ClientArgs
{
    int client_fd;
    struct sockaddr_in client_addr;
};

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

/* Receive one complete newline-terminated TCP command */
int receive_command(int client_fd, char *command, size_t command_size)
{
    size_t pos = 0;

    while (pos < command_size - 1)
    {
        char ch;
        int bytes_received = recv(
            client_fd,
            &ch,
            1,
            0
        );

        if (bytes_received <= 0)
        {
            return -1;
        }

        if (ch == '\n')
        {
            command[pos] = '\0';
            return 0;
        }

        if (ch != '\r')
        {
            command[pos++] = ch;
        }
    }

    command[pos] = '\0';
    return 0;
}

void write_log(const char *event)
{
    FILE *file;
    time_t now;
    struct tm *time_info;
    char timestamp[64];

    now = time(NULL);
    time_info = localtime(&now);

    strftime(
        timestamp,
        sizeof(timestamp),
        "%Y-%m-%d %H:%M:%S",
        time_info
    );

    file = fopen(LOG_FILE, "a");

    if (file == NULL)
        return;

    fprintf(
        file,
        "[%s] %s\n",
        timestamp,
        event
    );
    fclose(file);
}



/* Get CPU usage percentage */
double get_cpu_usage()
{
    FILE *file;
    long user1, nice1, system1, idle1;
    long user2, nice2, system2, idle2;
    long total1, total2;
    long idle_diff, total_diff;

    file = fopen("/proc/stat", "r");

    if (file == NULL)
        return 0.0;

    fscanf(
        file,
        "cpu %ld %ld %ld %ld",
        &user1,
        &nice1,
        &system1,
        &idle1
    );

    fclose(file);

    sleep(1);

    file = fopen("/proc/stat", "r");

    if (file == NULL)
        return 0.0;

    fscanf(
        file,
        "cpu %ld %ld %ld %ld",
        &user2,
        &nice2,
        &system2,
        &idle2
    );

    fclose(file);

    total1 =
        user1 + nice1 + system1 + idle1;

    total2 =
        user2 + nice2 + system2 + idle2;

    idle_diff =
        idle2 - idle1;

    total_diff =
        total2 - total1;

    if (total_diff == 0)
        return 0.0;

    return
        100.0 *
        (1.0 -
        ((double)idle_diff /
        (double)total_diff));
}


/* Get memory usage percentage */
double get_memory_usage()
{
    FILE *file;

    long total_memory = 0;
    long available_memory = 0;

    char line[256];

    file = fopen(
        "/proc/meminfo",
        "r"
    );

    if (file == NULL)
        return 0.0;

    while (fgets(
        line,
        sizeof(line),
        file))
    {
        if (sscanf(
            line,
            "MemTotal: %ld kB",
            &total_memory) == 1)
        {
            continue;
        }

        if (sscanf(
            line,
            "MemAvailable: %ld kB",
            &available_memory) == 1)
        {
            continue;
        }
    }

    fclose(file);

    if (total_memory == 0)
        return 0.0;

    return
        100.0 *
        ((double)(total_memory -
                  available_memory) /
         (double)total_memory);
}


/* Get system uptime */
long get_uptime()
{
    FILE *file;

    long uptime = 0;

    file = fopen(
        "/proc/uptime",
        "r"
    );

    if (file == NULL)
        return 0;

    fscanf(
        file,
        "%ld",
        &uptime
    );

    fclose(file);

    return uptime;
}


/* UDP monitoring thread */
void *monitor_function(void *arg)
{
    struct MonitorConfig *config =
        (struct MonitorConfig *)arg;

    char message[BUFFER_SIZE];

    while (config->running)
    {
        double cpu =
            get_cpu_usage();

        double memory =
            get_memory_usage();

        long uptime =
            get_uptime();

        snprintf(
            message,
            sizeof(message),
            "MONITOR CPU:%.2f MEM:%.2f UPTIME:%ld SID:%s",
            cpu,
            memory,
            uptime,
            SID
        );

        sendto(
            config->udp_socket,
            message,
            strlen(message),
            0,
            (struct sockaddr *)&config->controller_addr,
            sizeof(config->controller_addr)
        );

        sleep(MONITOR_INTERVAL);
    }

    return NULL;
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

char log_message[256];
snprintf(log_message, sizeof(log_message), "PUT completed: %s (%ld bytes)", filename, file_size);
write_log(log_message);

    char response[256];
    snprintf(response, sizeof(response), "OK FILE_RECEIVED %s", filename);
    send_response(client_fd, response);
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

    /* Send response line first. */
    char response[BUFFER_SIZE];

    snprintf(
        response,
        sizeof(response),
        "OK FILE_SEND %s %ld",
        filename,
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

char log_message[256];

    snprintf(
        log_message,
        sizeof(log_message),
        "GET completed: %s (%ld bytes)",
        filename,
        file_size
    );

    write_log(log_message);

    fclose(file);
}

void *client_handler(void *arg)
{
    char buffer[BUFFER_SIZE];
    struct ClientArgs *client_info = (struct ClientArgs *)arg;

    int client_fd = client_info->client_fd;
    struct sockaddr_in client_addr = client_info->client_addr;

    free(client_info);

  
    printf("Controller connected.\n");

    char log_message[256];

    snprintf(
        log_message,
        sizeof(log_message),
        "Controller connected from %s:%d",
        inet_ntoa(client_addr.sin_addr),
        ntohs(client_addr.sin_port)
    );

    write_log(log_message);

        /* Authentication state */
        int authenticated = 0;

        /* Per-controller UDP monitoring session */
        struct MonitorConfig monitor_config;
        pthread_t monitor_thread;
        int monitor_active = 0;

        memset(&monitor_config, 0, sizeof(monitor_config));

        /* Handle multiple commands on same connection */
        while (1)
        {
            /* Receive one complete TCP command */
            if (receive_command(client_fd, buffer, sizeof(buffer)) < 0)
            {
                /* Controller disconnected */
                break;
            }


            printf(
                "Received: %s\n",
                buffer
            );

            write_log(buffer);


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
        else if (file_size > MAX_FILE_SIZE)
        {
            send_response(
                client_fd,
                "ERR 413 FILE_TOO_LARGE"
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
            /* MONITOR START                      */
            /* ================================= */

            else if (strncmp(buffer, "MONITOR START ", 14) == 0)
            {
                int udp_port = 0;

                if (sscanf(buffer, "MONITOR START %d", &udp_port) != 1 ||
                    udp_port < 1 ||
                    udp_port > 65535)
                {
                    send_response(
                        client_fd,
                        "ERR 400 INVALID_UDP_PORT"
                    );
                }
                else if (!authenticated)
                {
                    send_response(
                        client_fd,
                        "ERR 002 NOT_AUTHENTICATED"
                    );
                }
                else if (monitor_active)
                {
                    send_response(
                        client_fd,
                        "ERR 409 MONITOR_ALREADY_RUNNING"
                    );
                }
                else
                {
                    monitor_config.udp_socket =
                        socket(AF_INET, SOCK_DGRAM, 0);

                    if (monitor_config.udp_socket < 0)
                    {
                        send_response(
                            client_fd,
                            "ERR 500 UDP_SOCKET_FAILED"
                        );
                    }
                    else
                    {
                        memset(
                            &monitor_config.controller_addr,
                            0,
                            sizeof(monitor_config.controller_addr)
                        );

                        monitor_config.controller_addr.sin_family =
                            AF_INET;

                        monitor_config.controller_addr.sin_addr =
                            client_addr.sin_addr;

                        monitor_config.controller_addr.sin_port =
                            htons(udp_port);

                        monitor_config.running = 1;

                        if (pthread_create(
                                &monitor_thread,
                                NULL,
                                monitor_function,
                                &monitor_config) != 0)
                        {
                            close(
                                monitor_config.udp_socket
                            );

                            monitor_config.running = 0;

                            send_response(
                                client_fd,
                                "ERR 500 MONITOR_THREAD_FAILED"
                            );
                        }
                        else
                        {
                            monitor_active = 1;

                            send_response(
                                client_fd,
                                "OK MONITOR_STARTED"
                            );

                            write_log(
                                "MONITOR STARTED"
                            );
                        }
                    }
                }
            }

            /* ================================= */
            /* MONITOR STOP                       */
            /* ================================= */

            else if (strcmp(buffer, "MONITOR STOP") == 0)
            {
                if (!authenticated)
                {
                    send_response(
                        client_fd,
                        "ERR 002 NOT_AUTHENTICATED"
                    );
                }
                else if (!monitor_active)
                {
                    send_response(
                        client_fd,
                        "ERR 409 MONITOR_NOT_RUNNING"
                    );
                }
                else
                {
                    monitor_config.running = 0;

                    pthread_join(
                        monitor_thread,
                        NULL
                    );

                    close(
                        monitor_config.udp_socket
                    );

                    monitor_active = 0;

                    send_response(
                        client_fd,
                        "OK MONITOR_STOPPED"
                    );

                    write_log(
                        "MONITOR STOPPED"
                    );
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
                if (monitor_active)
                {
                    monitor_config.running = 0;

                    pthread_join(
                        monitor_thread,
                        NULL
                    );

                    close(
                        monitor_config.udp_socket
                    );

                    monitor_active = 0;
                }

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


    close(client_fd);

    printf("Controller disconnected.\n");
    write_log("Controller disconnected");

    return NULL;
}

int main()
{
    int server_fd;
    int client_fd;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    socklen_t client_len;


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
    pthread_t thread;

struct ClientArgs *client_info =
    malloc(sizeof(struct ClientArgs));

if (client_info == NULL)
{
    perror("malloc");
    close(client_fd);
    continue;
}

client_info->client_fd = client_fd;
client_info->client_addr = client_addr;

if (pthread_create(&thread, NULL, client_handler, client_info) != 0)
{
    perror("pthread_create");
    free(client_info);
    close(client_fd);
    continue;
}

pthread_detach(thread);


       

    }


    /* Close server socket */
    close(server_fd);

    return 0;
}
