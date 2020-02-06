#include "cpb/cpb.h"
#include "cpb/http/http_server_module.h"
#include "cpb/http/http_request.h"
#include "cpb/http/http_decode.h"
#include <sqlite3.h
struct pastebin_module {
    struct cpb_http_server_module head;
    struct cpb *cpb_ref;
    int count;
    sqlite3 *db;
    struct cpb_str site_link;
};

static void form(struct cpb_request_state *rqstate) {
    struct cpb_str key,value;
    cpb_str_init_const_str(&key, "Content-Type");
    cpb_str_init_const_str(&value, "text/html");
    cpb_response_append_body_cstr(rqstate, "<!DOCTYPE html>");
    cpb_response_append_body_cstr(rqstate, "<html>"
                                                    "<head>"
                                                    "    <title>CPBIN!</title>"
                                                    "<style> *{margin: auto; margin-top: 10px; text-align: center; font-family: monospace; padding: 10px;}\nbody{max-width: 960px;} textarea{ text-align: left; padding: 0;}</style>"
                                                    "</head>"
                                                    "<body>");
    cpb_response_set_header(rqstate, &key, &value);
    cpb_response_append_body_cstr(rqstate, "<p>POWERED BY CPBIN HTTP SERVER</p> <br>");
    cpb_response_append_body_cstr(rqstate, "<form action=\"/\" method=\"POST\">"
                                                    "<textarea name=\"f\" rows=\"20\" cols=\"80\" required=""></textarea> <br>"
                                                    "<button> PASTE </button> <br>"
                                                    "</form>");
    cpb_response_append_body_cstr(rqstate, "</body>"
                                                    "</html>");
    cpb_response_end(rqstate);
}

static void info(struct pastebin_module *mod, struct cpb_request_state *rqstate) {
    struct cpb_str key,value;
    cpb_str_init_const_str(&key, "Content-Type");
    cpb_str_init_const_str(&value, "text/plain");
    cpb_response_set_header(rqstate, &key, &value);
    struct cpb_str str;
    cpb_str_init(mod->cpb_ref, &str);
    cpb_sprintf(mod->cpb_ref, &str, "curl -F 'f=<-' %s", mod->site_link.str);
    cpb_response_append_body(rqstate, str.str, str.len);
    cpb_str_deinit(mod->cpb_ref, &str);
    cpb_response_end(rqstate);
}

void randkey(char *out, int len) {
    for (int i=0; i<len; ) {
        out[i] = random() % 0xFF;
        if ((out[i] >= 'a' && out[i] <= 'z') || (out[i] >= 'A' && out[i] <= 'Z'))
            i++;
    }
}


static void destroy_module(struct cpb_http_server_module *module, struct cpb *cpb) {
    struct pastebin_module *mod = (struct pastebin_module *)module;
    sqlite3_close(mod->db);
    cpb_str_deinit(cpb, &mod->site_link);
    cpb_free(cpb, module);
    return CPB_OK;
}

struct sql_value {
    char *value;
    int len;
    int value_type; /*SQLITE_BLOB or SQLITE_TEXT*/
}
/*after the results are used call sqlite3_finalize(*destroy_handle)*/
static int exec_sql_returning_one(sqlite3 *db,
                                    char *stmt_sql,
                                    struct sql_value *params,
                                    int nparams,
                                    struct sql_value *results_out,
                                    int max_results,
                                    int *n_results,
                                    sqlite3_stmt **destroy_handle) 
{
    sqlite3_stmt *stmt = NULL;
    const char *unused_tail;
    *destroy_handle = NULL;
    if (n_results)
        *n_results = 0;
    if (sqlite3_prepare_v2(db, stmt_sql, -1, &stmt, &unused_tail) != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return 1;
    }
    
    int rc = SQLITE_OK;
    
    for (int i=0; i<nparams; i++) {
        if (params[i].value_type == SQLITE_BLOB) {
            rc = sqlite3_bind_blob(stmt, i+1, params[i].value, params[i].len, SQLITE_TRANSIENT);
            if (rc != SQLITE_OK) {
                sqlite3_finalize(stmt);
                return SQLITE_ERROR;
            }
        }
        else if (params[i].value_type == SQLITE_TEXT) {
            rc = sqlite3_bind_text(stmt, i+1, params[i].value, params[i].len, SQLITE_TRANSIENT);
            if (rc != SQLITE_OK) {
                sqlite3_finalize(stmt);
                return SQLITE_ERROR;
            }
        }
        else {
            sqlite3_finalize(stmt);
            return SQLITE_ERROR;
        }
    }
    
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        int count = sqlite3_column_count(stmt);
        if (!n_results) {
            sqlite3_finalize(stmt);        
            return SQLITE_ERROR;
        }
        *n_results = count;
        if (count > max_results) {
            sqlite3_finalize(stmt);        
            return SQLITE_TOOBIG;
        }
        for (int i=0; i<count; i++) {
            switch (sqlite3_column_type(stmt, i)) {
                case SQLITE_BLOB: {
                    void *r = sqlite3_column_blob(stmt, i);
                    results_out[i].value = r;
                    results_out[i].len = sqlite3_column_bytes(stmt, i);
                    results_out[i].value_type = SQLITE_BLOB;
                }
                break;
                case SQLITE_TEXT: {
                    results_out[i].value = sqlite3_column_text(stmt, i);
                    results_out[i].len = sqlite3_column_bytes(stmt, i);
                    results_out[i].value_type = SQLITE_TEXT;
                }
                break;
                default:{
                    sqlite3_finalize(stmt);
                    return SQLITE_ERROR;
                }
            }
        }
        *destroy_handle = stmt;    
    }
    
    return rc;
}

