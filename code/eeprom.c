// EEPROM storage management routines

// Copyright (c) 2006, Ian Chapman (Chapmip Consultancy)

// All rights reserved, except for those rights implicitly granted to
// GitHub Inc by publishing on GitHub and those rights granted by
// commercial agreement with the author.


#include <dcdefs.h>
#include <stcpip.h>
#include <string.h>
#include "i2c.h"
#include "crc.h"
#include "report.h"
#include "lan.h"
#include "timeout.h"
#include "wx_board.h"
#include "eeprom.h"


// Short-cut names for types of report output (see "report.h")

#define PROBLEM     (REPORT_EEPROM | REPORT_PROBLEM)
#define INFO        (REPORT_EEPROM | REPORT_INFO)
#define DETAIL      (REPORT_EEPROM | REPORT_DETAIL)

#define RAW_DETAIL  (DETAIL | REPORT_RAW)


// Externally-visible variables

EeLanInfo_t   ee_lan_info;
unsigned char ee_lan_valid;

EePostInfo_t  ee_post_info;
unsigned char ee_post_valid;

EePostStr_t   ee_post_host;
EePostStr_t   ee_post_path;
EePostStr_t   ee_post_proxy;

EeUnitInfo_t  ee_unit_info;


// Default LAN parameters

#define EE_DEF_LAN_USE_STATIC       0

#define EE_DEF_LAN_IP_ADDR          LAN_DEF_IP_ADDR
#define EE_DEF_LAN_NETMASK          LAN_DEF_NETMASK
#define EE_DEF_LAN_DNS_SERVER_IP    LAN_DEF_DNS_SERVER_IP
#define EE_DEF_LAN_ROUTER_IP        LAN_DEF_ROUTER_IP


// Default POST parameters

#define EE_DEF_POST_USE_PROXY       0

#define EE_DEF_POST_HOST            "ADD-POST-HOST-HERE"            // %% Add here %%
#define EE_DEF_POST_HOST_PORT       80
#define EE_DEF_POST_PATH            "/default.asp"

#define EE_DEF_POST_PROXY           "ADD-PROXY_HOST_HERE"           // %% Add here %%        
#define EE_DEF_POST_PROXY_PORT      80


// Default Unit parameters

#define EE_DEF_UNIT_ID_BASE         0
#define EE_DEF_UNIT_REPORT_MODE     0
#define EE_DEF_UNIT_UPDATE_SECS     0
#define EE_DEF_UNIT_RESERVED        0


// EEPROM I2C device type and address

#define EE_DEVICE   (I2C_SUB_16 | 0xA8)


// Maximum number of bytes which can be written in one operation

#define EE_PAGE_SIZE        32


// Delay after each EEPROM page write

#define EE_WRITE_DELAY_MS   50


// Magic number for block marker value

#define EE_BLK_MARKER       0x55AA


// Minimum size of block in EEPROM
// Must contain valid and crc fields plus data

#define EE_CRC_SIZE         sizeof(unsigned int)
#define EE_BLK_MIN_SIZE     (sizeof(int) + EE_CRC_SIZE + 1)


// *** EXTERNAL FUNCTIONS ***

// Initialisation routine (only call once on start-up of application)
// Initialises I2C bus, then reads stored parameters and updates flags
// If DIP switch 4 is set, then invalid parameters are set to defaults
// Returns 0 on success (even if parameters are not valid)
// Returns < 0 on I2C bus error or internal error

int ee_init(void)
    {
    int err;

    if ((err = i2c_init()) != 0)
        {
        report(PROBLEM, "i2c_init() returned %d", err);
        return err;
        }

    if ((err = ee_read_lan_parms()) < 0)
        {
        report(PROBLEM, "ee_read_lan_parms() returned %d", err);
        return err;
        }

    if ((err = ee_read_post_parms()) < 0)
        {
        report(PROBLEM, "ee_read_post_parms() returned %d", err);
        return err;
        }

    if ((err = ee_read_unit_parms()) < 0)
        {
        report(PROBLEM, "ee_read_unit_parms() returned %d", err);
        return err;
        }

    report(DETAIL, "ee_lan_valid = %d, ee_post_valid = %d", ee_lan_valid, ee_post_valid);

    if (wx_switch_4)
        {
        if (!ee_lan_valid)
            {
            report(INFO, "Writing default values to LAN parameters");

            if ((err = ee_write_lan_defaults()) < 0)
                {
                report(PROBLEM, "ee_write_lan_defaults() returned %d", err);
                return err;
                }

            report(DETAIL, "ee_lan_valid = %d", ee_lan_valid);
            }

        if (!ee_post_valid)
            {
            report(INFO, "Writing default values to POST parameters");

            if ((err = ee_write_post_defaults()) < 0)
                {
                report(PROBLEM, "ee_write_post_defaults() returned %d", err);
                return err;
                }

            report(DETAIL, "ee_post_valid = %d", ee_post_valid);
            }
        }

    return EE_SUCCESS;
    }


