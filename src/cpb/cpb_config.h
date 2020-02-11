#ifndef CPB_CONFIG_H
#define CPB_CONFIG_H

/*Eloop env config*/
#define CPB_MAX_ELOOPS 128
/*End Eloop env config*/

/*Server configuration*/
#define LISTEN_BACKLOG 8000
#define CPB_SOCKET_MAX 8192
#define CPB_SERVER_MAX_MODULES 11
#define CPB_HTTP_MIN_DELAY 0//ms

#define CPB_USE_READ_WRITE_FOR_TCP
#undef  CPB_USE_READ_WRITE_FOR_TCP

/*End Server configuration*/
/*Pcontrol config*/
#define CPB_MAX_PROCESSES 128
#define CPB_MAX_HOOKS 16
/*End Pcontrol config*/

#endif