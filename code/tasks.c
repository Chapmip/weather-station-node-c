// Routines to carry out weather station tasks

// Copyright (c) 2006, Ian Chapman (Chapmip Consultancy)

// All rights reserved, except for those rights implicitly granted to
// GitHub Inc by publishing on GitHub and those rights granted by
// commercial agreement with the author.


#include <stdio.h>
#include <dcdefs.h>
#include <string.h>
#include "timeout.h"
#include "wx_board.h"
#include "lan.h"
#include "post_client.h"
#include "davis.h"
#include "report.h"
#include "eeprom.h"
#include "bb_vars.h"
#include "wx_main.h"
#include "menu.h"
#include "rtc_utils.h"
#include "tasks.h"


// Short-cut names for types of report output (see "report.h")

#define PROBLEM     (REPORT_TASKS | REPORT_PROBLEM)
#define INFO        (REPORT_TASKS | REPORT_INFO)
#define DETAIL      (REPORT_TASKS | REPORT_DETAIL)

#define RAW_INFO    (INFO | REPORT_RAW)
#define RAW_DETAIL  (DETAIL | REPORT_RAW)


// Internal states for the tasks state machine

enum state_value
    {
    TASKS_IDLE = 0,
    TASKS_COLLECTING,
    TASKS_PROCESSING,
    TASKS_DELIVERING,
    TASKS_TIME_CHECKING,
    TASKS_TIME_SETTING,
    };


// Internal structure containing state variables

static struct
    {
    enum state_value state;             // Current state (see definition above)
    unsigned int collect_tmr;           // Time between data collection attempts
    unsigned long time_chk_tmr;         // Time between weather station time checks

    unsigned char new_data;             // Flag indicates new data was collected
    unsigned char collect_err_ctr;      // Counts consecutive collection failures

    unsigned char post_err_ctr;         // Counts consecutive POST failures

    } tasks_state;


// Timer values

#define INIT_COLLECT_SECS       30
#define FAST_COLLECT_SECS       60
#define SLOW_COLLECT_SECS       300

#define INIT_TIME_CHK_SECS      120
#define BACKOFF_TIME_CHK_SECS   300
#define NEXT_TIME_CHK_SECS      86400L      // 86,400 = 24 * 60 * 60


// Maximum consecutive errors before forced reset

#define MAX_COLLECT_ERRS        10
#define MAX_POST_ERRS           10


// *** INTERNAL FUNCTIONS ***

// Checks that string is less than specified length
// Will not count indefinitely if pointer is invalid
// Returns 0 if string is shorter than 'too_long' characters
// Returns -1 if string is 'too_long' characters or longer

static int check_str_len(const char * str, unsigned int too_long)
    {
    while (too_long--)
        {
        if (*str++ == '\0')
            return 0;
        }
    return -1;
    }


// Add station ID to POST body text in decimal format
// Returns 0 if okay, < 0 if ran out of space

static int add_station_id(void)
    {
    char buffer[6];             // Up to 5 chars plus zero for unsigned values

    sprintf(buffer, "%u", get_station_id());

    return post_add_variable("station", buffer, 0);
    }


// Add collected data (or error string if not collected) to POST body text
// Returns 0 if okay, < 0 if ran out of space

static int add_collected_data(void)
    {
    if (tasks_state.new_data)
        return post_add_variable("data", dav_data, DAV_DATA_LEN);
    else
        return post_add_variable("sererr", dav_error_str, 0);
    }


// Add details of previous POST error to POST body text
// State number is appended to error string as up to 15 more characters
// If error string is too long then fixed problem report is sent instead
// Returns 0 if okay, < 0 if ran out of space

static int add_post_error(void)
    {
    char buffer[64];

    if (check_str_len(bb_post_error_str, sizeof(buffer) - 15) != 0)
        return post_add_variable("posterr", BB_BAD_POST_ERR_STR, 0);

    sprintf(buffer, "%s (state %d)", bb_post_error_str, bb_post_error_state_num);

    report(DETAIL, "Previous POST error: %s", buffer);

    return post_add_variable("posterr", buffer, 0);
    }


