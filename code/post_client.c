// HTTP POST client routines

// Copyright (c) 2006, Ian Chapman (Chapmip Consultancy)

// All rights reserved, except for those rights implicitly granted to
// GitHub Inc by publishing on GitHub and those rights granted by
// commercial agreement with the author.


#include <stdio.h>
#include <dcdefs.h>
#include <stcpip.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include "timeout.h"
#include "wx_board.h"
#include "report.h"
#include "bb_vars.h"
#include "rtc_utils.h"
#include "wx_main.h"
#include "post_client.h"


// A complete definition of the format for HTTP commands, responses,
// and behaviour can be found in RFC2616 from http://www.ietf.org


// Short-cut names for types of report output (see "report.h")

#define PROBLEM		(REPORT_POST | REPORT_PROBLEM)
#define DETAIL		(REPORT_POST | REPORT_DETAIL)

#define RAW_DETAIL	(DETAIL | REPORT_RAW)


// POST command header format
// (includes variables for path, hostname, port, body length)

static const char post_fmt[] =	"POST %s%s%s HTTP/1.1\r\n" \
								"Connection: close\r\n" \
								"Host: %s:%u\r\n" \
								"User-Agent: Rabbit\r\n" \
								"Content-Type: application/x-www-form-urlencoded\r\n" \
								"Content-Length: %u\r\n" \
								"\r\n";


// Maximum length of hostname and path

#define MAX_HOST_LEN		64
#define MAX_PATH_LEN		64


// Maximum size of command buffer

#define CMD_BUF_SIZE		350


// Check that buffer size is adequate for variable parameters

#define MAX_CMD_PARM_SIZE	(7 + MAX_HOST_LEN + MAX_PATH_LEN + MAX_HOST_LEN + 5 + 5)

#if CMD_BUF_SIZE < (sizeof(post_fmt) + MAX_CMD_PARM_SIZE - 12 + 1)
#error "CMD_BUF_SIZE is too small"
#endif


// Default size of body buffer (if not specified on initialisation)

#define DEF_BODY_BUF_SIZE	512


// Identified responses from remote server (must appear at start of line)

#define RESP_SUCCESS    	1		// Must be one greater than array indexes
#define RESP_BAD_ID			2
#define RESP_BAD_DATA		3
#define RESP_REJECTED		4

static const char * post_resp[] =
	{
	"Success!",				// RESP_SUCCESS
	"Bad ID!",				// RESP_BAD_ID
	"Bad data!",			// RESP_BAD_DATA
	"Reject!",				// RESP_REJECTED
	};


// Other field labels within response line from remote server

#define RESP_LABEL_TIME_T	"Server time ="


// Maximum allowed time difference in seconds before real-time clock is updated

#define MAX_DIFF_TIME_T		40UL


// Maximum time to use cached IP address before requiring another DNS lookup

#define DNS_CACHE_SECS		3600


// Internal states for the POST server

enum state_value
	{
	POST_IDLE = 0,
	POST_STARTING,
	POST_RESOLVING,
    POST_OPENING,
    POST_AWAITING_ESTAB,
    POST_SENDING_COMMAND,
    POST_SENDING_BODY,
	POST_READING_STATUS,
    POST_READING_HEADERS,
	POST_CHECKING_BODY,
	POST_READING_BODY,
	};


// Internal structure containing state variables for POST client

