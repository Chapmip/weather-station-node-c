// Routines to manage configuration menu

// Copyright (c) 2006, Ian Chapman (Chapmip Consultancy)

// All rights reserved, except for those rights implicitly granted to
// GitHub Inc by publishing on GitHub and those rights granted by
// commercial agreement with the author.


#include <stdio.h>
#include <dcdefs.h>
#include <stcpip.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include "timeout.h"
#include "bb_vars.h"
#include "eeprom.h"
#include "lan.h"
#include "wx_board.h"
#include "tasks.h"
#include "davis.h"
#include "report.h"
#include "rtc_utils.h"
#include "stack_check.h"
#include "download.h"
#include "wx_main.h"
#include "menu.h"


// Error values returned by routines below
// Leave -1 available for EOF

#define MENU_TOUT				(-2)
#define MENU_ABORT				(-3)
#define MENU_BAD_SIZE			(-4)
#define MENU_NO_MATCH			(-5)


// Additional values returned on success

#define MENU_NO_CHANGE			1
#define MENU_UPDATE				2


// Mode values accepted by getline()

#define MODE_NORMAL				0
#define MODE_PASSWORD			1
#define MODE_MENU				2
#define MODE_FLAG				3
#define MODE_DIGITS				4
#define MODE_IP_VALUE			5
#define MODE_POST_STR			6
#define MODE_SIGNED				7
#define MODE_HEX				8


// Tab position for tabular display output

#define TAB_POSN				18


// Timer values

#define MAX_INPUT_WAIT_SECS		120
#define MAX_DAVIS_WAIT_SECS		20


// Menu structure definition

typedef struct
	{
	char cmd;					// Command letter to invoke item
	char * text;				// Description of item
	unsigned char flags;		// Flags relating to item
	int (* func)(void);			// Function to call for item
	} MenuItem_t;


// Bit masks for flags values (also user_mask values)

#define USER_TECH				0x01		// On-site technician
#define USER_ADMIN				0x02		// Administrator
#define USER_MAINT				0x04		// Maintenance and test

#define USER_HIGH				(USER_ADMIN | USER_MAINT)
#define USER_ALL				(USER_TECH | USER_ADMIN | USER_MAINT)

#define ONLY_STATIC				0x10		// Applies to static IP configuration
#define ONLY_PROXY				0x20		// Applies to proxy configuration


// Password strings corresponding to above user mask values

static const char * pwd_list[] =
	{
	"ADD-USER-TECH-PW-HERE",	// %% Add here for USER_TECH  %%
	"ADD-USER-ADMIN-PW-HERE",	// %% Add here for USER_ADMIN %%
	"ADD-USER-MAINT-PW-HERE",	// %% Add here for USER_MAINT %%
	};


// Menu state values

#define STATE_TOP				1
#define STATE_LAN				2
#define STATE_PROXY				3
#define STATE_POST				4
#define STATE_UNIT				5
#define STATE_DAVIS				6
#define STATE_DLOAD				7
#define STATE_TEST				8


// Label strings (used in several places)

#define LABEL_TOP_MENU			"Main menu"

#define LABEL_LAN_MENU			"Local network settings"
#define LABEL_PROXY_MENU		"Proxy server settings"
#define LABEL_POST_MENU			"Remote server settings"
#define LABEL_UNIT_MENU			"Unit settings"
#define LABEL_DAVIS_MENU		"Weather station calibration"
#define LABEL_DLOAD_MENU		"Firmware download options"
#define LABEL_TEST_MENU			"Test commands"
#define LABEL_RESET_DEFS		"Reset all settings to defaults"
#define LABEL_EXIT_MENU			"Exit from menu"

#define LABEL_LAN_MODE			"LAN mode"
#define LABEL_IP_ADDR			"IP address"
#define LABEL_NET_MASK			"network mask"
#define LABEL_DNS_SVR			"DNS server"
#define LABEL_ROUTER			"router"

#define LABEL_PROXY_MODE    	"proxy mode"
#define LABEL_PROXY_SVR			"proxy server"
#define LABEL_PROXY_PORT		"proxy port"

#define LABEL_POST_SVR			"remote server"
#define LABEL_POST_PORT			"remote port"
#define LABEL_POST_PATH			"file path"

#define LABEL_UNIT_BASE			"station ID base"
#define LABEL_UNIT_OFFSET		"+ Rotary switch"
#define LABEL_UNIT_ID			"= Station ID"
#define LABEL_UNIT_MODE			"console o/p mode"
#define LABEL_UNIT_UPDATE		"update period"

#define LABEL_DAVIS_BARDATA		"Read barometer calibration values"
#define LABEL_DAVIS_SET_BAR		"Change barometer calibration values"
#define LABEL_DAVIS_CHECK_TIME	"Check weather station clock"
#define LABEL_DAVIS_SET_TIME	"Set weather station clock"
#define LABEL_DAVIS_VERSION		"Check weather station version"
#define LABEL_DAVIS_COLLECT		"Collect test LOOP packet"

#define LABEL_DLOAD_CHECK		"Check for firmware update"

#define LABEL_TEST_DIP			"DIP switches"
#define LABEL_TEST_LEDS			"LED settings"
#define LABEL_TEST_BB_WORD		"BB test value"
#define LABEL_TEST_TIME_T		"Interface time_t"
#define LABEL_TEST_TIME_VALID	"Time valid flag"
#define LABEL_TEST_TIME_ASC		"Interface clock"
#define LABEL_TEST_HANDSHAKE	"serial handshake state"
#define LABEL_TEST_SERIAL		"Serial port test"
#define LABEL_TEST_STACK		"Check stack depth"
#define LABEL_TEST_REFRESH		"Refresh values"


// Other text strings used in several places

#define TEXT_CHANGE				"Change "
#define TEXT_ESC_RETURN			" (or ESC to return to top)"
#define TEXT_NAME_IP			" (name or IP address)"

#define TEXT_TIMED_OUT			"[Timed out]\r\n"
#define TEXT_ABORTED			"[Aborted]\r\n"


// Internal variables

static unsigned int input_tout_secs;		// Input time-out timer

static unsigned char user_mask;				// User bit mask from password entry

static unsigned char menu_state;			// Currently active menu
static const MenuItem_t * menu_base;		// Pointer to first item in menu
static unsigned char menu_num_items;		// Number of items in menu

