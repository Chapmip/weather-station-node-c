// Header file for routines to manage LAN connection (Ethernet and IP)

// Copyright (c) 2006, Ian Chapman (Chapmip Consultancy)

// All rights reserved, except for those rights implicitly granted to
// GitHub Inc by publishing on GitHub and those rights granted by
// commercial agreement with the author.


#ifndef	LAN_H
#define	LAN_H

// External variables

extern unsigned char lan_active;

// Function prototypes

void lan_init_vars(void);
int lan_start(void);
void lan_show_info(unsigned char type_flags);
longword lan_get_network_ip(void);
int lan_check_ok(void);
void lan_hold_off(void);

// Return values from lan_start()

#define LAN_STARTED_OK			0
#define LAN_SOCK_INIT_ERR		(-1)
#define LAN_EE_PARM_ERR			(-2)
#define LAN_IFCONFIG_ERR		(-3)
#define LAN_IF_UP_ERR			(-4)
#define LAN_IF_UP_TIMEOUT		(-5)
#define LAN_ERR_ETH_DISC		(-6)

// Return values from lan_check_ok()

#define LAN_OK					0
#define LAN_ETH_DOWN			(-1)
#define	LAN_IF_DOWN				(-2)
#define LAN_DHCP_DOWN			(-3)

// Default LAN parameters (used as fallback on DHCP failure)

#define LAN_DEF_IP_ADDR			"192.168.0.100"
#define LAN_DEF_NETMASK			"255.255.255.0"
#define LAN_DEF_DNS_SERVER_IP	"192.168.0.254"
#define LAN_DEF_ROUTER_IP		"192.168.0.254"

#endif
