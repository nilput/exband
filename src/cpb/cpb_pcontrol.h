#ifndef CPB_PCONTROL_H
#define CPB_PCONTROL_H
#include "cpb_config.h"
/*
Manage processes
*/
#include <unistd.h>
#include <sys/wait.h>

struct cpb_pcontrol {
    struct {
        pid_t pid;
    } children[CPB_MAX_PROCESSES];
    int nchildren;

    int config_nprocesses;
    struct {
        void (*hook)(void *data);
        void *data;
    } postfork_hooks[CPB_MAX_HOOKS];
    int npostfork_hooks;
    pid_t master_pid;
    int short_id;
    int stop;
};
int cpb_pcontrol_init(struct cpb_pcontrol *st, int nprocesses);
int cpb_pcontrol_is_single_process(struct cpb_pcontrol *st);
int cpb_pcontrol_is_worker(struct cpb_pcontrol *st);
int cpb_pcontrol_is_master(struct cpb_pcontrol *st);
int cpb_pcontrol_worker_id(struct cpb_pcontrol *st);
int cpb_pcontrol_maintain(struct cpb_pcontrol *st);
int cpb_pcontrol_child_maintain(struct cpb_pcontrol *st);
int cpb_pcontrol_add_postfork_hook(struct cpb_pcontrol *st,    void (*hook)(void *data), void *data);
int cpb_pcontrol_remove_postfork_hook(struct cpb_pcontrol *st, void (*hook)(void *data), void *data);
int cpb_pcontrol_stop(struct cpb_pcontrol *st);
struct cpb_eloop_env;
int cpb_pcontrol_child_setup(struct cpb_pcontrol *st, struct cpb_eloop_env *elist);
//boolean
int cpb_pcontrol_running(struct cpb_pcontrol *st);

int cpb_pcontrol_deinit(struct cpb_pcontrol *st);
#endif