static unsigned char menu_dav_init;			// Davis state machine initialised flag

static unsigned char menu_exit;				// Menu exit flag


// *** INTERNAL FUNCTIONS ***

// Waits for key to be pressed or timeout to occur
// Assumes that timeout timer has previously been set up
// Returns key if pressed or MENU_TOUT on timeout

static int getkey(void)
	{
	int ch;

	while (!CHK_TIMEOUT_UI_SECS(input_tout_secs))
		{
        ch = inchar();

		if (ch != EOF)
			return ch;
		}

	return MENU_TOUT;
	}


// Send new line combination to end current line or add blank line

static void display_crlf(void)
	{
	printf("\r\n");
	}


// Reports an error message in a standardised format

static void report_error(char * message)
    {
	printf("-- ERROR: %s --\r\n", message);
	}


// Checks a character to see whether it matches any of the menu commands
// Returns the index (>= 0) of the matching menu item, or -1 if no match

static int check_cmd_match(char ch)
	{
	unsigned char i;

	for (i = 0; i < menu_num_items; ++i)
		{
		if (menu_base[i].cmd == ch && (menu_base[i].flags & user_mask) != 0)
			return i;
		}

	return -1;
	}


// Checks whether menu command is allowed in the current mode
// Returns 1 if allowed or 0 if not allowed

static unsigned char check_cmd_enabled(unsigned char flags)
	{
	if ((flags & ONLY_STATIC) != 0 && !ee_lan_info.use_static)
		return 0;

	if ((flags & ONLY_PROXY) != 0 && !ee_post_info.use_proxy)
		return 0;

	return 1;
	}


// Attempts to read up to (size - 1) chars into buf (excluding terminating CR)
// with echoing and input screening based on mode value, and leaves zeroes in
// remainder of buf (i.e. at least one if size == sizeof(buf) and size != 0).
// Backspacing is supported to erase most recent char in buf and on screen.
//
// Returns number of characters before terminating CR (>= 0),
// or MENU_ABORT (< 0) if ESC was pressed to abort input,
// or MENU_TOUT (< 0) if timeout occurred between input chars,
// or MENU_BAD_SIZE (< 0) if called with size == 0.

static int getline(char * buf, unsigned char size, unsigned char mode)
	{
	int ch;
	int echo_ch;
	unsigned char pos;

	if (size == 0)
		return MENU_BAD_SIZE;					// -- EXIT --

	pos = 0;
    input_tout_secs = SET_TIMEOUT_UI_SECS(MAX_INPUT_WAIT_SECS);

    memset(buf, '\0', size);		// Zero entire buffer at first

	for (;;)
		{
		ch = getkey();
		input_tout_secs = SET_TIMEOUT_UI_SECS(MAX_INPUT_WAIT_SECS);

		if (ch < 0x20)
			{         				// Control char or -ve result
			switch (ch)
				{
				case MENU_CR:
					printf("\r\n");
					return pos;					// -- EXIT --

				case MENU_BS:
					if (pos > 0)
						{
						--pos;
						buf[pos] = '\0';
						printf("\x08 \x08");	// Erase char on screen
						}
					break;

				case MENU_ESC:
					printf(TEXT_ABORTED);
					return MENU_ABORT;			// -- EXIT --

				case MENU_TOUT:
					printf(TEXT_TIMED_OUT);
					return MENU_TOUT;			// -- EXIT --

				default:
					;				// Ignore other chars
				}
			}
		else
			{                   	// Normal character
            echo_ch = ch;

			switch (mode)
				{
				case MODE_NORMAL:
                    if (!isprint(ch))
						ch = 0;
					break;

				case MODE_PASSWORD:
					echo_ch = '*';
					if (!isgraph(ch))
						ch = 0;
					break;

				case MODE_MENU:
					ch = toupper(ch);
					if (check_cmd_match(ch) < 0)
						ch = 0;
					break;

				case MODE_FLAG:
                    ch = toupper(ch);
					if (ch != 'Y' && ch != 'N')
						ch = 0;
					break;

				case MODE_DIGITS:
					if (!isdigit(ch))
						ch = 0;
					break;

				case MODE_IP_VALUE:
					if (!isdigit(ch) && ch != '.')
						ch = 0;
					break;

				case MODE_POST_STR:
					if (!isgraph(ch))
						ch = 0;
					break;

				case MODE_SIGNED:
					if (!isdigit(ch) && (ch != '-' || pos != 0))
						ch = 0;
					break;

				case MODE_HEX:
					if (!isxdigit(ch))
						ch = 0;
					break;

				default:
					;				// No filtering
				}

			if (ch > 0 && pos < size - 1)
				{
				buf[pos] = ch;
				++pos;
				putchar(echo_ch);
				}
			}

		}
	}


// Prompts user to enter password and validates input against list
// Returns 1 on success (with user_mask set to relevant value)
// or 0 if no characters were entered before CR,
// or MENU_ABORT (< 0) if ESC was pressed to abort input,
// or MENU_TOUT (< 0) if timeout occurred between input chars,
// or MENU_NO_MATCH (< 0) if input did not match any password.

#define PWD_BUF_LEN	16					// Size of buffer including zero terminator

static int get_password(void)
	{
	int status;
	unsigned int i;
	unsigned char mask;
	char buf[PWD_BUF_LEN];

	printf("Access code: ");

	status = getline(buf, PWD_BUF_LEN, MODE_PASSWORD);
	if (status <= 0)
		return status;

	mask = 0x01;

	for (i = 0; i < sizeof(pwd_list) / sizeof(pwd_list[0]); ++i)
		{
		if (strcmpi(buf, pwd_list[i]) == 0)
			{
			user_mask = mask;
			return 1;
			}

		mask <<= 1;
		}

	report_error("ACCESS CODE REJECTED");
	return MENU_NO_MATCH;
	}


// Sets current menu structure (state, array base address and number of items)
// Returns MENU_UPDATE value for use as new status result

static int _set_menu(unsigned char state, const MenuItem_t * array, unsigned char num_items)
	{
	menu_state = state;
	menu_base = array;
	menu_num_items = num_items;

    return MENU_UPDATE;
	}


// Macro to simplify call to set_menu()

#define SET_MENU(STATE, ARRAY) _set_menu(STATE, ARRAY, (sizeof(ARRAY) / sizeof(ARRAY[0])))


