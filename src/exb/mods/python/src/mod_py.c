#define _GNU_SOURCE
#include <string.h>
#include <signal.h>
#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <dlfcn.h>

#include "../../../exb.h"
#include "../../../exb_str.h"
#include "../../../http/http_server_module.h"
#include "../../../http/http_request.h"
#include "../../../http/http_server.h"


#include "../../../http/http_server_events.h"
#include "../../../http/http_response.h"
#include "../../../http/http_parse.h"

//cffi
extern void exb_py_handle_request(struct exb_request_state *rqstate);

struct exb_py_module {
    struct exb_http_server_module head;
    struct exb *exb_ref;
    int count;
    struct exb_http_server *server;
    struct exb_str exb_py_module_path;
    struct exb_str user_py_module_path;
    struct defaultsigs {
        struct sigaction int_handler;
    } defaultsigs;

    bool gil_unlocked;
    PyGILState_STATE gstate;
};
static void save_signal_handlers(struct exb_py_module *mod) {
    struct sigaction tmp_handler;
    struct sigaction old_handler;
    memset(&tmp_handler, 0, sizeof(tmp_handler));
    sigemptyset(&tmp_handler.sa_mask);
    tmp_handler.sa_handler = SIG_IGN;
    sigaction(SIGINT, &tmp_handler, &old_handler );
    sigaction(SIGINT, &old_handler, NULL);
    mod->defaultsigs.int_handler = old_handler;
}
static void restore_signal_handlers(struct exb_py_module *mod) {
    sigaction(SIGINT, &mod->defaultsigs.int_handler, NULL);
}

static int destroy_python() {
    return Py_FinalizeEx() == 0 ? EXB_OK : EXB_INVALID_STATE_ERR;
}

static void destroy_module(struct exb_http_server_module *module, struct exb *exb) {
    struct exb_py_module *mod = (struct exb_py_module*) module;
    destroy_python();
    exb_str_deinit(exb, &mod->exb_py_module_path);
    exb_str_deinit(exb, &mod->user_py_module_path);
    exb_free(exb, module);
}

int split(char *buff, int buffsz, char **tok) {
    if (!tok || !*tok || !**tok)
        return 0;
    char *begin = *tok;
    while (*begin == ' ')
        begin++;
    char *end = strchr(begin, ' ');
    if (end == NULL)
        end = begin + strlen(begin);
    *tok = end;
    if (end - begin > buffsz)
        return 2;
    strncpy(buff, begin, end - begin);
    buff[end - begin] = 0;
    return 1;
}


static void handle_args(struct exb_py_module *mod, char *module_args) {
    char *tok = module_args;
    char buff[256];
    while (split(buff, 256, &tok)) {
        if (strncmp(buff, "py-module=", 10) == 0) {
            exb_str_init_strcpy(mod->exb_ref, &mod->user_py_module_path, buff+10);
        }
        else if (strncmp(buff, "exb-py-path=", 12) == 0) {
            exb_str_init_strcpy(mod->exb_ref, &mod->exb_py_module_path, buff+12);
        }
        else {
            fprintf(stderr, "Unknown module argument: '%s'\n", buff);
        }
    }
}

static void request_handler(struct exb_http_server_module *module, struct exb_request_state *rqstate, int reason) {
    struct exb_py_module *mod = (struct exb_py_module *) module;
    if (mod->gil_unlocked) {
        mod->gstate = PyGILState_Ensure();
        mod->gil_unlocked = false;
    }

    exb_py_handle_request(rqstate);

}
static void release_gil(struct exb_py_module *mod) {
    if (!mod->gil_unlocked) {
        PyGILState_Release(mod->gstate);
        mod->gil_unlocked = true;
    }
}

