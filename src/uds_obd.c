#include "uds_obd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/can.h>
#include <linux/can/raw.h>

#include "../lib/uds-c/uds.h"

static int can_socket = -1;
static DiagnosticShims shims;

// Fungsi kirim CAN via SocketCAN
static bool send_can(const uint32_t arbitration_id,
                     const uint8_t* data, const uint8_t size) {
    if (can_socket < 0) return false;
    struct can_frame frame;
    frame.can_id = arbitration_id;
    frame.can_dlc = size;
    memcpy(frame.data, data, size);
    return (write(can_socket, &frame, sizeof(frame)) == sizeof(frame));
}

// Fungsi debug opsional
static void debug_log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

// Inisialisasi CAN socket
bool uds_obd_init(const char* can_interface) {
    struct sockaddr_can addr;
    struct ifreq ifr;

    // Buka socket CAN
    can_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (can_socket < 0) {
        perror("Socket CAN gagal");
        return false;
    }

    strcpy(ifr.ifr_name, can_interface);
    if (ioctl(can_socket, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl SIOCGIFINDEX gagal");
        close(can_socket);
        return false;
    }

    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(can_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Bind CAN socket gagal");
        close(can_socket);
        return false;
    }

    // Setup shims untuk UDS-C
    shims = diagnostic_init_shims(debug_log, send_can, NULL);
    return true;
}

// Fungsi untuk request data ECU via UDS (contoh: baca RPM)
bool uds_request_rpm(uint16_t* rpm) {
    // Contoh: kirim ReadDataByIdentifier untuk RPM (0x0C)
    DiagnosticRequest request = {0};
    request.arbitration_id = OBD2_FUNCTIONAL_BROADCAST_ID;
    request.data[0] = 0x02; // service ID (ReadDataByIdentifier)
    request.data[1] = 0x01; // panjang data
    request.data[2] = 0x0C; // identifier RPM
    request.length = 3;

    // Kirim request dan tunggu respons (sederhana)
    DiagnosticResponse response;
    if (diagnostic_send_request(&shims, &request, &response)) {
        // Proses response sesuai ISO 14229
        // ... (implementasi parsing)
        *rpm = 2500; // contoh dummy
        return true;
    }
    return false;
}

void uds_obd_cleanup() {
    if (can_socket >= 0) {
        close(can_socket);
        can_socket = -1;
    }
}