// Displays available choices in current menu

static void display_menu(char * prefix)
	{
    unsigned char i;

	display_crlf();

	for (i = 0; i < menu_num_items; ++i)
		{
		if ((menu_base[i].flags & user_mask) != 0
			&& check_cmd_enabled(menu_base[i].flags))
			{
			printf("%c. %s%s\r\n", menu_base[i].cmd, prefix, menu_base[i].text);
			}
		}

	display_crlf();
	}


// Invites user to select a menu option and, if successful, calls the relevant function
// User prompt can be suffixed with string (e.g. to indicate how to escape from menu)
//
// Returns MENU_UPDATE (> 0) on success (called function returned MENU_UPDATE)
// or MENU_NO_CHANGE (> 0) on success (called function didn't return MENU_UPDATE or MENU_TOUT),
// or 0 if no characters were entered before CR during menu selection,
// or MENU_ABORT (< 0) if ESC was pressed to abort input during menu selection,
// or MENU_TOUT (< 0) if timeout occurred (either during menu selection or called function),
// or MENU_NO_MATCH (< 0) if input did not match any menu item or command is currently disabled.

static int select_menu_option(char * suffix)
	{
	int status;
	int index;
	char buf[2];							// Room for just one char plus zero-terminator

    printf("Select option%s: ", suffix);

	status = getline(buf, 2, MODE_MENU);
	if (status <= 0)
		return status;

	index = check_cmd_match(buf[0]);
	if (index < 0)
		return MENU_NO_MATCH;

	if (!check_cmd_enabled(menu_base[index].flags))
		return MENU_NO_MATCH;

	status = menu_base[index].func();
	switch(status)
		{
		case MENU_TOUT:
		case MENU_UPDATE:
			return status;
		}

	return MENU_NO_CHANGE;
	}


// Displays menu title in capitals with underlining for emphasis

static void display_title(char * title)
	{
	unsigned int i;
	char * ptr;

	display_crlf();

	ptr = title;
	while (*ptr != '\0')
		putchar(toupper(*ptr++));

	display_crlf();

	for (i = strlen(title); i > 0; --i)
		putchar('-');

	display_crlf();
	}


// Displays label string with tabbing spaces afterwards
// First character of label is automatically capitalised
// If label is blank then just spaces are displayed up to
// the tab position without any leading label or colon

static void display_label(char * label)
	{
	unsigned int i;

	if (*label != '\0')
		{
		putchar(toupper(*label));
		printf("%s: ", label + 1);
		}

	for (i = strlen(label) + 2; i < TAB_POSN; ++i)
		putchar(' ');
	}


// Displays labelled string value in tabbed format
// If value string is blank, then this is stated explicitly

static void display_item(char * label, char * value)
	{
	display_label(label);

	printf("%s\r\n", (*value != '\0' ? value : "[No value]"));
	}


// Displays labelled flag value as "Yes" (!0) or "No" (0)

static void display_flag_value(char * label, int value)
	{
	display_item(label, (value ? "Yes" : "No"));
	}


// Displays labelled initialisation state flag value
// !0 means initialised okay, 0 means not initialised

static void display_init_state(char * label, int value)
	{
	display_item(label, (value ? "OK" : "NOT INITIALISED"));
	}


// Prompts user to press a key before continuing
// Returns MENU_UPDATE if key pressed (including ESC)
// Returns MENU_TOUT if timeout occurred

static int await_any_key(void)
	{
	int ch;

	input_tout_secs = SET_TIMEOUT_UI_SECS(MAX_INPUT_WAIT_SECS);

	printf("-- Press any key to continue --\r\n");

	ch = getkey();
	if (ch < 0)
		return ch;

	return MENU_UPDATE;
	}


// Prompts user to enter flag value as "Y" or "N"
// Invalid input is rejected and user is re-prompted
//
// Returns MENU_UPDATE on success (value stored at pointer),
// or 0 if no characters were entered before CR,
// or MENU_ABORT (< 0) if ESC was pressed to abort input,
// or MENU_TOUT (< 0) if timeout occurred between input chars.

static int get_flag_value(char * label, int * ptr)
	{
	int status;
	char buf[2];				// Up to 1 character + zero terminator

	for (;;)
		{
		printf("%s (Y or N)? ", label);

		status = getline(buf, 2, MODE_FLAG);
		if (status <= 0)
			return status;

		switch(buf[0])
			{
			case 'Y':
				*ptr = 1;
	    		return MENU_UPDATE;

			case 'N':
				*ptr = 0;
				return MENU_UPDATE;
			}

		report_error("VALUE MUST BE Y OR N");
    	}
	}


// Displays labelled word value in decimal (0-65535)

static void display_word_value(char * label, word value)
	{
	display_label(label);

	printf("%u\r\n", value);
	}


// Displays labelled word value as decimal (0-65535) number of seconds

static void display_word_secs(char * label, word value)
	{
	display_label(label);

	printf("%u secs\r\n", value);
	}


// Prompts user to enter decimal word value (0 to max_val)
// Invalid input is rejected and user is re-prompted
//
// Returns MENU_UPDATE on success (value stored at pointer),
// or 0 if no characters were entered before CR,
// or MENU_ABORT (< 0) if ESC was pressed to abort input,
// or MENU_TOUT (< 0) if timeout occurred between input chars.

static int get_word_value(char * label, word * ptr, word max_val)
	{
	int status;
    longword val;
	char buf[6];				// Up to 5 decimal digits + zero terminator

	for (;;)
		{
		printf("Enter %s (0-%u): ", label, max_val);

		status = getline(buf, 6, MODE_DIGITS);
		if (status <= 0)
			return status;

		val = strtoul(buf, NULL, 10);

		if (val <= max_val)
			{
			*ptr = (word) val;
    		return MENU_UPDATE;
    		}

		report_error("VALUE MUST BE IN STATED RANGE");
    	}
	}


// Displays labelled IP value in dotted decimal format

static void display_ip_value(char * label, longword ip_value)
	{
	display_item(label, get_ip_string(ip_value));
	}


// Prompts user to enter IP value in dotted decimal format
// Invalid input is rejected and user is re-prompted
//
// Returns MENU_UPDATE on success (value stored at pointer),
// or 0 if no characters were entered before CR,
// or MENU_ABORT (< 0) if ESC was pressed to abort input,
// or MENU_TOUT (< 0) if timeout occurred between input chars.