// Add sequence number to POST body text in decimal format
// Value is constrained to 0 to 2^31 - 1 (2,147,483,647)
// Returns 0 if okay, < 0 if ran out of space

#define SEQ_NUM_MSK     0x7FFFFFFFUL

static int add_seq_num(void)
    {
    char buffer[11];                // Up to 10 chars plus zero for unsigned long values

    sprintf(buffer, "%lu", (bb_seq_num & SEQ_NUM_MSK));

    report(DETAIL, "Sequence number: %s", buffer);

    return post_add_variable("seq", buffer, 0);
    }


// Add local IP address to POST body text as 8 hex digits (4 bytes)
// Returns 0 if okay, < 0 if ran out of space

static int add_my_ip(void)
    {
    longword my_ip;

    my_ip = lan_get_network_ip();           // Big-endian format

    return post_add_variable("localip", (char *) &my_ip, sizeof(my_ip));
    }


// Add firmware version number to POST body text as merged string (no dot)
// Returns 0 if okay, < 0 if ran out of space

#define STR(X)          #X
#define XSTR(X)         STR(X)
#define STR_VERSION     XSTR(VER_MAJOR) XSTR(VER_MINOR)

static int add_firmware_version(void)
    {
    return post_add_variable("ver", STR_VERSION, 0);
    }


// Sets up the body text to post to the server
// Returns 0 if okay, < 0 if ran out of space

static int set_post_body(void)
    {
    int status;

    ++bb_seq_num;                       // Bump up sequence number for attempt

    post_clear_body();

    status = add_station_id();

    if (status < 0)
        {
        report(PROBLEM, "add_station_id() failed with %d", status);
        return -1;
        }

    status = add_collected_data();

    if (status < 0)
        {
        report(PROBLEM, "add_collected_data() failed with %d", status);
        return -2;
        }

    if (bb_post_error_flag)
        {
        status = add_post_error();

        if (status < 0)
            {
            report(PROBLEM, "add_post_error() failed with %d", status);
            return -3;
            }
        }

    status = add_seq_num();

    if (status < 0)
        {
        report(PROBLEM, "add_seq_num() failed with %d", status);
        return -4;
        }

    status = add_my_ip();

    if (status < 0)
        {
        report(PROBLEM, "add_my_ip() failed with %d", status);
        return -5;
        }

    status = add_firmware_version();

    if (status < 0)
        {
        report(PROBLEM, "add_firmware_version() failed with %d", status);
        return -6;
        }

    return 0;
    }


// Sets timer for next collection time
//
// Interval between updates is either selected by update_secs value in EEPROM
// (if > 0 and <= TASKS_MAX_UPDATE_SECS) or by DIP switch 3 (off = "slow" or
// on = "fast") if EEPROM value is 0 or too large

static void set_next_collection_time(void)
    {
    unsigned int interval_secs;

    if (ee_unit_info.update_secs - 1 < TASKS_MAX_UPDATE_SECS)
        interval_secs = ee_unit_info.update_secs;           // 1 to maximum
    else
        interval_secs = (wx_switch_3 ? SLOW_COLLECT_SECS : FAST_COLLECT_SECS);

    tasks_state.collect_tmr = SET_TIMEOUT_UI_SECS(interval_secs);

    report(INFO, "Next automatic collection in %u seconds (current time = %lu)", \
                  interval_secs, getSeconds());
    }


// *** EXTERNAL FUNCTIONS ***

// Initialise tasks state machine
// (must only be called once at start-up of application)
// Returns 0 on success, < 0 if unable to initialise

