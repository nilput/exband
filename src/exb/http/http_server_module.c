#include <dlfcn.h>
#include <string.h>
#include "../exb.h"
#include "../exb_str.h"
#include "../exb_str_list.h"
#include "../exb_log.h"
#include "http_server_module.h"
#include "http_server_module_internal.h"
#include "http_request_handler.h"

/*name := "dll_name:func_name"*/
int exb_http_server_module_load(struct exb *exb_ref,
                                struct exb_server *server,
                                int module_id,
                                char *handler_name,
                                char *module_args,
                                struct exb_str_list *import_list,
                                struct exb_http_server_module **module_out,
                                void **handle_out) 
{
    char *seperator = strchr(handler_name, ':');
    if (!seperator)
        return EXB_INVALID_ARG_ERR;
    struct exb_str dll_name;
    struct exb_str func_name;
    int err = exb_str_init_strlcpy(exb_ref, &dll_name, handler_name, seperator-handler_name);
    if (err != EXB_OK)
        return err;
    err = exb_str_init_strcpy(exb_ref, &func_name, seperator+1);
    if (err != EXB_OK) {
        exb_str_deinit(exb_ref, &dll_name);
        return err;
    }
    if ((err = exb_str_strappend(exb_ref, &func_name, "_init")) != EXB_OK) {
        exb_str_deinit(exb_ref, &dll_name);
        exb_str_deinit(exb_ref, &func_name);
        return err;
    }
    void *handle = dlopen(dll_name.str, RTLD_LAZY | RTLD_GLOBAL);
    if (!handle) {
        char *e = dlerror();
        fprintf(stderr, "Failed to load lib: \"%s\": %s\n", dll_name.str, e ? e : "");
        exb_str_deinit(exb_ref, &dll_name);
        exb_str_deinit(exb_ref, &func_name);
        return EXB_NOT_FOUND;
    }
    exb_http_server_module_init_func init_func = dlsym(handle, func_name.str);
    int init_success = init_func != NULL && (init_func(exb_ref, server, module_args, module_out) == 0) && module_out;
    if (init_success) {
        for (int i=0; i < import_list->len; i++) {
            exb_request_handler_func handler = dlsym(handle, import_list->elements[i].str); //can be NULL
            exb_http_server_module_on_load_resolve_handler(exb_ref,
                                                   server,
                                                   module_id,
                                                   *module_out,
                                                   import_list->elements[i].str, 
                                                   handler);
        }
        *handle_out = handle;
    }
    else {
        dlclose(handle);
    }
    exb_str_deinit(exb_ref, &dll_name);
    exb_str_deinit(exb_ref, &func_name);
    if (!init_func)
        return EXB_NOT_FOUND;
    else if (!init_success)
        return EXB_INIT_ERROR;
    return EXB_OK;
}
int exb_http_server_module_unload(struct exb *exb_ref, struct exb_http_server_module *module, void *handle) {
    
    if (module->destroy)
        module->destroy(module, exb_ref);
    dlclose(handle);
    return EXB_OK;
}
