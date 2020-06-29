// Main program module for Weather Station interface

// Copyright (c) 2006, Ian Chapman (Chapmip Consultancy)

// All rights reserved, except for those rights implicitly granted to
// GitHub Inc by publishing on GitHub and those rights granted by
// commercial agreement with the author.


#include <dcdefs.h>
#include <stcpip.h>
#include <stdio.h>
#include "timeout.h"
#include "wx_board.h"
#include "lan.h"
#include "udpdebug.h"
#include "report.h"
#include "tasks.h"
#include "eeprom.h"
#include "bb_vars.h"
#include "menu.h"
#include "wx_main.h"


// Short-cut names for types of report output (see "report.h")

#define PROBLEM		(REPORT_MAIN | REPORT_PROBLEM)
#define INFO		(REPORT_MAIN | REPORT_INFO)
#define DETAIL		(REPORT_MAIN | REPORT_DETAIL)

#define RAW_INFO	(INFO | REPORT_RAW)
#define RAW_DETAIL	(DETAIL | REPORT_RAW)


// Time constants

#define LAMP_TEST_SECS		1
#define MENU_PAUSE_SECS		3
#define RESET_DELAY_SECS	30


// Set up socket buffers

const char MAX_TCP_SOCKET_BUFFERS = 2;			// HTTP POST and Download connections
const char MAX_UDP_SOCKET_BUFFERS = 1;     		// UDP Debug


// Internal variables

static FILE * local_stdio;						// Handle for local (non-UDP) stdio port
static unsigned char udp_debug_active;			// Flag to indicate UDP debugging enabled


// *** INTERNAL FUNCTIONS ***

// Buffers for diagnostic serial output (if running from Flash)

static char diagInBuf[16];
static char diagOutBuf[1024];


// Initialise SerialA as stdio port if running from Flash
// or leaves stdio unchanged if running from RAM
// Always sets local_stdio to current stdio value
// Returns 0 on success or < 0 on failure

static int init_stdio(void)
	{
	if (_inFlash())
		{
    	if (!SerialInitA(BR_115200, SER_8BITS | SER_USE_C, SER_IP2, \
			 diagInBuf, sizeof(diagInBuf), diagOutBuf, sizeof(diagOutBuf)))
			{
			report(PROBLEM, "Could not initialise Serial Port A");
			local_stdio = _stdio;
			return -1;
			}
		_stdio = SerialA;
		}
	local_stdio = _stdio;
    return 0;
	}


// Attempt to activate UDP debugging and re-map stdio output to it
// Sends success or failure message to console before change
// Returns 0 on success or < 0 on failure

static int start_udp_debug(void)
	{
    if (debug_init(1) != 0)
		{
		report(PROBLEM, "Unable to switch on UDP debugging");
		return -1;
		}

	report(INFO, "Switching to UDP debug console");

	_stdio = debug_stdio;
	udp_debug_active = 1;

	return 0;
	}


// Pause for a specified number of milliseconds
// Calls net_tick() while waiting

static void pause_ms(unsigned int ms)
	{
	unsigned int tout;

    tout = SET_TIMEOUT_UI_MS(ms);

	while (!CHK_TIMEOUT_UI_MS(tout))
		net_tick();
	}


// Carry out lamp test on startup

static void do_lamp_test(void)
	{
	wx_set_leds(LED_ALL, LED_AMBER);
	pause_ms(LAMP_TEST_SECS * 1000);
	wx_set_leds(LED_ALL, LED_OFF);
	}


// Invite user to enter menu by pressing [ESC] within a few seconds
// Returns 1 if [ESC] was pressed, or 0 if not

static int invite_menu(void)
	{
	unsigned int tout;

	tout = SET_TIMEOUT_UI_MS(MENU_PAUSE_SECS * 1000);

	report(RAW_INFO, "Press [ESC] within %u seconds to re-configure unit\r\n", MENU_PAUSE_SECS);

	while (!CHK_TIMEOUT_UI_MS(tout))
		{
        if (inchar() == MENU_ESC)
			return 1;
		}

	return 0;
	}


// Force watchdog reset