static struct
	{
	enum state_value state;				// Current state (see definition above)
	int condition;						// Overall status return code (see header file)
	unsigned char resp_class;			// First digit of status response from server (e.g. 2XX)
	unsigned char resp_result;			// Ennumerated value of response message from server

	int dns;							// Handle for nameserver resolve
	unsigned int timeout;				// Timeout timer value
	unsigned char sock_opened;			// Flag indicating socket opened
	unsigned char servers_set;			// Flag indicating servers set okay

	char * server_host;					// Host name sent in "Host:" header line
	char * server_path;					// Path sent in "POST" command line
	word server_port;					// Port number sent in "Host:" header line

	char * request_host;				// Host to which TCP connection is made (may be proxy)
	longword request_ip;				// IP address resolved for above name
	word request_port;					// Destination port to which TCP connection is made

	char * abs_uri_prefix;				// Set to "http://" for proxy access, otherwise ""
	char * abs_uri_host;				// Set to server host for proxy access, otherwise ""

	longword cached_ip;					// Last resolved IP address (if any)
	unsigned int cache_timeout;			// Determines time at which cached IP address expires

	unsigned int msg_len;				// Length of message in buffer to send
	unsigned int msg_pos;				// Position in buffer of next message byte to send

	char cmd_buf[CMD_BUF_SIZE];			// Buffer for requests and responses

	char far * body_buf;				// Pointer to xmem buffer for body message to send
	unsigned int body_buf_size;			// Size of xmem buffer (set up on initialisation)
	unsigned int body_pos;				// Position in xmem buffer of next free character
	unsigned char body_overflow;		// Flag to indicate buffer overrun was prevented

	tcp_Socket socket;					// TCP socket for outbound HTTP connection

	} post_state;


// Number of seconds before timing out POST attempt

#define TIMEOUT_SECS		20


// Macro to reset timeout timer

#define	RESET_TIMEOUT()		post_state.timeout = SET_TIMEOUT_UI_SECS(TIMEOUT_SECS)


// *** INTERNAL FUNCTIONS ***

// Internal function attempts to add a character to the body buffer
// If url_encode is non-zero then non-alphanumeric chars are hex encoded
// Returns 0 on success or -1 if not enough room in body buffer

static int add_body_char(char ch, int url_encode)
	{
	char * fmt;
	unsigned int inc;

	fmt = "%c";						// Default is just add character
	inc = 1;

	if (url_encode)
		{
		if (!isalnum(ch))
			{
			if (ch == ' ')
				ch = '+';
			else
				{
				fmt = "%%%02X";
				inc = 3;
				}
			}
		}

	if (post_state.body_pos + inc >= post_state.body_buf_size)
		return -1;

	farsprintf((post_state.body_buf + post_state.body_pos), fmt, ch);
	post_state.body_pos += inc;

	return 0;
	}


// Internal function attempts to add URL-encoded ASCII string to the body buffer
// Returns 0 on success or -1 if not enough room in body buffer

static int add_body_string(const char * str)
	{
	while (*str != 0)
		{
		if (add_body_char(*str, 1) < 0)
			return -1;

		++str;
        }

	return 0;
	}


// Internal function converts low nibble of parameter to ASCII hex digit
// Returns ASCII character corresponding to value in low 4 bits of nibble

static char nibb_to_hex(char nibble)
	{
    nibble &= 0x0F;								// Discard upper bits

	if (nibble <= 9)
		return (char) (nibble + '0');			// Decimal digit
	else
		return (char) (nibble + 'A' - 10);		// Hex A-F digit
	}


// Internal function attempts to add fixed-length hexadecimal string to body buffer
// Note: len must be in range 0-32767
// Returns 0 on success or -1 if not enough room in body buffer

static int add_body_hexstring(const char * bytes, unsigned int len)
	{
	char far * body_ptr;

	if (post_state.body_pos + (len * 2) >= post_state.body_buf_size)
		return -1;				// Not enough room in body buffer

	body_ptr = post_state.body_buf + post_state.body_pos;

	post_state.body_pos += (len * 2);				// Move pointer on before len is trashed

	while (len--)
		{
		*body_ptr++ = nibb_to_hex(*bytes >> 4);		// Add upper nibble
		*body_ptr++ = nibb_to_hex(*bytes++);		// Add lower nibble
		}

	*body_ptr = '\0';				// Add zero terminator

	return 0;
    }


// Internal function checks name for numeric IP address
// Returns 0 if no match
// Updates request_ip and returns 1 if match found

static int check_direct_ip(char * name)
	{
	longword result;

	result = inet_addr(name);

	if (result == 0L)
		return 0;

	post_state.request_ip = result;
	return 1;
	}


// Internal function attempts to get a response line from the server
// Returns 0 if no data pending or 1 if line of data received