static int init_db(struct pastebin_module *mod, int reinit) {
    char *table_exists_sql = "SELECT name FROM sqlite_master WHERE type='table' AND name='pastebin'";
    char *table_create_sql =   "CREATE TABLE pastebin ("
                        "  id INTEGER PRIMARY KEY," //aliases rowid and is autoincremented
                        "  key TEXT NOT NULL,"
                        "  value BLOB NOT NULL"
                        ");";
    char *table_drop_sql = "DROP TABLE IF EXISTS pastebin";
    char *index_create_sql = "CREATE UNIQUE INDEX idx_pastebin_key "
                             "ON pastebin(key);";
    char *index_drop_sql = "DROP INDEX IF EXISTS idx_pastebin_key";
    sqlite3_stmt *stmt;
    struct sql_value out[1];
    int ncol = 0;
    int rv = SQLITE_DONE;
    rv = exec_sql_returning_one(mod->db, table_exists_sql, NULL,  0, out, 1, &ncol, &stmt);
    sqlite3_finalize(stmt);
    if (rv == SQLITE_ROW && !reinit) {
        //alrady inititalized
        return 0;
    }
    if (reinit) {
        exec_sql_returning_one(mod->db, table_drop_sql, NULL,  0, out, 1, &ncol, &stmt);
        sqlite3_finalize(stmt);
        exec_sql_returning_one(mod->db, index_drop_sql, NULL,  0, out, 1, &ncol, &stmt);
        sqlite3_finalize(stmt);
    }
    
    rv = exec_sql_returning_one(mod->db, table_create_sql, NULL,  0, out, 1, &ncol, &stmt);
    sqlite3_finalize(stmt);
    if (rv == SQLITE_DONE) {
        rv = exec_sql_returning_one(mod->db, index_create_sql, NULL,  0, out, 1, &ncol, &stmt);
        sqlite3_finalize(stmt);
        if (rv != SQLITE_DONE) {
            exec_sql_returning_one(mod->db, table_drop_sql, NULL,  0, out, 1, &ncol, &stmt);
            sqlite3_finalize(stmt);
        }
    }
    if (rv != SQLITE_DONE) {
        return 1;
    }
    return 0;
}   
/*allocates a new string*/
/*splits "argname=value argname2=value" string*/
char *module_get_arg(struct pastebin_module *mod, char *mod_args, char *arg_name) {
    char *cursor = mod_args;
    do {
        if (strncmp(cursor, arg_name, strlen(arg_name)) == 0 && cursor[strlen(arg_name)] == '=') {
            cursor = cursor + strlen(arg_name) + 1;
            char *end = strchr(cursor, ' ');
            if (end == NULL)
                end = cursor + strlen(cursor);
            char *val = cpb_malloc(mod->cpb_ref, end - cursor + 1);
            if (val) {
                strncpy(val, cursor, end - cursor);
                val[end - cursor] = 0;
            }
            return val;
        }
    } while ((cursor = strchr(cursor, ' ')) && cursor++);
    return NULL;
}

