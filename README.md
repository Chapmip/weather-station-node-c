# Important preamble

***THIS IS NOT OPEN SOURCE SOFTWARE*** ⁠— See [NO_LICENSE.TXT](/NO_LICENSE.TXT) for further information.

The purpose of publishing this code is to demonstrate a modular approach to a design of embedded 'C' drivers and application code for an Internet-connected nodal device.

Not all components of the final build are included here.  The missing elements are third-party commercial products that have been licensed for use in the end-product.  Some details of the client implementation have also been redacted (see below).

# Weather station node controller

These 'C' code modules are drawn from my commercial design in 2006-7 for a node controller to connect a weather station over the Internet to a central server for regular relaying of weather data.  With the agreement of my client, I have published my code with implementation-specific details replaced by placeholders (marked with "%%" in code files).

The processing platform for this node controller is a [RabbitCore® RCM3720 module](https://www.digi.com/products/models/20-101-1329).  The development toolchain was the [Softools Rabbit 'C' compiler](https://www.softools.com/scrabbit.htm) and associated libraries.

See [References](/README.md#references) section for further information.

# Quick links

* [Overall purpose of node controller](/README.md#overall-purpose-of-node-controller)
* [Hierarchy of code modules](/README.md#hierarchy-of-code-modules)
* [Descriptions of code modules](/README.md#descriptions-of-code-modules)
 * [My code modules (included)](/README.md#my-code-modules-included)
 * [Third-party files (not included)](/README.md#third-party-files-not-included)
* [References](/README.md#references)

# Overall purpose of node controller

Each node controller is paired with a weather station (from [Davis Instruments Corp.](https://www.davisinstruments.com/)) at a series of locations.  The node controller connects to the weather station via a serial interface and to the local area network via an Ethernet cable.  The weather station is configured to collect data from the weather station at periodic intervals and to relay these data over the Internet to a central server using a subset of the HTTP protocol (v1.1).

The node controller can be monitored and configured either over a direct serial connection or remotely (via a UDP connection to a Windows PC elsewhere on the local network or beyond).  The node controller also includes the capability for remote firmware updating on demand from a binary file held on the central server.  Configuration parameters for the node controller are held in non-volatile memory (EEPROM for static data and battery-backed RAM for dynamic data) so that the node controller can recover readily from a power interruption.

# Hierarchy of code modules

The code modules in this repository fall into the following hierarchy:

![Hierarchy of weather station code modules](/photos/wx-code-hierarchy.png?raw=true "Hierarchy of weather station code modules")

# Descriptions of code modules

## My code modules (included)

The following code files have been written by the author and are therefore included here.

### [`wx_main.c`](/code/wx_main.c) module (and [wx_main.h`](/code/wx_main.h) header)

The `wx_main.c` module (and .h header) ... xxxxxx


## Third-party files (not included)

The following third-party files are required to complete the build but are not included here.

### `Cstart` module (.asm file)

This third-party module is part of the [Softools Rabbit 'C' compiler](https://www.softools.com/scrabbit.htm) library.  It handles the low-level startup of the Rabbit module from power-up or reset, then passes control to the 'C' `main()` function.

The `Cstart` module needs to be linked into the overall project build.

### `SCRabbit` module (.lib file)

This third-party module is part of the [Softools Rabbit 'C' compiler](https://www.softools.com/scrabbit.htm) library.  It provides a set of functions specifically associated with the Rabbit module (as distinct from standard 'C' libraries).

The `SCRabbit` module needs to be linked into the overall project build.

### `Bootp_01` module (.c file)

This third-party module is part of the [Softools Rabbit 'C' compiler](https://www.softools.com/scrabbit.htm) library.  It provides support functions for setting up the Ethernet and LAN interfaces on the local network, including the DHCP protocol for automatic assignment of IP address and associated network parameters.

The `Bootp_01` module does not directly expose variables and functions to application code, but needs to be linked into the overall project build.

### `STCPIP-DHCP` module (.lib file)

This third-party module is part of the [Softools Rabbit 'C' compiler](https://www.softools.com/scrabbit.htm) library.  It provides support functions for setting up the Ethernet and LAN interfaces on the local network, including the DHCP protocol for automatic assignment of IP address and associated network parameters.

The `STCPIP-DHCP` module does not directly expose variables and functions to application code, but needs to be linked into the overall project build.

### `udpdebug` module (.c and .h files)

This third-party module is a free library offered by [SHDesigns](https://www.shdesigns.org) that can be downloaded from [here](https://www.shdesigns.org/rabbit/udpdebug.shtml).  It enables the debug console output from the weather station node controller to be accessed over a UDP connection from a Windows PC on the local network (or potentially at a remote location using UDP tunneling).  The PC needs to be running the corresponding UDP Debug application that is supplied with the library.

The `udpdebug` module exposes the following variables and functions in its header file (`udpdebug.h`):

    extern char debug_autocr;
    extern FILE debug_stdio[1];
    
    int debug_init(int ena);
    int debug_kbhit(void);
    int near debug_getchar(void);
    void near debug_putchar(char c);
    int debug_tick(void);

### `WEB_DL` module (.c and .h files)

This third-party module is a commercial library offered by [SHDesigns](https://www.shdesigns.org) that manages the process of remote firmware updates.  The library enables the weather station node controller to poll for a firmware update hosted on the central server.  If an update is available, then the corresponding binary file can be downloaded into a holding memory and then flashed into program memory.  The library includes safeguards to protect the node controller from a failed or partial download, or various other error conditions during  the update process.  Further information is available [here](https://www.shdesigns.org/rabbit/resident.shtml).

The `WEB_DL` module exposes the following variables and functions in its header file (`WEB_DL.h`):

    extern unsigned _sector_size;
    
    void set_flash_start(long addr);
    int write_sector(long block,char * buff, int bsize);
    int CheckWebVersion(char * url, char * server_ip, unsigned port, long * version);
    int GetWebUpdate(void);

# References

## Toolchain and libraries

* [Softools Rabbit 'C' compiler](https://www.softools.com/scrabbit.htm)
* [SHDesigns UDP Debug library (Free)](https://www.shdesigns.org/rabbit/udpdebug.shtml)
* [SHDesigns Web Download Library (Commercial Product)](https://www.shdesigns.org/rabbit/resident.shtml)
* [Dynamic C I2C library](https://ftp1.digi.com/support/documentation/0220061_b.pdf)

## Rabbit core module

* [Rabbit Semiconductor](https://en.wikipedia.org/wiki/Rabbit_Semiconductor)
* [RabbitCore® RCM3700 Series Microprocessor Core Modules](https://www.digi.com/products/embedded-systems/system-on-modules/rcm3700)
* [RCM3720 module](https://www.digi.com/products/models/20-101-1329)

## Weather station

* [Davis Instruments Corp.](https://www.davisinstruments.com)
* [Vantage Pro™ and Vantage Pro2™ weather station](https://www.davisinstruments.com/solution/vantage-pro2/)
* [Vantage Pro™, Vantage Pro2™ and Vantage Vue™ Serial Communication Reference Manual](https://www.davisinstruments.com/support/weather/download/VantageSerialProtocolDocs_v261.pdf)

## Other references

* [Information on 16-bit CCITT standard CRC for blocks of data sent by Vantage Pro1™ weather station](http://srecord.sourceforge.net/crc16-ccitt.html)
* [Data sheet for 24LC64 Microchip® 64K I2C Serial EEPROM](http://ww1.microchip.com/downloads/en/devicedoc/21189f.pdf)
