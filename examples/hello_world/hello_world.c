#include "cpb.h"
#include "http/http_handler_module.h"
#include "http/http_request.h"

struct hello_world_module {
    struct cpb_http_handler_module head;
    struct cpb *cpb_ref;
    int (*handle_request)(struct cpb_http_handler_module *module, struct cpb_request_state *rqstate, int reason);
    void (*destroy)(struct cpb_http_handler_module *module, struct cpb *cpb);
    int count;
};
static int handle_request(struct cpb_http_handler_module *module, struct cpb_request_state *rqstate, int reason) {
    struct hello_world_module *mod = (struct hello_world_module *) module;

    struct cpb_str path;
    //cpb_request_repr(rqstate);

    cpb_str_init(mod->cpb_ref, &path);
    cpb_str_slice_to_copied_str(mod->cpb_ref, rqstate->path_s, rqstate->input_buffer, &path);
    if (cpb_str_streqc(mod->cpb_ref, &path, "/post") || cpb_str_streqc(mod->cpb_ref, &path, "/post/")) {
        struct cpb_str key,value;
        cpb_str_init_const_str(&key, "Content-Type");
        cpb_str_init_const_str(&value, "text/html");
        cpb_response_append_body_cstr(&rqstate->resp, "<!DOCTYPE html>");
        cpb_response_append_body_cstr(&rqstate->resp, "<html>"
                                                      "<head>"
                                                      "    <title>CPBIN!</title>"
                                                      "</head>"
                                                      "<body>");
        if (cpb_request_has_body(rqstate)) {
            if (reason == CPB_HTTP_HANDLER_HEADERS) {
                rqstate->body_handling = CPB_HTTP_B_BUFFER;
            }
            else {
                cpb_response_append_body_cstr(&rqstate->resp, "You posted: <br>");
                cpb_response_append_body_cstr(&rqstate->resp, "<p>");

                /*XSS!*/
                if (rqstate->body_decoded.str)
                    cpb_response_append_body_cstr(&rqstate->resp, rqstate->body_decoded.str);

                cpb_response_append_body_cstr(&rqstate->resp, "</p>");
                cpb_response_append_body_cstr(&rqstate->resp, "</body>"
                                                              "</html>");
                cpb_response_end(&rqstate->resp);
            }
        }
        else {
            cpb_response_set_header(&rqstate->resp, &key, &value);
            cpb_response_append_body_cstr(&rqstate->resp, "<p> Post something! </p> <br>");
            cpb_response_append_body_cstr(&rqstate->resp, "<form action=\"\" method=\"POST\">"
                                                          "Text: <input type=\"text\" name=\"text\"><br>"
                                                          "</form>");
            cpb_response_append_body_cstr(&rqstate->resp, "</body>"
                                                          "</html>");
            cpb_response_end(&rqstate->resp);
        }
        
    }
    else if (cpb_str_startswithc(mod->cpb_ref, &path, "/count")) 
    {
        struct cpb_str key,value, tmp;
        cpb_str_init_const_str(&key, "Content-Type");
        cpb_str_init_const_str(&value, "text/html");
        cpb_str_init(mod->cpb_ref, &tmp);
        cpb_sprintf(mod->cpb_ref, &tmp, "<!DOCTYPE html>"
                                        "<html>"
                                        "<head>"
                                        "    <title>CPBIN!</title>"
                                        "</head>"
                                        "<body>"
                                        "<p>Count : <b>%d</b></p><br>"
                                        "<form action=\"/reset/\"><button>reset</button></form>"
                                        "</body>"
                                        "</html>", mod->count++);
        cpb_response_append_body_cstr(&rqstate->resp, tmp.str);
        cpb_str_deinit(mod->cpb_ref, &tmp);
        cpb_response_end(&rqstate->resp);
    }
    else if (cpb_str_streqc(mod->cpb_ref, &path, "/reset") || cpb_str_startswithc(mod->cpb_ref, &path, "/reset/")) {
        mod->count = 0;
        cpb_response_redirect_and_end(&rqstate->resp, 307, "/count/");
    }
    else {
        struct cpb_str key,value;
        cpb_str_init_const_str(&key, "Content-Type");
        cpb_str_init_const_str(&value, "text/plain");
        cpb_response_set_header(&rqstate->resp, &key, &value);
        cpb_response_append_body(&rqstate->resp, "Hello World!\r\n", 14);
        struct cpb_str str;
        
        cpb_str_init(mod->cpb_ref, &str);
        
        cpb_sprintf(mod->cpb_ref, &str, "Requested URL: '%s'", path.str);
        
        cpb_response_append_body(&rqstate->resp, str.str, str.len);
        cpb_str_deinit(mod->cpb_ref, &str);
        cpb_response_end(&rqstate->resp);
    }
    
    cpb_str_deinit(mod->cpb_ref, &path);   
}
static int destroy_module(struct cpb_http_handler_module *module, struct cpb *cpb) {
    cpb_free(cpb, module);
}
int handler_init(struct cpb *cpb, struct cpb_server *server, struct cpb_http_handler_module **module_out) {
    struct hello_world_module *mod = cpb_malloc(cpb, sizeof(struct hello_world_module));
    if (!mod)
        return CPB_NOMEM_ERR;
    mod->cpb_ref = cpb;
    mod->head.handle_request = handle_request;
    mod->head.destroy = destroy_module;
    mod->count = 0;
    *module_out = (struct cpb_http_handler_module*)mod;
    return CPB_OK;
}