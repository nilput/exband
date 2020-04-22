#!/usr/bin/python3
from cffi import FFI
ffibuilder = FFI()
ffibuilder.cdef('struct exb;')
ffibuilder.cdef('struct exb_server;')
ffibuilder.cdef('struct exb_request_state;')
#ffibuilder.cdef('struct exb_request_state *exb_py_get_request();')
ffibuilder.cdef('int exb_response_set_header_c(struct exb_request_state *rqstate, char *name, char *value);')
ffibuilder.cdef('int exb_response_append_body_cstr(struct exb_request_state *rqstate, char *text);')
ffibuilder.cdef('int exb_response_end(struct exb_request_state *rqstate);')
ffibuilder.cdef('extern "Python" void py_handle_request(struct exb_request_state *);')
ffibuilder.set_source('_exb_ffi', '''
    #include "exb_py_ffi.h"

    static void py_handle_request(struct exb_request_state *rqstate);

    CFFI_DLLEXPORT void exb_py_handle_request(struct exb_request_state *rqstate)
    {
        py_handle_request(rqstate);
    }
''', extra_compile_args=["-I../src", "-I../../../"], library_dirs=[], libraries=[])


if __name__ == '__main__':
    ffibuilder.compile(verbose=True)