int tasks_init(void)
    {
    int status;

    memset(&tasks_state, 0, sizeof(tasks_state));       // Zero all state variables

    tasks_state.collect_tmr = SET_TIMEOUT_UI_SECS(INIT_COLLECT_SECS);
    tasks_state.time_chk_tmr = SET_TIMEOUT_UL_SECS(INIT_TIME_CHK_SECS);
    tasks_state.state = TASKS_IDLE;

    status = post_init(2048);

    if (status < 0)
        {
        report(PROBLEM, "post_init() failed with %d", status);
        wx_set_leds(LED_POST, LED_RED);
        return TASKS_POST_INIT_ERR;
        }

    status = dav_init_all();

    if (status < 0)
        {
        report(PROBLEM, "dav_init_all() failed with %d", status);
        wx_set_leds(LED_DAVIS, LED_RED);
        return TASKS_DAV_INIT_ERR;
        }

    if (ee_post_valid == 0)
        {
        report(PROBLEM, "EEPROM parameters for POST are invalid");
        wx_set_leds(LED_POST, LED_RED);
        return TASKS_EE_INIT_ERR;
        }

    if (ee_post_info.use_proxy == 0)
        {
        status = post_set_server(ee_post_host.str, ee_post_info.host_port,
                 ee_post_path.str, NULL, 0);            // No proxy
        }
    else
        {
        status = post_set_server(ee_post_host.str, ee_post_info.host_port,
                 ee_post_path.str, ee_post_proxy.str, ee_post_info.proxy_port);
        }

    if (status < 0)
        {
        report(PROBLEM, "post_set_server() failed with %d", status);
        wx_set_leds(LED_POST, LED_RED);
        return TASKS_SERVER_INIT_ERR;
        }

    return TASKS_INIT_OK;
    }


// Main "tick" routine which drives data collection state machine
// Return value indicates current status (see header file)
// 0 means okay, < 0 means problem

