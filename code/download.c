// Wrapper routines for SHDesigns Web-Based Downloader

// Copyright (c) 2006, Ian Chapman (Chapmip Consultancy)

// All rights reserved, except for those rights implicitly granted to
// GitHub Inc by publishing on GitHub and those rights granted by
// commercial agreement with the author.


#include <stdio.h>
#include <string.h>
#include <stcpip.h>
#include "wx_board.h"
#include "report.h"
#include "eeprom.h"
#include "wx_main.h"
#include "download.h"
#include "WEB_DL.h"


// Short-cut names for types of report output (see "report.h")

#define PROBLEM		(REPORT_DOWNLOAD | REPORT_PROBLEM)
#define DETAIL		(REPORT_DOWNLOAD | REPORT_DETAIL)
#define INFO		(REPORT_DOWNLOAD | REPORT_INFO)


// Constant definitions

#define DL_HTTP_HDR			"http://"
#define DL_MAX_URL_LEN      ((sizeof(DL_HTTP_HDR) - 1) + (EE_POST_STR_MAX_LEN * 2))


// Only included for Serial Flash download

#ifdef COPY2FLASH
#ifdef EXTERNAL_STORAGE

#include <sflash.h>

// Routines to support EXTERNAL_STORAGE option in Web downloader library
// These routines support the RCM37x0 serial flash memory

// Initialise RCM 37x0 flash storage for writes
// Also sets _sector_size so that the library knows what block size to write
// Returns 0 if no error

int flash_init()
	{
	int err;

	sfspi_init();
	err = sf_init();
	if (err)
		{
#ifdef WEB_DEBUG
		report(PROBLEM, "Serial Flash init failed");
		net_tick();
#endif
		return err;
		}

	sf_setReverse(1);			// Required to prevent byte reversal

#ifdef WEB_DEBUG
	report(DETAIL, "Serial Flash Initialized");
	report(DETAIL, "# of blocks: %d", sf_blocks);
	report(DETAIL, "size of blocks: %d", sf_blocksize);
	net_tick();
#endif

	_sector_size = sf_blocksize;

	return 0;
	}


// Writes data to RCM37x0 flash storage
//  -  block is the starting offset
//  -  buff is the data to write
//  -  bsize should be _sector_size but may be smaller on last block
// Returns 0 if no error

int write_sector(long block, char * buff, int bsize)
	{
	int bnum;

	bnum = (int) (block / (long) sf_blocksize);

#ifdef WEB_DEBUG
	report(DETAIL, "Write to serial flash, block=%d, size=%d", bnum, bsize);
	net_tick();
#endif

    sf_writeRAM((char *) buff, 0, bsize);

    return sf_RAMToPage(bnum);
	}

#endif		// EXTERNAL_STORAGE
#endif		// COPY2FLASH


// *** EXTERNAL FUNCTIONS ***

// Checks remote update Web server for its current version of firmware
// 'web_host' points to hostname or dotted IP address of server
// 'path' points to path on server to file containing details
// 'send_full_url' forces full URL to be sent in GET command
// (ignored in proxy mode as full URL is always sent)
//
// Returns 1 is updated version is available
// Returns 0 if no updated version is available
// Returns < 0 if error occurs (see GetWebUpdate() documentation)

int check_download(char *web_host, char *path, int send_full_url)
	{
	char *conn_host;
	word conn_port;
	long version;
	int  retval;
	char url[DL_MAX_URL_LEN + 1];

	if (strlen(web_host) > EE_POST_STR_MAX_LEN)
		return -20;

	if (strlen(path) > EE_POST_STR_MAX_LEN)
		return -21;

	if (ee_post_info.use_proxy || send_full_url)
		{
		strcpy(url, DL_HTTP_HDR);
    	strcat(url, web_host);
		strcat(url, path);
    	}
	else
		{
		strcpy(url, path);
		}

	if (!ee_post_info.use_proxy)
		{
		conn_host = web_host;
		conn_port = 80;
		}
	else
		{
		conn_host = ee_post_proxy.str;
		conn_port = ee_post_info.proxy_port;
		}

	report(INFO,   "Checking for new firmware...");
	report(DETAIL, "Attempting connection to %s port %u", conn_host, conn_port);
	report(DETAIL, "Attempting to get %s", url);
	net_tick();

	wx_set_leds(LED_DOWNLOAD, LED_AMBER);

	retval = CheckWebVersion(url, conn_host, conn_port, &version);

	if (retval < 0)
		{
		wx_set_leds(LED_DOWNLOAD, LED_RED);
		report(PROBLEM, "CheckWebVersion() returned %d", retval);
		return retval;
		}

	wx_set_leds(LED_DOWNLOAD, LED_GREEN);
	report(INFO, "CheckWebVersion() returned version %ld", version);

	if (version <= (VER_MAJOR * 100) + VER_MINOR)
		{
		report(INFO, "Current firmware is up-to-date");
        return 0;
		}

	report(INFO, "Updated firmware is available");
	return 1;
	}


// Attempts to download, burn into flash and run a new version of firmware
//
// Does not return on success (runs new code)
// Returns < 0 if error occurs (see GetWebUpdate() documentation)

int get_download(void)
	{
	int retval;

#ifdef EXTERNAL_STORAGE
	set_flash_start(0L);				// Serial flash chip
#else
	set_flash_start(0x40000L);			// 2nd flash chip in Main Flash
#endif

	report(INFO, "Attempting to download new firmware...");
	net_tick();

	wx_set_leds(LED_DOWNLOAD, LED_AMBER);

	retval = GetWebUpdate();

	wx_set_leds(LED_DOWNLOAD, LED_RED);
	report(PROBLEM, "GetWebUpdate() returned %d", retval);
	report(PROBLEM, "Firmware download not completed");

	return retval;
	}
