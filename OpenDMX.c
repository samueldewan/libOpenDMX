//
//  OpenDMX.c
//  OpenDMX
//
//  Created by Samuel Dewan on 2016-12-01.
//  Copyright © 2016 Samuel Dewan. All rights reserved.
//

# ifndef _WIN32

#include "OpenDMX.h"
#include "LinkedList.h"

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

static int send_packet (const opendmx_device *device) {
    int break_byte = 0;
    int error = set_baud_rate(device->device_desc, 76800);              // Drop to lower baud rate
    error = error || (write(device->device_desc, &break_byte, 1) != 1); // transmit a zero
    // At 76.8kbaud this will hold the line low for 104µs (break) then high (the stop bits) for 26µs (MAB)
    error = error || set_baud_rate(device->device_desc, 250000);        // Return to the porper baud rate
    error = error || (write(device->device_desc, &opendmx_start_byte, 1)  != 1);// send the start code
    error = error || (write(device->device_desc, device->slots, OPENDMX_UNIVERSE_LENGTH) != OPENDMX_UNIVERSE_LENGTH);// send the DMX slots
    return error;
}

void *opendmx_thread (void *device) {
    opendmx_start((opendmx_device*) device);
    return NULL;
}

int opendmx_start (opendmx_device *device) {
    device->running = 1;
    device->error = 0;
    uint8_t errors = 0;
    while (device->running) {
        if (send_packet(device)) {
            errors++;
        } else {
            errors = 0;
        }
        if (errors >= 8) {
            device->running = 0;
            device->error = 1;
            return -1;
        }
        struct timespec tim;
        tim.tv_sec = 0;
        tim.tv_nsec = opendmx_interpacket_time;
        nanosleep(&tim, NULL);
    }
    return 0;
}

void open_dmx_stop (opendmx_device *device) {
    device->running = 0;
}

int opendmx_close_device (opendmx_device *device) {
    open_dmx_stop(device);
    int result = close(device->device_desc);
    if (result != 0) {
        return result;
    }
    free(device);
    return 0;
}

uint8_t opendmx_get_slot (const opendmx_device *device, int slot) {
    return ((0 <= slot) && (slot < 512)) ? device->slots[slot] : 0;
}

int opendmx_set_slot (opendmx_device *device, int slot, uint8_t value) {
    if ((0 > slot) || (slot >= 512)) {
        return -1;
    }
    device->slots[slot] = value;
    return 0;
}

struct opendmx_iterator *open_dmx_get_devices () {
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
    while ((modem_service = IOIteratorNext(matching_services))) {
        CFTypeRef bsd_path_cf_string;
        bsd_path_cf_string = IORegistryEntryCreateCFProperty(modem_service, CFSTR(kIODialinDeviceKey), kCFAllocatorDefault, 0);
        if (bsd_path_cf_string) {
            char *path = list_append(devices, OPENDMX_MAX_DEV_NAME_LENGTH);
            Boolean result = CFStringGetCString(bsd_path_cf_string, path, OPENDMX_MAX_DEV_NAME_LENGTH, kCFStringEncodingUTF8);
            CFRelease(bsd_path_cf_string);
            
            if (!result) {
                list_remove(devices, devices->length - 1);
            }
        }
        (void) IOObjectRelease(modem_service);
    }
    struct opendmx_iterator *device_list = (struct opendmx_iterator*)malloc(sizeof(struct opendmx_iterator));
    device_list->list = devices;
    device_list->iterator = list_iterator(devices);
    return device_list;
#elif __linux__
    // linux
#else
#   error "Unsupported platform"
#endif
    return 0;
}

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

void opendmx_iterator_free (const struct opendmx_iterator *iter) {
    free(iter->iterator);
    list_free(iter->list);
    free(iter);
}

int opendmx_iterator_to_array (const struct opendmx_iterator *iter, char **buffer, int max_entries) {
    return list_array(iter->list, buffer, max_entries);
}

#endif // _WIN32
