#include "exb/exb.h"
#include "exb/http/http_server_module.h"
#include "exb/http/http_request.h"

struct hello_world_module {
    struct exb_http_server_module head;
    struct exb *exb_ref;
    int count;
};
static int handle_request(struct exb_http_server_module *module, struct exb_request_state *rqstate, int reason) {
    struct hello_world_module *mod = (struct hello_world_module *) module;

    struct exb_str path;
    //exb_request_repr(rqstate);

    exb_str_init_empty(&path);
    exb_str_slice_to_copied_str(mod->exb_ref, rqstate->path_s, rqstate->input_buffer, &path);
    if (exb_str_streqc(mod->exb_ref, &path, "/post") || exb_str_streqc(mod->exb_ref, &path, "/post/")) {
        struct exb_str key,value;
        exb_str_init_const_str(&key, "Content-Type");
        exb_str_init_const_str(&value, "text/html");
        exb_response_append_body_cstr(rqstate, "<!DOCTYPE html>");
        exb_response_append_body_cstr(rqstate, "<html>"
                                                      "<head>"
                                                      "    <title>EXBIN!</title>"
                                                      "</head>"
                                                      "<body>");
        if (exb_request_has_body(rqstate)) {
            if (reason == EXB_HTTP_HANDLER_HEADERS) {
                rqstate->body_handling = EXB_HTTP_B_BUFFER;
            }
            else {
                exb_response_append_body_cstr(rqstate, "You posted: <br>");
                exb_response_append_body_cstr(rqstate, "<p>");

                /*XSS!*/
                if (rqstate->body_decoded.str)
                    exb_response_append_body_cstr(rqstate, rqstate->body_decoded.str);

                exb_response_append_body_cstr(rqstate, "</p>");
                exb_response_append_body_cstr(rqstate, "</body>"
                                                              "</html>");
                exb_response_end(rqstate);
            }
        }
        else {
            exb_response_set_header(rqstate, &key, &value);
            exb_response_append_body_cstr(rqstate, "<p> Post something! </p> <br>");
            exb_response_append_body_cstr(rqstate, "<form action=\"\" method=\"POST\">"
                                                          "Text: <input type=\"text\" name=\"text\"><br>"
                                                          "</form>");
            exb_response_append_body_cstr(rqstate, "</body>"
                                                          "</html>");
            exb_response_end(rqstate);
        }
        
    }
    else if (exb_str_startswithc(mod->exb_ref, &path, "/count")) 
    {
        struct exb_str key,value, tmp;
        exb_str_init_const_str(&key, "Content-Type");
        exb_str_init_const_str(&value, "text/html");
        exb_str_init_empty(&tmp);
        exb_sprintf(mod->exb_ref, &tmp, "<!DOCTYPE html>"
                                        "<html>"
                                        "<head>"
                                        "    <title>EXBIN!</title>"
                                        "</head>"
                                        "<body>"
                                        "<p>Count : <b>%d</b></p><br>"
                                        "<form action=\"/reset/\"><button>reset</button></form>"
                                        "</body>"
                                        "</html>", mod->count++);
        exb_response_append_body_cstr(rqstate, tmp.str);
        exb_str_deinit(mod->exb_ref, &tmp);
        exb_response_end(rqstate);
    }
    else if (exb_str_streqc(mod->exb_ref, &path, "/reset") || exb_str_startswithc(mod->exb_ref, &path, "/reset/")) {
        mod->count = 0;
        exb_response_redirect_and_end(rqstate, 307, "/count/");
    }
    else {
        struct exb_str key,value;
        exb_str_init_const_str(&key, "Content-Type");
        exb_str_init_const_str(&value, "text/plain");
        exb_response_set_header(rqstate, &key, &value);
        exb_response_append_body(rqstate, "Hello World!\r\n", 14);
        struct exb_str str;
        
        exb_str_init_empty(&str);
        
        exb_sprintf(mod->exb_ref, &str, "Requested URL: '%s'", path.str);
        
        exb_response_append_body(rqstate, str.str, str.len);
        exb_str_deinit(mod->exb_ref, &str);
        exb_response_end(rqstate);
    }
    
    exb_str_deinit(mod->exb_ref, &path);   
}
static void destroy_module(struct exb_http_server_module *module, struct exb *exb) {
    exb_free(exb, module);
}
int handler_init(struct exb *exb, struct exb_server *server, char *module_args, struct exb_http_server_module **module_out) {
    (void) module_args;
    struct hello_world_module *mod = exb_malloc(exb, sizeof(struct hello_world_module));
    if (!mod)
        return EXB_NOMEM_ERR;
    mod->exb_ref = exb;
    
    mod->head.destroy = destroy_module;
    mod->count = 0;
    
    if (exb_server_set_module_request_handler(server, (struct exb_http_server_module*)mod, handle_request) != EXB_OK) {
        destroy_module((struct exb_http_server_module*)mod, exb);
        return EXB_MODULE_LOAD_ERROR;
    }
    *module_out = (struct exb_http_server_module*)mod;
    return EXB_OK;
}
