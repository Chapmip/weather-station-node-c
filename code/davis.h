// Header file for routines to collect data from Davis Instruments weather station

// Copyright (c) 2006, Ian Chapman (Chapmip Consultancy)

// All rights reserved, except for those rights implicitly granted to
// GitHub Inc by publishing on GitHub and those rights granted by
// commercial agreement with the author.


#ifndef	DAVIS_H
#define	DAVIS_H

// Constants

#define DAV_DATA_LEN			99

// External variables

extern unsigned char dav_data[DAV_DATA_LEN];
extern unsigned char dav_data_valid;
extern const char * dav_error_str;

// Status of the data collection process (values returned by dav_get_status and dav_tick)

#define DAV_WRONG_TIME			2
#define DAV_SUCCESS				1
#define	DAV_PENDING				0
#define DAV_NOT_STARTED			(-1)
#define DAV_SERIAL_ERR			(-2)
#define	DAV_TIMEOUT				(-3)
#define	DAV_ABORTED				(-4)
#define	DAV_NO_WAKEUP			(-5)
#define DAV_BAD_WAKEUP			(-6)
#define	DAV_NO_ACK				(-7)
#define DAV_NEG_ACK				(-8)
#define DAV_BAD_ACK				(-9)
#define DAV_NO_DATA				(-10)
#define DAV_BAD_DATA			(-11)
#define	DAV_BAD_CRC				(-12)
#define DAV_NO_TIME				(-13)
#define DAV_BAD_TIME			(-14)
#define DAV_BAD_STATE			(-15)

// Function prototypes

char dav_init_serial(void);
int dav_init_all(void);

void dav_start_collect(void);
void dav_start_set_bar(int barometer, int elevation);
void dav_start_echo_resp(char * cmd);
void dav_start_check_time(void);
void dav_start_set_time(void);

void dav_abort(void);
int dav_get_status(void);
void dav_dump_data(void);
int dav_tick(void);

#endif