static int get_ip_value(char * label, longword * ptr)
	{
	int status;
    longword val;
	char buf[16];				// Up to 15 characters + zero terminator

	for (;;)
		{
		printf("Enter %s (w.x.y.z): ", label);

		status = getline(buf, 16, MODE_IP_VALUE);
		if (status <= 0)
			return status;

		val = inet_addr(buf);
		if (val != 0)
			{
			*ptr = val;
    		return MENU_UPDATE;
    		}

		report_error("VALUE MUST BE IN DOTTED DECIMAL FORMAT (NOT 0.0.0.0)");
    	}
	}


// Attempts to get a POST string from the user and write it to EEPROM
// The supplied user prompt is displayed along with format description text
//
// If a non-blank string is entered, then an attempt is made to write the
// string to EEPROM and read it back, but the result of this operation is
// not returned (although the EEPROM routines may send failure messages).
//
// Returns MENU_UPDATE on success (value entered and write attempted),
// or 0 if no characters were entered before CR,
// or MENU_ABORT (< 0) if ESC was pressed to abort input,
// or MENU_TOUT (< 0) if timeout occurred between input chars.

static int change_post_str(char * label, char * format,
						   unsigned char ee_loc, EePostStr_t * ptr)
	{
	int status;
	char buf[EE_POST_STR_MAX_LEN + 1];

	printf("Enter %s%s: ", label, format);

	status = getline(buf, sizeof(buf), MODE_POST_STR);

	if (status <= 0)
		return status;

	(void) ee_write_post_str(ee_loc, ptr, buf);
	(void) ee_read_post_str(ee_loc, ptr);

	return MENU_UPDATE;
	}


// Prompts user to enter signed decimal value
// Value must be in the range min_val to max_val, or zero if zero_special is !0
// Invalid input is rejected and user is re-prompted
//
// Returns MENU_UPDATE on success (value stored at pointer),
// or 0 if no characters were entered before CR,
// or MENU_ABORT (< 0) if ESC was pressed to abort input,
// or MENU_TOUT (< 0) if timeout occurred between input chars.

static int get_int_value(char * label, int * ptr, int min_val, int max_val,
						 int zero_special)
	{
	int status;
    long val;
	char buf[7];				// Sign + 1-5 decimal digits + zero terminator

	for (;;)
		{
		printf("Enter %s (%d to %d%s): ", label, min_val, max_val,
				(zero_special ? ", or 0" : ""));

		status = getline(buf, 7, MODE_SIGNED);
		if (status <= 0)
			return status;

		val = strtol(buf, NULL, 10);

		if ((val >= min_val && val <= max_val) || (zero_special && val == 0))
			{
			*ptr = (int) val;
    		return MENU_UPDATE;
    		}

		report_error("VALUE MUST BE IN STATED RANGE");
    	}
	}


// Executes Davis command previously set up by call to dav_start_xxx()
// Success or failure is reported back to the user
// Aborts if user presses [ESC] or time-out expires

static void exec_davis_cmd(void)
	{
	int status;

	printf("Press [ESC] to abort command\r\n");
    input_tout_secs = SET_TIMEOUT_UI_SECS(MAX_DAVIS_WAIT_SECS);

	for (;;)
		{
		if (dav_tick() != DAV_PENDING)
			break;

		if (inchar() == MENU_ESC)
			{
			printf(TEXT_ABORTED);
			dav_abort();
			return;
			}

		if (CHK_TIMEOUT_UI_SECS(input_tout_secs))
			{
			printf(TEXT_TIMED_OUT);
			dav_abort();
			return;
			}
		}

	status = dav_get_status();

	if (status > 0)
		printf("\r\nCommand succeeded");
	else
		printf("\r\nCommand failed");

    printf(" - result code %d\r\n", status);
	}


// Displays labelled byte value in hexadecimal (0-FF)

static void display_hex_byte(char * label, unsigned char value)
	{
	display_label(label);

	printf("0x%02X\r\n", value);
	}


// Prompts user to enter hexadecimal byte value (0 to FF)
// Invalid input is rejected and user is re-prompted
//
// Returns MENU_UPDATE on success (value stored at pointer),
// or 0 if no characters were entered before CR,
// or MENU_ABORT (< 0) if ESC was pressed to abort input,
// or MENU_TOUT (< 0) if timeout occurred between input chars.

static int get_hex_byte(char * label, unsigned char * ptr)
	{
	int status;
    unsigned int val;
	char buf[3];				// Up to 2 hex digits + zero terminator

	for (;;)
		{
		printf("Enter %s in hexadecimal (00-FF): ", label);

		status = getline(buf, 3, MODE_HEX);
		if (status <= 0)
			return status;

		val = strtoui(buf, NULL, 16);

		if (val <= 255)
			{
			*ptr = (unsigned char) val;
    		return MENU_UPDATE;
    		}

		report_error("VALUE MUST BE 00-FF");
    	}
	}


// Displays labelled longword value in decimal (0 to ULONG_MAX)

static void display_longword_value(char * label, longword value)
	{
	display_label(label);

	printf("%lu\r\n", value);
	}


// Prompts user to enter decimal longword value (0 to ULONG_MAX-1)
// Invalid input is rejected and user is re-prompted
//
// Returns MENU_UPDATE on success (value stored at pointer),
// or 0 if no characters were entered before CR,
// or MENU_ABORT (< 0) if ESC was pressed to abort input,
// or MENU_TOUT (< 0) if timeout occurred between input chars.

static int get_longword_value(char * label, longword * ptr)
	{
	int status;
    longword val;
	char buf[11];				// Up to 10 decimal digits + zero terminator

	for (;;)
		{
		printf("Enter %s: ", label);

		status = getline(buf, 11, MODE_DIGITS);
		if (status <= 0)
			return status;

		val = strtoul(buf, NULL, 10);

		if (val != ULONG_MAX)
			{
			*ptr = val;
    		return MENU_UPDATE;
    		}

		report_error("VALUE MUST BE 0 TO 4294967295");
    	}
	}


// Converts integral metres to integral feet using integer maths
// Returns correct result for metre values from -9987 to 9987
// Uses unsigned values internally so that >> operator works okay

