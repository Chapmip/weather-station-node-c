// Real-time clock based utility routines

// Copyright (c) 2006, Ian Chapman (Chapmip Consultancy)

// All rights reserved, except for those rights implicitly granted to
// GitHub Inc by publishing on GitHub and those rights granted by
// commercial agreement with the author.


#include <time.h>
#include <string.h>
#include <rabbit.h>
#include "rtc_utils.h"


// External variables

char rtc_validated;                                 // Set and tested outside module


// External functions

// Gets current RTC time as an ASCII string without \n terminator
// N.B. String is held in a static buffer which is overwritten by subsequent calls
// Code is NOT portable because it relies on overwriting of a const char buffer

char * rtc_str(void)
    {
    time_t rtc_val;
    char * str;
    char * lf_ptr;

    rtc_val = time(NULL);

    str = (char *) asctime(gmtime(&rtc_val));       // NOT PORTABLE!

    lf_ptr = strchr(str, '\n');                     // Remove LF char if present
    if (lf_ptr != NULL)
        *lf_ptr = '\0';                             // NOT PORTABLE!

    return str;
    }


// Calculates absolute difference in seconds between supplied time and RTC time

unsigned long rtc_diff(time_t comp_val)
    {
    time_t rtc_val;

    rtc_val = time(NULL);

    if (rtc_val >= comp_val)
        return rtc_val - comp_val;
    else
        return comp_val - rtc_val;
    }


// Updates RTC with supplied time
// Wrapper around function in "Rabbit.h"

void rtc_update(time_t new_val)
    {
    writeRTC(new_val);
    }
