// Header file for real-time clock based utility routines

// Copyright (c) 2006, Ian Chapman (Chapmip Consultancy)

// All rights reserved, except for those rights implicitly granted to
// GitHub Inc by publishing on GitHub and those rights granted by
// commercial agreement with the author.


#ifndef RTC_UTILS_H
#define RTC_UTILS_H


// External variables

extern char rtc_validated;


// Function prototypes

char * rtc_str(void);
unsigned long rtc_diff(time_t comp_val);
void rtc_update(time_t new_val);


#endif
