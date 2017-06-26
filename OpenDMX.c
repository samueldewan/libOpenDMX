//
//  OpenDMX.c
//  OpenDMX
//
//  Created by Samuel Dewan on 2016-12-01.
//  Copyright © 2016 Samuel Dewan. All rights reserved.
//

#define _XOPEN_SOURCE 800

#include "OpenDMX.h"
#include "LinkedList.h"

#include <sys/ioctl.h>
#include <sys/time.h>

#define OPENDMX_USE_D2XX

#define OPENDMX_DATA_BAUD_RATE 250000
#define OPENDMX_BREAK_BAUD_RATE 56000   // At 56kbaud this will hold the line low for 143µs (break) then high (the stop bits) for 36µs (MAB)

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
#include <stdlib.h>

#include <linux/serial.h>

#define SERIAL_PATH     "/sys/class/tty/*/device/driver"
#define DEVICE_FORMAT   "/dev/%s"
#define SYS_PREFIX_LENGTH   15
#define SYS_POSTFIX_LENGTH  14

#elif _WIN32
#define OPENDMX_USE_D2XX
#else
#   error "Unsupported platform"
#endif

uint8_t opendmx_start_byte = 0;
long opendmx_interpacket_time = OPENDMX_PERIOD_LOW;

typedef struct opendmx_handle {
#ifdef OPENDMX_USE_D2XX
    void                    *ftdi_handle;
#else
    int                     device_handle;
#endif
    volatile unsigned int   running:1;
    volatile unsigned int   error:1;
    volatile unsigned int   lock:1;
    uint8_t                 slots[OPENDMX_UNIVERSE_LENGTH];
} opendmx_device;

struct opendmx_iterator {
    struct list             *list;
    struct list_iterator    *iterator;
};

static int send_packet (opendmx_device *device);
static int close_output (const opendmx_device *device);

# ifndef OPENDMX_USE_D2XX
opendmx_device *opendmx_open_device (const char *port_name) {
    struct opendmx_handle *device = malloc(sizeof(*device));
    
    // Get device file
    device->device_handle = open(port_name, O_WRONLY | O_NOCTTY | O_NDELAY | O_ASYNC | O_NONBLOCK);
    if (device->device_handle == -1) {
        goto error;     // failed to open device
    }
    
    // Block further attempts to open device while we are using it
    if (ioctl(device->device_handle, TIOCEXCL) != 0) {
        goto error;
    }
    
    // Now that device is open, enable blocking on further IO ops
    if (fcntl(device->device_handle, F_SETFL, 0) != 0) {
        goto error;     // failed to enable blocking
    }
    
    // Get current settings
    struct termios settings;

    if (tcgetattr(device->device_handle, &settings) != 0) {
        goto error;     // failed to get settings
    }
    
    // cflag (8N2)
    //settings.c_cflag &= ~CRTSCTS;   // No flow control
    settings.c_cflag &= ~PARENB;    // No parity check
    settings.c_cflag &= ~CSIZE;     // Clear size bytes
    settings.c_cflag |= CS8;        // 8 bits per char
    settings.c_cflag |= CSTOPB;     // 2 stop bits
    settings.c_cflag |= CLOCAL;     // Local mode
    
    // oflag (disable output processing)
    settings.c_oflag &= ~(OCRNL | ONLCR | ONLRET |  ONOCR | OFILL | OPOST);
    // lflag (disable line processing)
    settings.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG | ECHOE);
    // iflag (no software flow control)
    settings.c_iflag &= ~(IXON | IXOFF | IXANY);

    // Timout settings
    settings.c_cc[VMIN] = 1;
    settings.c_cc[VTIME] = 0;
    
    // Raw mode allows the use of ioctl?
#ifdef __APPLE__
    cfmakeraw(&settings);
#endif
    
    // Flush port
    tcflush(device->device_handle, TCOFLUSH);
    fcntl(device->device_handle, F_SETFL, 0);
    // Apply settings (applyright-away, clear io buffers)
    if (tcsetattr(device->device_handle, TCSAFLUSH, &settings) != 0) {
        goto error;     // failed to set settings
    }
    
    // Initialize universe
    for (int i = 0; i < OPENDMX_UNIVERSE_LENGTH; i++) {
        device->slots[i] = 0;
    }
    
    device->lock = 0;
    
    // It worked!
    return device;
    
    // Falure path
