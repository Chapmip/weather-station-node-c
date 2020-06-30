// Routines to manage LAN connection (Ethernet and IP)

// Copyright (c) 2006, Ian Chapman (Chapmip Consultancy)

// All rights reserved, except for those rights implicitly granted to
// GitHub Inc by publishing on GitHub and those rights granted by
// commercial agreement with the author.


#include <dcdefs.h>
#include <stcpip.h>
#include <stdio.h>
#include "timeout.h"
#include "wx_board.h"
#include "report.h"
#include "eeprom.h"
#include "wx_main.h"
#include "lan.h"


// Short-cut names for types of report output (see "report.h")

#define PROBLEM     (REPORT_LAN | REPORT_PROBLEM)
#define INFO        (REPORT_LAN | REPORT_INFO)
#define DETAIL      (REPORT_LAN | REPORT_DETAIL)

#define RAW_INFO    (INFO | REPORT_RAW)
#define RAW_DETAIL  (DETAIL | REPORT_RAW)


// Externally-visible variables

unsigned char lan_active;                       // Flag to indicate LAN connection opened


// Internal variables

static unsigned char lan_dhcp_used;             // Flag to indicate DHCP used


// Timeout values

#define DHCP_TOUT_SECS          6               // DHCP server response time-out (default: 6)
#define MAX_IF_UP_SECS          30              // Maximum time for IP interface to start
#define IF_BACK_OFF_SECS        17              // Back-off time prior to IP interface retry
#define HOLD_OFF_SECS           120             // Hold-off time after IP interface failure


// Other constants

#define IF_MAX_RETRIES          5               // Maximum retries to bring interface up


// Label strings (used in several places)

#define LABEL_MY_IP_ADDR        "My IP address: "
#define LABEL_NET_MASK          "Network mask:  "
#define LABEL_MAC_ADDR          "MAC address:   "
#define LABEL_ETH_MTU           "Ethernet MTU:  "
#define LABEL_DHCP_SVR          "DHCP server:   "
#define LABEL_DNS_SVR           "DNS server:    "
#define LABEL_ROUTER            "Router:        "
#define LABEL_DHCP_LEASE        "DHCP lease:    "
#define LABEL_CURR_TMR          "Current timer: "


// *** INTERNAL FUNCTIONS ***

// Attempt to set host name of unit (sent to DHCP server)
// to weather station identification prefix plus station ID
// Returns result of sethostname() call

static const char host_name_prefix[] = HOST_NAME_PREFIX;

static char * set_unit_host_name(void)
    {
    char buffer[sizeof(host_name_prefix) + 5];      // Up to 5 digits for unsigned int
    char *name;

    sprintf(buffer, "%s%u", host_name_prefix, get_station_id());

    name = sethostname(buffer);
    report(DETAIL, "Host name is %s", (name != NULL) ? name : "NULL");

    return name;
    }


// Report problem with ifconfig() command
// Description string and status value are included in output

static void report_ifconfig_err(char * desc, int status)
    {
    report(PROBLEM, "ifconfig(%s) failed with %d", desc, status);
    }


// Wait for IP interface to come up or to fail to come up
// Returns result values for lan_start() -- see header file

static int await_if_result(void)
    {
    int status;
    unsigned int tout;

    tout = SET_TIMEOUT_UI_SECS(MAX_IF_UP_SECS);

    do
        {
        tcp_tick(NULL);

        status = ifpending(IF_DEFAULT);

        switch(status)
            {
            case IF_COMING_UP:
            case IF_UP:
                break;                              // Valid states

            case IF_DOWN:
                report(PROBLEM, "Unable to bring up LAN interface");
                return LAN_IF_UP_ERR;

            default:
                report(PROBLEM, "ifpending() returned invalid state %d", status);
                return LAN_IF_UP_ERR;
            }

        if (CHK_TIMEOUT_UI_SECS(tout))
            {
            report(PROBLEM, "ifpending() timed out waiting for IF_UP");
            return LAN_IF_UP_TIMEOUT;
            }

        } while (status == IF_COMING_UP);

    return LAN_STARTED_OK;
    }


