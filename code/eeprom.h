// Header file for EEPROM storage management routines

// Copyright (c) 2006, Ian Chapman (Chapmip Consultancy)

// All rights reserved, except for those rights implicitly granted to
// GitHub Inc by publishing on GitHub and those rights granted by
// commercial agreement with the author.


#ifndef	EEPROM_H
#define	EEPROM_H

#include "i2c.h"							// Include I2C status values


// Constants

#define EE_POST_STR_MAX_LEN		64			// Maximum string length


// Structure definitions

typedef struct
	{
	unsigned int marker;					// Must be first element
    int use_static;
	longword ip_addr;
	longword netmask;
	longword dns_server_ip;
	longword router_ip;
	unsigned int crc;						// Must be last element
	} EeLanInfo_t;

typedef struct
	{
	unsigned int marker;					// Must be first element
    int use_proxy;
	word host_port;
	word proxy_port;
	unsigned int crc;						// Must be last element
	} EePostInfo_t;

typedef struct
	{
	unsigned int marker;					// Must be first element
	char str[EE_POST_STR_MAX_LEN + 1];
	unsigned int crc;						// Must be last element
	} EePostStr_t;

typedef struct
	{
	unsigned int marker;					// Must be first element
    word id_base;
	word report_mode;
	word update_secs;
	word reserved;
	unsigned int crc;						// Must be last element
	} EeUnitInfo_t;


// External variables

extern EeLanInfo_t	 ee_lan_info;
extern unsigned char ee_lan_valid;

extern EePostInfo_t	 ee_post_info;
extern unsigned char ee_post_valid;

extern EePostStr_t	 ee_post_host;
extern EePostStr_t	 ee_post_path;
extern EePostStr_t	 ee_post_proxy;

extern EeUnitInfo_t	 ee_unit_info;


// Status of EEPROM block operations (other than I2C values)

#define EE_BAD_CRC				(I2C_MAX_ERR + 2)
#define EE_BAD_MARKER			(I2C_MAX_ERR + 1)
#define EE_SUCCESS				0
#define EE_BAD_BLK_SIZE			(I2C_MIN_ERR - 1)


// 8-bit EEPROM location identifiers
// Multiplied by EE_PAGE_SIZE to give physical subaddress

#define EE_LOC_LAN_INFO			0
#define EE_LOC_POST_INFO		1
#define EE_LOC_POST_HOST		2			// Allow 4 sectors
#define EE_LOC_POST_PATH		6			// Allow 4 sectors
#define EE_LOC_POST_PROXY		10			// Allow 4 sectors
#define EE_LOC_UNIT_INFO		14


// Function prototypes

int ee_init(void);

void ee_dump_blk(void * blk_base, size_t blk_size);

int ee_read_blk(unsigned char ee_loc, void * blk_base, size_t blk_size);
int ee_write_blk(unsigned char ee_loc, void * blk_base, size_t blk_size);
int ee_compare_blk(unsigned char ee_loc, void * blk_base, size_t blk_size);

int ee_write_lan_info(void);
int ee_write_post_info(void);
int ee_write_unit_info(void);

int ee_read_post_str(unsigned char ee_loc, EePostStr_t * ptr);
int ee_write_post_str(unsigned char ee_loc, EePostStr_t * ptr, const char * src);

int ee_read_lan_parms(void);
int ee_read_post_parms(void);
int ee_read_unit_parms(void);

int ee_write_lan_defaults(void);
int ee_write_post_defaults(void);
int ee_write_unit_defaults(void);


#endif
