#ifndef UDS_OBD_H
#define UDS_OBD_H

#include <stdbool.h>
#include <stdint.h>

bool uds_obd_init(const char* can_interface);
bool uds_request_rpm(uint16_t* rpm);
void uds_obd_cleanup();

#endif
