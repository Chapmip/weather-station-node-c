// Header file for WX board-specific routines

// Copyright (c) 2006, Ian Chapman (Chapmip Consultancy)

// All rights reserved, except for those rights implicitly granted to
// GitHub Inc by publishing on GitHub and those rights granted by
// commercial agreement with the author.


#ifndef	WX_BOARD_H
#define	WX_BOARD_H


// LED bit masks

#define LED_LAN				0x03			// Left-most LED (PL1 pin 8, 9)
#define LED_DAVIS			0x0C
#define LED_POST			0x30
#define LED_DOWNLOAD		0xC0			// Right-most LED (PL1 pins 2, 3)

#define LED_ALL				0xFF			// All LEDs at once


// LED states

#define LED_OFF				0x00			// All bits clear
#define LED_GREEN			0x55            // Bits 0, 2, 4, 6 set
#define LED_RED				0xAA			// Bits 1, 3, 5, 7 set
#define LED_AMBER			0xFF			// All bits set


// Global variables

extern unsigned char wx_dip_state;			// Overall bit map of switches

extern unsigned char wx_rotary_sel;			// Rotary switch selection (0-15)

extern unsigned char wx_switch_1;			// Individual switch states (0 or 1)
extern unsigned char wx_switch_2;
extern unsigned char wx_switch_3;
extern unsigned char wx_switch_4;


// Function prototypes

void wx_set_leds(unsigned char mask, unsigned char new_state);
void wx_get_switches(void);

void wx_set_dtr_true(void);
void wx_set_dtr_false(void);
void wx_set_rts_true(void);
void wx_set_rts_false(void);

int wx_get_dsr(void);
int wx_get_cts(void);
int wx_get_dcd(void);
int wx_get_ri(void);

void wx_set_rs485_enable(int enable);

int wx_chk_slave_atn(void);

void wx_init_board(void);


#endif
