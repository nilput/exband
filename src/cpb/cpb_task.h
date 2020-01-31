#ifndef CPB_TASK_H
#define CPB_TASK_H
#include "cpb_errors.h"
#include "cpb_msg.h"
struct cpb_thread;
struct cpb_task {
    struct cpb_error err;
    struct cpb_msg msg;
    void (*run)(struct cpb_thread *thread, struct cpb_task *task);
};

#endif