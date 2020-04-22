#ifndef EXB_EVENT_H
#define EXB_EVENT_H
#include "exb_msg.h"

struct exb_event {
    void (*handle)(struct exb_event event);
    struct exb_msg msg;
};
#endif // EXB_EVENT_H