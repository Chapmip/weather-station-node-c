// Routines to collect data from Davis Instruments weather station

// Copyright (c) 2006, Ian Chapman (Chapmip Consultancy)

// All rights reserved, except for those rights implicitly granted to
// GitHub Inc by publishing on GitHub and those rights granted by
// commercial agreement with the author.


#include <stdio.h>
#include <string.h>
#include <time.h>
#include "timeout.h"
#include "wx_board.h"
#include "crc.h"
#include "report.h"
#include "rtc_utils.h"
#include "davis.h"


// A complete definition of the protocol for data collection can be found in
// the Davis Instruments document "Vantage Pro and Vantage Pro2 Serial Support"
// (this code is based on Issue 2.2 dated 01-25-2005)


// Short-cut names for types of report output (see "report.h")

#define PROBLEM		(REPORT_DAVIS | REPORT_PROBLEM)
#define INFO		(REPORT_DAVIS | REPORT_INFO)
#define DETAIL		(REPORT_DAVIS | REPORT_DETAIL)

#define RAW_INFO	(INFO | REPORT_RAW)
#define RAW_DETAIL	(DETAIL | REPORT_RAW)


// Externally-visible variables

unsigned char dav_data[DAV_DATA_LEN];			// Binary data from weather station
unsigned char dav_data_valid;					// Flag to indicate data valid
const char * dav_error_str;						// Points to last error string


// Positions of various elements in binary data buffer

#define DAV_DATA_LOO			 0
#define DAV_DATA_BAR_L			 7
#define DAV_DATA_BAR_H			 8
#define DAV_DATA_IN_TEMP_L		 9
#define DAV_DATA_IN_TEMP_H		10
#define DAV_DATA_OUT_TEMP_L		12
#define DAV_DATA_OUT_TEMP_H		13
#define DAV_DATA_WIND_SPEED		14
#define DAV_DATA_WIND_DIR_L		16
#define DAV_DATA_WIND_DIR_H		17
#define DAV_DATA_LF				95
#define DAV_DATA_CR				96
#define DAV_DATA_CRC_H			97
#define DAV_DATA_CRC_L			98


// Special characters and strings used in data exchange

#define DAV_ACK					0x06
#define DAV_NAK					0x21

#define DAV_OK_STR				"\n\rOK\n\r"
#define DAV_OK_LEN				(sizeof(DAV_OK_STR) - 1)


// Internal states for the data collection state machine

enum state_value
	{
	DAV_IDLE = 0,
	DAV_STARTING,
	DAV_AWAITING_LF,
	DAV_AWAITING_CR,
    DAV_AWAITING_ACK,
    DAV_AWAITING_DATA,
    DAV_CHECKING_DATA,
	DAV_AWAITING_OK,
	DAV_ECHOING_RESP,
	DAV_AWAITING_TIME,
	};


// Valid command types

enum cmd_value
	{
	DAV_CMD_COLLECT = 0,
	DAV_CMD_SET_BAR,
	DAV_CMD_ECHO_RESP,
	DAV_CMD_CHK_TIME,
	DAV_CMD_SET_TIME,
	DAV_CMD_EXPECT_ACK,
	};


// Internal structure containing state variables

static struct
	{
	enum state_value state;				// Current state (see definition above)
	int condition;						// Overall status return code (see header file)

	unsigned int timeout;				// Timeout timer value

	enum cmd_value cmd_id;				// Command numeric identifier
	char * cmd_str;						// Command line string

	unsigned char attempt_count;		// Counter of attempts to send command
	unsigned int resp_tout;				// Timeout for response to command

	int parm1;							// Optional command parameter #1
	int parm2;							// Optional command parameter #2

	} dav_state;


// Number of seconds before overall time-out for state machine

#define TIMEOUT_SECS			20


// Macro to reset overall timeout timer

#define	RESET_TIMEOUT()			dav_state.timeout = SET_TIMEOUT_UI_SECS(TIMEOUT_SECS)


// Retry counts and timeouts for responses at individual stages