static int get_response(void)
	{
	if (sock_bytesready(&post_state.socket) == -1)
		return 0;

	sock_gets(&post_state.socket, post_state.cmd_buf, sizeof(post_state.cmd_buf));

	if (post_state.cmd_buf[0])
		report(DETAIL, "Read: %s", post_state.cmd_buf);
	else
		report(DETAIL, "Read: (blank line)");

	return 1;
	}


// Internal function attempts to send a message to the server
// Returns 0 on successful write but with data still pending,
// or 1 if all data has been written, or < 0 on failure

static int send_message(char far * msg_buf)
	{
	int rc;

	rc = sock_xfastwrite(&post_state.socket, (long) (msg_buf + post_state.msg_pos),
			  			(post_state.msg_len - post_state.msg_pos));

	if (rc < 0)
		{
		bb_post_error_str = "sock_xfastwrite() failed";
		report(PROBLEM, "%s with %d", bb_post_error_str, rc);
		return -1;						// Error
		}

	if (rc > 0)
		report(DETAIL, "Wrote %d bytes", rc);

	post_state.msg_pos += rc;

	if (post_state.msg_pos == post_state.msg_len)
		{
		report(DETAIL, "Write completed (%d bytes)", post_state.msg_pos);
		return 1;						// Completed
		}

	return 0;						// Still pending
	}


// Internal function checks response status line from the server
// Updates resp_class value if valid status line is found
// Returns 0 on valid response, < 0 if response not in correct HTTP format

static int check_resp_status(void)
	{
	char *ptr;
	int count;
	int class;

	ptr = &post_state.cmd_buf[0];

	if (strnicmp(ptr, "HTTP/", 5) != 0)
		{
		bb_post_error_str = "HTTP header not found in status response";
		report(PROBLEM, bb_post_error_str);
		return -1;
		}

	ptr = strpbrk(ptr, " \t");
	if (!ptr)
		{
		bb_post_error_str = "Delimiting space not found in status response";
		report(PROBLEM, bb_post_error_str);
		return -2;
		}

	count = strspn(ptr, " \t");
	if (count <= 0)
		{
		bb_post_error_str = "Unable to skip past space in status response";
		report(PROBLEM, bb_post_error_str);
		return -3;
		}

	ptr += count;

	class = *ptr;
	if (class < '1' || class > '5')
		{
		bb_post_error_str = "Unexpected class digit";
		report(PROBLEM, "%s: %c", bb_post_error_str, class);
		return -4;
		}

	post_state.resp_class = (class - '0');
	return 0;
	}


// Internal function checks response line from server for response message
// Updates resp_result value if valid response line is found
// Returns 0 if no response identified, or response value (> 0) if found

static int check_resp_result(void)
	{
	unsigned char i;

	for (i = 0; i < sizeof(post_resp) / sizeof(post_resp[0]); ++i)
		{
		if (strnicmp(post_state.cmd_buf, post_resp[i], strlen(post_resp[i])) == 0)
			{
			post_state.resp_result = i + 1;
			report(DETAIL, "Found response %u", post_state.resp_result);
			return post_state.resp_result;
			}
		}

	return 0;
	}


// Internal function checks response line from server for time_t value
// Just returns quietly if time_t tag is not found
// Returns -1 if time_t value not found
// Returns -2 if time_t value is zero or non-numeric
// Returns -3 if time_t value is too large
// Returns  0 if real-time clock is unchanged (was within range of time_t value)
// Returns  1 if real-time clock has been adjusted to received time_t value

static int check_resp_time_t(void)
	{
	char * ptr;
	time_t value;

	if (strnicmp(post_state.cmd_buf, RESP_LABEL_TIME_T, sizeof(RESP_LABEL_TIME_T) - 1) != 0)
		return -1;

	ptr = post_state.cmd_buf + sizeof(RESP_LABEL_TIME_T) - 1;

	value = strtoul(ptr, NULL, 10);
	if (value == 0UL)
		{
		report(PROBLEM, "time_t value is zero or non-numeric");
		return -2;
		}
	else if (value == ULONG_MAX)
		{
		report(PROBLEM, "time_t value is too large");
		return -3;
		}

	if (rtc_diff(value) < MAX_DIFF_TIME_T)
		{
		report(DETAIL, "Interface clock matches time_t value");
		rtc_validated = 1;
		return 0;
		}

	rtc_update(value);					// Update real-time clock

	report(DETAIL, "Interface clock adjusted to %s", rtc_str());
	rtc_validated = 0;
	return 1;
	}


