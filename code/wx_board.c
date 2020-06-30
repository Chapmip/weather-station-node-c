// WX board-specific routines

// Copyright (c) 2006, Ian Chapman (Chapmip Consultancy)

// All rights reserved, except for those rights implicitly granted to
// GitHub Inc by publishing on GitHub and those rights granted by
// commercial agreement with the author.


#include <rabbit.h>
#include "wx_board.h"


/*  Note that wx_init_board() initialises the following:
 *
 *      External I/O (LED outputs and switch inputs)
 *      DTR and RTS outputs for Serial Port E (both set false)
 *      DSR, DCD and RI inputs for Serial Port E
 *      Transmit enable for RS-485 on Serial Port D (disabled)
 *      Slave attention input for I2C bus
 *
 *  It does not initialise any of the following:
 *
 *      I2C bus (SCL and SDA pins)
 *      Serial Ports A-F
 */


// Hidden shadow register for current LED state

static unsigned char led_state;


// Externally-visible current switch states

unsigned char wx_dip_state;

unsigned char wx_rotary_sel;

unsigned char wx_switch_1;
unsigned char wx_switch_2;
unsigned char wx_switch_3;
unsigned char wx_switch_4;


// Macros to set LED outputs and get switch states using external I/O bus

#define SET_LEDS(UC_VAL)    oute(0x8000, UC_VAL)    // Strobe out byte using PE4
#define GET_SWITCHES()      ine(0xE000)             // Strobe in byte using PE7


// Change state of one or more LEDs
// Call with mask = LED_xxxx (name(s) of LEDs -- can be ORed together)
//  and new_state = LED_NONE, LED_GREEN, LED_RED or LED_YELLOW

void wx_set_leds(unsigned char mask, unsigned char new_state)
    {
    unsigned char changes;

    changes = (led_state ^ new_state) & mask;
    led_state ^= changes;

    SET_LEDS(led_state);
    }


// Read switches and update stored switch state

void wx_get_switches(void)
    {
    wx_dip_state = ~GET_SWITCHES();                 // Invert so closure is a one bit

    wx_rotary_sel = ((wx_dip_state >> 4) & 0x0F);   // Shift down upper nibble

    wx_switch_1 = ((wx_dip_state & 1) != 0);        // Get boolean switch states
    wx_switch_2 = ((wx_dip_state & 2) != 0);
    wx_switch_3 = ((wx_dip_state & 4) != 0);
    wx_switch_4 = ((wx_dip_state & 8) != 0);
    }


// Initialise for external I/O bus access to LEDs and switches

static void init_ext_IO(void)
    {
    enableIObus();                          // Required for ine/oute calls

    ioSrSetBitsI(PEFR,  0x90);              // Configure pins as I/O strobes
    ioSrSetBitsI(PEDDR, 0x90);              // Configure pins as outputs

    ioSrOutI(IB7CR, 0x10);                  // Set PE7 as active-low read strobe
    ioSrOutI(IB4CR, 0x28);                  // Set PE4 as active-low write strobe

    led_state = 0x00;                       // Clear all LEDs at first
    SET_LEDS(led_state);

    wx_get_switches();                      // Get initial switch states
    }


// Initialisation and control routines for Serial Port E handshake lines

// Bits on Parallel Port F

#define DTR_BIT         0                   // Output (initially high)
#define RTS_BIT         1                   // Output (initially high)
#define CTS_BIT         4                   // Input
#define DSR_BIT         6                   // Input
#define DCD_BIT         7                   // Input

#define PF_OUT_MSK      ((1 << DTR_BIT) | (1 << RTS_BIT))
#define PF_INP_MSK      ((1 << CTS_BIT) | (1 << DSR_BIT) | (1 << DCD_BIT))

#define PF_SER_MSK      (PF_OUT_MSK | PF_INP_MSK)

// Bit on Parallel Port E

#define RI_BIT          5                   // Input


// Set handshake output lines true or false

void wx_set_dtr_true(void)
    {
    ioSrClearBitI(PFDR, DTR_BIT);           // Low sets DTR to +ve voltage
    }