// Dump contents of specified block to console in hexadecimal format

void ee_dump_blk(void * blk_base, size_t blk_size)
    {
    unsigned char col_ctr;
    char * ptr;

    ptr = blk_base;

    while (blk_size > 0)
        {
        if (blk_size > 16)
            col_ctr = 16;
        else
            col_ctr = blk_size;

        while (col_ctr > 0)
            {
            report(RAW_DETAIL, "%02X ", *ptr);
            ++ptr;
            --col_ctr;
            --blk_size;
            }

        report(RAW_DETAIL, "\r\n");
        }
    }


// Read data block from specified EEPROM location (multiple of page size)
// If read fails then destination block is zeroed (unless bad block size)
// Returns 0 on success (including marker and CRC match)
// Returns < 0 on I2C error or if bad block size specified
// Returns > 0 if block was read okay but marker or CRC did not match

int ee_read_blk(unsigned char ee_loc, void * blk_base, size_t blk_size)
    {
    int err;
    size_t info_size;
    unsigned int subaddr;
    unsigned int crc_calc;
    unsigned int * crc_ptr;

    if (blk_size < EE_BLK_MIN_SIZE)
        return EE_BAD_BLK_SIZE;             // Don't try to zero block

    info_size = blk_size - EE_CRC_SIZE;
    subaddr = (unsigned int) ee_loc * EE_PAGE_SIZE;

    if ((err = i2c_read_blk(EE_DEVICE, subaddr, blk_base, blk_size)) != 0)
        {
        report(PROBLEM, "I2C read returned error value %d", err);
        goto zero_block;
        }

    if ((* (unsigned int *) blk_base) != EE_BLK_MARKER)
        {
        report(INFO, "Block at %d does not contain a valid marker", ee_loc);
        err = EE_BAD_MARKER;
        goto zero_block;
        }

    crc_calc = crc_calculate(blk_base, info_size);
    crc_ptr = (unsigned int *) (((char *) blk_base) + info_size);

    if (*crc_ptr != crc_calc)
        {
        report(INFO, "Block CRC %04X did not match calculated CRC %04X",
               *crc_ptr, crc_calc);
        err = EE_BAD_CRC;
        goto zero_block;
        }

    report(DETAIL, "Read from block at %d succeeded", ee_loc);
    return EE_SUCCESS;

    // Exception handler
    zero_block:
        memset(blk_base, 0, info_size);
        return err;
    }


// Write data block to specified EEPROM location (multiple of page size)
// Returns 0 on success (including subsequent comparison)
// Returns < 0 on I2C error or if bad block size specified
// Returns > 0 if subsequent comparison failed

int ee_write_blk(unsigned char ee_loc, void * blk_base, size_t blk_size)
    {
    int err;
    size_t info_size;
    unsigned int subaddr;
    unsigned int crc_calc;
    unsigned int * crc_ptr;
    char * ptr;
    size_t count;
    unsigned int len;
    unsigned int tout;

    if (blk_size < EE_BLK_MIN_SIZE)
        return EE_BAD_BLK_SIZE;

    info_size = blk_size - EE_CRC_SIZE;

    * ((unsigned int *) blk_base) = EE_BLK_MARKER;      // Set block marker field

    crc_calc = crc_calculate(blk_base, info_size);
    crc_ptr = (unsigned int *) (((char *) blk_base) + info_size);

    * crc_ptr = crc_calc;

    report(DETAIL, "Wrote CRC %04X to block %d", crc_calc, ee_loc);

    subaddr = (unsigned int) ee_loc * EE_PAGE_SIZE;
    ptr = blk_base;
    count = blk_size;

    while (count > 0)
        {
        if (count > EE_PAGE_SIZE)
            len = EE_PAGE_SIZE;
        else
            len = count;

        if ((err = i2c_write_blk(EE_DEVICE, subaddr, ptr, len)) != 0)
            {
            report(PROBLEM, "I2C write returned error value %d", err);
            return err;
            }

        tout = SET_TIMEOUT_UI_MS(EE_WRITE_DELAY_MS);
        while (!CHK_TIMEOUT_UI_MS(tout))
            ;

        subaddr += len;
        ptr += len;
        count -= len;
        }

    if ((err = ee_compare_blk(ee_loc, blk_base, blk_size)) != 0)
        return err;

    report(DETAIL, "Write to block at %d succeeded", ee_loc);
    return EE_SUCCESS;
    }