static void force_reset(void)
	{
	ipset3();				// Disable interrupts

	WDT_ENABLE();			// Enable watchdog
    WDT_250mS();

	while (1)				// Wait for watchdog reset
		;
	}


// *** EXTERNAL FUNCTIONS ***

// Carries out background tick functions on network (if active)

void net_tick(void)
	{
	if (lan_active)
		{
		tcp_tick(NULL);

		if (udp_debug_active)
			debug_tick();
		}
	}


// Check for input character from stdio
// Returns character if available, or EOF if none

int inchar(void)
	{
    net_tick();

	if (!_inFlash() && !udp_debug_active && !kbhit())
		return EOF;
	else
		return getchar();
	}


// Stop UDP debugging and re-maps stdio output to local console
// Must only be called after init_stdio() has been called

void stop_udp_debug(void)
	{
	if (udp_debug_active)
		{
		(void) debug_init(0);
		_stdio = local_stdio;
		udp_debug_active = 0;

		report(INFO, "Returned to local debug console");
		}
	}


// Get weather station ID (base value + rotary switch offset)

unsigned int get_station_id(void)
	{
    return ee_unit_info.id_base + (unsigned int) wx_rotary_sel;
	}


// Convert IP address to text string in dotted decimal format
// Returns pointer to string (valid until another call)
// N.B. Cannot be called recursively -- value is overwritten by next call

char * get_ip_string(longword ip_addr)
	{
	static char ip_buf[16];

	return inet_ntoa(ip_buf, ip_addr);
	}


// Main routine

void main(void)
	{
	WDT_DISABLE();
	startTimer(100, 0, 1);
	ipset0();

	lan_init_vars();
	udp_debug_active = 0;
	ee_unit_info.report_mode = 0;

	if (init_stdio() != 0)
		goto Delayed_Reset;

 	report(DETAIL, "Firmware version number %u.%02u", VER_MAJOR, VER_MINOR);

	wx_init_board();
	bb_init();

	do_lamp_test();

	if (ee_init() < 0)
		{
		wx_set_leds(LED_ALL, LED_RED);
		goto Delayed_Reset;
		}

	if (invite_menu())
		{
		if (menu_exec())
			goto Reset;
		}

	report(DETAIL, "Initialising tasks...");

	if (tasks_init() != TASKS_INIT_OK)
		goto Delayed_Reset;

	report(DETAIL, "Initialising LAN interface...");

	switch(lan_start())
		{
		case LAN_STARTED_OK:
			break;

		case LAN_ERR_ETH_DISC:
            goto Reset;

		case LAN_IFCONFIG_ERR:
		case LAN_IF_UP_ERR:
		case LAN_IF_UP_TIMEOUT:
            goto Lan_Hold_Off;

		case LAN_SOCK_INIT_ERR:
		case LAN_EE_PARM_ERR:
		default:
			goto Delayed_Reset;
		}

	report(RAW_DETAIL, "\r\n");

	lan_show_info(RAW_INFO);

	wx_get_switches();
	if (wx_switch_1)
		(void) start_udp_debug();

	report(DETAIL, "Waiting for first data collection...");

	for (;;)
		{
		switch(tasks_run())
			{
			case TASKS_OK:
				break;

			case TASKS_MENU:
				if (menu_exec())
					goto Reset;
				break;

			case TASKS_ETH_DOWN:
			case TASKS_COLLECT_FAIL:
			case TASKS_POST_FAIL:
				goto Reset;

			case TASKS_LAN_DOWN:
				goto Lan_Hold_Off;

			case TASKS_POST_START_ERR:
			case TASKS_BAD_STATE:
			default:
				goto Delayed_Reset;
			}
		}

// Exception handlers

Lan_Hold_Off:		// LAN is down
	report(DETAIL, "Holding off LAN prior to reset...");
	lan_hold_off();
	goto Reset;

Delayed_Reset:		// LAN may be up or down
	report(DETAIL, "Pausing prior to reset...");
	pause_ms(RESET_DELAY_SECS * 1000);
	/* Fall through to Reset */

Reset:				// LAN may be up or down
	report(DETAIL, "Resetting system...");
	pause_ms(1000);
	if (_inFlash())
        force_reset();
	}