int tasks_run(void)
    {
    int status;

    net_tick();

    // Process current state
    switch(tasks_state.state)
        {
        // Waiting to initiate next task
        case TASKS_IDLE:
            (void) dav_tick();          // Eat any serial chars

            if (CHK_TIMEOUT_UI_SECS(tasks_state.collect_tmr))
                {
                report(DETAIL, "Starting automatic data collection");
                set_next_collection_time();
                dav_start_collect();
                tasks_state.state = TASKS_COLLECTING;
                }
            else if (CHK_TIMEOUT_UL_SECS(tasks_state.time_chk_tmr))
                {
                tasks_state.time_chk_tmr = SET_TIMEOUT_UL_SECS(BACKOFF_TIME_CHK_SECS);
                if (rtc_validated)
                    {
                    dav_start_check_time();
                    report(DETAIL, "Checking weather station clock");
                    tasks_state.state = TASKS_TIME_CHECKING;
                    }
                else
                    report(DETAIL, "Cannot check weather station clock"
                                   " -- Interface clock not yet validated");
                }
            else                        // Not time for collection yet
                {
                wx_get_switches();      // Refresh input switch states
                switch(inchar())        // Check for user input
                    {
                    case EOF:
                        break;          // Nothing received

                    case MENU_ESC:
                        return TASKS_MENU;          // -- EXIT --

                    default:
                        report(DETAIL, "Manually starting data collection");
                        set_next_collection_time();
                        dav_start_collect();
                        tasks_state.state = TASKS_COLLECTING;
                        break;
                    }
                switch(lan_check_ok())  // Check LAN connection
                    {
                    case LAN_OK:
                        break;          // Connection okay

                    case LAN_ETH_DOWN:
                        return TASKS_ETH_DOWN;      // -- EXIT --

                    default:
                        return TASKS_LAN_DOWN;      // -- EXIT --
                    }
                }
            break;

        // Collecting data from weather station
        case TASKS_COLLECTING:
            if (dav_tick() != DAV_PENDING)
                {
                if (dav_get_status() == DAV_SUCCESS)
                    {
                    report(DETAIL, "Data collected okay\x07");

                    tasks_state.new_data = 1;       // Mark data as collected
                    tasks_state.collect_err_ctr = 0;

                    dav_dump_data();

                    tasks_state.state = TASKS_PROCESSING;
                    }
                else
                    {
                    report(PROBLEM, "Error collecting data\x07");

                    if (!dav_data_valid)
                        tasks_state.new_data = 0;   // Invalidate data if overwritten

                    if (++tasks_state.collect_err_ctr >= MAX_COLLECT_ERRS)
                        {
                        report(PROBLEM, "Too many consecutive collection errors");
                        return TASKS_COLLECT_FAIL;      // -- EXIT --
                        }

                    tasks_state.state = TASKS_PROCESSING;
                    }
                }
            break;

        // Processing data into POST request
        case TASKS_PROCESSING:
            (void) dav_tick();          // Eat any serial chars

            (void) set_post_body();

            report(DETAIL, "Delivering data to remote server");

            status = post_start();

            if (status < 0)
                {
                report(PROBLEM, "post_start() failed with %d", status);
                return TASKS_POST_START_ERR;        // -- EXIT --
                }

            tasks_state.state = TASKS_DELIVERING;
            break;

        // Delivering data in POST request
        case TASKS_DELIVERING:
            (void) dav_tick();          // Eat any serial chars

            if (post_tick() != POST_PENDING)
                {
                if (post_get_status() == POST_SUCCESS)
                    {
                    report(DETAIL, "Data delivered okay to remote server\x07");

                    tasks_state.new_data = 0;       // Mark data as delivered

                    bb_post_error_flag = 0;

                    tasks_state.post_err_ctr = 0;
                    }
                else
                    {
                    report(PROBLEM, "Problem delivering data to remote server\x07");

                    bb_post_error_flag = 1;

                    if (++tasks_state.post_err_ctr >= MAX_POST_ERRS)
                        {
                        report(PROBLEM, "Too many consecutive POST errors");
                        return TASKS_POST_FAIL;     // -- EXIT --
                        }

                    // Don't invalidate data -- allow re-send
                    }

                report(RAW_INFO, "\r\n");

                lan_show_info(RAW_DETAIL);

                report(RAW_INFO, "Press [ESC] to re-configure unit "
                                 "or other key for immediate collection\r\n");

                tasks_state.state = TASKS_IDLE;
                }
            break;

        // Checking weather station clock against Interface clock
        case TASKS_TIME_CHECKING:
            if (dav_tick() != DAV_PENDING)
                {
                switch (dav_get_status())
                    {
                    case DAV_SUCCESS:
                        report(DETAIL, "Weather station clock is set okay\x07");
                        tasks_state.time_chk_tmr = SET_TIMEOUT_UL_SECS(NEXT_TIME_CHK_SECS);
                        tasks_state.state = TASKS_IDLE;
                        break;

                    case DAV_WRONG_TIME:
                        if (rtc_validated)
                            {
                            dav_start_set_time();
                            report(DETAIL, "Resetting weather station clock");
                            tasks_state.state = TASKS_TIME_SETTING;
                            }
                        else
                            {
                            report(DETAIL, "Cannot reset weather station clock"
                                           " -- Interface clock not yet validated");
                            tasks_state.state = TASKS_IDLE;
                            }
                        break;

                    default:
                        report(PROBLEM, "Error checking weather station clock\x07");
                        tasks_state.state = TASKS_IDLE;
                        break;
                    }
                }
            break;

        // Setting weather station clock to match Interface clock
        case TASKS_TIME_SETTING:
            if (dav_tick() != DAV_PENDING)
                {
                if (dav_get_status() == DAV_SUCCESS)
                    {
                    report(DETAIL, "Weather station clock has been reset\x07");
                    tasks_state.state = TASKS_IDLE;
                    }
                else
                    {
                    report(PROBLEM, "Error setting weather station clock\x07");
                    tasks_state.state = TASKS_IDLE;
                    }
                }
            break;

        // Undefined state value
        default:
            report(PROBLEM, "Bad state encountered");
            return TASKS_BAD_STATE;
        }

    return TASKS_OK;                                // -- EXIT --
    }
