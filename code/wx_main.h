// Project-wide definitions for Weather Station interface

// Copyright (c) 2006, Ian Chapman (Chapmip Consultancy)

// All rights reserved, except for those rights implicitly granted to
// GitHub Inc by publishing on GitHub and those rights granted by
// commercial agreement with the author.


#ifndef WX_MAIN_H
#define WX_MAIN_H

#include <dcdefs.h>

// Firmware version number

#define VER_MAJOR           1
#define VER_MINOR           25          // Must not be 08 or 09!

// Prefix for host name sent to DHCP server

#define HOST_NAME_PREFIX    "weather-"

// Function prototypes

void net_tick(void);
int inchar(void);
void stop_udp_debug(void);
unsigned int get_station_id(void);
char * get_ip_string(longword ip_addr);

#endif