error:
    if (device->device_handle != -1) {
        close(device->device_handle);
    }
    free(device);
    
    return NULL;
}


static int set_baud_rate (const int device, const int speed) {
#ifdef __APPLE__
    // macOS - use ioctl
    return (ioctl(device, IOSSIOSPEED, &speed) != 0);
    // That was easy
    return 0;
#elif __linux__
    struct serial_struct ser;
    ioctl (device, TIOCGSERIAL, &ser);
    
    // set custom divisor
    ser.custom_divisor = ser.baud_base / speed;
    // update flags
    ser.flags &= ~ASYNC_SPD_MASK;
    ser.flags |= ASYNC_SPD_CUST;
    
    if (ioctl (device, TIOCSSERIAL, ser) < 0) {
        return 1;
    }
    return 0;
#else
#   error "Unsupported platform"
#endif
}

static int send_packet (const opendmx_device *device) {
    const int break_byte = 0; // Need to define this as a constant so I that can get a pointer to it
    int error = set_baud_rate(device->device_handle, OPENDMX_BREAK_BAUD_RATE);         // Drop to lower baud rate
    error = error || (write(device->device_handle, &break_byte, 1) != 1); // transmit a zero
    error = error || set_baud_rate(device->device_handle, OPENDMX_DATA_BAUD_RATE);        // Return to the proper baud rate
    error = error || (write(device->device_handle, &opendmx_start_byte, 1)  != 1); // send the start code
    error = error || (write(device->device_handle, device->slots, OPENDMX_UNIVERSE_LENGTH) != OPENDMX_UNIVERSE_LENGTH);// send the DMX slots
    return error;
}

static int close_output (const opendmx_device *device) {
    return (close(device->device_handle) != 0);
}

#endif  //  not OPENDMX_USE_D2XX

void *opendmx_thread (void *device) {
    opendmx_start((opendmx_device*) device);    // Start the DMX device
    return NULL;
}

int opendmx_start (opendmx_device *device) {
    device->running = 1;
    device->error = 0;
    uint8_t errors = 0; // Tracks the number of frames which have failed to send
    while (device->running) {   // Run as along as the device hasn't been told not to
        int lock_status = device->lock;
        device->lock = 1;
        errors = (errors << 1) | (send_packet(device) ? 1 : 0);
        if ((errors & 0xFF) == 0xFF) {
            // If 8 errors have occured in a row, stop DMX output and register an error. This usually means that the DMX output device has been disconected.
            device->running = 0;
            device->error = 1;
            device->lock = lock_status;
            return -1;
        }
        device->lock = lock_status;
        // Wait for the interpacket time
        struct timespec tim;
        tim.tv_sec = 0;
        tim.tv_nsec = opendmx_interpacket_time;
        nanosleep(&tim, NULL);
    }
    return 0;
}

void opendmx_stop (opendmx_device *device) {
    device->running = 0;
}