static int initialize_python(struct exb_py_module *mod) {

    char *p = getenv("PYTHONPATH");
    struct exb_str ppath;
    int rv;
    exb_str_init_const_str(&ppath, "");
    if ((p && (rv = exb_str_init_strcpy(mod->exb_ref, &ppath, p)) != EXB_OK) || 
        ((rv = exb_str_strappend(mod->exb_ref, &ppath, mod->exb_py_module_path.str)) != EXB_OK))
    {
        return rv;
    }
    setenv("PYTHONPATH", ppath.str, 1);
    exb_str_deinit(mod->exb_ref, &ppath);
    save_signal_handlers(mod);
    Py_Initialize();

    if (!PyEval_ThreadsInitialized()) 
    { 
        PyEval_InitThreads(); 

    } 

    restore_signal_handlers(mod);
    PyObject * pName = PyUnicode_DecodeFSDefault("_exb_py");
    /* Error checking of pName left out */

    PyObject * pModule = PyImport_Import(pName);
    Py_DECREF(pName);

    if (pModule == NULL) {
        PyErr_Print();
        fprintf(stderr, "Failed to load \"%s\"\n", "_exb_py");
        Py_FinalizeEx();
        return 1;
    }

    const char *func_name = "module_init";
    PyObject * pFunc = PyObject_GetAttrString(pModule, func_name);
    /* pFunc is a new reference */

    if (!pFunc || !PyCallable_Check(pFunc)) {
        if (PyErr_Occurred())
            PyErr_Print();
        fprintf(stderr, "Cannot find function \"%s\"\n", func_name);
        Py_DECREF(pFunc);
        Py_DECREF(pModule);
        Py_FinalizeEx();
        return 1;
    }
    PyObject * pArgs = PyTuple_New(2);
    {
        PyObject * moduleptr = PyLong_FromVoidPtr(mod);
        if (!pArgs || !moduleptr) {
            Py_DECREF(pArgs);
            Py_DECREF(pModule);
            Py_DECREF(pFunc);
            Py_DECREF(moduleptr);
            fprintf(stderr, "Cannot convert argument\n");
            Py_FinalizeEx();
            return 1;
        }
        PyTuple_SetItem(pArgs, 0, moduleptr);
        PyTuple_SetItem(pArgs, 1, PyUnicode_DecodeFSDefault(mod->user_py_module_path.str));
    }
    PyObject * pValue = PyObject_CallObject(pFunc, pArgs);
    Py_DECREF(pArgs);

    long ret = 1;
    if (pValue != NULL) {
        ret = PyLong_AsLong(pValue);
        printf("Result of call: %ld\n", ret);
        Py_DECREF(pValue);
    }
    
    if (ret != 0) {
        Py_DECREF(pFunc);
        Py_DECREF(pModule);
        PyErr_Print();
        fprintf(stderr,"module initialization failed\n");
        Py_FinalizeEx();
        return 1;
    }

    if (exb_server_set_module_request_handler(mod->server, (struct exb_http_server_module*)mod, request_handler) != EXB_OK) {
        //destroy_module((struct exb_http_server_module*)mod, exb);
        //return EXB_MODULE_LOAD_ERROR;
        fprintf(stderr, "setting py module as request handler failed\n");
    }
    /*
    PyThreadState* _main = PyThreadState_Get();
    PyThreadState_Clear(_main);
    PyThreadState_Delete(_main);
    */
    PyThreadState_DeleteCurrent();
    mod->gil_unlocked = true;

    return 0;
}

int exb_py_init(struct exb *exb, struct exb_server *server, char *module_args, struct exb_http_server_module **module_out) {
    (void) module_args;
    struct exb_py_module *mod = exb_malloc(exb, sizeof(struct exb_py_module));
    if (!mod)
        return EXB_NOMEM_ERR;
    exb_str_init_const_str(&mod->user_py_module_path, "");
    exb_str_init_const_str(&mod->exb_py_module_path, "exb.py");
    mod->exb_ref = exb;
    mod->head.destroy = destroy_module;
    mod->count = 0;
    mod->server = server;

    handle_args(mod, module_args);

    if (initialize_python(mod) != 0) {
        
    }


    
    *module_out = (struct exb_http_server_module*)mod;
    return EXB_OK;
}
