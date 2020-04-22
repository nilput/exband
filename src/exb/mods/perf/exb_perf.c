#include "../../cpb.h"
#include "../../http/http_server_module.h"
#include "../../http/http_request.h"
#include "../../http/http_server.h"
#include "cpb_event_perf_gen.h"
#include "cpb_event_perf_act.h"
#include <string.h>
#include <signal.h>

#include "../../http/http_server_events.h"
#include "../../http/http_response.h"
#include "../../http/http_parse.h"

struct perf_module {
    struct cpb_http_server_module head;
    struct cpb *cpb_ref;
    struct cpb_perf_gen gen;
    int count;
    struct cpb_http_server *server;
};

static void destroy_module(struct cpb_http_server_module *module, struct cpb *cpb) {
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


#define SAMPLE_MAX 2048
struct {
    char buff[SAMPLE_MAX];
    size_t len;
} samples[8];
int nsamples;
int read_samples() {
    char name[64] = "./src/cpb/mods/perf/samples/sample_X.txt";
    char *digit = strchr(name, 'X');
    for (int i=1; i<=9; i++) {
        *digit = '0' + i;
        FILE *f = fopen(name, "rb");
        if (!f)
            continue;
        size_t cap = SAMPLE_MAX - 1;
        size_t rd = 0;
        while (cap > 0 && (rd = fread(samples[nsamples].buff + samples[nsamples].len, 1, cap, f)) != 0) {
            cap -= rd;
            samples[nsamples].len += rd;
        }
        fclose(f);
        nsamples++;
    }
    return 0;
}

static void parse_test(struct perf_module *mod) {
    struct cpb_eloop *eloop = cpb_server_get_any_eloop(mod->server);
    read_samples();
    if (nsamples == 0) {
        printf("Found no request samples\n");
        return;
    }
    int max = 4000000;
    for (int i=0; i<max; i++) {
        struct cpb_request_state *rqstate = cpb_server_new_rqstate(mod->server, eloop, 0);
        rqstate->istate = CPB_HTTP_I_ST_WAITING_FOR_HEADERS;
        int idx = i / (max / nsamples);
        memcpy(rqstate->input_buffer, samples[idx].buff, samples[idx].len);
        rqstate->input_buffer_len = samples[idx].len;
        struct cpb_error err = cpb_make_error(1);
        if (cpb_str_has_crlfcrlf(rqstate->input_buffer, 0, rqstate->input_buffer_len)) {
            err = cpb_request_http_parse(rqstate);
        }
        
        if (err.error_code != 0) {
            printf("request parsing failed\n");
        }
        else {
            if (i % (max / 10) == 0)
                printf("request method: %d\n", rqstate->method);
        }
        cpb_server_destroy_rqstate(mod->server, eloop, rqstate);
    }
}

static void itoa_test(int use_printf) {
    char buff[64];
    unsigned magic = 0;
    srand(2312);
    int num = random();
    for (int i=0; i<10000000; i++) {
        if (use_printf) {
            snprintf(buff, 64, "%d", num);
        }
        else {
            int n;
            cpb_itoa(buff, 64, &n, num);
        }
        magic += buff[0];
    }
    printf("magic: %u\n", magic);
}

static void handle_args(struct perf_module *mod, char *module_args) {
    char *tok = module_args;
    char buff[64];
    while (split(buff, 64, &tok)) {
        printf("'%s'\n", buff);
        if (strcmp(buff, "--events") == 0) {
            cpb_perf_gen_init(&mod->gen, cpb_server_get_any_eloop(mod->server), 4000000);
            cpb_assert_h(mod->gen.eloop, "");
            cpb_perf_gen_begin(&mod->gen);
        }
        else if (strcmp(buff, "--parse") == 0) {
            parse_test(mod);
            raise(SIGTERM);
        }
        else if (strcmp(buff, "--itoa-printf") == 0) {
            itoa_test(1);
            raise(SIGTERM);
        }
        else if (strcmp(buff, "--itoa-cpb") == 0) {
            itoa_test(0);
            raise(SIGTERM);
        }
    }
}
int cpb_perf_init(struct cpb *cpb, struct cpb_server *server, char *module_args, struct cpb_http_server_module **module_out) {
    (void) module_args;
    struct perf_module *mod = cpb_malloc(cpb, sizeof(struct perf_module));
    if (!mod)
        return CPB_NOMEM_ERR;
    mod->cpb_ref = cpb;
    mod->head.destroy = destroy_module;
    mod->count = 0;
    mod->server = server;

    printf("s: %d\n", sizeof(struct cpb_http_multiplexer));
    printf(".mp: %p\n", (void *)&server->mp);

    handle_args(mod, module_args);
    *module_out = (struct cpb_http_server_module*)mod;
    return CPB_OK;
}