// Internal function cleans up pending DNS enquiry or open TCP socket

static void post_cleanup(void)
	{
	if (post_state.dns > 0)
		{
		report(DETAIL, "Cancelling resolve request");
		(void) resolve_cancel(post_state.dns);
		post_state.dns = 0;
		}

	if (post_state.sock_opened)
		{
		report(DETAIL, "Closing socket");
		sock_abort(&post_state.socket);
		post_state.sock_opened = 0;
		}
	}


// *** EXTERNAL FUNCTIONS ***

// Initialise POST state machine and allocate body buffer
// (must only be called once at start-up of application)
// Returns 0 on success, < 0 if unable to allocate buffer

int post_init(unsigned int body_max_size)
	{
	memset(&post_state, 0, sizeof(post_state));		// First, clear all state variables

    post_state.condition = POST_NOT_STARTED;

	post_state.cmd_buf[0] = '\0';					// Zero-length string in near buffer

	rtc_validated = 0;								// Real-time clock not checked yet

	if (body_max_size != 0)
		post_state.body_buf_size = body_max_size;
	else
		post_state.body_buf_size = DEF_BODY_BUF_SIZE;

	post_state.body_buf = (char far *) xalloc(post_state.body_buf_size);

	if (!post_state.body_buf)
		{
		report(PROBLEM, "Failed to allocate body_buf storage (%u bytes)",
						 post_state.body_buf_size);
		return -1;
		}

	report(DETAIL, "Allocated body_buf storage (%u bytes at %06lX)",
					post_state.body_buf_size, (long) post_state.body_buf);

	post_state.body_buf[0] = '\0';					// Zero-length string in xmem buffer

	return 0;
	}


// Sets up server details for POST state machine
// Invokes proxy if proxy_host/proxy_port are non-zero
// Invalidates any cached DNS result for IP address
// Returns 0 if okay, < 0 if a string parameter is invalid

int post_set_server(char * host, word port, char * path, char * proxy_host, word proxy_port)
	{
    int len;

	post_state.servers_set = 0;			// Assume failure

	post_state.cached_ip = 0L;			// Invalidate any cached IP address

	if ((len = strlen(host)) == 0 || len > MAX_HOST_LEN)
		return -1;

	if ((len = strlen(path)) == 0 || len > MAX_PATH_LEN)
		return -2;

	post_state.server_host = host;
	post_state.server_path = path;
	post_state.server_port = port;

	if (proxy_host != NULL)
		{
		if ((len = strlen(proxy_host)) == 0 || len > MAX_HOST_LEN)
			return -3;

        post_state.request_host = proxy_host;

		post_state.abs_uri_prefix = "http://";
		post_state.abs_uri_host = host;
		}
    else		// No proxy
		{
		post_state.request_host = host;

		post_state.abs_uri_prefix = "";
		post_state.abs_uri_host = "";
		}

	if (proxy_port != 0)
		post_state.request_port = proxy_port;
	else
		post_state.request_port = port;

	post_state.servers_set = 1;
	return 0;
	}


// Clears body buffer for addition of new variables

void post_clear_body(void)
	{
	post_state.body_pos = 0;
	post_state.body_overflow = 0;
	post_state.body_buf[0] = '\0';
	}


// Adds name/value pair to body buffer with URL-encoding
// If hexlen = 0, then value is treated as pointer to zero-terminated ASCII string
// If hexlen > 0, then value is treated as pointer to fixed-length (hexlen) binary
// string (i.e. may contain zeroes) for output as pairs of hexadecimal digits
// Returns 0 on success, < 0 on error

