#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "web_server.h"
#include "uds_obd.h"

// Global flag untuk graceful shutdown
static volatile int keep_running = 1;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        keep_running = 0;
    }
}

int main(int argc, char **argv) {
    const char *can_iface = "vcan0"; // default virtual CAN

    if (argc > 1) {
        can_iface = argv[1];
    }

    printf("========================================\n");
    printf("   ECU Tuner Web Server (UDS over CAN)  \n");
    printf("========================================\n");
    printf("CAN Interface: %s\n", can_iface);
    printf("HTTP Port: 5000\n\n");

    // Inisialisasi CAN/UDS
    if (!uds_obd_init(can_iface)) {
        fprintf(stderr, "Gagal inisialisasi CAN interface '%s'\n", can_iface);
        fprintf(stderr, "Pastikan interface CAN sudah up (contoh: sudo ip link set %s up type can)\n", can_iface);
        return EXIT_FAILURE;
    }
    printf("[OK] CAN interface siap\n");

    // Setup signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Jalankan web server (blocking)
    start_web_server();

    // Cleanup
    uds_obd_cleanup();
    printf("\n[OK] Server dihentikan. CAN ditutup.\n");

    return EXIT_SUCCESS;
}
