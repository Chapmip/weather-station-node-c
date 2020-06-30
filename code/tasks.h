// Header file for routines to carry out weather station tasks

// Copyright (c) 2006, Ian Chapman (Chapmip Consultancy)

// All rights reserved, except for those rights implicitly granted to
// GitHub Inc by publishing on GitHub and those rights granted by
// commercial agreement with the author.


#ifndef TASKS_H
#define TASKS_H

// Task initialisation status (values returned by tasks_init)

#define TASKS_INIT_OK           0
#define TASKS_POST_INIT_ERR     (-1)
#define TASKS_DAV_INIT_ERR      (-2)
#define TASKS_EE_INIT_ERR       (-3)
#define TASKS_SERVER_INIT_ERR   (-4)

// Task status (values returned by tasks_run)

#define TASKS_MENU              1
#define TASKS_OK                0
#define TASKS_POST_START_ERR    (-1)
#define TASKS_ETH_DOWN          (-2)
#define TASKS_LAN_DOWN          (-3)
#define TASKS_COLLECT_FAIL      (-4)
#define TASKS_POST_FAIL         (-5)
#define TASKS_BAD_STATE         (-6)

// Maximum number of seconds between updates

#define TASKS_MAX_UPDATE_SECS   3600

// Function prototypes

int tasks_init(void);
int tasks_run(void);

#endif