#define MAX_WAKEUP_ATTEMPTS		5

#define MAX_WAKEUP_MS			1200
#define MAX_RESP_MS				2000
#define MAX_DATA_MS				2000
#define MAX_ECHO_MS				1000
#define MAX_TIME_MS				2000


// Time buffer and definitions

#define DAV_TIME_LEN			8

static unsigned char dav_time[DAV_TIME_LEN];

#define DAV_TIME_SEC			0		// Positions of elements in time buffer
#define DAV_TIME_MIN			1
#define DAV_TIME_HOUR			2
#define DAV_TIME_DAY			3
#define DAV_TIME_MONTH			4
#define DAV_TIME_YEAR			5
#define DAV_TIME_CRC_H			6
#define DAV_TIME_CRC_L			7


// Maximum allowed time difference in seconds before weather station clock is updated

#define MAX_DIFF_TIME_T			30UL


// Buffers for incoming and outgoing serial data

static char inBuffE[128];
static char outBuffE[16];


// *** INTERNAL FUNCTIONS ***

// Internal function cleans up serial port state

static void dav_cleanup(void)
	{
	(void) SerialErrorE();					// Ignore any serial error

	SerialSendFlushE();						// Flush buffers
    SerialRecvFlushE();
	}


// Starts processing of command state machine
// Performs clean-up on serial port state

static void dav_start(enum cmd_value id, char * str)
	{
	report(DETAIL, "Starting");

	dav_cleanup();

	wx_set_leds(LED_DAVIS, LED_AMBER);

	dav_state.state = DAV_STARTING;
	dav_state.condition = DAV_PENDING;

	dav_state.cmd_id = id;
	dav_state.cmd_str = str;

	dav_state.parm1 = 0;
	dav_state.parm2 = 0;

	RESET_TIMEOUT();
	}


// Internal function sends a wakeup character to the weather station
// if the maximum number of attempts has not been exceeded
// Returns 0 if no attempts left or 1 if wakeup char is sent

static unsigned char send_wakeup(void)
	{
	if (dav_state.attempt_count == 0)
		return 0;                    		// Maximum tries exceeded

	--dav_state.attempt_count;
	dav_state.resp_tout = SET_TIMEOUT_UI_MS(MAX_WAKEUP_MS);

	report(DETAIL, "Sending wakeup char");

	SerialSendFlushE();
    SerialRecvFlushE();

	SerialPutcE('\n');

	return 1;								// Success
	}


// Internal function sends a command to the weather station

static void send_command(void)
	{
	dav_state.resp_tout = SET_TIMEOUT_UI_MS(MAX_RESP_MS);

	SerialSendFlushE();
    SerialRecvFlushE();

	switch(dav_state.cmd_id)
		{
		case DAV_CMD_SET_BAR:
			report(DETAIL, "Sending '%s=%d %d' command", dav_state.cmd_str,
							dav_state.parm1, dav_state.parm2);
			fprintf(SerialE, "%s=%d %d", dav_state.cmd_str,
							dav_state.parm1, dav_state.parm2);
			break;

		default:
			report(DETAIL, "Sending '%s' command", dav_state.cmd_str);
			fputs(dav_state.cmd_str, SerialE);
			break;
		}

	fputc('\n', SerialE);
	}


// Internal function to check received CRC for data against calculated CRC
// Appears to take 41-51ms to complete
// Returns boolean result of comparison (i.e. 0 if no match or !0 if match)

static int dav_check_data_crc(void)
	{
	unsigned int crc_calc;
   	unsigned int crc_recv;

	crc_calc = crc_calculate(&dav_data[0], DAV_DATA_LEN - 2);

	crc_recv = (unsigned int) dav_data[DAV_DATA_CRC_H] << 8;
	crc_recv |= dav_data[DAV_DATA_CRC_L];

	report(DETAIL, "Calculated CRC is %04X, Received CRC is %04X", \
			crc_calc, crc_recv);

	return (crc_calc == crc_recv);
	}


