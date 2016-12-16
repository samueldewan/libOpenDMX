//
//  OpenDMX.c
//  OpenDMX
//
//  Created by Samuel Dewan on 2016-12-01.
//  Copyright © 2016 Samuel Dewan. All rights reserved.
//

#include "OpenDMX.h"
#include "LinkedList.h"

#include <sys/ioctl.h>
#include <sys/time.h>

#ifdef __APPLE__
// macOS
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/IOBSD.h>
#elif __linux__
// Linux
#include <unistd.h>
#include <stdio.h>
#include <glob.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <time.h>

#define SERIAL_PATH     "/sys/class/tty/*/device/driver"
#define DEVICE_FORMAT   "/dev/%s"
#define SYS_PREFIX_LENGTH   15
#define SYS_POSTFIX_LENGTH  14
#elif _WIN32
#define OPENDMX_USE_D2XX
#else
#   error "Unsupported platform"
#endif

uint8_t opendmx_start_byte;
long opendmx_interpacket_time = OPENDMX_PERIOD_MID;

typedef struct opendmx_handle {
    int             device_desc;
    volatile int    running:1;
    volatile int    error:1;
    uint8_t         slots[OPENDMX_UNIVERSE_LENGTH];
} opendmx_device;

struct opendmx_iterator {
    struct list             *list;
    struct list_iterator    *iterator;
};