int post_add_variable(const char * name, const char * value, unsigned int hexlen)
	{
	unsigned int start_pos;

	if (name[0] == '\0')
		return -2;					// Fail if zero-length string

	if (hexlen == 0 && value[0] == '\0')
		return -3;					// Fail if zero-length ASCII string

	start_pos = post_state.body_pos;

    if (post_state.body_pos != 0)
		{
		if (add_body_char('&', 0) < 0)
			goto no_room;
		}

	if (add_body_string(name) < 0 || add_body_char('=', 0) < 0)
		goto no_room;

	if (hexlen == 0)
		{
		if (add_body_string(value) < 0)
			goto no_room;
		}
	else
		{
		if (add_body_hexstring(value, hexlen) < 0)
			goto no_room;
		}

	return 0;

	// Exception handler to restore buffer to state on entry
	no_room:

		post_state.body_pos = start_pos;
		post_state.body_buf[post_state.body_pos] = '\0';

		post_state.body_overflow = 1;

		return -1;
	}


// Checks state of body buffer overflow flag
// Returns 0 if no overflow, !0 if overflow

int post_check_overflow(void)
	{
    return post_state.body_overflow;
	}


// Starts processing of POST state machine
// Returns 0 on success
// Returns -1 if servers not set up properly via post_set_servers
// Returns -2 if no body text has been set up to send

int post_start(void)
	{
	post_cleanup();

    if (!post_state.servers_set)
		{
		report(PROBLEM, "Cannot start - servers not set");
		wx_set_leds(LED_POST, LED_RED);

		post_state.state = POST_IDLE;
		post_state.condition = POST_CANNOT_START;
		return -1;
		}

	if (post_state.body_pos == 0)
		{
		report(PROBLEM, "Cannot start - no body text");
		wx_set_leds(LED_POST, LED_RED);

		post_state.state = POST_IDLE;
		post_state.condition = POST_CANNOT_START;
		return -2;
		}

	report(DETAIL, "Starting");

	wx_set_leds(LED_POST, LED_AMBER);

	post_state.state = POST_STARTING;
	post_state.condition = POST_PENDING;
	post_state.resp_class = 0;
	post_state.resp_result = 0;

	RESET_TIMEOUT();
	return 0;
	}


// Aborts POST state machine immediately
// Performs clean-up on pending DNS query or open TCP socket

void post_abort(void)
	{
	report(DETAIL, "Aborting");

	post_cleanup();

	wx_set_leds(LED_POST, LED_RED);

	bb_post_error_str = "Aborted";
	bb_post_error_state_num = -1;

	post_state.state = POST_IDLE;
	post_state.condition = POST_ABORTED;
	}


// Get current status of POST state machine (see header file)
// 0 means activity pending, < 0 means failure, > 0 means success

int post_get_status(void)
	{
	return post_state.condition;
	}


// Get class digit (1-5) from server response to last POST transaction
// Call this routine if POST state machine exited with POST_SERVER_ERR
// to find out failure code (e.g. 4 for 404 Not Found error)
// 0 means no code received

int post_get_resp_class(void)
	{
	return post_state.resp_class;
	}


// Main "tick" routine which drives POST state machine
// Return value indicates current status (see header file)
// 0 means activity pending, < 0 means failure, > 0 means success

