#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include "../exb_eloop.h"
#include "../exb_threadpool.h"
#include "../exb_errors.h"
#include "http_server.h"
#include "http_server_internal.h"
#include "http_server_events.h"
#include "http_server_events_internal.h"
int exb_response_end(struct exb_request_state *rqstate) {
    return exb_response_end_i(rqstate);
}