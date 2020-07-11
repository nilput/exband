#ifndef EXB_IO_RESULT_H
#define EXB_IO_RESULT_H

enum exb_io_flags {
    EXB_IO_FLAG_CLIENT_CLOSED = 1,
    EXB_IO_FLAG_IO_ERROR = 2,   //also generally non recoverable unless proven otherwise
    EXB_IO_FLAG_CONN_FATAL = 4, //non recoverable for the connection
};

struct exb_io_result {
    size_t nbytes;
    int flags;
};

static struct exb_io_result exb_make_io_result(size_t nbytes, int flags) {
    struct exb_io_result res = {.nbytes = nbytes, .flags = flags};
    return res;
}

#endif