void wx_set_dtr_false(void)
    {
    ioSrSetBitI(PFDR, DTR_BIT);             // High sets DTR to -ve voltage
    }

void wx_set_rts_true(void)
    {
    ioSrClearBitI(PFDR, RTS_BIT);           // Low sets RTS to +ve voltage
    }

void wx_set_rts_false(void)
    {
    ioSrSetBitI(PFDR, RTS_BIT);             // High sets RTS to -ve voltage
    }


// Read state of handshake lines
// Returns !0 if logic low (+ve voltage = true) or 0 if logic high (-ve voltage = false)

int wx_get_dsr(void)
    {
    if (ini(PFDR) & (1 << DSR_BIT))
        return 0;                           // High means false
    else
        return 1;                           // Low means true
    }

int wx_get_cts(void)
    {
    if (ini(PFDR) & (1 << CTS_BIT))
        return 0;                           // High means false
    else
        return 1;                           // Low means true
    }

int wx_get_dcd(void)
    {
    if (ini(PFDR) & (1 << DCD_BIT))
        return 0;                           // High means false
    else
        return 1;                           // Low means true
    }

int wx_get_ri(void)
    {
    if (ini(PEDR) & (1 << RI_BIT))          // On Parallel Port E!
        return 0;                           // High means false
    else
        return 1;                           // Low means true
    }


// Initialise all handshake lines for Serial Port E

static void init_SerialE_handshake(void)
    {
    ioSrOutI(PFCR, 0x00);                   // Normal clocking of output register
    ioSrClearBitsI(PFFR,  PF_SER_MSK);      // Normal function on port bits
    ioSrClearBitsI(PFDCR, PF_OUT_MSK);      // Normal high/low output bits
    ioSrSetBitsI  (PFDR,  PF_OUT_MSK);      // Set outputs high initially
    ioSrSetBitsI  (PFDDR, PF_OUT_MSK);      // Configure pins as outputs
    ioSrClearBitsI(PFDDR, PF_INP_MSK);      // Configure pins as inputs

    ioSrClearBitI(PEFR,  RI_BIT);           // Normal function on port bit
    ioSrClearBitI(PEDDR, RI_BIT);           // Configure pin as input
    }


// Initialisation and control routine for RS-485 transmit enable line on Serial Port D

// Bit on Parallel Port F

#define DE_BIT          5                   // Output (initially low)


// Set transmit enable line to specified state
// This function may be passed to SerialInit485D()

void wx_set_rs485_enable(int enable)
    {
    if (enable)
        ioSrSetBitI(PFDR, DE_BIT);          // High enables RS-485 transmitter
    else
        ioSrClearBitI(PFDR, DE_BIT);        // Low disables RS-485 transmitter
    }


// Initialise transmit enable line as low output

static void init_rs485_enable(void)
    {
    ioSrOutI(PFCR, 0x00);                   // Normal clocking of output register
    ioSrClearBitI(PFFR,  DE_BIT);           // Normal function on port bit
    ioSrClearBitI(PFDCR, DE_BIT);           // Normal high/low output bit
    ioSrClearBitI(PFDR,  DE_BIT);           // Set output low initially
    ioSrSetBitI  (PFDDR, DE_BIT);           // Configure pin as output
    }


// Initialisation and read routine for slave attention line on I2C bus

// Bit on Parallel Port E

#define ATN_BIT         1                   // Input


// Read state of slave attention line
// Returns !0 if logic low (asserted) or 0 if logic high (not asserted)

int wx_chk_slave_atn(void)
    {
    if (ini(PEDR) & (1 << ATN_BIT))         // On Parallel Port E!
        return 0;                           // High means false
    else
        return 1;                           // Low means true
    }


// Initialise slave attention line as input

static void init_slave_atn(void)
    {
    ioSrClearBitI(PEFR,  ATN_BIT);          // Normal function on port bit
    ioSrClearBitI(PEDDR, ATN_BIT);          // Configure pin as input
    }


// Initialise everything in this module

void wx_init_board(void)
    {
    init_ext_IO();
    init_SerialE_handshake();
    init_rs485_enable();
    init_slave_atn();
    }
