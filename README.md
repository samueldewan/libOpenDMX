# libOpenDMX

A library for outputing DMX with a serial device. 
The library is designed to work with FTDI based devices (like the Enttec Open DMX USB), it may work with other serial devices which support 250 kbaud (ie. RPi uart).
libOpenDMX.c uses the virtual serial port drivers (preinstalled on macOS and most Linux distros) to interact with the serial port through a device file.
libOpenDMXD2XX.c uses the FTDI direct drivers to intertact with the serial port. It is suitable for use on Windows or Linux and macOS with the virtual serial port driver unloaded.