// Compares data block to specified EEPROM location (multiple of page size)
// Returns 0 on successful comparison
// Returns < 0 on I2C error
// Returns > 0 if comparison failed

int ee_compare_blk(unsigned char ee_loc, void * blk_base, size_t blk_size)
    {
    int err;
    unsigned int subaddr;

    subaddr = (unsigned int) ee_loc * EE_PAGE_SIZE;

    if ((err = i2c_compare_blk(EE_DEVICE, subaddr, blk_base, blk_size)) != 0)
        {
        report(INFO, "I2C compare returned error value %d", err);
        return err;
        }

    report(DETAIL, "Compare of block at %d succeeded", ee_loc);
    return EE_SUCCESS;
    }


// Write LAN info block to EEPROM
// Returns 0 on success (including subsequent comparison)
// Returns < 0 on I2C error or if bad block size specified
// Returns > 0 if subsequent comparison failed

int ee_write_lan_info(void)
    {
    return ee_write_blk(EE_LOC_LAN_INFO, &ee_lan_info, sizeof(ee_lan_info));
    }


// Write POST info block to EEPROM
// Returns 0 on success (including subsequent comparison)
// Returns < 0 on I2C error or if bad block size specified
// Returns > 0 if subsequent comparison failed

int ee_write_post_info(void)
    {
    return ee_write_blk(EE_LOC_POST_INFO, &ee_post_info, sizeof(ee_post_info));
    }


// Write unit info block to EEPROM
// Returns 0 on success (including subsequent comparison)
// Returns < 0 on I2C error or if bad block size specified
// Returns > 0 if subsequent comparison failed

int ee_write_unit_info(void)
    {
    return ee_write_blk(EE_LOC_UNIT_INFO, &ee_unit_info, sizeof(ee_unit_info));
    }


// Read POST string from specified EEPROM location (multiple of page size)
// Ensures that string is always zero-terminated
// If read fails then string is completely zeroed
// Returns 0 on success (including marker and CRC match)
// Returns < 0 on I2C error or internal error
// Returns > 0 if block was read okay but marker or CRC did not match

int ee_read_post_str(unsigned char ee_loc, EePostStr_t * ptr)
    {
    int err;

    err = ee_read_blk(ee_loc, ptr, sizeof(EePostStr_t));

    ptr->str[EE_POST_STR_MAX_LEN] = 0;

    return err;
    }


// Write POST string to specified EEPROM location (multiple of page size)
// Ensures that string is padded with zeroes and always zero-terminated
// If source string is too long then extra characters are discarded
// Returns 0 on success (including subsequent comparison)
// Returns < 0 on I2C error or internal error
// Returns > 0 if subsequent comparison failed

int ee_write_post_str(unsigned char ee_loc, EePostStr_t * ptr, const char * src)
    {
    memset(ptr->str, 0, EE_POST_STR_MAX_LEN);
    strncpy(ptr->str, src, EE_POST_STR_MAX_LEN);

    ptr->str[EE_POST_STR_MAX_LEN] = 0;

    return ee_write_blk(ee_loc, ptr, sizeof(EePostStr_t));
    }


// Read LAN parameters from EEPROM and update validity flag
// Returns 0 on successful read (although parameters may not be valid)
// Returns < 0 on I2C error or internal error

int ee_read_lan_parms(void)
    {
    int err;

    ee_lan_valid = 0;

    if ((err = ee_read_blk(EE_LOC_LAN_INFO, &ee_lan_info, sizeof(ee_lan_info))) < 0)
        return err;

    if (ee_lan_info.marker)
        ee_lan_valid = 1;

    return EE_SUCCESS;
    }


