#include "web_server.h"
#include "uds_obd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 5000
#define BUFFER_SIZE 4096

static void handle_request(int client_fd) {
    char buffer[BUFFER_SIZE];
    read(client_fd, buffer, sizeof(buffer)-1);

    // Parse sederhana: cek apakah request untuk API
    if (strstr(buffer, "GET /api/ecu/rpm") != NULL) {
        uint16_t rpm;
        char response[256];
        if (uds_request_rpm(&rpm)) {
            sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"rpm\": %d}", rpm);
        } else {
            sprintf(response, "HTTP/1.1 500 Internal Error\r\nContent-Type: text/plain\r\n\r\nGagal membaca RPM");
        }
        write(client_fd, response, strlen(response));
        return;
    }
    // ... tambahkan endpoint lain (VIN, suhu mesin, dll)

    // Default: layani file statis dari folder public/
    // Implementasi sederhana (abaikan path traversal)
    char path[256] = "public";
    strcat(path, buffer+4); // asumsi "GET /..."
    // ... (parsing lengkap ditingkatkan sesuai kebutuhan)

    close(client_fd);
}

void start_web_server() {
    int server_fd, client_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, 3);

    printf("Server ECU berjalan di http://localhost:%d\n", PORT);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (client_fd >= 0) {
            handle_request(client_fd);
            close(client_fd);
        }
    }
}
