// Header file for battery-backed variables and initialisation routine

// Copyright (c) 2006, Ian Chapman (Chapmip Consultancy)

// All rights reserved, except for those rights implicitly granted to
// GitHub Inc by publishing on GitHub and those rights granted by
// commercial agreement with the author.


#ifndef BB_VARS_H
#define BB_VARS_H


// Constants

#define BB_BAD_POST_ERR_STR     "Invalid POST error string"


// External variables

#pragma seg(BSS,BB_BSS)

extern unsigned long bb_seq_num;

extern unsigned char bb_post_error_flag;
extern const char * bb_post_error_str;
extern int bb_post_error_state_num;

extern unsigned int bb_test_word;

#pragma seg(BSS)


// Function prototypes

void bb_init(void);


#endif
