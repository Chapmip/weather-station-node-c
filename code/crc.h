// Header file for routine to calculate 16-bit CCITT standard CRC for
// blocks of data sent by Vantage Pro(TM) weather station

// Copyright (c) 2006, Ian Chapman (Chapmip Consultancy)

// All rights reserved, except for those rights implicitly granted to
// GitHub Inc by publishing on GitHub and those rights granted by
// commercial agreement with the author.


#ifndef	CRC_H
#define	CRC_H

// Function prototypes

unsigned int crc_calculate(const void * blk_start, size_t blk_size);

#endif
