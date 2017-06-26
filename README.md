# libOpenDMX

A library for outputing DMX with a serial device. 

Designed to work with FTDI based devices such as Enttec's Open DMX USB, this libary is suitable for use on macOS, Linux or Windows. This library should also work with any serial port which supports 250kbaud and 56kbaud on macOS and Linux. On Windows it supports any FTDI USB to serial adaptor.

When used in a POSIX compatable environment libOpenDMX.c uses the virtual serial port drivers (preinstalled on macOS and most Linux distros) to interact with the serial port through a device file. On Windows FTDI's D2XX driver is used to directly interface with a USB to serial adaptor, which means that only FTDI hardware (like that used in the Enttec Open DMX USB) is supported.

Most of the libraries planed features are now implemented, however they have yet to be compiled on Linux or Windows and have not been tested with a real lighting system on macOS.

### Example Use:

```c
#include <pthread.h>
#include <OpenDMX.h>

struct opendmx_iterator *devices = opendmx_get_devices(); // Determine if any devices are avaliable, and if they are, which device should be used here

opendmx_device *universe = opendmx_open_device(opendmx_iterator_next(devices)); // Opens the first avaliable serial port, not a good idea for actual use as the first port will rarely actually be a DMX device

opendmx_iterator_free(devices); // Remeber to free the device iterator, but to only free it after opening the device as freeing the iterator will free all of it's device strings

pthread_t dmx_thread; // Run DMX output on a separate thread as it will block the thread it is running on
pthread_create(&dmx_thread, 0, opendmx_thread, universe); // libOpenDMX provides the opendmx_thread fuction for easy integration with pthreads

opendmx_set_slot(universe, 0, 255); // Do DMX stuffs here

opendmx_stop(universe); // Stops output on the DMX device, ending the DMX thread
opendmx_close_device(universe); // Close and free the DMX universe and all of it's atributes
```

### Warning:

This libary and the example code in this repository are incomplete and as of yet untested. While this code is unlikely to cause damage to any equipment unless paired with faulty hardware, I would recomend extensive testing before trusting this library to run a show.