// Show example data readings

static void show_example_data(void)
	{
	unsigned int value;

	value = (unsigned int) dav_data[DAV_DATA_BAR_H] << 8;
	value |= dav_data[DAV_DATA_BAR_L];
	report(RAW_INFO, "Barometer: %u inHg x 1000, ", value);

	value = (unsigned int) dav_data[DAV_DATA_IN_TEMP_H] << 8;
	value |= dav_data[DAV_DATA_IN_TEMP_L];
	report(RAW_INFO, "In Temp: %u F x 10, ", value);

	value = (unsigned int) dav_data[DAV_DATA_OUT_TEMP_H] << 8;
	value |= dav_data[DAV_DATA_OUT_TEMP_L];
	report(RAW_INFO, "Out Temp: %u F x 10\r\n", value);

	report(RAW_INFO, "Wind Speed: %u mph, ", dav_data[DAV_DATA_WIND_SPEED]);

	value = (unsigned int) dav_data[DAV_DATA_WIND_DIR_H] << 8;
	value |= dav_data[DAV_DATA_WIND_DIR_L];
	report(RAW_INFO, "Wind Direction: %u degrees\r\n\r\n", value);
	}


// Checks whether serial buffer contains OK response
// Returns 0 if not, !0 if so

static int dav_check_ok_resp(void)
	{
	char buf[DAV_OK_LEN + 1];

	fread(buf, 1, DAV_OK_LEN, SerialE);
	buf[DAV_OK_LEN] = '\0';

	return (strcmp(buf, DAV_OK_STR) == 0);
	}


// Echoes responses until no chars are seen for timeout period
// Returns 1 if characters echoed during this call (resets timeout)
// Returns 0 if no characters echoed during this call
// Returns -1 if timeout has occurred

static int dav_echo_resp(void)
	{
	int ch;

    ch = SerialGetcE();

	if (ch != EOF)
		{
		dav_state.resp_tout = SET_TIMEOUT_UI_MS(MAX_ECHO_MS);
		putchar(ch);
		return 1;
		}
	else if (CHK_TIMEOUT_UI_MS(dav_state.resp_tout))
		{
		report(RAW_INFO, "[End of output]\r\n");
		return -1;
		}

	return 0;
	}


// Internal function to check received CRC for time against calculated CRC
// Returns boolean result of comparison (i.e. 0 if no match or !0 if match)

static int dav_check_time_crc(void)
	{
	unsigned int crc_calc;
   	unsigned int crc_recv;

	crc_calc = crc_calculate(&dav_time[0], DAV_TIME_LEN - 2);

	crc_recv = (unsigned int) dav_time[DAV_TIME_CRC_H] << 8;
	crc_recv |= dav_time[DAV_TIME_CRC_L];

	report(DETAIL, "Calculated CRC is %04X, Received CRC is %04X", \
			crc_calc, crc_recv);

	return (crc_calc == crc_recv);
	}


// Internal function to calculate CRC for time data and put it in buffer

static void dav_calc_time_crc(void)
	{
	unsigned int crc_calc;

	crc_calc = crc_calculate(&dav_time[0], DAV_TIME_LEN - 2);

	dav_time[DAV_TIME_CRC_H] = ((crc_calc >> 8) & 0xFF);
	dav_time[DAV_TIME_CRC_L] = (crc_calc & 0xFF);
	}


// Internal function to dump the contents of the time buffer

static void dav_dump_time(void)
	{
	unsigned int index;

	report(DETAIL, "Time buffer:");

	for (index = 0; index < DAV_TIME_LEN; ++index)
		report(RAW_DETAIL, " %02X", dav_time[index]);

	report(RAW_DETAIL, "\r\n");
	}


// Internal function to send RTC time to weather station