int convert_metres_to_feet(int metres)
	{
	unsigned long acc;
	int feet;

	if (metres >= 0)
		acc = metres;
	else
		acc = -metres;

	acc = ((acc * 53753) + 8192) >> 14;		/* Divide by 8192 */

	feet = (int) acc;

	if (metres >= 0)
		return feet;
	else
		return -feet;
	}


// Converts integral millibars to integral thousanths of Hg inches
// Returns correct result for millibar values from 0 to 2219

unsigned int convert_millibars_to_thousanths(unsigned int millibars)
	{
	unsigned long acc;

	acc = millibars;
	acc = ((acc * 1935276L) + 32768) >> 16;		/* Divide by 65536 */

	return (unsigned int) acc;
	}


// *** MENU STRUCTURES AND INDIRECTLY-CALLED MENU FUNCTIONS ***

// Prototypes for indirectly-called functions

static int _nearcall open_lan_menu(void);
static int _nearcall open_proxy_menu(void);
static int _nearcall open_post_menu(void);
static int _nearcall open_unit_menu(void);
static int _nearcall open_davis_menu(void);
static int _nearcall open_dload_menu(void);
static int _nearcall open_test_menu(void);
static int _nearcall reset_defaults(void);
static int _nearcall exit_menu(void);

static int _nearcall change_lan_mode(void);
static int _nearcall change_ip_addr(void);
static int _nearcall change_net_mask(void);
static int _nearcall change_dns_svr(void);
static int _nearcall change_router(void);

static int _nearcall change_proxy_mode(void);
static int _nearcall change_proxy_svr(void);
static int _nearcall change_proxy_port(void);

static int _nearcall change_post_svr(void);
static int _nearcall change_post_port(void);
static int _nearcall change_post_path(void);

static int _nearcall change_unit_base(void);
static int _nearcall change_unit_mode(void);
static int _nearcall change_unit_update(void);

static int _nearcall exec_davis_bardata(void);
static int _nearcall exec_davis_set_bar(void);
static int _nearcall exec_davis_check_time(void);
static int _nearcall exec_davis_set_time(void);
static int _nearcall exec_davis_version(void);
static int _nearcall exec_davis_collect(void);

static int _nearcall exec_download_check(void);

static int _nearcall change_test_leds(void);
static int _nearcall change_test_bb_word(void);
static int _nearcall change_test_rtc(void);
static int _nearcall change_test_handshake(void);
static int _nearcall exec_serial_test(void);
static int _nearcall exec_stack_check(void);
static int _nearcall refresh_test_values(void);


// Instances of menu structure arrays

static const MenuItem_t menu_top[] =
	{
	{ '1', LABEL_LAN_MENU,   USER_ALL, open_lan_menu },
	{ '2', LABEL_PROXY_MENU, USER_ALL, open_proxy_menu },
	{ '3', LABEL_POST_MENU,  USER_HIGH, open_post_menu },
	{ '4', LABEL_UNIT_MENU,  USER_HIGH, open_unit_menu },
	{ '5', LABEL_DAVIS_MENU, USER_ALL, open_davis_menu },
	{ '6', LABEL_DLOAD_MENU, USER_ALL, open_dload_menu },
	{ '8', LABEL_TEST_MENU,  USER_HIGH, open_test_menu },
	{ '9', LABEL_RESET_DEFS, USER_HIGH, reset_defaults },
	{ '0', LABEL_EXIT_MENU,  USER_ALL, exit_menu },
	};

static const MenuItem_t menu_lan[] =
	{
	{ 'M', LABEL_LAN_MODE,  USER_ALL, change_lan_mode },
	{ 'I', LABEL_IP_ADDR,  (USER_ALL | ONLY_STATIC), change_ip_addr },
	{ 'N', LABEL_NET_MASK, (USER_ALL | ONLY_STATIC), change_net_mask },
	{ 'D', LABEL_DNS_SVR,  (USER_ALL | ONLY_STATIC), change_dns_svr },
	{ 'R', LABEL_ROUTER,   (USER_ALL | ONLY_STATIC), change_router },
	};

static const MenuItem_t menu_proxy[] =
	{
	{ 'M', LABEL_PROXY_MODE,  USER_ALL, change_proxy_mode },
	{ 'S', LABEL_PROXY_SVR,  (USER_ALL | ONLY_PROXY), change_proxy_svr },
	{ 'P', LABEL_PROXY_PORT, (USER_ALL | ONLY_PROXY), change_proxy_port },
	};

static const MenuItem_t menu_post[] =
	{
	{ 'S', LABEL_POST_SVR,  USER_HIGH, change_post_svr },
	{ 'P', LABEL_POST_PORT, USER_HIGH, change_post_port },
	{ 'F', LABEL_POST_PATH, USER_HIGH, change_post_path },
	};

static const MenuItem_t menu_unit[] =
	{
	{ 'S', LABEL_UNIT_BASE,   USER_HIGH, change_unit_base },
	{ 'C', LABEL_UNIT_MODE,   USER_HIGH, change_unit_mode },
	{ 'U', LABEL_UNIT_UPDATE, USER_HIGH, change_unit_update },
	};

static const MenuItem_t menu_davis[] =
	{
	{ 'B', LABEL_DAVIS_BARDATA,    USER_ALL, exec_davis_bardata },
	{ 'C', LABEL_DAVIS_SET_BAR,    USER_ALL, exec_davis_set_bar },
	{ 'T', LABEL_DAVIS_CHECK_TIME, USER_ALL, exec_davis_check_time },
	{ 'S', LABEL_DAVIS_SET_TIME,   USER_ALL, exec_davis_set_time },
	{ 'V', LABEL_DAVIS_VERSION,    USER_ALL, exec_davis_version },
	{ 'L', LABEL_DAVIS_COLLECT,    USER_ALL, exec_davis_collect },
	};

static const MenuItem_t menu_dload[] =
	{
	{ 'F', LABEL_DLOAD_CHECK, USER_ALL, exec_download_check },
	};

