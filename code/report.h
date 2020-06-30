// Header file for routines to manage report output

// Copyright (c) 2006, Ian Chapman (Chapmip Consultancy)

// All rights reserved, except for those rights implicitly granted to
// GitHub Inc by publishing on GitHub and those rights granted by
// commercial agreement with the author.


#ifndef REPORT_H
#define REPORT_H

// Function prototypes

int report_check_active(unsigned char type_flags);
void report_suppress_next_nl(void);
void report(unsigned char type_flags, const char *fmt, ...);

// Number of reporting modes that can be selected

#define REPORT_NUM_MODES        2

// Values to identify calling task (bits 0-2)
// Do not change without reviewing array entries in .c file!

#define REPORT_SOURCE_MSK       0x07

#define REPORT_MAIN             0
#define REPORT_TASKS            1
#define REPORT_EEPROM           2
#define REPORT_LAN              3
#define REPORT_DAVIS            4
#define REPORT_POST             5
#define REPORT_DOWNLOAD         6

// Special bit mask for "raw" output (bit 3)

#define REPORT_RAW              0x08

// Bit masks for different types of report (bits 4-7)

#define REPORT_TYPE_MSK         0xF0

#define REPORT_AFFIRM           0x80
#define REPORT_PROBLEM          0x40
#define REPORT_INFO             0x20
#define REPORT_DETAIL           0x10

#endif
