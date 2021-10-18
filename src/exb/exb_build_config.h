#ifndef EXB_BUILD_CONFIG_H
#define EXB_BUILD_CONFIG_H

#define EXBAND_VERSION_STR "v0.1.1"

/*Eloop env config*/
#define EXB_MAX_ELOOPS 128
/*End Eloop env config*/

/*Server configuration*/
#define LISTEN_BACKLOG 2000
#define EXB_SOCKET_MAX 8192
#define EXB_SERVER_MAX_MODULES 11
#define EXB_SERVER_MAX_NAMED_HANDLERS 16
#define EXB_SERVER_MAX_RULES   32
#define EXB_HTTP_MIN_DELAY 0//ms

//#define EXB_WITH_SSL        0
#define EXB_WITH_OPENSSL_DH
//#undef EXB_WITH_OPENSSL_DH
#define EXB_USE_OPENSSL_ECDH
//#undef EXB_USE_OPENSSL_ECDH
#define EXB_SNI_MAX_DOMAINS 8

//This will be allocated on the stack
//and also used for small buffers that are kept in case we get bytes out of wbio
// and we have nowhere to store them
#define EXB_SSL_RW_BUFFER_SIZE 8192


#define EXB_MAX_DOMAINS     8

#define EXB_USE_READ_WRITE_FOR_TCP
#undef  EXB_USE_READ_WRITE_FOR_TCP
/*End Server configuration*/

/*Process control config*/
#define EXB_MAX_PROCESSES 128
#define EXB_MAX_HOOKS 16
/*End Process control config*/

//Number of digits to represent an int
#define EXB_INT_DIGITS 14
#define EXB_LONG_LONG_DIGITS 24

/*
    0: not accurate at all
    1: somewhat accurate
    2: accurate
*/
#define EXB_ELOOP_TIME_ACCUARCY 1

#define EXB_HTTP_ADD_DATE_HEADER
//#undef  EXB_ADD_DATE_HEADER

#endif