static const MenuItem_t menu_test[] =
	{
	{ 'L', TEXT_CHANGE LABEL_TEST_LEDS, USER_HIGH, change_test_leds },
	{ 'B', TEXT_CHANGE LABEL_TEST_BB_WORD, USER_HIGH, change_test_bb_word },
	{ 'T', TEXT_CHANGE LABEL_TEST_TIME_T, USER_HIGH,  change_test_rtc },
	{ 'H', TEXT_CHANGE LABEL_TEST_HANDSHAKE, USER_HIGH, change_test_handshake },
	{ 'S', LABEL_TEST_SERIAL,   USER_HIGH, exec_serial_test },
	{ 'K', LABEL_TEST_STACK,    USER_HIGH, exec_stack_check },
	{ 'R', LABEL_TEST_REFRESH,  USER_HIGH, refresh_test_values },
	};


// *** INDIRECTLY-CALLED FUNCTIONS ***

// Top menu functions

static int _nearcall open_lan_menu(void)
	{
	return SET_MENU(STATE_LAN, menu_lan);
	}

static int _nearcall open_proxy_menu(void)
	{
	return SET_MENU(STATE_PROXY, menu_proxy);
	}

static int _nearcall open_post_menu(void)
	{
	return SET_MENU(STATE_POST, menu_post);
	}

static int _nearcall open_unit_menu(void)
	{
	return SET_MENU(STATE_UNIT, menu_unit);
	}

static int _nearcall open_davis_menu(void)
	{
	return SET_MENU(STATE_DAVIS, menu_davis);
	}

static int _nearcall open_dload_menu(void)
	{
	return SET_MENU(STATE_DLOAD, menu_dload);
	}

static int _nearcall open_test_menu(void)
	{
	return SET_MENU(STATE_TEST, menu_test);
	}

static int _nearcall reset_defaults(void)
	{
	int status;
	int agree;

	status = get_flag_value("Are you sure", &agree);
	if (status < 0)
		return status;

	if (!agree)
		return MENU_NO_CHANGE;

    (void) ee_write_lan_defaults();
	(void) ee_write_post_defaults();
	(void) ee_write_unit_defaults();

	return MENU_UPDATE;
	}

static int _nearcall exit_menu(void)
	{
	menu_exit = 1;
	return MENU_UPDATE;
	}


// LAN menu functions

static int _nearcall change_lan_mode(void)
	{
	int status;

	status = get_flag_value("Use static IP configuration", &ee_lan_info.use_static);

    if (status == MENU_UPDATE)
		(void) ee_write_lan_info();

	return status;
	}

static int _nearcall change_ip_addr(void)
	{
	int status;

	status = get_ip_value(LABEL_IP_ADDR, &ee_lan_info.ip_addr);

    if (status == MENU_UPDATE)
		(void) ee_write_lan_info();

	return status;
	}

static int _nearcall change_net_mask(void)
	{
	int status;

	status = get_ip_value(LABEL_NET_MASK, &ee_lan_info.netmask);

    if (status == MENU_UPDATE)
		(void) ee_write_lan_info();

	return status;
	}

static int _nearcall change_dns_svr(void)
	{
	int status;

	status = get_ip_value(LABEL_DNS_SVR, &ee_lan_info.dns_server_ip);

    if (status == MENU_UPDATE)
		(void) ee_write_lan_info();

	return status;
	}

static int _nearcall change_router(void)
	{
	int status;

	status = get_ip_value(LABEL_ROUTER, &ee_lan_info.router_ip);

    if (status == MENU_UPDATE)
		(void) ee_write_lan_info();

	return status;
	}


// Proxy menu functions

static int _nearcall change_proxy_mode(void)
	{
	int status;

	status = get_flag_value("Enable proxy mode", &ee_post_info.use_proxy);

    if (status == MENU_UPDATE)
		(void) ee_write_post_info();

	return status;
	}

static int _nearcall change_proxy_svr(void)
	{
	return change_post_str(LABEL_PROXY_SVR, TEXT_NAME_IP,
						   EE_LOC_POST_PROXY, &ee_post_proxy);
	}

static int _nearcall change_proxy_port(void)
	{
	int status;

	status = get_word_value(LABEL_PROXY_PORT, &ee_post_info.proxy_port, 65535);

    if (status == MENU_UPDATE)
		(void) ee_write_post_info();

	return status;
	}


// POST menu functions

static int _nearcall change_post_svr(void)
	{
	return change_post_str(LABEL_POST_SVR, TEXT_NAME_IP,
						   EE_LOC_POST_HOST, &ee_post_host);
	}

static int _nearcall change_post_port(void)
	{
	int status;

	status = get_word_value(LABEL_POST_PORT, &ee_post_info.host_port, 65535);

    if (status == MENU_UPDATE)
		(void) ee_write_post_info();

	return status;
	}

static int _nearcall change_post_path(void)
	{
	return change_post_str(LABEL_POST_PATH, " (e.g. /default.asp)",
						   EE_LOC_POST_PATH, &ee_post_path);
	}


// Unit menu functions

static int _nearcall change_unit_base(void)
	{
	int status;

	status = get_word_value(LABEL_UNIT_BASE, &ee_unit_info.id_base,	65535);

    if (status == MENU_UPDATE)
		(void) ee_write_unit_info();

	return status;
	}

static int _nearcall change_unit_mode(void)
	{
	int status;

	status = get_word_value(LABEL_UNIT_MODE, &ee_unit_info.report_mode,
							REPORT_NUM_MODES);

    if (status == MENU_UPDATE)
		(void) ee_write_unit_info();

	return status;
	}

static int _nearcall change_unit_update(void)
	{
	int status;

	status = get_word_value(LABEL_UNIT_UPDATE, &ee_unit_info.update_secs,
							TASKS_MAX_UPDATE_SECS);

	if (status == MENU_UPDATE)
		(void) ee_write_unit_info();

	return status;
	}


// Davis command menu functions

static int _nearcall exec_davis_bardata(void)
	{
	printf("-- Note that values are displayed in weather station units  --\r\n");
	printf("-- e.g. inches Hg x 1000 and feet, not millibars and metres --\r\n");

	dav_start_echo_resp("BARDATA");
	exec_davis_cmd();

	return await_any_key();
	}

static int _nearcall exec_davis_set_bar(void)
	{
	int status;
	int millibars;
	int thousanths;
	int metres;
	int feet;

	status = get_int_value("barometer offset in millibars",
							&millibars, 678, 1100, 1);
	if (status < 0)
		return status;

    if (millibars != 0)
		{
		thousanths = convert_millibars_to_thousanths(millibars);
		printf("Converted to %d thousanths of an inch of Hg\r\n\r\n",
				thousanths);
		}
	else
		thousanths = 0;

	status = get_int_value("elevation in metres",
							&metres, -609, 4572, 0);

	if (status < 0)
		return status;

	feet = convert_metres_to_feet(metres);
	printf("Converted to %d feet\r\n\r\n", feet);

	dav_start_set_bar(thousanths, feet);
	exec_davis_cmd();

	return await_any_key();
	}