static struct cpb_str get_post_param(struct cpb *cpb_ref, struct cpb_request_state *rqstate, char *param_name) {
    struct cpb_str str;
    cpb_str_init_empty(&str);
    char *content_type_header = NULL;
    int content_type_header_len = 0;
    if (rqstate->headers.h_content_type_idx != -1) {
        content_type_header = rqstate->input_buffer + rqstate->headers.headers[rqstate->headers.h_content_type_idx].value.index;
        content_type_header_len = rqstate->headers.headers[rqstate->headers.h_content_type_idx].value.len;
    }
    if (content_type_header && cpb_content_type_is(content_type_header, content_type_header_len, "multipart/form-data")) {
        struct cpb_form_parts fp;
        struct cpb_str_slice content_type =  rqstate->headers.headers[rqstate->headers.h_content_type_idx].value;
        if (cpb_decode_multipart(cpb_ref, &fp, rqstate->input_buffer + content_type.index, content_type.len, rqstate->body_decoded.str, rqstate->body_decoded.len ) == CPB_OK) {
            for (int i=0; i<fp.nparts; i++) {
                if (strcmp(fp.buff.str + fp.keys[i].index, "f") == 0) {
                    str = cpb_str_slice_to_const_str((struct cpb_str_slice){fp.values[i].index, fp.values[i].len}, rqstate->body_decoded.str);
                    break;
                }
            }
            cpb_form_parts_deinit(cpb_ref, &fp);
        }
    }
    else if (content_type_header && cpb_content_type_is(content_type_header, content_type_header_len, "application/x-www-form-urlencoded")) {
        struct cpb_form_parts fp;
        if (cpb_urlencode_decode_parts(cpb_ref, &fp, rqstate->body_decoded.str, rqstate->body_decoded.len) == CPB_OK) {
            for (int i=0; i<fp.nparts; i++) {
                if (strcmp(fp.buff.str + fp.keys[i].index, "f") == 0) {
                    cpb_str_strlcpy(cpb_ref, &str, fp.buff.str + fp.values[i].index, fp.values[i].len);
                    break;
                }
            }
            cpb_form_parts_deinit(cpb_ref, &fp);
        }
    }
    else {
        str = cpb_str_const_view(&rqstate->body_decoded);
    }
    return str;
}