// Read POST parameters from EEPROM and update validity flag
// Returns 0 on successful read (although parameters may not be valid)
// Returns < 0 on I2C error or internal error

int ee_read_post_parms(void)
    {
    int err;

    ee_post_valid = 0;

    if ((err = ee_read_blk(EE_LOC_POST_INFO, &ee_post_info, sizeof(ee_post_info))) < 0)
        return err;

    if ((err = ee_read_post_str(EE_LOC_POST_HOST, &ee_post_host)) < 0)
        return err;

    if ((err = ee_read_post_str(EE_LOC_POST_PATH, &ee_post_path)) < 0)
        return err;

    if ((err = ee_read_post_str(EE_LOC_POST_PROXY, &ee_post_proxy)) < 0)
        return err;

    if (ee_post_info.marker && ee_post_host.marker && ee_post_path.marker)
        {
        if (!ee_post_info.use_proxy || ee_post_proxy.marker)
            ee_post_valid = 1;
        }

    return EE_SUCCESS;
    }


// Read unit parameters from EEPROM
// Returns 0 on successful read (although parameters may not be valid)
// Returns < 0 on I2C error or internal error

int ee_read_unit_parms(void)
    {
    int err;

    if ((err = ee_read_blk(EE_LOC_UNIT_INFO, &ee_unit_info, sizeof(ee_unit_info))) < 0)
        return err;

    return EE_SUCCESS;
    }


// Write default values to LAN parameters in EEPROM and update validity flag
// Returns 0 if no I2C or internal errors (although parameters may not be valid)
// Returns < 0 on I2C error or internal error

int ee_write_lan_defaults(void)
    {
    int err;

    ee_lan_info.use_static      = EE_DEF_LAN_USE_STATIC;
    ee_lan_info.ip_addr         = inet_addr(EE_DEF_LAN_IP_ADDR);
    ee_lan_info.netmask         = inet_addr(EE_DEF_LAN_NETMASK);
    ee_lan_info.dns_server_ip   = inet_addr(EE_DEF_LAN_DNS_SERVER_IP);
    ee_lan_info.router_ip       = inet_addr(EE_DEF_LAN_ROUTER_IP);

    if ((err = ee_write_lan_info()) < 0)
        return err;

    if ((err = ee_read_lan_parms()) < 0)
        return err;

    return EE_SUCCESS;
    }


// Write default values to POST parameters in EEPROM and update validity flag
// Returns 0 if no I2C or internal errors (although parameters may not be valid)
// Returns < 0 on I2C error or internal error

int ee_write_post_defaults(void)
    {
    int err;

    ee_post_info.use_proxy      = EE_DEF_POST_USE_PROXY;
    ee_post_info.host_port      = EE_DEF_POST_HOST_PORT;
    ee_post_info.proxy_port     = EE_DEF_POST_PROXY_PORT;

    if ((err = ee_write_post_info()) < 0)
        return err;

    if ((err = ee_write_post_str(EE_LOC_POST_HOST, &ee_post_host, EE_DEF_POST_HOST)) < 0)
        return err;

    if ((err = ee_write_post_str(EE_LOC_POST_PATH, &ee_post_path, EE_DEF_POST_PATH)) < 0)
        return err;

    if ((err = ee_write_post_str(EE_LOC_POST_PROXY, &ee_post_proxy, EE_DEF_POST_PROXY)) < 0)
        return err;

    if ((err = ee_read_post_parms()) < 0)
        return err;

    return EE_SUCCESS;
    }


// Write default values to unit parameters in EEPROM
// Returns 0 if no I2C or internal errors (although parameters may not be valid)
// Returns < 0 on I2C error or internal error

int ee_write_unit_defaults(void)
    {
    int err;

    ee_unit_info.id_base = EE_DEF_UNIT_ID_BASE;
    ee_unit_info.report_mode = EE_DEF_UNIT_REPORT_MODE;
    ee_unit_info.update_secs = EE_DEF_UNIT_UPDATE_SECS;
    ee_unit_info.reserved = EE_DEF_UNIT_RESERVED;

    if ((err = ee_write_unit_info()) < 0)
        return err;

    if ((err = ee_read_unit_parms()) < 0)
        return err;

    return EE_SUCCESS;
    }