static int _nearcall exec_davis_check_time(void)
	{
	dav_start_check_time();
	exec_davis_cmd();

	switch(dav_get_status())
		{
		case DAV_SUCCESS:
			printf("Time is synchronised\r\n");
			break;

		case DAV_WRONG_TIME:
			printf("Time is not synchronised\r\n");
			break;
		}

	return await_any_key();
	}

static int _nearcall exec_davis_set_time(void)
	{
	int status;
	int agree;

	if (!rtc_validated)
		{
		status = get_flag_value("Interface clock has not been validated - proceed anyway?",
								&agree);
		if (status < 0)
			return status;

		if (!agree)
			return MENU_NO_CHANGE;
		}

	dav_start_set_time();
	exec_davis_cmd();

	return await_any_key();
	}

static int _nearcall exec_davis_version(void)
	{
	dav_start_echo_resp("VER");
	exec_davis_cmd();

	return await_any_key();
	}

static int _nearcall exec_davis_collect(void)
	{
	dav_start_collect();
	exec_davis_cmd();

	return await_any_key();
	}


// Firmware download menu functions

static int _nearcall exec_download_check(void)
	{
	int status;
	int retval;
	int agree;
	char host_buf[EE_POST_STR_MAX_LEN + 1];
	char path_buf[EE_POST_STR_MAX_LEN + 1];

    if (!lan_active)
		{
		printf("ERROR - NET has not yet been initialised\r\n");
		return MENU_NO_CHANGE;
		}

	if (!ee_post_valid)
		{
		printf("ERROR - EEPROM parameters not valid\r\n");
		return MENU_NO_CHANGE;
		}

	printf("Enter download host (default is '%s'):\r\n", ee_post_host.str);
	status = getline(host_buf, sizeof(host_buf), MODE_POST_STR);
	if (status < 0)
		return status;
	if (status == 0)
		strcpy(host_buf, ee_post_host.str);

	printf("Enter download path (default is '%s'):\r\n", DL_DEF_PATH);
	status = getline(path_buf, sizeof(path_buf), MODE_POST_STR);
	if (status < 0)
		return status;
	if (status == 0)
		strcpy(path_buf, DL_DEF_PATH);

	agree = 1;				// Default value for full URL choice

	if (!ee_post_info.use_proxy)
		{
        status = get_flag_value("Send full URL in HTTP 1.0 GET command",
								&agree);
		if (status < 0)
			return status;
		}

	printf("Checking for firmware update...\r\n");

	retval = check_download(host_buf, path_buf, agree);

	if (retval != 1)
		return await_any_key();

	printf("\r\n");

	if (!_inFlash())
		printf("Warning: Full download not possible when running in RAM\r\n");

	status = get_flag_value("Do you wish to initiate new firmware download",
							&agree);
	if (status < 0)
		return status;

	if (!agree)
		return MENU_UPDATE;

	retval = get_download();

	return await_any_key();
	}


// Test command menu functions

static int _nearcall change_test_leds(void)
	{
	int status;
	unsigned char value;

	status = get_hex_byte(LABEL_TEST_LEDS, &value);

	if (status == MENU_UPDATE)
        wx_set_leds(LED_ALL, value);

	return status;
	}

static int _nearcall change_test_bb_word(void)
	{
	int status;

	status = get_word_value(LABEL_TEST_BB_WORD, (word *) &bb_test_word, 65535);

	return status;
	}

static int _nearcall change_test_rtc(void)
	{
	int status;
	longword value;

	status = get_longword_value(LABEL_TEST_TIME_T, &value);

	if (status == MENU_UPDATE)
		{
		rtc_update(value);
		rtc_validated = 0;
		}

	return status;
	}

static int _nearcall change_test_handshake(void)
	{
	int status;
	word value;

	status = get_word_value(LABEL_TEST_HANDSHAKE, &value, 1);

	if (status == MENU_UPDATE)
		{
		if (value == 0)
			wx_set_dtr_false();
		else
			wx_set_dtr_true();
		}

	return status;
	}

static int _nearcall exec_serial_test(void)
	{
	int ch;

	if (!dav_init_serial())
		{
		printf("Unable to initialise serial port\r\n");
		return MENU_UPDATE;						// -- EXIT --
		}

	printf("Press [ESC] to exit terminal mode\r\n");
    input_tout_secs = SET_TIMEOUT_UI_SECS(MAX_INPUT_WAIT_SECS);

	for (;;)
		{
		if (CHK_TIMEOUT_UI_SECS(input_tout_secs))
			{
			printf("\r\n" TEXT_TIMED_OUT);
			return MENU_TOUT;					// -- EXIT --
			}

		ch = inchar();
		if (ch != EOF)
			{
		    input_tout_secs = SET_TIMEOUT_UI_SECS(MAX_INPUT_WAIT_SECS);

        	if (ch == MENU_ESC)
        		return MENU_UPDATE;           	// -- EXIT --

			SerialPutcE(ch);	// Send char
			putchar(ch);		// Echo char
			}

		ch = SerialErrorE();
		if (ch != 0)
			printf("\r\n[Serial error 0x%02X occurred]\r\n", ch);

		ch = SerialGetcE();
		if (ch != EOF)
			putchar(ch);		// Show char
		}
	}

static int _nearcall exec_stack_check(void)
	{
	report_stack();
	return MENU_NO_CHANGE;
	}

static int _nearcall refresh_test_values(void)
	{
	return MENU_UPDATE;
	}


// *** DISPLAY FUNCTIONS FOR EACH MENU ***

// Show values relating to top-level menu

#define STR(X)			#X
#define XSTR(X)			STR(X)

static void show_top_values(void)
	{
	display_title(LABEL_TOP_MENU);
	display_crlf();
	display_item("Firmware version", XSTR(VER_MAJOR) "." XSTR(VER_MINOR));
	}


// Show values relating to LAN settings menu

