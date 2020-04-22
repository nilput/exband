import sys,os.path,os
from importlib import import_module

request_handler = None

def module_init(mod_ptr, user_module):
    print('mod ptr: ', mod_ptr)
    print('arg: ', user_module)
    module_path = None
    module_name = None
    if os.path.isdir(user_module):
        print('DIR!')
        module_path = user_module
        module_name = os.path.basename(user_module)
    elif any(map(os.path.exists, [user_module, user_module+'.py'])):
        print('EXISTS!')
        module_path = os.path.dirname(user_module)
        module_name = os.path.basename(user_module)
    else:
        print('Module "{}" not found.'.format(user_module))
        return 1
    sys.path = [module_path,] + sys.path
    imported_module = import_module(module_name, package=module_name)
    global request_handler
    request_handler = imported_module.handler

    return 0
from ._exb_ffi import ffi, lib

def asbytes(text):
    if isinstance(text, str):
        return text.encode('utf-8')
    if not isinstance(text, bytes):
        raise TypeError()
    return text

class Request:
    def __init__(self, rqstate_ptr):
        self._rqstate_ptr = rqstate_ptr
        self._is_ended = False
    def end(self):
        if self._is_ended:
            raise RuntimeError('response already ended')
        lib.exb_response_end(self._rqstate_ptr)
        self._is_ended = True
    def append_body(self, text):
        lib.exb_response_append_body_cstr(self._rqstate_ptr, asbytes(text))
    def set_header(self, header, value):
        lib.exb_response_set_header_c(self._rqstate_ptr, asbytes(header), asbytes(value))
    

@ffi.def_extern()
def py_handle_request(rqstate_ptr):
    try:
        request = Request(rqstate_ptr)
        request_handler(request)
    finally:
        request.end()