static void dav_send_time(void)
	{
    time_t now_val;
	struct tm * now_ptr;

	now_val = time(NULL);
	now_ptr = gmtime(&now_val);

	report(DETAIL, "now_val = %lu", now_val);

	dav_time[DAV_TIME_SEC]   = now_ptr->tm_sec;
	dav_time[DAV_TIME_MIN]   = now_ptr->tm_min;
    dav_time[DAV_TIME_HOUR]  = now_ptr->tm_hour;
	dav_time[DAV_TIME_DAY]   = now_ptr->tm_mday;
	dav_time[DAV_TIME_MONTH] = now_ptr->tm_mon + 1;
	dav_time[DAV_TIME_YEAR]  = now_ptr->tm_year;

	dav_calc_time_crc();
	dav_dump_time();

	fwrite(dav_time, 1, DAV_TIME_LEN, SerialE);
	}


// Internal function to check weather station clock against RTC time
// Returns !0 if time is within allowed range, or 0 if out of range

static int dav_check_time_diff(void)
	{
	struct tm cmp_time;
	time_t cmp_val;

	cmp_time.tm_sec  = dav_time[DAV_TIME_SEC];
	cmp_time.tm_min  = dav_time[DAV_TIME_MIN];
    cmp_time.tm_hour = dav_time[DAV_TIME_HOUR];
	cmp_time.tm_mday = dav_time[DAV_TIME_DAY];
	cmp_time.tm_mon  = (int) dav_time[DAV_TIME_MONTH] - 1;
	cmp_time.tm_year = dav_time[DAV_TIME_YEAR];

    cmp_val = mktime(&cmp_time);

	report(DETAIL, "cmp_val = %lu", cmp_val);

	if (rtc_diff(cmp_val) < MAX_DIFF_TIME_T)
        return 1;
	else
		return 0;
	}


// *** EXTERNAL FUNCTIONS ***

// Initialise serial port using buffers in this module
// (can be called more than once during application)
// Returns !0 on success, 0 if unable to initialise serial port

char dav_init_serial(void)
	{
	return SerialInitE(BR_19200, SER_8BITS, SER_IP2, 	// Weather station port
					   inBuffE, sizeof(inBuffE),
					   outBuffE, sizeof(outBuffE));
	}


// Initialise serial port and data collection state machine
// (must only be called once at start-up of application)
// Returns 0 on success, < 0 if unable to initialise serial port

int dav_init_all(void)
	{
	if (!dav_init_serial())
		{
		dav_error_str = "Cannot initialise serial port";
		report(PROBLEM, dav_error_str);
		return -1;
		}

	dav_error_str = "Serial port initialised okay";
	report(DETAIL, dav_error_str);

	wx_set_dtr_true();                             	// Enable handshake lines
	wx_set_rts_true();

	memset(&dav_state, 0, sizeof(dav_state));		// Zero all state variables

    dav_state.condition = DAV_NOT_STARTED;

	memset(dav_data, 0, sizeof(dav_data));			// Clear data buffer
	dav_data_valid = 0;

	memset(dav_time, 0, sizeof(dav_time));			// Clear time buffer

	return 0;
	}


// Starts data collection state machine

void dav_start_collect(void)
	{
	dav_start(DAV_CMD_COLLECT, "LOOP 1");
	}


// Starts setting barometer and elevation values

void dav_start_set_bar(int barometer, int elevation)
	{
	dav_start(DAV_CMD_SET_BAR, "BAR");

	dav_state.parm1 = barometer;
	dav_state.parm2 = elevation;
	}


// Starts a command which returns text responses

void dav_start_echo_resp(char * cmd)
	{
	dav_start(DAV_CMD_ECHO_RESP, cmd);
	}


// Starts a command which checks weather station clock

void dav_start_check_time(void)
	{
	dav_start(DAV_CMD_CHK_TIME, "GETTIME");
	}


// Starts a command which checks weather station clock

void dav_start_set_time(void)
	{
	dav_start(DAV_CMD_SET_TIME, "SETTIME");
	}


// Aborts data collection state machine immediately
// Performs clean-up on serial port state

