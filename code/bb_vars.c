// Battery-backed variables and initialisation routine

// Copyright (c) 2006, Ian Chapman (Chapmip Consultancy)

// All rights reserved, except for those rights implicitly granted to
// GitHub Inc by publishing on GitHub and those rights granted by
// commercial agreement with the author.


#include "wx_main.h"
#include "bb_vars.h"


// Constants

#define BB_MAGIC_NUMBER     0xFACE55AAUL            // Expected value in BB memory


// Battery-backed variables (some external)

#pragma seg(BSS,BB_BSS)

static unsigned long bb_mem_flag;                   // Indicates contents okay

unsigned long bb_seq_num;                           // 32-bit sequence number

static unsigned char bb_ver_major;                  // Firmware major version number
static unsigned char bb_ver_minor;                  // Firmware minor version number

unsigned char bb_post_error_flag;                   // Indicates previous POST error
const char * bb_post_error_str;                     // Description of previous POST error
int bb_post_error_state_num;                        // Last state for previous POST error

unsigned int bb_test_word;                          // Scratch value for test commands

#pragma seg(BSS)


// *** EXTERNAL FUNCTIONS ***

// Initialise battery-backed RAM if contents are not valid
// Ensures that string pointer is made safe if firmware version has changed

void bb_init(void)
    {
    if (bb_mem_flag != BB_MAGIC_NUMBER)
        {
        bb_mem_flag = BB_MAGIC_NUMBER;              // Memory trashed -- clear it

        bb_seq_num = 0UL;

        bb_ver_major = 0;                           // Force clear of POST error values
        bb_ver_minor = 0;
        }

    if (bb_ver_major != VER_MAJOR || bb_ver_minor != VER_MINOR)
        {
        bb_ver_major = VER_MAJOR;
        bb_ver_minor = VER_MINOR;

        bb_post_error_flag = 0;                     // Clear POST error values
        bb_post_error_str = BB_BAD_POST_ERR_STR;
        bb_post_error_state_num = -1;
        }
    }
