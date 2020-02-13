from cffi import FFI
ffibuilder = FFI()
ffibuilder.cdef('struct cpb;')
ffibuilder.cdef('struct cpb_server;')
ffibuilder.cdef('struct cpb_request_state;')
#ffibuilder.cdef('struct cpb_request_state *cpb_py_get_request();')
ffibuilder.cdef('int cpb_response_set_header_c(struct cpb_request_state *rqstate, char *name, char *value);')
ffibuilder.cdef('int cpb_response_append_body_cstr(struct cpb_request_state *rqstate, char *text);')
ffibuilder.cdef('int cpb_response_end(struct cpb_request_state *rqstate);')
ffibuilder.cdef('extern "Python" void py_handle_request(struct cpb_request_state *);')
ffibuilder.set_source('_cpb_ffi', '''
    #include "cpb_py_ffi.h"

    static void py_handle_request(struct cpb_request_state *rqstate);

    CFFI_DLLEXPORT void cpb_py_handle_request(struct cpb_request_state *rqstate)
    {
        py_handle_request(rqstate);
    }
''', extra_compile_args=["-I../src", "-I../../../"], library_dirs=[], libraries=[])


if __name__ == '__main__':
    ffibuilder.compile(verbose=True)