int post_tick(void)
	{
	int rc;

	if (post_state.state == POST_IDLE)			// Nothing to do?
		return post_state.condition;  			// -- EXIT --

	// If appropriate, check whether socket has closed prematurely
	if (post_state.sock_opened && post_state.state != POST_READING_BODY)
		{
		if (!tcp_tick(&post_state.socket))
			{
			bb_post_error_str = "Socket closed unexpectedly";
			report(PROBLEM, "%s in state %d", bb_post_error_str, post_state.state);
			post_state.sock_opened = 0;
			post_state.condition = POST_CONNECTION_LOST;
			goto post_error;
			}
		}

	// Check whether timeout has occurred
	if (CHK_TIMEOUT_UI_SECS(post_state.timeout))
		{
		bb_post_error_str = "Timed out";
		report(PROBLEM, "%s in state %d", bb_post_error_str, post_state.state);
		post_state.condition = POST_TIMEOUT;
		goto post_error;
	   	}

	// Process current state

	switch(post_state.state)
		{
		// Attempt to open connection to HTTP server
		case POST_STARTING:

			if (post_state.cached_ip != 0L &&		// Use cached IP address?
				!CHK_TIMEOUT_UI_SECS(post_state.cache_timeout))
				{
				post_state.request_ip = post_state.cached_ip;
				post_state.state = POST_OPENING;
				RESET_TIMEOUT();
				}
			else if (check_direct_ip(post_state.request_host))
				{
				post_state.state = POST_OPENING;
				RESET_TIMEOUT();
				}
			else
				{
				report(DETAIL, "Resolving %s", post_state.request_host);
				post_state.dns = resolve_name_start(post_state.request_host);
				if (post_state.dns <= 0)             // Must be 1 or greater
					{
					bb_post_error_str = "Error starting resolve";
					report(PROBLEM, "%s (%d)", bb_post_error_str, post_state.dns);
					post_state.condition = POST_DNS_ERR;
					goto post_error;
					}

				post_state.state = POST_RESOLVING;
				RESET_TIMEOUT();
				}
			break;

		// Attempt to resolve server name to IP address
		case POST_RESOLVING:
			tcp_tick(NULL);					// Needed! (or else returns 0.0.0.0)

			rc = resolve_name_check(post_state.dns, &post_state.request_ip);

			if (rc == RESOLVE_SUCCESS)
				{
				post_state.dns = 0;
				post_state.cached_ip = post_state.request_ip;    // Update cache
				post_state.cache_timeout = SET_TIMEOUT_UI_SECS(DNS_CACHE_SECS);
				post_state.state = POST_OPENING;
				RESET_TIMEOUT();
				}
			else if (rc != RESOLVE_AGAIN)
				{
				if (rc == RESOLVE_FAILED)
					{
					bb_post_error_str = "Resolve failed - host name does not exist";
					report(PROBLEM, bb_post_error_str);
					}
				else
					{
					bb_post_error_str = "Error during resolve";
					report(PROBLEM, "%s (%d)", bb_post_error_str, rc);
					}

				post_state.dns = 0;
				post_state.condition = POST_DNS_ERR;
				goto post_error;
				}
   			break;

		// Attempt to open TCP connection to server
		case POST_OPENING:
			report(DETAIL, "Opening to %s:%u", get_ip_string(post_state.request_ip), post_state.request_port);

			if (!tcp_open(&post_state.socket, 0, post_state.request_ip, post_state.request_port, NULL))
				{
				bb_post_error_str = "Error opening socket";
				report(PROBLEM, bb_post_error_str);
				post_state.condition = POST_SOCKET_ERR;
				goto post_error;
				}

			post_state.sock_opened = 1;
			sock_mode(&post_state.socket, TCP_MODE_ASCII);
			post_state.state = POST_AWAITING_ESTAB;
			RESET_TIMEOUT();
			break;

		// Wait for connection to be established
		case POST_AWAITING_ESTAB:
			if (sock_established(&post_state.socket))
				{
				report(DETAIL, "Connected");

				sprintf(post_state.cmd_buf, post_fmt,
						post_state.abs_uri_prefix, post_state.abs_uri_host,
						post_state.server_path, post_state.server_host,
						post_state.server_port, post_state.body_pos);

				report(DETAIL, "Sending command header:");
				report(RAW_DETAIL, "%s", post_state.cmd_buf);

				post_state.msg_len = strlen(post_state.cmd_buf);
				post_state.msg_pos = 0;

				post_state.state = POST_SENDING_COMMAND;
				RESET_TIMEOUT();
				}
			break;

		// Send command to server
		case POST_SENDING_COMMAND:
			switch(send_message(post_state.cmd_buf))
				{
				case 1:
					report(DETAIL, "Sending body text:");
					report(RAW_DETAIL, "%ls\r\n", post_state.body_buf);

					post_state.msg_len = post_state.body_pos;
					post_state.msg_pos = 0;
					post_state.state = POST_SENDING_BODY;
					RESET_TIMEOUT();
					break;

				case 0:
					break;

				default:
					post_state.condition = POST_SEND_ERR;
					goto post_error;
				}
			break;

		// Send body text associated with command
		case POST_SENDING_BODY:
			switch(send_message(post_state.body_buf))
				{
				case 1:
					post_state.state = POST_READING_STATUS;
					RESET_TIMEOUT();
					break;

				case 0:
					break;

				default:
					post_state.condition = POST_SEND_ERR;
					goto post_error;
				}
			break;

		// Wait for status message in response
		case POST_READING_STATUS:
			if (get_response())				// Line received?
				{
				if (check_resp_status() != 0)
					{
					post_state.condition = POST_RESP_ERR;
					goto post_error;
					}
				if (post_state.resp_class != 1 && post_state.resp_class != 2)
					{
					bb_post_error_str = "Remote server returned error class";
					report(PROBLEM, "%s %d", bb_post_error_str, post_state.resp_class);
					post_state.condition = POST_SERVER_ERR;
					goto post_error;
					}

				report(DETAIL, "Remote server returned class %d", post_state.resp_class);
				post_state.state = POST_READING_HEADERS;
				RESET_TIMEOUT();
				}
			break;

		// Read headers of response
		case POST_READING_HEADERS:
			if (get_response())				// Line received?
				{
				if (!post_state.cmd_buf[0])			// Blank line?
					{
					report(DETAIL, "End of headers found");

					if (post_state.resp_class == 1)		// Was 1XX Continue
						{
						post_state.state = POST_READING_STATUS;
						RESET_TIMEOUT();
						}
					else								// Was 2XX OK
						{
						post_state.state = POST_CHECKING_BODY;
						RESET_TIMEOUT();
						}
					}
				}
			break;

		// Check body of response for success response
		case POST_CHECKING_BODY:
			if (get_response())				// Response line received?
				{
				(void) check_resp_time_t();

				if (check_resp_result() > 0)
					{
					post_state.state = POST_READING_BODY;
					RESET_TIMEOUT();
					}
				}
			break;

		// Read body of response until socket is closed (content is ignored)
		case POST_READING_BODY:
			if (!tcp_tick(&post_state.socket))		// Socket closed?
				{
				report(DETAIL, "Connection closed");

				post_state.sock_opened = 0;

				switch(post_state.resp_result)
					{
					case RESP_SUCCESS:
						break;						// Nothing to do

					case RESP_BAD_ID:
						bb_post_error_str = "Station ID rejected by server";
						report(PROBLEM, bb_post_error_str);
						post_state.condition = POST_BAD_ID;
						goto post_error;

					case RESP_BAD_DATA:
						report(PROBLEM, "Server reported invalid data from sensor suite");
						wx_set_leds(LED_DAVIS, LED_OFF);		// SPECIAL CASE!
						break;

					case RESP_REJECTED:
					default:
						bb_post_error_str = "Transaction rejected by server";
						report(PROBLEM, bb_post_error_str);
						post_state.condition = POST_REJECTED;
						goto post_error;
                    }

				wx_set_leds(LED_POST, LED_GREEN);

				bb_post_error_str = "Succeeded";
				bb_post_error_state_num = post_state.state;

				post_state.state = POST_IDLE;
				post_state.condition = POST_SUCCESS;
				return post_state.condition;    	// -- EXIT --
				}

			(void) get_response();			// Receive line
			break;

		// Undefined state value
		default:
			bb_post_error_str = "Bad state encountered";
			report(PROBLEM, bb_post_error_str);
			post_state.condition = POST_BAD_STATE;
			goto post_error;
		}

	// Pending states fall out of bottom of switch() block here

	post_state.condition = POST_PENDING;
	return post_state.condition;       				// -- EXIT --

	// POST error handler
	post_error:
		post_cleanup();
		wx_set_leds(LED_POST, LED_RED);

		bb_post_error_state_num = post_state.state;

		post_state.cached_ip = 0L;			// Invalidate cached IP address

		post_state.state = POST_IDLE;
		return post_state.condition;             	// -- EXIT --
	}
