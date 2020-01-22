#ifndef CPB_EVENT_H
#define CPB_EVENT_H
#include "cpb_msg.h"

struct cpb_event_handler_itable;
struct cpb_event {
    struct cpb_event_handler_itable *itable;
    struct cpb_msg msg;
};
struct cpb_event_handler_itable {
    void (*handle)(struct cpb_event event);
    void (*destroy)(struct cpb_event event);
};


#endif // CPB_EVENT_H