// Checks and reports the DHCP fallback status of the LAN connection
//
// If DHCP was not used or fallback did not occur then the LAN LED is set green
// If fallback occurred then the LAN LED is set red and the POST LED is set amber
// If an ifconfig() read error occurs then the LAN LED is set red
//
// Returns LAN_STARTED_OK for success whether or not fallback has occurred
// Returns LAN_IFCONFIG_ERR if an ifconfig() read error occurs

static int check_fallback(void)
    {
    int status;
    int dhcp_fb;

    if (lan_dhcp_used)
        {
        status = ifconfig(IF_DEFAULT, IFG_DHCP_FELLBACK, &dhcp_fb, IFS_END);
        if (status != 0)
            {
            report_ifconfig_err("FB?", status);
            wx_set_leds(LED_LAN, LED_RED);
            return LAN_IFCONFIG_ERR;                    // -- EXIT --
            }

        if (dhcp_fb != 0)
            {
            report(PROBLEM, "DHCP failed -- LAN interface in fallback mode");

            lan_dhcp_used = 0;

            status = ifconfig(IF_DEFAULT,
                              IFS_NAMESERVER_SET, inet_addr(LAN_DEF_DNS_SERVER_IP),
                              IFS_ROUTER_SET, inet_addr(LAN_DEF_ROUTER_IP),
                              IFS_END);
            if (status != 0)
                {
                report_ifconfig_err("FB-SET", status);
                wx_set_leds(LED_LAN, LED_RED);
                return LAN_IFCONFIG_ERR;                // -- EXIT --
                }

            wx_set_leds(LED_LAN, LED_RED);
            wx_set_leds(LED_POST, LED_AMBER);       // SPECIAL CASE
            return LAN_STARTED_OK;                      // -- EXIT --
            }
        }

    report(DETAIL, "Started LAN interface okay");
    wx_set_leds(LED_LAN, LED_GREEN);
    return LAN_STARTED_OK;                              // -- EXIT --
    }


// Show labelled IP address value in dotted decimal format

static void show_ip_value(char * label, longword ip_addr)
    {
    printf("%s%s\r\n", label, get_ip_string(ip_addr));
    }


// Show detailed LAN parameters (if available)

static void show_lan_parms(void)
    {
    longword ipaddr;
    longword netmask;
    unsigned char mac_buf[6];
    word mtu;

    if (ifconfig(IF_DEFAULT, IFG_IPADDR, &ipaddr, IFG_NETMASK, &netmask, \
                 IFG_HWA, mac_buf, IFG_MTU, &mtu, IFS_END) == 0)
        {
        show_ip_value(LABEL_MY_IP_ADDR, ipaddr);
        show_ip_value(LABEL_NET_MASK, netmask);

        printf(LABEL_MAC_ADDR "%02X:%02X:%02X:%02X:%02X:%02X\r\n", mac_buf[0], \
                mac_buf[1], mac_buf[2], mac_buf[3], mac_buf[4], mac_buf[5]);

        printf(LABEL_ETH_MTU "%u\r\n", mtu);
        }
    else
        {
        printf("Unable to read LAN parms\r\n\r\n");
        }
    }


// Show static IP parameters not included in LAN parameters

static void show_static_parms(void)
    {
    show_ip_value(LABEL_DNS_SVR, ee_lan_info.dns_server_ip);
    show_ip_value(LABEL_ROUTER, ee_lan_info.router_ip);
    }


// Show detailed DHCP parameters (if available)

static void show_dhcp_parms(void)
    {
    DHCPInfo * info_ptr;
    unsigned int i;

    if (ifconfig(IF_DEFAULT, IFG_DHCP_INFO, &info_ptr, IFS_END) == 0)
        {
        show_ip_value(LABEL_DHCP_SVR, info_ptr->dhcp_server);

        for (i = 0; i < DHCP_NUM_DNS; ++i)
            show_ip_value(LABEL_DNS_SVR, info_ptr->dns[i]);

        for (i = 0; i < DHCP_NUM_ROUTERS; ++i)
            show_ip_value(LABEL_ROUTER, info_ptr->router[i]);

        printf(LABEL_DHCP_LEASE "%lu (t1 = %lu, t2 = %lu)\r\n", \
                info_ptr->lease, info_ptr->t1, info_ptr->t2);
        printf(LABEL_CURR_TMR "%lu\r\n", getSeconds());
        }
    else
        {
        printf("Unable to read DHCP parms\r\n");
        }
    }


