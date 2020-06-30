// Macros to enable timeouts of various lengths and granularity to be set and checked


#ifndef	TIMEOUT_H
#define	TIMEOUT_H

#include <Rabbit.h>

// To start a timer, call SET macro with timeout period and store result in variable
// (unsigned long for UL routines or unsigned int for UI macros)

// To check for timeout, call CHK macro with variable from SET and interpret boolean result
// (0 for no timeout, !0 for timeout)

// UL routines accept timeout values in range 1 to 2,147,483,647 (= 2^31 - 1)

#define SET_TIMEOUT_UL_MS(MS)		(getMilliSeconds() + (MS))
#define CHK_TIMEOUT_UL_MS(UL)		((long) (getMilliSeconds() - (UL)) >= 0)

#define SET_TIMEOUT_UL_SECS(SECS)	(getSeconds() + (SECS))
#define CHK_TIMEOUT_UL_SECS(UL)		((long) (getSeconds() - (UL)) >= 0)

// UI routines accept timeout values in range 1 to 32,767 (= 2^15 - 1)

#define SET_TIMEOUT_UI_MS(MS)		((unsigned int) getMilliSeconds() + (MS))
#define CHK_TIMEOUT_UI_MS(UI)		((int) ((unsigned int) getMilliSeconds() - (UI)) >= 0)

#define SET_TIMEOUT_UI_SECS(SECS)	((unsigned int) getSeconds() + (SECS))
#define CHK_TIMEOUT_UI_SECS(UI)		((int) ((unsigned int) getSeconds() - (UI)) >= 0)

#endif
