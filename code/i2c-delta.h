// Additions to header file for standard I2C bus function library

// Copyright (c) 2006, Ian Chapman (Chapmip Consultancy)

// All rights reserved, except for those rights implicitly granted to
// GitHub Inc by publishing on GitHub and those rights granted by
// commercial agreement with the author.


// -- NEW FUNCTIONALITY ADDED TO I2C LIBRARY --

// The following additions have been made to the "i2c.h" file derived from the
// corresponding Dynamic C library (I2C.LIB) provided by Rabbit Semiconductor Inc.
// as ported to the Softools C compiler using the Softools conversion tool.


#ifndef I2C_H
#define I2C_H


// Rationalised return values for all functions
// Changes from DC library affect i2c_check_ack(), i2c_write_char() and i2c_wr_wait()
// 0 means success, < 0 means error, > 0 means other result

#define I2C_SUCCESS                0        // Must be zero
#define I2C_CLK_TIMEOUT          (-1)
#define I2C_NAK                  (-2)       // Changed from 1 in DC library
#define I2C_TOO_MANY_RETRIES     (-3)       // Changed from -1 in DC library
#define I2C_COMPARE_MISMATCH       1


// Minimum and maximum error values as base numbers for calling module

#define I2C_MIN_ERR     I2C_TOO_MANY_RETRIES
#define I2C_MAX_ERR     I2C_COMPARE_MISMATCH


// Functions converted from Dynamic C library
// These only handle part of an I2C bus transaction with a slave device

int i2c_init(void);                         // Must be called on startup
int i2c_unlock_bus(void);
int i2c_start_tx(void);
int i2c_startw_tx(void);
int i2c_send_ack(void);
int i2c_send_nak(void);
int i2c_read_char(char *ch);
int i2c_check_ack(void);
int i2c_write_char(char d);
void i2c_stop_tx(void);
int i2c_wr_wait(char d);


// -- NEW FUNCTIONALITY ADDED TO I2C LIBRARY --

// I2C action bits -- internal constants (do not use in external routines)

#define I2C_RD_MSK          0x8000          // Read operation
#define I2C_WR_MSK          0x4000          // Write operation
#define I2C_CP_MSK          0x2000          // Compare only during read

// I2C device bits -- internal constants (do not use in external routines)

#define I2C_SUB_MSK         0x0800          // Enables subaddressing
#define I2C_LNG_MSK         0x0400          // Specifies 16-bit subaddress

// I2C device types -- use in calls to extended functions
// OR with 8-bit I2C bus address to get 'device' (dev) value

#define I2C_NO_SUB          0
#define I2C_SUB_8           I2C_SUB_MSK
#define I2C_SUB_16          (I2C_SUB_MSK | I2C_LNG_MSK)

// I2C actions -- use in calls to i2c_action()
// OR with 'device' value to get 'device_action' value for i2c_action()

#define I2C_POLL            0
#define I2C_READ            I2C_RD_MSK
#define I2C_WRITE           I2C_WR_MSK
#define I2C_COMPARE         (I2C_RD_MSK | I2C_CP_MSK)


// New functions which carry out an entire I2C bus transaction with a slave device

int i2c_action(unsigned int device_action, unsigned int subaddr,  \
               char * blk_ptr, unsigned int count);

int i2c_read_byte(unsigned int device, unsigned int subaddr);
int i2c_write_byte(unsigned int device, unsigned int subaddr, char value);

// Short-cut macros for individual I2C block actions

#define i2c_poll(dev)                       i2c_action(((dev) | I2C_POLL), 0, (char *) 0, 0)
#define i2c_read_blk(dev, sa, ptr, cnt)     i2c_action(((dev) | I2C_READ), sa, ptr, cnt)
#define i2c_write_blk(dev, sa, ptr, cnt)    i2c_action(((dev) | I2C_WRITE), sa, ptr, cnt)
#define i2c_compare_blk(dev, sa, ptr, cnt)  i2c_action(((dev) | I2C_COMPARE), sa, ptr, cnt)


// Number of bytes written, read or compared during I2C block operation (from i2c_action)

extern unsigned int i2c_byte_count;


#endif
