#ifndef EXB_CONFIG_H
#define EXB_CONFIG_H

/*Eloop env config*/
#define EXB_MAX_ELOOPS 128
/*End Eloop env config*/

/*Server configuration*/
#define LISTEN_BACKLOG 8000
#define EXB_SOCKET_MAX 8192
#define EXB_SERVER_MAX_MODULES 11
#define EXB_HTTP_MIN_DELAY 0//ms

#define EXB_USE_READ_WRITE_FOR_TCP
#undef  EXB_USE_READ_WRITE_FOR_TCP

/*End Server configuration*/
/*Pcontrol config*/
#define EXB_MAX_PROCESSES 128
#define EXB_MAX_HOOKS 16
/*End Pcontrol config*/

#endif