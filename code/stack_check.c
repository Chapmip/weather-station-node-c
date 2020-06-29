// Stack depth checking routines

// Copyright (c) 2006, Ian Chapman (Chapmip Consultancy)

// All rights reserved, except for those rights implicitly granted to
// GitHub Inc by publishing on GitHub and those rights granted by
// commercial agreement with the author.


#include <stdio.h>
#include "stack_check.h"


// Pointers to first and last bytes in stack

#define STACK_END			((char *) 0xDFFF)
#define STACK_START			(STACK_END - STACK_SIZE + 1)


// Stack filler values (as per "Cstart.asm")

#define STACK_MARKER_1		0x5555
#define STACK_MARKER_2		0xAAAA
#define STACK_FILLER		0x55


// *** EXTERNAL FUNCTIONS ***

// Check maximum depth of stack by searching for lowest unchanged stack byte
// Returns number of stack bytes used so far (0 to STACK_SIZE)
// Returns -1 if stack marker is not found

int check_stack(void)
	{
    char * stack_ptr;

	if ((* (unsigned int *) (STACK_START + 0) != STACK_MARKER_1)
	 || (* (unsigned int *) (STACK_START + 2) != STACK_MARKER_2))
	 	return -1;

	for (stack_ptr = STACK_START + 4; stack_ptr <= STACK_END; ++stack_ptr)
		{
        if (* stack_ptr != STACK_FILLER)
			break;
		}

	return (int) (STACK_END + 1 - stack_ptr);
	}


// Report maximum depth of stack used so far

void report_stack(void)
	{
	int stack_used;

	stack_used = check_stack();

	if (stack_used < 0)
		printf("STACK: ERROR - Unable to find stack marker\r\n");
	else
		printf("STACK: Maximum %d bytes used\r\n");
	}
