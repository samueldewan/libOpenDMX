//
//  libOpenDMX.h
//  libOpenDMX
//
//  Created by Samuel Dewan on 2016-12-01.
//  Copyright © 2016 Samuel Dewan. All rights reserved.
//


#ifndef libOpenDMX_h
#define libOpenDMX_h

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <inttypes.h>

#include "linkedList.h"

#ifdef __APPLE__
// macOS
#include <IOKit/serial/ioss.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/IOBSD.h>
#elif __linux__
// linux
#else
#   error "Unsupported platform"
#endif

#define OPENDMX_UNIVERSE_LENGTH     512
// Packet length = 104µs (break) + 26µs (MAB) + 40µs (start) + 20480µs (slots) = 20650000 nanoseconds
// Interpacket times:
#define OPENDMX_PERIOD_HIGH         4350000         // 4350000 + 20650000 = 25000000 nanoseconds per packet = 40pps
#define OPENDMX_PERIOD_MID          12683333        // 12683333 + 20650000 = 33333333 nanoseconds per packet = 30pps
#define OPENDMX_PERIOD_LOW          29350000        // 29350000 + 20650000 = 50000000 nanoseconds per packet = 20pps

#define OPENDMX_MAX_DEV_NAME_LENGTH 64

struct opendmx_handle {
    int             device_desc;
    volatile int    running:1;
    uint8_t         slots[OPENDMX_UNIVERSE_LENGTH];
};

/**
 *  The start byte for dmx packets
 */
extern uint8_t opendmx_start_byte;

/**
 *  The time between packets in nanoseconds
 */
extern long opendmx_interpacket_time;

/**
 *  Opens an openDMX device for dmx_output.
 *  @param device_file Path to the serial device to be used, must support 250k and 76.8k baud
 *  @returns A pointer to an opendmx_handle, or NULL if device creation failed
 */
extern struct opendmx_handle* opendmx_open_device(const char* device_file);

/**
 *  Starts DMX output.
 *  @note This function blocks the thread it is called on until dmx output for the device is stoped.
 *  @param device The divice for which to output DMX
 *  @return 0 if DMX output is successful, < 0 if
 */
extern int opendmx_start (struct opendmx_handle *device);

/**
 *  Stops DMX output.
 *  @param device The device for which the DMX output is to be stopped.
 */
extern void open_dmx_stop (struct opendmx_handle *device);

/**
 * Closes and frees opendmx device.
 * @param device The handle to be closed.
 * @return 0 if device successfully closed, > 0 otherwise
 */
extern int opendmx_close_device (struct opendmx_handle *device);

/**
 * Gets the value for a DMX slot.
 * @param device The device to get the value from.
 * @param slot The slot to get.
 * @returns The value for the specified slot, zero if the slot does not exist.
 */
extern uint8_t opendmx_get_slot (struct opendmx_handle *device, int slot);

/**
 * Set the value for a DMX slot.
 * @param device The in which to set the slot.
 * @param slot The slot to be assigned a new value.
 * @value The new value for the slot.
 * @returns 0 if the assignment was successful, < 0 otherwise (ie. the slot does not exist)
 */
extern int opendmx_set_slot (struct opendmx_handle *device, int slot, uint8_t value);

#endif /* libOpenDMX_h */