# ifndef OPENDMX_USE_D2XX
opendmx_device *opendmx_open_device (const char *port_name) {
    struct opendmx_handle *device = (struct opendmx_handle*)malloc(sizeof(struct opendmx_handle));
    
    // Get device file
    device->device_desc = open(port_name, O_RDWR | O_NOCTTY | O_NDELAY | O_ASYNC);
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
    
#ifdef __APPLE__
    cfmakeraw(&settings);           // Raw mode allows the use of ioctl
#endif
    
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
#endif  //  not OPENDMX_USE_D2XX

static int set_baud_rate (const int device, const int speed) {
#ifdef __APPLE__
    // macOS - use ioctl
    if (ioctl(device, IOSSIOSPEED, &speed) == -1) {
        return -1;
    }
    // That was easy
    return 0;
#elif __linux__
    // linux - use baud rate aliasing
//    ioctl(device, TIOCGSERIAL, &ss);
    // TODO actually make this work
#else
#   error "Unsupported platform"
#endif
}

static int send_packet (const opendmx_device *device) {
    const break_byte = 0; // Need to define this as a constant so I that can get a pointer to it
    int error = set_baud_rate(device->device_desc, 76800);              // Drop to lower baud rate
    error = error || (write(device->device_desc, &break_byte, 1) != 1); // transmit a zero
    // At 76.8kbaud this will hold the line low for 104µs (break) then high (the stop bits) for 26µs (MAB)
    error = error || set_baud_rate(device->device_desc, 250000);        // Return to the porper baud rate
    error = error || (write(device->device_desc, &opendmx_start_byte, 1)  != 1);// send the start code
    error = error || (write(device->device_desc, device->slots, OPENDMX_UNIVERSE_LENGTH) != OPENDMX_UNIVERSE_LENGTH);// send the DMX slots
    return error;
}

void *opendmx_thread (void *device) {
    opendmx_start((opendmx_device*) device);    // Start the DMX device
    return NULL;
}

#ifndef OPENDMX_USE_D2XX
int opendmx_start (opendmx_device *device) {
    device->running = 1;
    device->error = 0;
    uint8_t errors = 0; // Tracks the number of frames which have failed to send
    while (device->running) {   // Run as along as the device hasn't been told not to
        errors = (errors << 1) | (send_packet(device) ? 1 : 0);
        if ((errors & 0xFF) == 0xFF) {
            // If 8 errors have occured in a row, stop DMX output and register an error. This usually means that the DMX output has been disconected.
            device->running = 0;
            device->error = 1;
            return -1;
        }
        // Wait for the interpacket time
        struct timespec tim;
        tim.tv_sec = 0;
        tim.tv_nsec = opendmx_interpacket_time;
        nanosleep(&tim, NULL);
    }
    return 0;
}
#endif // not OPENDMX_USE_D2XX

void opendmx_stop (opendmx_device *device) {
    device->running = 0;
}

int opendmx_close_device (opendmx_device *device) {
    opendmx_stop(device);
    if (close(device->device_desc) != 0) {
        return 1;
    }
    free(device);
    return 0;
}

uint8_t opendmx_get_slot (const opendmx_device *device, int slot) {
    return ((0 <= slot) && (slot < OPENDMX_UNIVERSE_LENGTH)) ? device->slots[slot] : 0;
}

int opendmx_set_slot (opendmx_device *device, int slot, uint8_t value) {
    if ((0 > slot) || (slot >= OPENDMX_UNIVERSE_LENGTH)) {
        return -1;
    }
    device->slots[slot] = value;
    return 0;
}

#ifdef __linux__
static char *trim_path(char *path) {
    // Get string starting at the beginig of the device name
    char *sub_path = (path + (sizeof(char) * SYS_PREFIX_LENGTH));
    // Get the length of the char up to the end of the device name
    int len = (int)strlen(sub_path) - SYS_POSTFIX_LENGTH;
    char* str = (char*)malloc(sizeof(char) * len);
    // Copy only the device name to a new string
    strncpy(str, sub_path, len);
    return str;
}
#endif

# ifndef OPENDMX_USE_D2XX
struct opendmx_iterator *opendmx_get_devices () {
#ifdef __APPLE__
    // macOS - use IOBSD
    kern_return_t kern_result;
    CFMutableDictionaryRef classes;
    io_iterator_t matching_services;
    mach_port_t master_port;
    
    kern_result = IOMasterPort(MACH_PORT_NULL, &master_port);
    if (KERN_SUCCESS != kern_result) {
        // Failed to get master port.
        return 0;
    }
    
    classes = IOServiceMatching(kIOSerialBSDServiceValue);
    if (classes == NULL) {
        // IOServiceMatching returned a NULL dictionary
        return 0;
    }
    // Look for devices that claim to be serial ports.
    CFDictionarySetValue(classes, CFSTR(kIOSerialBSDTypeKey), CFSTR(kIOSerialBSDAllTypes));
    kern_result = IOServiceGetMatchingServices(master_port, classes, &matching_services);
    if (KERN_SUCCESS  != kern_result) {
        return 0;
    }
    
    struct list *devices = (struct list *)malloc(sizeof(struct list));
    devices->first = NULL;
    devices->length = 0;
    
    io_object_t modem_service;
    // Iterates through the list of devices adding each device file to the list which will be returned
    while ((modem_service = IOIteratorNext(matching_services))) {
        CFTypeRef bsd_path_cf_string;
        // Get the device file path
        bsd_path_cf_string = IORegistryEntryCreateCFProperty(modem_service, CFSTR(kIODialinDeviceKey), kCFAllocatorDefault, 0);
        if (bsd_path_cf_string) {
            // If the device file path exists, append it to the list of paths
            char *path = list_append(devices, OPENDMX_MAX_DEV_NAME_LENGTH);
            Boolean result = CFStringGetCString(bsd_path_cf_string, path, OPENDMX_MAX_DEV_NAME_LENGTH, kCFStringEncodingUTF8);
            CFRelease(bsd_path_cf_string);
            
            if (!result) {
                list_remove(devices, devices->length - 1);
            }
        }
        (void) IOObjectRelease(modem_service);
    }
    // Create an return an iterator of the list of device files
    struct opendmx_iterator *device_list = (struct opendmx_iterator*)malloc(sizeof(struct opendmx_iterator));
    device_list->list = devices;
    device_list->iterator = list_iterator(devices);
    return device_list;
#elif __linux__
    // linux
    char* name = SERIAL_PATH;
    struct list *devices = (struct list *)malloc(sizeof(struct list));
    devices->first = NULL;
    devices->length = 0;
    
    size_t num_paths;
    glob_t globed_paths;
    char **paths;
    
    // Get all of the devices in /sys/class/tty which have a /device/driver file. And devices with out a driver to not acutally exist.
    glob(name, 0, 0, &globed_paths);
    num_paths = globed_paths.gl_pathc;
    for (paths = globed_paths.gl_pathv; num_paths > 0; paths++, num_paths--) {
        char *p = trim_path(*paths);    // Trim everything but the device name from the path
        int len = (int)strlen(p) + 6;
        char *path = list_append(devices, len);
        snprintf(path, len, DEVICE_FORMAT, p);  // Prepend /dev/ to the device name to get the device file path
        free(p);
    }
    globfree(&globed_paths);
    
    struct opendmx_iterator *device_list = (struct opendmx_iterator*)malloc(sizeof(struct opendmx_iterator));
    device_list->list = devices;
    device_list->iterator = list_iterator(devices);
    return device_list;
#else
#   error "Unsupported platform"
#endif
    return 0;
}
#endif // not OPENDMX_USE_D2XX

int opendmx_is_running (opendmx_device *device) {
    return device->running;
}

int opendmx_has_error (opendmx_device *device) {
    return device->error;
}

// MARK: Iterator
char *opendmx_iterator_next (struct opendmx_iterator *iter) {
    return list_iterator_next(iter->iterator);
}

int opendmx_iterator_has_next (const struct opendmx_iterator *iter) {
    return iter->iterator->next != NULL;
}

int opendmx_iterator_length (const struct opendmx_iterator *iter) {
    return iter->list->length;
}

void opendmx_iterator_free (struct opendmx_iterator *iter) {
    free(iter->iterator);
    list_free(iter->list);
    free(iter);
}

int opendmx_iterator_to_array (const struct opendmx_iterator *iter, char **buffer, int max_entries) {
    return list_array(iter->list, buffer, max_entries);
}
