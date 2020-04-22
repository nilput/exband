#ifndef EXB_HTTP_STATUS_H
#define EXB_HTTP_STATUS_H

#include "../exb_assert.h"
#include "../exb_errors.h"
#include <string.h>
//doesnt add a null terminator
static int exb_write_status_code(char *dest,
        int dest_size,
        int *written_bytes,
        int status_code,
        int http_major,
        int http_minor) 
{
    #define STATUS_BUFF_LEN 64
    char buff[STATUS_BUFF_LEN] = "HTTP/x.x ";
    exb_assert_h(http_major < 10 && http_minor < 10, "");
    buff[5] = '0' + http_major;
    buff[7] = '0' + http_minor;

    char *status = NULL;
    if (status_code == 200)
        status = "200 OK";
    else if (status_code == 201)
        status = "201 Created";
    else if (status_code == 202)
        status = "202 Accepted";
    else if (status_code == 204)
        status = "204 No Content";
    else if (status_code == 301)
        status = "301 Moved Permanently";
    else if (status_code == 302)
        status = "302 Found";
    else if (status_code == 303)
        status = "303 See Other";
    else if (status_code == 307)
        status = "307 Temporary Redirect";
    else if (status_code == 400)
        status = "400 Bad Request";
    else if (status_code == 401)
        status = "401 Unauthorized";
    else if (status_code == 403)
        status = "403 Forbidden";
    else if (status_code == 404)
        status = "404 Not Found";
    else if (status_code == 405)
        status = "405 Method Not Allowed";
    else if (status_code == 406)
        status = "406 Not Acceptable";
    else if (status_code == 412)
        status = "412 Precondition Failed";
    else if (status_code == 415)
        status = "415 Unsupported Media Type";
    else if (status_code == 500)
        status = "500 Internal Server Error";
    else if (status_code == 501)
        status = "501 Not Implemented";
    else 
        return EXB_INVALID_ARG_ERR;
    //thats impossible, the largest of the currently hardcoded status lines 26 chars + http version is 8 chars + 2 crlf
    exb_assert_h(((strlen(status) + strlen(buff) + 1) <= STATUS_BUFF_LEN), ""); 
    #undef STATUS_BUFF_LEN
    strcat(buff, status);
    strcat(buff, "\r\n");
    int buff_len = strlen(buff);
    if (buff_len > dest_size)
        return EXB_OUT_OF_RANGE_ERR;
    memcpy(dest, buff, buff_len);
    *written_bytes = buff_len;
    return EXB_OK;
}
/*
'<,'>s/\v((\d+).+)/if (status_code == \2){^Mstatus = "\1";^M}/gc
200 OK
201 Created
202 Accepted
204 No Content
301 Moved Permanently
302 Found
303 See Other
307 Temporary Redirect
400 Bad Request
401 Unauthorized
403 Forbidden
404 Not Found
405 Method Not Allowed
406 Not Acceptable
412 Precondition Failed
415 Unsupported Media Type
500 Internal Server Error
501 Not Implemented
*/

#endif