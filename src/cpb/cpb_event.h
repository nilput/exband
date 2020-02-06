#ifndef CPB_EVENT_H
#define CPB_EVENT_H
#include "cpb_msg.h"

struct cpb_event {
    void (*handle)(struct cpb_event event);
    struct cpb_msg msg;
};
#endif // CPB_EVENT_H