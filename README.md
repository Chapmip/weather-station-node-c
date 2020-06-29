# Important preamble

***THIS IS NOT OPEN SOURCE SOFTWARE*** ⁠— See [NO_LICENSE.TXT](/NO_LICENSE.TXT) for further information.

The purpose of publishing this code is to demonstrate a modular approach to a design of embedded 'C' drivers and application code for an Internet-connected nodal device.

Not all components of the final build are included here.  The missing elements are third-party commercial products that have been licensed for use in the end-product.  Some details of the client implementation have also been redacted (see below).

# Weather station node controller

These 'C' code modules are drawn from my commercial design in 2006-7 for a node controller to connect a weather station over the Internet to a central server for regular updates of weather data.  With the agreement of my client, I have published my code with implementation-specific details replaced by placeholders (marked with "%%" in code files).

The processing platform for this node controller is a [RabbitCore® RCM3720 module](https://www.digi.com/products/models/20-101-1329).  The development toolchain was the [Softools Rabbit 'C' compiler](https://www.softools.com/scrabbit.htm) and associated libraries.

See [References](/README.md#references) section for further information.

# Quick links

* [Hierarchy of code modules](/README.md#hierarchy-of-code-modules)
* [Descriptions of code modules](/README.md#descriptions-of-code-modules)
* [Third-party files (not included)](/README.md#third-party-files-not-included)
* [References](/README.md#references)

# Hierarchy of code modules

The code modules in this repository fall into the following hierarchy:

![Hierarchy of weather station code modules](/photos/wx-code-hierarchy.png?raw=true "Hierarchy of weather station code modules")

# Descriptions of code modules

The individual code modules are described below, together with guidance on their use.

## [`wx_main`](/code/wx_main.c) module

The `wx_main.c` module (and .h header) ... xxx


## Third-party files (not included)

The following third-party files are required to complete the build but are not included here:

* `Bootp_01.c` — xxx
* `Cstart.asm` — xxx
* `SCRabbit.lib` — xxx
* `STCPIP-DHCP.lib` — xxx
* `udpdebug.c` and `udpdebug.h` — xxx
* `WEB_DL.c` and `WEB_DL.h` — xxx

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