int opendmx_close_device (opendmx_device *device) {
    opendmx_stop(device);
    while (device->lock);
    if (close_output(device) != 0) return 1;
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

# ifndef OPENDMX_USE_D2XX
#ifdef __linux__
static char *trim_path(char *path) {
    // Get string starting at the beginig of the device name
    char *sub_path = (path + (sizeof(char) * SYS_PREFIX_LENGTH));
    // Get the length of the char up to the end of the device name
    int len = (int)strlen(sub_path) - SYS_POSTFIX_LENGTH;
    char *str = malloc(sizeof(char) * len);
    // Copy only the device name to a new string
    strncpy(str, sub_path, len);
    return str;
}
#endif // __Linux__

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
    
    struct list *devices = malloc(sizeof(*devices));
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
    struct opendmx_iterator *device_list = malloc(sizeof(*device_list));
    device_list->list = devices;
    device_list->iterator = list_iterator(devices);
    return device_list;
#elif __linux__
    // linux
    char* name = SERIAL_PATH;
    struct list *devices = malloc(sizeof(*devices));
    devices->first = NULL;
    devices->length = 0;
    
    size_t num_paths;
    glob_t globed_paths;
    char **paths;
    
    // Get all of the devices in /sys/class/tty which have a /device/driver file. Any devices with out a driver most likely do not acutally exist.
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
    
    struct opendmx_iterator *device_list = malloc(sizeof(*device_list));
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



//-------D2XX--------
#ifdef OPENDMX_USE_D2XX
#include "/usr/local/include/ftd2xx.h"

opendmx_device *opendmx_open_device(char* serial_number) {
    struct opendmx_handle *device = malloc(sizeof(*device));
    
    FT_STATUS ftstatus;
    
    // Open the device
    ftstatus = FT_OpenEx(serial_number, FT_OPEN_BY_SERIAL_NUMBER, &device->ftdi_handle);
//    ftstatus = FT_Open(0, &device->ftdi_handle);
    if (ftstatus != FT_OK) goto error;
    
    // Set device settings
    ftstatus = FT_SetBaudRate(device->ftdi_handle, 250000);
    if (ftstatus != FT_OK) goto error_with_open_device;
    ftstatus = FT_SetDataCharacteristics(device->ftdi_handle, FT_BITS_8, FT_STOP_BITS_2, FT_PARITY_NONE);
    if (ftstatus != FT_OK) goto error_with_open_device;
    
    // Initialize universe
    for (int i = 0; i < OPENDMX_UNIVERSE_LENGTH; i++) {
        device->slots[i] = 0;
    }
    
    device->lock = 0;
    
    return device;
    
error_with_open_device:
    FT_Close(device->ftdi_handle);
error:
    free(device);
    return NULL;
}

static int send_packet (opendmx_device *device) {
    int break_byte = 0; // Need to define this as a variable so I that can get a pointer to it
    uint bytes_sent = 0;
    int error = FT_SetBaudRate(device->ftdi_handle, OPENDMX_BREAK_BAUD_RATE) != FT_OK;                        // Drop to lower baud rate
    error = error || FT_Write(device->ftdi_handle, &break_byte, 1, &bytes_sent)  != FT_OK;  // transmit a zero
    error = error || bytes_sent != 1;
    error = error || FT_SetBaudRate(device->ftdi_handle, OPENDMX_DATA_BAUD_RATE) != FT_OK;                  // Return to the proper baud rate
    error = error || FT_Write(device->ftdi_handle, &opendmx_start_byte, 1, &bytes_sent) != FT_OK;// send the start code
    error = error || bytes_sent != 1;
    error = error || FT_Write(device->ftdi_handle, device->slots, OPENDMX_UNIVERSE_LENGTH, &bytes_sent) != FT_OK;  // send the DMX slots
    error = error || bytes_sent != OPENDMX_UNIVERSE_LENGTH;
    return error;
}

static int close_output (const opendmx_device *device) {
    return FT_Close(device->ftdi_handle) != FT_OK;
}

struct opendmx_iterator *opendmx_get_devices () {
    FT_STATUS ftstatus;
    unsigned int num_devs;
    
    struct list *devices = malloc(sizeof(*devices));
    devices->first = NULL;
    devices->length = 0;

    ftstatus = FT_ListDevices(&num_devs,NULL,FT_LIST_NUMBER_ONLY);
    if (ftstatus != FT_OK) goto error;
    
    for (int i = 0; i < num_devs; i++) {
        list_append(devices, 64);
    }
    
    char **serial_nums = malloc(sizeof(char*) * num_devs + 1);
    list_array(devices, serial_nums, num_devs);
    serial_nums[num_devs] = NULL;
    
    ftstatus = FT_ListDevices(serial_nums, &num_devs, FT_LIST_ALL|FT_OPEN_BY_SERIAL_NUMBER);
    
    free(serial_nums);
    
    // Create an return an iterator of the list of device files
    struct opendmx_iterator *device_list = malloc(sizeof(*device_list));
    device_list->list = devices;
    device_list->iterator = list_iterator(devices);
    return device_list;
error:
    list_free(devices);
    return NULL;
}

#endif // OPENDMX_USE_D2XX