// Show fallback static IP parameters not included in LAN parameters

static void show_fallback_parms(void)
    {
    show_ip_value(LABEL_DNS_SVR, inet_addr(LAN_DEF_DNS_SERVER_IP));
    show_ip_value(LABEL_ROUTER, inet_addr(LAN_DEF_ROUTER_IP));
    }


// *** EXTERNAL FUNCTIONS ***

// Initialise LAN variables
// (must only be called once at start-up of application)

void lan_init_vars(void)
    {
    lan_active = 0;
    }


// Attempts to set up LAN interface
// Waits forever for Ethernet connection to become active
// Returns 0 on success or < 0 on failure (see header file for values)

int lan_start(void)
    {
    int status;
    unsigned int tout;
    unsigned char retry_ctr;

    lan_active = 0;

    usingRealtek();                 // Or usingAll();

    status = sock_init();

    if (status != 0)
        {
        report(PROBLEM, "sock_init() failed with %d", status);
        wx_set_leds(LED_LAN, LED_RED);
        return LAN_SOCK_INIT_ERR;                   // -- EXIT --
        }

    while (!pd_havelink(IF_DEFAULT))
        ;

    report(DETAIL, "Ethernet connection is active");
    wx_set_leds(LED_LAN, LED_AMBER);

    if (ee_lan_valid == 0)
        {
        report(PROBLEM, "EEPROM parameters for LAN are invalid");
        wx_set_leds(LED_LAN, LED_RED);
        return LAN_EE_PARM_ERR;                     // -- EXIT --
        }

    (void) set_unit_host_name();

    retry_ctr = IF_MAX_RETRIES;

    for (;;)
        {
        if (ee_lan_info.use_static == 0)
            {
            status = ifconfig(IF_DEFAULT, IFS_DHCP, 1,
                              IFS_DHCP_TIMEOUT, DHCP_TOUT_SECS,
                              IFS_DHCP_FALLBACK, (retry_ctr == 1),
                              IFS_IPADDR, inet_addr(LAN_DEF_IP_ADDR),
                              IFS_NETMASK, inet_addr(LAN_DEF_NETMASK),
                              IFS_UP, IFS_END);
            lan_dhcp_used = 1;
            }
        else
            {
            status = ifconfig(IF_DEFAULT, IFS_DHCP, 0,
                              IFS_IPADDR, ee_lan_info.ip_addr,
                              IFS_NETMASK, ee_lan_info.netmask,
                              IFS_NAMESERVER_SET, ee_lan_info.dns_server_ip,
                              IFS_ROUTER_SET, ee_lan_info.router_ip,
                              IFS_UP, IFS_END);
            lan_dhcp_used = 0;
            }

        if (status != 0)
            {
            report_ifconfig_err("UP", status);
            wx_set_leds(LED_LAN, LED_RED);
            return LAN_IFCONFIG_ERR;                // -- EXIT --
            }

        status = await_if_result();

        if (status == LAN_STARTED_OK)
            break;

        if (--retry_ctr == 0)
            {
            report(PROBLEM, "Maximum retries exceeded");
            wx_set_leds(LED_LAN, LED_RED);
            return status;                          // -- EXIT --
            }

        report(INFO, "Retrying in %u seconds...", IF_BACK_OFF_SECS);

        status = ifconfig(IF_DEFAULT, IFS_DOWN, IFS_END);

        if (status != 0)
            {
            report_ifconfig_err("DOWN", status);
            wx_set_leds(LED_LAN, LED_RED);
            return LAN_IFCONFIG_ERR;                // -- EXIT --
            }

        tout = SET_TIMEOUT_UI_SECS(IF_BACK_OFF_SECS);

        while (!CHK_TIMEOUT_UI_SECS(tout))
            {
            tcp_tick(NULL);

            if (!pd_havelink(IF_DEFAULT))
                {
                report(DETAIL, "Ethernet connection has gone down");
                wx_set_leds(LED_LAN, LED_OFF);
                return LAN_ERR_ETH_DISC;            // -- EXIT --
                }
            }

        report(INFO, "Retrying...");
        }

    status = check_fallback();
    if (status != LAN_STARTED_OK)
        return status;                              // -- EXIT --

    lan_active = 1;             // Success!
    return LAN_STARTED_OK;                          // -- EXIT --
    }


