#include "web_server.h"
#include "uds_obd.h"
#include "handlers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 5000
#define BUFFER_SIZE 8192

// Forward declarations
static void handle_request(int client_fd);
static void serve_static_file(int client_fd, const char *path);
static void send_error(int client_fd, int code, const char *msg);
static void send_response(int client_fd, const char *status, const char *content_type, const char *body);
static const char *get_mime_type(const char *path);

// Routing: periksa path dan panggil handler
static int route_request(const char *request, int client_fd) {
    // Ekstrak method dan path
    char method[8] = {0};
    char path[256] = {0};
    sscanf(request, "%7s %255s", method, path);

    // Hanya mendukung GET
    if (strcmp(method, "GET") != 0) {
        send_error(client_fd, 405, "Method Not Allowed");
        return 1;
    }

    // API Endpoints
    if (strcmp(path, "/api/ecu/rpm") == 0) {
        handle_api_rpm(client_fd);
        return 1;
    }
    if (strcmp(path, "/api/ecu/vin") == 0) {
        handle_api_vin(client_fd);
        return 1;
    }
    if (strcmp(path, "/api/ecu/temp") == 0) {
        handle_api_engine_temp(client_fd);
        return 1;
    }
    if (strcmp(path, "/api/ecu/dtc") == 0) {
        handle_api_dtc(client_fd);
        return 1;
    }
    // Tambahan: status CAN
    if (strcmp(path, "/api/ecu/status") == 0) {
        const char *json = "{\"can_connected\": true, \"interface\": \"vcan0\"}";
        send_response(client_fd, "200 OK", "application/json", json);
        return 1;
    }

    // Jika tidak cocok dengan API, layani file statis
    serve_static_file(client_fd, path);
    return 1;
}

// === Fungsi untuk static file ===
static const char *get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    if (strcmp(ext, ".json") == 0) return "application/json";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".svg") == 0) return "image/svg+xml";
    return "text/plain";
}

static void serve_static_file(int client_fd, const char *path) {
    // Keamanan: cegah directory traversal
    if (strstr(path, "..") != NULL) {
        send_error(client_fd, 403, "Forbidden");
        return;
    }

    // Jika root, arahkan ke index.html
    if (strcmp(path, "/") == 0) {
        path = "/index.html";
    }

    char full_path[512];
    snprintf(full_path, sizeof(full_path), "public%s", path);

    // Coba buka file
    int file_fd = open(full_path, O_RDONLY);
    if (file_fd < 0) {
        // Coba fallback ke index.html jika direktori
        struct stat st;
        if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            snprintf(full_path, sizeof(full_path), "public%s/index.html", path);
            file_fd = open(full_path, O_RDONLY);
        }
        if (file_fd < 0) {
            send_error(client_fd, 404, "File not found");
            return;
        }
    }

    // Dapatkan ukuran file
    struct stat file_stat;
    fstat(file_fd, &file_stat);
    size_t file_size = file_stat.st_size;

    // Kirim header
    char header[512];
    const char *mime = get_mime_type(full_path);
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n",
             mime, file_size);
    write(client_fd, header, strlen(header));

    // Kirim isi file
    char buffer[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
        write(client_fd, buffer, bytes_read);
    }
    close(file_fd);
}

static void send_response(int client_fd, const char *status, const char *content_type, const char *body) {
    char header[512];
    size_t body_len = body ? strlen(body) : 0;
    snprintf(header, sizeof(header),
             "HTTP/1.1 %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n",
             status, content_type, body_len);
    write(client_fd, header, strlen(header));
    if (body) {
        write(client_fd, body, body_len);
    }
}

static void send_error(int client_fd, int code, const char *msg) {
    char status[32];
    snprintf(status, sizeof(status), "%d Error", code);
    char body[256];
    snprintf(body, sizeof(body), "<h1>%d</h1><p>%s</p>", code, msg);
    send_response(client_fd, status, "text/html", body);
}

static void handle_request(int client_fd) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        close(client_fd);
        return;
    }
    buffer[bytes_read] = '\0';

    // Proses request
    route_request(buffer, client_fd);
}

void start_web_server() {
    int server_fd, client_fd;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server ECU berjalan di http://localhost:%d\n", PORT);
    printf("Tekan Ctrl+C untuk berhenti.\n");

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }
        handle_request(client_fd);
        close(client_fd);
    }

    close(server_fd);
}
