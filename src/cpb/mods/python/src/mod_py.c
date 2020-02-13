#define _GNU_SOURCE
#include <string.h>
#include <signal.h>
#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <dlfcn.h>

#include "../../../cpb.h"
#include "../../../cpb_str.h"
#include "../../../http/http_server_module.h"
#include "../../../http/http_request.h"
#include "../../../http/http_server.h"


#include "../../../http/http_server_events.h"
#include "../../../http/http_response.h"
#include "../../../http/http_parse.h"

//cffi
extern void cpb_py_handle_request(struct cpb_request_state *rqstate);

struct cpb_py_module {
    struct cpb_http_server_module head;
    struct cpb *cpb_ref;
    int count;
    struct cpb_http_server *server;
    struct cpb_str cpb_py_module_path;
    struct cpb_str user_py_module_path;
    struct defaultsigs {
        struct sigaction int_handler;
    } defaultsigs;

    bool gil_unlocked;
    PyGILState_STATE gstate;
};
static void save_signal_handlers(struct cpb_py_module *mod) {
    struct sigaction tmp_handler;
    struct sigaction old_handler;
    memset(&tmp_handler, 0, sizeof(tmp_handler));
    sigemptyset(&tmp_handler.sa_mask);
    tmp_handler.sa_handler = SIG_IGN;
    sigaction(SIGINT, &tmp_handler, &old_handler );
    sigaction(SIGINT, &old_handler, NULL);
    mod->defaultsigs.int_handler = old_handler;
}
static void restore_signal_handlers(struct cpb_py_module *mod) {
    sigaction(SIGINT, &mod->defaultsigs.int_handler, NULL);
}

static int destroy_python() {
    return Py_FinalizeEx() == 0 ? CPB_OK : CPB_INVALID_STATE_ERR;
}

static void destroy_module(struct cpb_http_server_module *module, struct cpb *cpb) {
    struct cpb_py_module *mod = (struct cpb_py_module*) module;
    destroy_python();
    cpb_str_deinit(cpb, &mod->cpb_py_module_path);
    cpb_str_deinit(cpb, &mod->user_py_module_path);
    cpb_free(cpb, module);
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


static void handle_args(struct cpb_py_module *mod, char *module_args) {
    char *tok = module_args;
    char buff[256];
    while (split(buff, 256, &tok)) {
        if (strncmp(buff, "py-module=", 10) == 0) {
            cpb_str_init_strcpy(mod->cpb_ref, &mod->user_py_module_path, buff+10);
        }
        else if (strncmp(buff, "cpb-py-path=", 12) == 0) {
            cpb_str_init_strcpy(mod->cpb_ref, &mod->cpb_py_module_path, buff+12);
        }
        else {
            fprintf(stderr, "Unknown module argument: '%s'\n", buff);
        }
    }
}

static void request_handler(struct cpb_http_server_module *module, struct cpb_request_state *rqstate, int reason) {
    struct cpb_py_module *mod = (struct cpb_py_module *) module;
    if (mod->gil_unlocked) {
        mod->gstate = PyGILState_Ensure();
        mod->gil_unlocked = false;
    }

    cpb_py_handle_request(rqstate);

}
static void release_gil(struct cpb_py_module *mod) {
    if (!mod->gil_unlocked) {
        PyGILState_Release(mod->gstate);
        mod->gil_unlocked = true;
    }
}

static int initialize_python(struct cpb_py_module *mod) {

    char *p = getenv("PYTHONPATH");
    struct cpb_str ppath;
    int rv;
    cpb_str_init_const_str(&ppath, "");
    if ((p && (rv = cpb_str_init_strcpy(mod->cpb_ref, &ppath, p)) != CPB_OK) || 
        ((rv = cpb_str_strappend(mod->cpb_ref, &ppath, mod->cpb_py_module_path.str)) != CPB_OK))
    {
        return rv;
    }
    setenv("PYTHONPATH", ppath.str, 1);
    cpb_str_deinit(mod->cpb_ref, &ppath);
    save_signal_handlers(mod);
    Py_Initialize();

    if (!PyEval_ThreadsInitialized()) 
    { 
        PyEval_InitThreads(); 

    } 

    restore_signal_handlers(mod);
    PyObject * pName = PyUnicode_DecodeFSDefault("_cpb_py");
    /* Error checking of pName left out */

    PyObject * pModule = PyImport_Import(pName);
    Py_DECREF(pName);

    if (pModule == NULL) {
        PyErr_Print();
        fprintf(stderr, "Failed to load \"%s\"\n", "_cpb_py");
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

    if (cpb_server_set_module_request_handler(mod->server, (struct cpb_http_server_module*)mod, request_handler) != CPB_OK) {
        //destroy_module((struct cpb_http_server_module*)mod, cpb);
        //return CPB_MODULE_LOAD_ERROR;
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

int cpb_py_init(struct cpb *cpb, struct cpb_server *server, char *module_args, struct cpb_http_server_module **module_out) {
    (void) module_args;
    struct cpb_py_module *mod = cpb_malloc(cpb, sizeof(struct cpb_py_module));
    if (!mod)
        return CPB_NOMEM_ERR;
    cpb_str_init_const_str(&mod->user_py_module_path, "");
    cpb_str_init_const_str(&mod->cpb_py_module_path, "cpb.py");
    mod->cpb_ref = cpb;
    mod->head.destroy = destroy_module;
    mod->count = 0;
    mod->server = server;

    handle_args(mod, module_args);

    if (initialize_python(mod) != 0) {
        
    }


    
    *module_out = (struct cpb_http_server_module*)mod;
    return CPB_OK;
}
