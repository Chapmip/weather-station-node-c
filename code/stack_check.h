// Header file for stack depth checking routines

// Copyright (c) 2006, Ian Chapman (Chapmip Consultancy)

// All rights reserved, except for those rights implicitly granted to
// GitHub Inc by publishing on GitHub and those rights granted by
// commercial agreement with the author.


#ifndef STACK_CHECK_H
#define STACK_CHECK_H


// Stack size in bytes (must agree with definition in "Cstart.asm")

#define STACK_SIZE      4096


// Function prototypes

int check_stack(void);
void report_stack(void);


#endif
