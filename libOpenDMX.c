//
//  libOpenDMX.c
//  libOpenDMX
//
//  Created by Samuel Dewan on 2016-12-01.
//  Copyright © 2016 Samuel Dewan. All rights reserved.
//

#include "libOpenDMX.h"

uint8_t opendmx_start_byte;
long opendmx_interpacket_time = OPENDMX_PERIOD_MID;

struct opendmx_handle* opendmx_open_device (const char *port_name) {
    struct opendmx_handle *device = (struct opendmx_handle*)malloc(sizeof(struct opendmx_handle));
    
    // Get device file
    device->device_desc = open(port_name, O_RDWR | O_NOCTTY | O_ASYNC);
    if (device->device_desc == -1) {
        goto error;     // failed to open device
    }
    
    // Now that device is open, enable blocking on further IO ops
    if (fcntl(device->device_desc, F_SETFL, 0) == -1) {
        goto error;     // failed enable blocking
    }
    
    // Get current settings
    struct termios settings;

    if (tcgetattr(device->device_desc, &settings) != 0) {
        goto error;     // failed to get settings
    }
    
    cfmakeraw(&settings);           // Raw mode allows the use of ioctl
    
    // Clear flags
    settings.c_cflag &= ~CREAD;     // Disables reading from the device
    settings.c_cflag &= ~PARENB;    // Disables parity bit
    
    // Set flags
    settings.c_cflag |= CLOCAL;     // Ignores status lines
    settings.c_cflag |= CS8;        // 8 data bits
    settings.c_cflag |= CSTOPB;     // 2 stop bits
    
    // Apply settings (apply right-away, clear io buffers)
    if (tcsetattr(device->device_desc, TCSAFLUSH, &settings) != 0) {
        goto error;     // failed to set settings
    }
    
    // It worked!
    return device;
    
    // Falure path
error:
    if (device->device_desc != -1) {
        close(device->device_desc);
    }
    free(device);
    
    return NULL;
}

static int set_baud_rate (const int device, const int speed) {
#ifdef __APPLE__
    // macOS - use ioctl
    if (ioctl(device, IOSSIOSPEED, &speed) == -1) {
        return -1;
    }
    return 0;
#elif __linux__
    // linux - use baud rate aliasing
    ioctl(device, TIOCGSERIAL, &ss);
    // TODO actually make this work
#else
#   error "Unsupported platform"
#endif
}

static int send_packet (const struct opendmx_handle *device) {
    int break_byte = 0;
    int error = set_baud_rate(device->device_desc, 76800);              // Drop to lower baud rate
    error = error || (write(device->device_desc, &break_byte, 1) != 1); // transmit a zero
    // At 76.8kbaud this will hold the line low for 104µs (break) then high (the stop bits) for 26µs (MAB)
    error = error || set_baud_rate(device->device_desc, 250000);        // Return to the porper baud rate
    error = error || (write(device->device_desc, &opendmx_start_byte, 1)  != 1);// send the start code
    error = error || (write(device->device_desc, device->slots, OPENDMX_UNIVERSE_LENGTH) != OPENDMX_UNIVERSE_LENGTH);// send the DMX slots
    return error;
}

int opendmx_start (struct opendmx_handle *device) {
    device->running = 1;
    uint8_t errors = 0;
    while (device->running) {
        if (send_packet(device)) {errors++;}
        if (errors >= 8) {return -1;}
        struct timespec tim;
        tim.tv_sec = 0;
        tim.tv_nsec = opendmx_interpacket_time;
        nanosleep(&tim, NULL);
    }
    return 0;
}

void open_dmx_stop (struct opendmx_handle *device) {
    device->running = 0;
}

int opendmx_close_device (struct opendmx_handle *device) {
    open_dmx_stop(device);
    int result = close(device->device_desc);
    if (result != 0) {
        return result;
    }
    free(device);
    return 0;
}

uint8_t opendmx_get_slot (struct opendmx_handle *device, int slot) {
    return ((0 <= slot) && (slot < 512)) ? device->slots[slot] : 0;
}

int opendmx_set_slot (struct opendmx_handle *device, int slot, uint8_t value) {
    if ((0 > slot) || (slot >= 512)) {
        return -1;
    }
    device->slots[slot] = value;
    return 0;
}

