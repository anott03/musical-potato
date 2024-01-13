#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#include "parser.h"

#define PORT 8080
#define BUFFER_SIZE 104857600

const char *get_method_str(Method m) {
    switch (m) {
        case GET:
            return "GET";
            break;
        case POST:
            return "POST";
            break;
        case UNSUPPORTED:
            return "UNSUPPORTED";
            break;
    }
}

const char *get_file_extension(const char *file_name) {
    const char *dot = strrchr(file_name, '.');
    if (!dot || dot == file_name) {
        return "";
    }
    return dot + 1;
}

/*
 * returns the HTML mime time for a corresponding file extension
 */
const char *get_mime_type(const char *file_ext) {
    if (strcasecmp(file_ext, "html") == 0 || strcasecmp(file_ext, "htm") == 0) {
        return "text/html";
    } else if (strcasecmp(file_ext, "txt") == 0) {
        return "text/plain";
    } else if (strcasecmp(file_ext, "jpg") == 0 || strcasecmp(file_ext, "jpeg") == 0) {
        return "image/jpeg";
    } else if (strcasecmp(file_ext, "png") == 0) {
        return "image/png";
    } else if (strcasecmp(file_ext, "json") == 0) {
        return "application/json";
    } else {
        return "application/octet-stream";
    }
}

int read_file_contents(const char *file_name,
        char *response, size_t *response_len, char *header) {
    int file_fd = open(file_name, O_RDONLY);
    if (file_fd == -1) {
        return -1;
    }

    struct stat file_stat;
    fstat(file_fd, &file_stat);
    off_t file_size = file_stat.st_size;

    *response_len = 0;
    memcpy(response, header, strlen(header));
    *response_len += strlen(header);

    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, response + *response_len, BUFFER_SIZE - *response_len)) > 0) {
        *response_len += bytes_read;
    }
    close(file_fd);
    return 1;
}

void build_http_response(const char *file_name,
    const char *file_ext, char *response, size_t *response_len) {
    const char *mime_type = get_mime_type(file_ext);
    char *header = (char *)malloc(BUFFER_SIZE * sizeof(char));
    snprintf(header, BUFFER_SIZE,
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "\r\n",
             mime_type);

    /* files first */
    int file_read_result = read_file_contents(file_name, response, response_len, header);
    if (file_read_result > 0) {
        free(header);
        return;
    }
    /* dynamic routes second */

    /* otherwise not found */
    const char *not_found_file = "404.html";
    const char *not_found_file_ext = "html";
    const char *not_found_mime_type = "text/html";
    snprintf(header, BUFFER_SIZE,
             "HTTP/1.1 404 Not Found\r\n"
             "Content-Type: %s\r\n"
             "\r\n",
             not_found_mime_type);
    int not_found_file_read_result = read_file_contents(not_found_file, response, response_len, header);
    if (not_found_file_read_result < 0) {
        free(header);
        perror("Error reading 404.html\n");
        exit(EXIT_FAILURE);
    }
    free(header);
}

void *handle_client(void *arg) {
    int client_fd = *((int *) arg);
    char *buffer = (char*) malloc(BUFFER_SIZE * sizeof(char));

    /* reads BUFFER_SIZE bytes into buffer from the client */
    ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
    if (bytes_received > 0) {

        HttpRequest *req = parse_request(buffer);

        /* logging */
        time_t rawtime = time(NULL);
        struct tm *timeinfo;
        timeinfo = localtime(&rawtime);
        printf("%s %s: %s\n\n", asctime(timeinfo), get_method_str(req->method), req->url);

        if (req->method == GET) {
            char *file_name = req->url;
            ++file_name;

            char file_ext[32];
            strcpy(file_ext, get_file_extension(file_name));

            char *response = (char *) malloc(BUFFER_SIZE * 2 * sizeof(char));
            size_t response_len;
            build_http_response(file_name, file_ext, response, &response_len);

            send(client_fd, response, response_len, 0);

            free(response);
            free_request(req);
        }
    }

    close(client_fd);
    free(arg);
    free(buffer);

    return NULL;
}

int main() {
    int server_fd; /* server file descriptor */
    struct sockaddr_in server_addr;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed\n");
        exit(EXIT_FAILURE);
    }

    /*
     * AF_INET: use IPv4 (vs IPv6)
     * SOCK_STREAM: use TCP (vs UDP)
     * INADDR_ANY: the server accepts connections from any network interface
     */
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    /*
     * `bind` binds the socket to a port, meaning that the socket will listen to
     * any clients trying to connect to that port.
     */
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed\n");
        exit(EXIT_FAILURE);
    }

    /*
     * `listen` takes the maximum number of pending connections (10 in this case)
     */
    if (listen(server_fd, 10) < 0) {
        perror("listen failed\n");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

loop: while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int *client_fd = malloc(sizeof(int));

        if ((*client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len)) < 0) {
            perror("client connection failed\n");
            goto loop; /* continue would be safer here, but goto is more fun */
        }

        /*
         * create a thread to handle the connection
         * the thread runs the procedure `handle_client` on parameter client_fd
         */
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_client, (void *)client_fd);
        pthread_detach(thread_id);
    }

    return EXIT_SUCCESS;
}
