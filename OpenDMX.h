//
//  OpenDMX.h
//  OpenDMX
//
//  Created by Samuel Dewan on 2016-12-01.
//  Copyright © 2016 Samuel Dewan. All rights reserved.
//

#ifndef OpenDMX_h
#define OpenDMX_h

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <inttypes.h>

#ifdef __APPLE__
// macOS
#include <IOKit/IOKitLib.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/IOBSD.h>
#elif __linux__
// Linux
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

typedef struct opendmx_handle opendmx_device;

struct opendmx_iterator;

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
extern opendmx_device *opendmx_open_device(const char* device_file);

/**
 *  A helper function designed to be used with a pthread. Calls opendmx_start.
 *  @param device The divice on which to output DMX, must be an opendmx_device
 *  @returns NULL, will not return until opendmx_stop is called on the associated opendmx_device.
 */
extern void *opendmx_thread (void *device);

/**
 *  Starts DMX output.
 *  @note This function blocks the thread it is called on until dmx output for the device is stoped.
 *  @param device The divice on which to output DMX
 *  @return 0 if DMX output is successful, < 0 if
 */
extern int opendmx_start (opendmx_device *device);

/**
 *  Stops DMX output.
 *  @param device The device for which the DMX output is to be stopped.
 */
extern void open_dmx_stop (opendmx_device *device);

/**
 *  Closes and frees opendmx device.
 *  @param device The handle to be closed.
 *  @return 0 if device successfully closed, > 0 otherwise
 */
extern int opendmx_close_device (opendmx_device *device);

/**
 *  Gets the value for a DMX slot.
 *  @param device The device to get the value from.
 *  @param slot The slot to get.
 *  @returns The value for the specified slot, zero if the slot does not exist.
 */
extern uint8_t opendmx_get_slot (const opendmx_device *device, int slot);

/**
 *  Set the value for a DMX slot.
 *  @param device The in which to set the slot.
 *  @param slot The slot to be assigned a new value.
 *  @value The new value for the slot.
 *  @returns 0 if the assignment was successful, < 0 otherwise (ie. the slot does not exist)
 */
extern int opendmx_set_slot (opendmx_device *device, int slot, uint8_t value);

/**
 *  Check if opendmx device is outputing DMX
 *  @returns 1 if DMX output is active, 0 otherwise.
 */
extern int opendmx_is_running (opendmx_device *device);

/**
 *  Check if opendmx device encountered an error since it was last started.
 *  @note The opendmx device stops transmitting an registers an error if more than 8 packets in a row fail.
 *  @returns 1 if an error had occured, 0 otherwise.
 */
extern int opendmx_has_error (opendmx_device *device);

/**
 *  Get a list of avaliable serial ports which could be used for DMX output. One macOS and Linux devices are referenced by device file name (ie. /dev/ttyUSB0). On Windows devices are referenced by serial number, and only FTDI serial devices will be listed.
 *  @note Devices listed are not nessasarly openDMX devices, and may not even support DMX output at all (the only real requirment is that the device supports 250kbaud and 72.8k baud)
 *  @returns An iterator for the avaliable serial ports.
 */
extern struct opendmx_iterator *open_dmx_get_devices ();

/**
 *  Get the next item from an opendmx iterator.
 *  @param iter The iterator to get the next item from.
 *  @returns The next item from the iterator, NULL if there are no further items.
 */
extern char *opendmx_iterator_next (struct opendmx_iterator *iter);

/**
 *  Determines if an opendmx_iterator has more devices avaliable.
 *  @param iter The iterator.
 *  @returns 0 if there are no more items left, 1 otherwise.
 */
extern int opendmx_iterator_has_next (const struct opendmx_iterator *iter);

/**
 *  Determin the total number of items in an opendmx iterator.
 *  @param iter The iterator.
 *  @returns The number of items in the iterator.
 */
extern int opendmx_iterator_length (const struct opendmx_iterator *iter);

/**
 *  Get an array of all of the devices in an iterator.
 *  @param iter The iterator.
 *  @param buffer The buffer in which to put the device identifiers.
 *  @param max_entries The maximum number of entries whcih can be put into the array.
 *  @returns The number of items in the array.
 */
int opendmx_iterator_to_array (const struct opendmx_iterator *iter, char **buffer, int max_entries);

/**
 *  Frees an opendmx iterator and all of it's associated values.
 *  @param iter The iterator.
 */
extern void opendmx_iterator_free (const struct opendmx_iterator *iter);

#endif /* OpenDMX_h */
