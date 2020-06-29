// Header file for routines to manage configuration menu

// Copyright (c) 2006, Ian Chapman (Chapmip Consultancy)

// All rights reserved, except for those rights implicitly granted to
// GitHub Inc by publishing on GitHub and those rights granted by
// commercial agreement with the author.


#ifndef	MENU_H
#define	MENU_H

// Specific ASCII characters used in and outside menu routines

#define MENU_BS					0x08
#define MENU_CR					0x0D
#define MENU_ESC				0x1B

// Function prototypes

int menu_exec(void);

#endif
