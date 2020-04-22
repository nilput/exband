#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include "../cpb_eloop.h"
#include "../cpb_threadpool.h"
#include "../cpb_errors.h"
#include "http_server.h"
#include "http_server_internal.h"
#include "http_server_events.h"
#include "http_server_events_internal.h"
int cpb_response_end(struct cpb_request_state *rqstate) {
    return cpb_response_end_i(rqstate);
}