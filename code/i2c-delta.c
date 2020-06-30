// Additions to standard I2C bus function library

// Copyright (c) 2006, Ian Chapman (Chapmip Consultancy)

// All rights reserved, except for those rights implicitly granted to
// GitHub Inc by publishing on GitHub and those rights granted by
// commercial agreement with the author.


// -- NEW FUNCTIONALITY ADDED TO I2C LIBRARY --

// The following additions have been made to the "i2c.c" file derived from the
// corresponding Dynamic C library (I2C.LIB) provided by Rabbit Semiconductor Inc.
// as ported to the Softools C compiler using the Softools conversion tool.


#include <rabbit.h>
#include "i2c.h"


// New functions carry out an entire I2C bus transaction with a slave device
// (poll for presence, read or write or compare a block of bytes or a single byte)


// Externally-visible variable indicates number of bytes written, read or compared
// during I2C block operation (via call to i2c_action)

unsigned int i2c_byte_count;


/* START FUNCTION DESCRIPTION ********************************************
i2c_action                                                      <I2C.LIB>

SYNTAX:         int i2c_action(unsigned int device_action, unsigned int subaddr,
                char * blk_ptr, unsigned int count);

DESCRIPTION:    Main I2C transaction routine which can carry out one of several
                actions on the I2C slave device (see short-cut macros)

PARAMETER1:     unsigned int device_action is I2C bus address of slave device
                ORed with I2C device type ORed with action (see header file)

PARAMETER2:     unsigned int subaddr is 8-bit or 16-bit subaddress (if applicable)

PARAMETER3:     char * blk_ptr is pointer to block of length count (if not poll)

PARAMETER4:     unsigned int count is number of bytes to read / write / compare

NOTES:          If count is zero, then only polling occurs regardless of action
                If read and write bits are both set, then only a read occurs

RETURN VALUE:   0 on success, < 0 on error, or I2C_COMPARE_MISMATCH (if compare)

END DESCRIPTION **********************************************************/

int i2c_action(unsigned int device_action, unsigned int subaddr,  \
               char * blk_ptr, unsigned int count)
    {
    int err;
    char value;

    i2c_byte_count = 0;                                 // No bytes read, written or compared yet

    if (count == 0)
        device_action &= ~(I2C_RD_MSK | I2C_WR_MSK);    // Force poll only if count is zero

    // Send I2C start condition
    if ((err = i2c_start_tx()) != 0)
        goto i2c_problem;

    value = (device_action & 0xFE);                     // R/W bit is zero for most accesses
    if ((device_action & (I2C_RD_MSK | I2C_SUB_MSK)) == I2C_RD_MSK)
        value |= 0x01;                                  // Set R/W bit for read without subaddress

    // Send I2C bus address
    if ((err = i2c_write_char(value)) != 0)
        goto i2c_problem;

    if ((device_action & (I2C_RD_MSK | I2C_WR_MSK)) == 0)
        goto i2c_success;                               // Poll completed - exit with success

    if ((device_action & I2C_SUB_MSK) != 0)             // Subaddress required?
        {
        if ((device_action & I2C_LNG_MSK) != 0)         // 16-bit subaddress?
            {
            // Send high byte of subaddress
            if ((err = i2c_write_char((subaddr >> 8) & 0xFF)) != 0)
                goto i2c_problem;
            }

        // Send low byte of subaddress
        if ((err = i2c_write_char(subaddr & 0xFF)) != 0)
            goto i2c_problem;
        }

    if ((device_action & I2C_RD_MSK) != 0)              // Read operation?
        {
        if ((device_action & I2C_SUB_MSK) != 0)         // Subaddress turnaround?
            {
            // Send repeated I2C start condition
            if ((err = i2c_start_tx()) != 0)
                goto i2c_problem;

            // Re-send I2C bus address with R/W bit set for read
            if ((err = i2c_write_char((device_action & 0xFE) | 0x01)) != 0)
                goto i2c_problem;
            }

        while (count--)                                 // Count down bytes
            {
            // Read byte from I2C slave
            if ((err = i2c_read_char(&value)) != 0)
                goto i2c_problem;

            if ((device_action & I2C_CP_MSK) != 0)
                {
                if (*blk_ptr != value)                  // Compare, not store
                    goto i2c_compare_fail;
                }
            else
                {
                *blk_ptr = value;                       // Store read value
                }

            if (count != 0)
                // Send I2C ACK condition
                err = i2c_send_ack();                   // Another byte to be read
            else
                // Send I2C NAK condition
                err = i2c_send_nak();                   // Last byte read

            if (err != 0)
                goto i2c_problem;

            ++blk_ptr;                                  // Point to next in block
            ++i2c_byte_count;                           // Bump up byte count
            }
        }
    else                                                // Write operation
        {
        while (count--)                                 // Count down bytes
            {
            // Write byte to I2C slave
            if ((err = i2c_write_char(*blk_ptr)) != 0)
                goto i2c_problem;

            ++blk_ptr;                                  // Point to next in block
            ++i2c_byte_count;                           // Bump up byte count
            }
        }

i2c_success:
    // Send I2C stop condition
    i2c_stop_tx();
    return I2C_SUCCESS;                                 // Indicate success

// Exception handlers

i2c_compare_fail:
    // Send I2C NAK condition and ignore outcome
    (void) i2c_send_nak();
    // Send I2C stop condition
    i2c_stop_tx();
    return I2C_COMPARE_MISMATCH;                        // Indicate mismatch

i2c_problem:
    // Send I2C stop condition
    i2c_stop_tx();
    return err;                                         // Indicate error condition
    }


/* START FUNCTION DESCRIPTION ********************************************
i2c_read_byte                                                   <I2C.LIB>

SYNTAX:         int i2c_read_byte(unsigned int device, unsigned int subaddr);

DESCRIPTION:    Wrapper routine to simplify single-byte I2C read

PARAMETER1:     unsigned int device is I2C bus address of slave device
                ORed with I2C device type (see header file)

PARAMETER2:     unsigned int subaddr is 8-bit or 16-bit subaddress (if applicable)

RETURN VALUE:   Read byte (0-255) on success or < 0 on error

END DESCRIPTION **********************************************************/

int i2c_read_byte(unsigned int device, unsigned int subaddr)
    {
    int err;
    char value;

    if ((err = i2c_action((device | I2C_READ), subaddr, &value, 1)) != 0)
        return err;

    return value;
    }


/* START FUNCTION DESCRIPTION ********************************************
i2c_write_byte                                                  <I2C.LIB>

SYNTAX:         int i2c_write_byte(unsigned int device, unsigned int subaddr,
                char value);

DESCRIPTION:    Wrapper routine to simplify single-byte I2C write

PARAMETER1:     unsigned int device is I2C bus address of slave device
                ORed with I2C device type (see header file)

PARAMETER2:     unsigned int subaddr is 8-bit or 16-bit subaddress (if applicable)

PARAMETER3:     char value is byte to write

RETURN VALUE:   0 on success or < 0 on error

END DESCRIPTION **********************************************************/

int i2c_write_byte(unsigned int device, unsigned int subaddr, char value)
    {
    return i2c_action((device | I2C_WRITE), subaddr, &value, 1);
    }