void dav_abort(void)
	{
	report(DETAIL, "Aborting");

	dav_cleanup();

	wx_set_leds(LED_DAVIS, LED_RED);

	dav_state.state = DAV_IDLE;
	dav_state.condition = DAV_ABORTED;

	dav_error_str = "Aborted";
	}


// Get current status of data collection state machine (see header file)
// 0 means activity pending, < 0 means failure, > 0 means success

int dav_get_status(void)
	{
	return dav_state.condition;
	}


// Dump data to console

#define DUMP_COLS	20

void dav_dump_data(void)
	{
    unsigned int loc;
	unsigned char col;

	report(RAW_INFO, "\r\n  + :");

	for (col = 0; col < DUMP_COLS; ++col)
		report(RAW_INFO, " %2u", col);

	report(RAW_INFO, "\r\n----:");

	for (col = 0; col < DUMP_COLS; ++col)
		report(RAW_INFO, "---", col);

	for (loc = 0, col = DUMP_COLS; loc < DAV_DATA_LEN; ++loc, ++col)
		{
		if (col >= DUMP_COLS)
			{
			report(RAW_INFO, "\r\n%3u :", loc);
			col = 0;
			}

		report(RAW_INFO, " %02X", dav_data[loc]);
		}

	report(RAW_INFO, "\r\n\r\n");

    show_example_data();
	}


// Determines which state to enter after a command is sent

static void set_post_cmd_state(void)
	{
	switch(dav_state.cmd_id)
		{
		case DAV_CMD_SET_BAR:
		case DAV_CMD_ECHO_RESP:
			dav_state.state = DAV_AWAITING_OK;
			break;

		default:
			dav_state.state = DAV_AWAITING_ACK;
			break;
		}
	}


// Determines which state to enter after an ACK is received
// Returns DAV_PENDING if command has completed or DAV_PENDING if not

static int set_post_ack_state(void)
	{
	switch(dav_state.cmd_id)
		{
		case DAV_CMD_COLLECT:
			dav_state.resp_tout = SET_TIMEOUT_UI_MS(MAX_DATA_MS);
			dav_state.state = DAV_AWAITING_DATA;
			break;

		case DAV_CMD_CHK_TIME:
			dav_state.resp_tout = SET_TIMEOUT_UI_MS(MAX_TIME_MS);
			dav_state.state = DAV_AWAITING_TIME;
			break;

		case DAV_CMD_SET_TIME:
			dav_send_time();
			dav_state.cmd_id = DAV_CMD_EXPECT_ACK;
			dav_state.resp_tout = SET_TIMEOUT_UI_MS(MAX_TIME_MS);
			dav_state.state = DAV_AWAITING_ACK;
			break;

		default:
			return DAV_SUCCESS;		// Commands that just return ACK
		}

	return DAV_PENDING;
	}


// Determines which state to enter after an OK is received
// Returns DAV_PENDING if command has completed or DAV_PENDING if not

static int set_post_ok_state(void)
	{
	switch(dav_state.cmd_id)
		{
		case DAV_CMD_ECHO_RESP:
			report(RAW_INFO, "\r\n[Start of output]\r\n");
			dav_state.resp_tout = SET_TIMEOUT_UI_MS(MAX_ECHO_MS);
			dav_state.state = DAV_ECHOING_RESP;
			break;

		default:
     		return DAV_SUCCESS;		// Commands that just return OK
     	}

	return DAV_PENDING;
	}


// Main "tick" routine which drives data collection state machine
// Return value indicates current status (see header file)
// 0 means activity pending, < 0 means failure, > 0 means success