// Display information about LAN connection
// Report is only sent if source and type specified by type_flags is enabled

void lan_show_info(unsigned char type_flags)
    {
    int status;
    int dhcp;
    int dhcp_ok;
    int dhcp_fb;

    if (!report_check_active(REPORT_LAN | (type_flags & REPORT_TYPE_MSK)))
        return;

    status = ifconfig(IF_DEFAULT, IFG_DHCP, &dhcp, IFG_DHCP_OK, &dhcp_ok,
                      IFG_DHCP_FELLBACK, &dhcp_fb, IFS_END);

    if (status == 0)
        {
        if (dhcp == 0)
            printf("Static IP configuration (DHCP disabled)\r\n");
        else if (dhcp_fb == 0)
            printf("DHCP enabled (%s)\r\n", dhcp_ok ? "Lease OK" : "Lease EXPIRED");
        else
            printf("Fallback static IP configuration (DHCP failed)\r\n");
        }
    else
        {
        printf("Unable to read DHCP state\r\n");
        }

    show_lan_parms();

    if (status == 0)
        {
        if (dhcp == 0)
            show_static_parms();
        else if (dhcp_fb == 0)
            show_dhcp_parms();
        else
            show_fallback_parms();
        }

    printf("\r\n");
    }


// Get own IP address in network order (first byte = first field)
// Returns 32-bit IP address on success, 0 on failure

longword lan_get_network_ip(void)
    {
    longword my_ip;

    // Get IP address in Rabbit host format (little-endian)
    if (ifconfig(IF_DEFAULT, IFG_IPADDR, &my_ip, IFS_END) == 0)
        {
        my_ip = htonl(my_ip);       // Convert to network order (big-endian)
        return my_ip;
        }
    else
        return 0L;
    }


// Checks that LAN interface is okay
// Stops UDP debugging (if active) and clears lan_active flag if not okay
// Returns 0 if okay or < 0 if not okay (see header file for values)

int lan_check_ok(void)
    {
    int dhcp_ok;

    if (!pd_havelink(IF_DEFAULT))
        {
        stop_udp_debug();
        lan_active = 0;
        report(PROBLEM, "Ethernet interface has gone down");
        wx_set_leds(LED_LAN, LED_OFF);
        return LAN_ETH_DOWN;
        }

    if (!ifstatus(IF_DEFAULT))
        {
        stop_udp_debug();
        lan_active = 0;
        report(PROBLEM, "IP interface has gone down");
        wx_set_leds(LED_LAN, LED_RED);
        return LAN_IF_DOWN;
        }

    // The following explicit check on DHCP_OK may not be necessary since
    // ifdown() is called when lease expires and DHCP_OK flag is cleared

    if (lan_dhcp_used)
        {
        if (ifconfig(IF_DEFAULT, IFG_DHCP_OK, &dhcp_ok, IFS_END) == 0)
            {
            if (!dhcp_ok)
                {
                stop_udp_debug();
                lan_active = 0;
                report(PROBLEM, "DHCP lease has expired");
                wx_set_leds(LED_LAN, LED_RED);
                return LAN_DHCP_DOWN;
                }
            }
        else
            {
            report(PROBLEM, "Unable to read DHCP status -- assumed okay");
            }
        }

    return LAN_OK;
    }


// Wait for hold-off time or until Ethernet interface goes down

void lan_hold_off(void)
    {
    unsigned int tout;

    tout = SET_TIMEOUT_UI_SECS(HOLD_OFF_SECS);

    while (!CHK_TIMEOUT_UI_SECS(tout))
        {
        if (!pd_havelink(IF_DEFAULT))
            {
            wx_set_leds(LED_LAN, LED_OFF);
            return;
            }
        }
    }
