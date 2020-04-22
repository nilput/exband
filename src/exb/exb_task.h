#ifndef EXB_TASK_H
#define EXB_TASK_H
#include "exb_errors.h"
#include "exb_msg.h"
struct exb_thread;
struct exb_task {
    struct exb_error err;
    struct exb_msg msg;
    void (*run)(struct exb_thread *thread, struct exb_task *task);
};

#endif