int dav_tick(void)
	{
	if (dav_state.state == DAV_IDLE)			// Not active?
		{
		dav_cleanup();							// Clean out serial port
		return dav_state.condition;  			// -- EXIT --
		}

	// Check whether serial port error has occurred
	if (SerialErrorE())
		{
		dav_error_str = "Serial port error";
		report(PROBLEM, "%s in state %d", dav_error_str, dav_state.state);
		dav_state.condition = DAV_SERIAL_ERR;
		goto dav_error;
		}

	// Check whether overall timeout has occurred
	if (CHK_TIMEOUT_UI_SECS(dav_state.timeout))
		{
		dav_error_str = "Timed out";
		report(PROBLEM, "%s in state %d", dav_error_str, dav_state.state);
		dav_state.condition = DAV_TIMEOUT;
		goto dav_error;
	   	}

	// Process current state

	switch(dav_state.state)
		{
		// Attempt to wake up weather station
		case DAV_STARTING:
			dav_state.attempt_count = MAX_WAKEUP_ATTEMPTS;
			(void) send_wakeup();
            dav_state.state = DAV_AWAITING_LF;
			RESET_TIMEOUT();
			break;

		// Wait for first character of wakeup response
		case DAV_AWAITING_LF:
			if (SerialGetcE() == '\n')
				{
				dav_state.state = DAV_AWAITING_CR;
				// No report message or global timeout reset
				}
			else if (CHK_TIMEOUT_UI_MS(dav_state.resp_tout))
				{
                if (!send_wakeup())
					{
					dav_error_str = "No wakeup response received";
					report(PROBLEM, dav_error_str);
					dav_state.condition = DAV_NO_WAKEUP;
					goto dav_error;
					}
				}
			break;

		// Wait for second character of wakeup response
		case DAV_AWAITING_CR:
			switch(SerialGetcE())
				{
                case '\r':
					report(DETAIL, "Wakeup response received");
					send_command();
					set_post_cmd_state();
					RESET_TIMEOUT();
					break;

				case '\n':
				case EOF:
					if (!CHK_TIMEOUT_UI_MS(dav_state.resp_tout))
						break;

					// Fall through to default if timed out

				default:
					if (send_wakeup())
						{
						dav_state.state = DAV_AWAITING_LF;
						// No report message or global timeout reset
						}
					else
						{
						dav_error_str ="Bad wakeup response received";
						report(PROBLEM, dav_error_str);
						dav_state.condition = DAV_BAD_WAKEUP;
						goto dav_error;
						}
					break;
				}
			break;

		// Wait for acknowledge response
		case DAV_AWAITING_ACK:
			switch(SerialGetcE())
				{
				case DAV_ACK:
					report(DETAIL, "Acknowledgement received");
                    if (set_post_ack_state() == DAV_SUCCESS)
						goto dav_successful;
                    RESET_TIMEOUT();
					break;

				case EOF:
					if (CHK_TIMEOUT_UI_MS(dav_state.resp_tout))
						{
						dav_error_str = "No acknowledgement received";
						report(PROBLEM, dav_error_str);
						dav_state.condition = DAV_NO_ACK;
						goto dav_error;
						}
					break;

				case DAV_NAK:
					dav_error_str = "Negative acknowledgement received";
					report(PROBLEM, dav_error_str);
					dav_state.condition = DAV_NEG_ACK;
					goto dav_error;

				default:
					dav_error_str = "Bad acknowledgement received";
					report(PROBLEM, dav_error_str);
					dav_state.condition = DAV_BAD_ACK;
					goto dav_error;
				}
			break;

		// Wait for data packet of required length
		case DAV_AWAITING_DATA:
			if (SerialRecvCountE() >= DAV_DATA_LEN)
				{
				dav_data_valid = 0;
				fread(dav_data, 1, DAV_DATA_LEN, SerialE);

				report(DETAIL, "Data received");
				dav_state.state = DAV_CHECKING_DATA;
				RESET_TIMEOUT();
				}
			else if (CHK_TIMEOUT_UI_MS(dav_state.resp_tout))
				{
				dav_error_str = "No data received";
				report(PROBLEM, dav_error_str);
				dav_state.condition = DAV_NO_DATA;
				goto dav_error;
				}
			break;

  		// Process received data
		case DAV_CHECKING_DATA:
			if (strncmp(&dav_data[DAV_DATA_LOO], "LOO", 3) != 0)
				{
				dav_error_str = "Data does not start with 'LOO'";
				report(PROBLEM, dav_error_str);
				dav_state.condition = DAV_BAD_DATA;
				goto dav_error;
				}

			if (dav_data[DAV_DATA_LF] != '\n' || dav_data[DAV_DATA_CR] != '\r')
				{
				dav_error_str = "Data does not contain LF, CR";
				report(PROBLEM, dav_error_str);
				dav_state.condition = DAV_BAD_DATA;
				goto dav_error;
				}

			if (!dav_check_data_crc())
				{
				dav_error_str = "Data failed CRC check";
				report(PROBLEM, dav_error_str);
				dav_state.condition = DAV_BAD_CRC;
				goto dav_error;
				}

			report(DETAIL, "Data is valid");
			dav_data_valid = 1;
			goto dav_successful;

		// Wait for OK response
		case DAV_AWAITING_OK:
			if (SerialRecvCountE() >= DAV_OK_LEN)
				{
				if (dav_check_ok_resp())
					{
					report(DETAIL, "OK response received");
                    if (set_post_ok_state() == DAV_SUCCESS)
						goto dav_successful;
                    RESET_TIMEOUT();
					}
				else
					{
					dav_error_str = "Bad acknowledgement received";
					report(PROBLEM, dav_error_str);
					dav_state.condition = DAV_BAD_ACK;
					goto dav_error;
					}
				}
			else if (CHK_TIMEOUT_UI_MS(dav_state.resp_tout))
				{
				dav_error_str = "No acknowledgement received";
				report(PROBLEM, dav_error_str);
				dav_state.condition = DAV_NO_ACK;
				goto dav_error;
				}
			break;

		// Wait for OK response
		case DAV_ECHOING_RESP:
			if (dav_echo_resp() < 0)
				goto dav_successful;
			break;

		// Wait for time packet of required length
		case DAV_AWAITING_TIME:
			if (SerialRecvCountE() >= DAV_TIME_LEN)
				{
				fread(dav_time, 1, DAV_TIME_LEN, SerialE);
				report(DETAIL, "Time received");
				dav_dump_time();
				if (!dav_check_time_crc())
					{
					dav_error_str = "Time failed CRC check";
					report(PROBLEM, dav_error_str);
					dav_state.condition = DAV_BAD_TIME;
					goto dav_error;
					}
				if (!dav_check_time_diff())
					{
					dav_error_str = "Time does not match Interface clock";
					report(DETAIL, dav_error_str);
					dav_state.condition = DAV_WRONG_TIME;
					goto dav_time_mismatch;
					}
				report(DETAIL, "Time is correct");
				goto dav_successful;
				}
			else if (CHK_TIMEOUT_UI_MS(dav_state.resp_tout))
				{
				dav_error_str = "No time received";
				report(PROBLEM, dav_error_str);
				dav_state.condition = DAV_NO_TIME;
				goto dav_error;
				}
			break;

		// Undefined state value
		default:
			dav_error_str = "Bad state encountered";
			report(PROBLEM, dav_error_str);
			dav_state.condition = DAV_BAD_STATE;
			goto dav_error;
		}

	// Pending states fall out of bottom of switch() block here

	dav_state.condition = DAV_PENDING;
	return dav_state.condition;       				// -- EXIT --

	// Data collection success handler
	dav_successful:
		dav_error_str = "Success";
		wx_set_leds(LED_DAVIS, LED_GREEN);
		dav_state.state = DAV_IDLE;
		dav_state.condition = DAV_SUCCESS;
		return dav_state.condition;       			// -- EXIT --

	// Data collection time mismatch handler
	dav_time_mismatch:
		// No need for cleanup here
		wx_set_leds(LED_DAVIS, LED_GREEN);
		dav_state.state = DAV_IDLE;
		return dav_state.condition;       			// -- EXIT --

	// Data collection error handler
	dav_error:
		dav_cleanup();
		wx_set_leds(LED_DAVIS, LED_RED);
		dav_state.state = DAV_IDLE;
		return dav_state.condition;             	// -- EXIT --
	}
