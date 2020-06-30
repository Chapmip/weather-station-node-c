// Header file for HTTP POST client routines

// Copyright (c) 2006, Ian Chapman (Chapmip Consultancy)

// All rights reserved, except for those rights implicitly granted to
// GitHub Inc by publishing on GitHub and those rights granted by
// commercial agreement with the author.


#ifndef POST_CLIENT_H
#define POST_CLIENT_H


// Status of the POST process (values returned by post_get_status and post_tick)

#define POST_SUCCESS            1
#define POST_PENDING            0
#define POST_NOT_STARTED        (-1)
#define POST_CANNOT_START       (-2)
#define POST_TIMEOUT            (-3)
#define POST_ABORTED            (-4)
#define POST_DNS_ERR            (-5)
#define POST_SOCKET_ERR         (-6)
#define POST_CONNECTION_LOST    (-7)
#define POST_SEND_ERR           (-8)
#define POST_RESP_ERR           (-9)
#define POST_SERVER_ERR         (-10)
#define POST_BAD_ID             (-11)
#define POST_REJECTED           (-12)
#define POST_BAD_STATE          (-13)


// Function prototypes

int post_init(unsigned int body_max_size);
int post_set_server(char * host, word port, char * path, char * proxy_host, word proxy_port);

void post_clear_body(void);
int post_add_variable(const char * name, const char * value, unsigned int hexlen);
int post_check_overflow(void);

int post_start(void);
void post_abort(void);

int post_get_status(void);
int post_get_resp_class(void);

int post_tick(void);


#endif
