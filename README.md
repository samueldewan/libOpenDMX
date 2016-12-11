# libOpenDMX

A library for outputing DMX with a serial device. 

Designed to work with FTDI based devices such as Enttec's Open DMX USB, this libary is suitable for use on macOS, Linux or Windows. This library should also work with any serial port which supports 250kbaud and 76.8kbaud on macOS and Linux. On Windows it supports any FTDI USB to serial adaptor.

When used in a POSIX compatable environment libOpenDMX.c uses the virtual serial port drivers (preinstalled on macOS and most Linux distros) to interact with the serial port through a device file. On Windows FTDI's D2XX driver is used to directly interface with a USB to serial adaptor, this results in 



This library is in the very early stages of development. At this point macOS support is implemented for the most part, but untested. Linux support for changing the serial port baud rate and identifying serial devices is yet to be implemented. The D2XX version of the library, for use on Windows, is not yet implemented.

### Example Use:

```C
#include "OpenDMX.h"

char **avaliable_devices = open_dmx_get_devices();
// Determine if any devices are avaliable, and if they are, which device should be used here

opendmx_device *universe = opendmx_open_device(avaliable_devices[0]); // 0 is a placeholder

```



### Warning:

This libary and the example code in this repository are incomplete and as of yet untested. While this code is unlikely to cause damage to any equipment unless paired with faulty hardware, I would recomend extensive testing before trusting this library to run a show.

 I take even less than the normal no responsability for any damage which may result from the use of this code.