static void show_lan_values(void)
	{
	display_title(LABEL_LAN_MENU);

	if (ee_read_lan_parms() < 0)
		return;

	display_crlf();
	display_init_state("LAN settings", ee_lan_valid);

	display_item(LABEL_LAN_MODE, ee_lan_info.use_static ? "Static IP" : "DHCP");

	if (ee_lan_info.use_static)
		{
		display_ip_value(LABEL_IP_ADDR, ee_lan_info.ip_addr);
    	display_ip_value(LABEL_NET_MASK, ee_lan_info.netmask);
		display_ip_value(LABEL_DNS_SVR, ee_lan_info.dns_server_ip);
		display_ip_value(LABEL_ROUTER, ee_lan_info.router_ip);
		}
	}


// Show values relating to proxy settings menu

static void show_proxy_values(void)
	{
    display_title(LABEL_PROXY_MENU);

	if (ee_read_post_parms() < 0)
		return;

	display_crlf();
	display_init_state("Proxy settings", ee_post_valid);

	display_item(LABEL_PROXY_MODE, ee_post_info.use_proxy ? "Enabled" : "Disabled");

	if (ee_post_info.use_proxy)
		{
		display_item(LABEL_PROXY_SVR, ee_post_proxy.str);
    	display_word_value(LABEL_PROXY_PORT, ee_post_info.proxy_port);
		}
	}


// Show values relating to remote server settings menu

static void show_post_values(void)
	{
	display_title(LABEL_POST_MENU);

	if (ee_read_post_parms() < 0)
		return;

	display_crlf();
	display_init_state("Server settings", ee_post_valid);

	display_item(LABEL_POST_SVR, ee_post_host.str);
    display_word_value(LABEL_POST_PORT, ee_post_info.host_port);
	display_item(LABEL_POST_PATH, ee_post_path.str);
	}


// Show values relating to unit settings menu

static void show_unit_values(void)
	{
	display_title(LABEL_UNIT_MENU);

	wx_get_switches();

	if (ee_read_unit_parms() < 0)
		return;

	display_crlf();
	display_word_value(LABEL_UNIT_BASE, ee_unit_info.id_base);
	display_word_value(LABEL_UNIT_OFFSET, wx_rotary_sel);
	display_word_value(LABEL_UNIT_ID, get_station_id());

	display_crlf();

	if (ee_unit_info.report_mode - 1 < REPORT_NUM_MODES)
		display_word_value(LABEL_UNIT_MODE, ee_unit_info.report_mode);
	else
		display_item(LABEL_UNIT_MODE, "0 (DIP 2)");

	if (ee_unit_info.update_secs - 1 < TASKS_MAX_UPDATE_SECS)
		display_word_secs(LABEL_UNIT_UPDATE, ee_unit_info.update_secs);
	else
		display_item(LABEL_UNIT_UPDATE, "0 (DIP 3)");
	}


// Initialise Davis state machine and show any associated values

static void show_davis_values(void)
	{
	display_title(LABEL_DAVIS_MENU);

	if (!menu_dav_init)
		{
        if (dav_init_all() < 0)
			report_error("Unable to initialise weather station port\r\n");
		else
			menu_dav_init = 1;
		}
	}


// Show values relating to firmware download menu

static void show_dload_values(void)
	{
	display_title(LABEL_DLOAD_MENU);
	}


// Show values relating to test commands menu

static void show_test_values(void)
	{
	display_title(LABEL_TEST_MENU);

	wx_get_switches();

	display_crlf();
	display_hex_byte(LABEL_TEST_DIP, wx_dip_state);
	display_word_value(LABEL_TEST_BB_WORD, bb_test_word);
	display_longword_value(LABEL_TEST_TIME_T, time(NULL));
	display_word_value(LABEL_TEST_TIME_VALID, rtc_validated);
	display_item(LABEL_TEST_TIME_ASC, rtc_str());
	}


// *** EXTERNAL FUNCTIONS ***

// Main menu routine
// Return value indicates whether anything was changed
// 0 means no changes, 1 means no changes

int menu_exec(void)
	{
	int status;

	menu_dav_init = 0;
	menu_exit = 0;

	if (get_password() <= 0)
		return 0;

	status = SET_MENU(STATE_TOP, menu_top);

	for (;;)
		{
		switch(menu_state)
			{
			case STATE_TOP:
				if (status > 0)
					{
					show_top_values();
					display_menu("");
    				}
				status = select_menu_option("");
				break;

			case STATE_LAN:
				if (status == MENU_UPDATE)
					{
					show_lan_values();
					display_menu(TEXT_CHANGE);
    				}
				status = select_menu_option(TEXT_ESC_RETURN);
				break;

			case STATE_PROXY:
				if (status == MENU_UPDATE)
					{
					show_proxy_values();
					display_menu(TEXT_CHANGE);
    				}
				status = select_menu_option(TEXT_ESC_RETURN);
                break;

			case STATE_POST:
				if (status == MENU_UPDATE)
					{
					show_post_values();
					display_menu(TEXT_CHANGE);
    				}
				status = select_menu_option(TEXT_ESC_RETURN);
				break;

			case STATE_UNIT:
				if (status == MENU_UPDATE)
					{
					show_unit_values();
					display_menu(TEXT_CHANGE);
    				}
				status = select_menu_option(TEXT_ESC_RETURN);
				break;

			case STATE_DAVIS:
				if (status == MENU_UPDATE)
					{
					show_davis_values();
					display_menu("");
    				}
				status = select_menu_option(TEXT_ESC_RETURN);
				break;

			case STATE_DLOAD:
				if (status == MENU_UPDATE)
					{
					show_dload_values();
					display_menu("");
    				}
				status = select_menu_option(TEXT_ESC_RETURN);
				break;

			case STATE_TEST:
				if (status == MENU_UPDATE)
					{
					show_test_values();
					display_menu("");
    				}
				status = select_menu_option(TEXT_ESC_RETURN);
				break;

			default:
				menu_exit = 1;
			}

		if (menu_exit || status == MENU_TOUT)
			{
			printf("-- EXITING MENU --\r\n");
			return 1;							// -- EXIT --
			}

		if (status == MENU_NO_MATCH)
			report_error("OPTION IS DISABLED");
		else if (status < 0 && menu_state != STATE_TOP)
			status = SET_MENU(STATE_TOP, menu_top);
    	}
	}
