// Routines to manage report output

// Copyright (c) 2006, Ian Chapman (Chapmip Consultancy)

// All rights reserved, except for those rights implicitly granted to
// GitHub Inc by publishing on GitHub and those rights granted by
// commercial agreement with the author.


#include <dcdefs.h>
#include <stdio.h>
#include <stdarg.h>
#include "wx_board.h"
#include "eeprom.h"
#include "report.h"


// Internal definitions

#define REPORT_ALL      REPORT_TYPE_MSK
#define REPORT_NONE     0


// Report source names

static const char * report_source[8] =
    {
    "MAIN",
    "TASKS",
    "EEPROM",
    "NET",
    "SER",
    "UP",
    "DL",
    "???",
    };


// Report enable masks

static const unsigned char report_enable[REPORT_NUM_MODES][8] =
    {
        {               // Terse format (switch OFF / mode 1)
        REPORT_ALL,
        REPORT_ALL,
        (REPORT_PROBLEM | REPORT_INFO),
        (REPORT_PROBLEM | REPORT_INFO),
        (REPORT_PROBLEM | REPORT_INFO),
        (REPORT_PROBLEM | REPORT_INFO),
        (REPORT_PROBLEM | REPORT_INFO),
        (REPORT_PROBLEM | REPORT_INFO),
        },

        {               // Verbose format (switch ON / mode 2)
        REPORT_ALL,
        REPORT_ALL,
        REPORT_ALL,
        REPORT_ALL,
        REPORT_ALL,
        REPORT_ALL,
        REPORT_ALL,
        REPORT_ALL,
        },
    };


// Internal variables

static unsigned char no_nl_next = 0;        // New-line suppression flag


// *** EXTERNAL FUNCTIONS ***

// Determine whether the specified type of report is enabled
// Report mode is either selected by report_mode value in EEPROM (if 1 or 2)
// or by DIP switch 2 if EEPROM value is 0 or > 2

int report_check_active(unsigned char type_flags)
    {
    unsigned int mode;
    unsigned char val;

    mode = ee_unit_info.report_mode - 1;
    if (mode >= REPORT_NUM_MODES)
        mode = (unsigned int) wx_switch_2 & 0x01;

    val = report_enable[mode][type_flags & REPORT_SOURCE_MSK];

    return ((val & type_flags & REPORT_TYPE_MSK) != 0);
    }


// Suppresses the inclusion of a new-line sequence at the end of the next
// report() call only (has no effect if report output is unformatted)

void report_suppress_next_nl(void)
    {
    no_nl_next = 1;
    }


// If the specified type of report is enabled, then send it to the console

void report(unsigned char type_flags, const char *fmt, ...)
    {
    unsigned char formatted;
    va_list argp;

    if (report_check_active(type_flags))
        {
        formatted = !(type_flags & REPORT_RAW);

        if (formatted)
            {
            printf("%s: ", report_source[type_flags & REPORT_SOURCE_MSK]);

            if ((type_flags & REPORT_PROBLEM) != 0)
                printf("ERROR - ");
            }

        va_start(argp, fmt);
        vprintf(fmt, argp);
        va_end(argp);

        if (formatted && !no_nl_next)
            printf("\r\n");
       }

    no_nl_next = 0;
    }
