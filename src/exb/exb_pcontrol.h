#ifndef EXB_PCONTROL_H
#define EXB_PCONTROL_H
#include "exb_build_config.h"
#include "exb_config.h"
/*
Manage processes
*/
#include <unistd.h>
#include <sys/wait.h>

struct exb_pcontrol {
    enum exb_processing_mode omode;
    struct {
        pid_t pid;
    } children[EXB_MAX_PROCESSES];
    int nchildren;

    int config_nprocesses;
    struct {
        void (*hook)(void *data);
        void *data;
    } postfork_hooks[EXB_MAX_HOOKS];
    int npostfork_hooks;
    pid_t master_pid;
    int short_id;
    int stop;
};

int exb_pcontrol_init(struct exb_pcontrol *st, int nprocesses, enum exb_processing_mode omode);
int exb_pcontrol_is_single_process(struct exb_pcontrol *st);
int exb_pcontrol_is_worker(struct exb_pcontrol *st);
int exb_pcontrol_is_master(struct exb_pcontrol *st);
int exb_pcontrol_worker_id(struct exb_pcontrol *st);
int exb_pcontrol_maintain(struct exb_pcontrol *st);
int exb_pcontrol_child_maintain(struct exb_pcontrol *st);
int exb_pcontrol_add_postfork_hook(struct exb_pcontrol *st, void (*hook)(void *data), void *data);
int exb_pcontrol_remove_postfork_hook(struct exb_pcontrol *st, void (*hook)(void *data), void *data);
int exb_pcontrol_stop(struct exb_pcontrol *st);
struct exb_evloop_pool;
int exb_pcontrol_child_setup(struct exb_pcontrol *st, struct exb_evloop_pool *elist);
//boolean
int exb_pcontrol_is_running(struct exb_pcontrol *st);

int exb_pcontrol_deinit(struct exb_pcontrol *st);
#endif