static int handle_request(struct cpb_http_server_module *module, struct cpb_request_state *rqstate, int reason) {
    struct pastebin_module *mod = (struct pastebin_module *) module;

    struct cpb_str path;
    //cpb_request_repr(rqstate);

    cpb_str_init(mod->cpb_ref, &path);
    cpb_str_slice_to_copied_str(mod->cpb_ref, rqstate->path_s, rqstate->input_buffer, &path);
    if ((rqstate->method == CPB_HTTP_M_POST) && (path.len == 1 && path.str[0] == '/')) {
        if (!cpb_request_has_body(rqstate)) {
            cpb_assert_h(reason != CPB_HTTP_HANDLER_BODY, "");
            info(mod, rqstate);
        }
        else if (reason == CPB_HTTP_HANDLER_HEADERS) {
            rqstate->body_handling = CPB_HTTP_B_BUFFER;
        }
        else if (reason == CPB_HTTP_HANDLER_BODY) {
            
            char *insert_stmt = "INSERT INTO pastebin(key, value) VALUES (?, ?)";
            char key_buff[5];
            randkey(key_buff, 4);
            key_buff[4] = 0;
            struct cpb_str posted_value = get_post_param(mod->cpb_ref, rqstate, "f");
            struct sql_value values[2] = {{.len = 4, .value=key_buff, .value_type=SQLITE_TEXT},
                                          {.len = posted_value.len, .value=posted_value.str, .value_type=SQLITE_BLOB}};
            struct sql_value out[1];
            int nresults = 0;
            sqlite3_stmt *stmt = NULL;
            int rc = exec_sql_returning_one(mod->db, insert_stmt, values, 2, out, 1, &nresults, &stmt);
            sqlite3_finalize(stmt);
            if (rc == SQLITE_DONE) {
                cpb_response_append_body_cstr(rqstate, mod->site_link.str);
                cpb_response_append_body_cstr(rqstate, "/");
                cpb_response_append_body_cstr(rqstate, key_buff);
                cpb_response_append_body_cstr(rqstate, "\r\n");
                cpb_response_end(rqstate);
            }
            else {
                cpb_response_set_status_code(rqstate, 500);
                cpb_response_append_body_cstr(rqstate, "Error!...");
                cpb_response_end(rqstate);
            }
            cpb_str_deinit(mod->cpb_ref, &posted_value);
            
        }
        else {
            cpb_assert_h(0, "unknown reason");
        }
        
    }
    else if (rqstate->method == CPB_HTTP_M_GET) {
        if (cpb_str_streqc(mod->cpb_ref, &path, "/post") || cpb_str_startswithc(mod->cpb_ref, &path, "/post/")) {
            form(rqstate);
        }
        else if (path.len > 1) {
            char *key_begin = path.str + 1;
            int key_len = (path.str + path.len) - key_begin;
            if (strchr(key_begin, '/')) {
                key_len = strchr(key_begin, '/') - key_begin;
            }
            char *select_stmt = "SELECT value FROM pastebin WHERE key = ?";
            struct sql_value key[1] = {{.len = key_len, .value=key_begin, .value_type=SQLITE_TEXT}};
            struct sql_value value[1];
            int nresults = 0;
            sqlite3_stmt *stmt = NULL;
            int rc = exec_sql_returning_one(mod->db, select_stmt, key, 1, value, 1, &nresults, &stmt);
            if (rc == SQLITE_DONE) {
                cpb_response_set_status_code(rqstate, 404);
            }
            else if (rc == SQLITE_ROW) {
                cpb_assert_h(nresults == 1, "unexpected no. results");
                if (nresults != 1 || (value[0].len && !value[0].value) || value[0].value_type != SQLITE_BLOB) {
                    cpb_response_set_status_code(rqstate, 500);
                    cpb_response_append_body_cstr(rqstate, "Error!...");
                }
                cpb_response_append_body(rqstate, value[0].value, value[0].len);
            }
            else {
                cpb_response_set_status_code(rqstate, 500);
                cpb_response_append_body_cstr(rqstate, "Error!");
            }
            sqlite3_finalize(stmt);
            cpb_response_end(rqstate);
        }
        else {
            info(mod, rqstate);
        }
    }
    else {
        info(mod, rqstate);
    }
    
    cpb_str_deinit(mod->cpb_ref, &path);   
    return CPB_OK;
}

int handler_init(struct cpb *cpb, struct cpb_server *server, char *module_args, struct cpb_http_server_module **module_out) {

    srand(time(NULL));

    struct pastebin_module *mod = cpb_malloc(cpb, sizeof(struct pastebin_module));
    if (!mod)
        return CPB_NOMEM_ERR;
    mod->cpb_ref = cpb;
    
    mod->head.destroy = destroy_module;
    mod->count = 0;
    mod->db = NULL;

    cpb_str_init_const_str(&mod->site_link, "http://127.0.0.1");
    char *site_link = module_get_arg(mod, module_args, "site_link");
    if (site_link) {
        cpb_str_strcpy(mod->cpb_ref, &mod->site_link, site_link);
        cpb_free(cpb, site_link);
    }

    char *db_path = module_get_arg(mod, module_args, "db_path");
    if (!db_path) {
        fprintf(stderr, "expected argument db_path=<path> in module args\n");
        cpb_free(cpb, mod);
    }
    int rc = sqlite3_open(db_path, &mod->db);
    if ( rc ){
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(mod->db));
        sqlite3_close(mod->db);
        cpb_free(cpb, mod);
        cpb_free(cpb, db_path);
        return 1;
    }
    cpb_free(cpb, db_path);

    char *reinit_db = module_get_arg(mod, module_args, "db_reinit");
    int do_reinit_db = reinit_db && atoi(reinit_db) != 0;
    cpb_free(cpb, reinit_db);
    if (init_db(mod, do_reinit_db) != 0) {
        fprintf(stderr, "Can't init db\n");
        sqlite3_close(mod->db);
        cpb_free(cpb, mod);
        return 1;
    }

    if (cpb_server_set_module_request_handler(server, (struct cpb_http_server_module*)mod, handle_request) != CPB_OK) {
        destroy_module((struct cpb_http_server_module*)mod, cpb);
        return CPB_MODULE_LOAD_ERROR;
    }
    *module_out = (struct cpb_http_server_module*)mod;
    return CPB_OK;
}
