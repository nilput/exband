#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include "cpb_pcontrol.h"
#include "cpb_errors.h"
#include "cpb.h"
#include "cpb_eloop_env.h"

int cpb_pcontrol_init(struct cpb_pcontrol *st, int nprocesses) {
    st->npostfork_hooks = 0;
    st->nchildren = 0;
    st->config_nprocesses = nprocesses;
    st->master_pid = getpid();
    st->stop = 0;
    st->short_id = 0;
    return CPB_OK;
}
int cpb_pcontrol_is_master(struct cpb_pcontrol *st) {
    return st->master_pid == getpid();
}
int cpb_pcontrol_is_worker(struct cpb_pcontrol *st) {
    return st->master_pid != getpid();
}
int cpb_pcontrol_is_single_process(struct cpb_pcontrol *st) {
    return st->config_nprocesses == 1;
}
int cpb_pcontrol_running(struct cpb_pcontrol *st) {
    return !st->stop;
}

//FIXME: this is probably broken
//returns 0 if it didnt timeout
static int waitpid_with_timeout(pid_t pid, pid_t *pid_out, int *state, int timeout_ms)
{
    if ((*pid_out = waitpid(pid, state, WNOHANG)) > 0)
        return 0;
    const int granularity_ms = 50;
    for (int i=0; i < (timeout_ms / granularity_ms + 1); i++) {
        usleep(granularity_ms * 1000);
        if ((*pid_out = waitpid(pid, state, WNOHANG)) > 0)
            return 0;
    }
    return -1;
}
int cpb_pcontrol_worker_id(struct cpb_pcontrol *st) {
    return st->short_id;
}

int cpb_pcontrol_maintain(struct cpb_pcontrol *st) {
    cpb_assert_h(cpb_pcontrol_is_master(st), "");
    if (st->stop)
        return CPB_OK;
    int config_nprocesses = st->config_nprocesses - 1;
    //make sure they're alive
    int some_died = 1;
    while (some_died) {
        some_died = 0;
        for (int i=0; i<st->nchildren; i++) {
            pid_t rpid;
            int state;
            if (waitpid_with_timeout(st->children[i].pid, &rpid, &state, 0) == 0) {
                fprintf(stderr, "Detected child %d died!\n", rpid);
                if (i != st->nchildren - 1) {
                    //popback
                    st->children[i] = st->children[st->nchildren - 1];
                }
                st->nchildren--;
                some_died = 1;
                break;
            }
        }
    }
    //make sure they have been spawned
    for (int i=st->nchildren; i<config_nprocesses; i++) {
        pid_t child_pid = fork();
        if (child_pid == 0) {
            //child
            st->short_id = i;
            for (int j=0; j<st->npostfork_hooks; j++) {
                st->postfork_hooks[j].hook(st->postfork_hooks[j].data);
            }
            return CPB_CHILD;
        }
        else if (child_pid == -1) {
            //error
            return CPB_FORK_ERROR;
        }
        else {
            //parent
            st->children[st->nchildren].pid = child_pid;
            st->nchildren++;
        }
    }
}

int cpb_pcontrol_add_postfork_hook(struct cpb_pcontrol *st,    void (*hook)(void *data), void *data) {
    cpb_assert_h(cpb_pcontrol_is_master(st), "");
    if (st->npostfork_hooks >= CPB_MAX_HOOKS)
        return CPB_OUT_OF_RANGE_ERR;
    st->postfork_hooks[st->npostfork_hooks].hook = hook;
    st->postfork_hooks[st->npostfork_hooks].data = data;
    st->npostfork_hooks++;
    return CPB_OK;
}
int cpb_pcontrol_remove_postfork_hook(struct cpb_pcontrol *st, void (*hook)(void *data), void *data) {
    for (int i=0; i<st->npostfork_hooks; i++) {
        if (st->postfork_hooks[st->npostfork_hooks].hook == hook && 
            st->postfork_hooks[st->npostfork_hooks].data == data) 
        {
            //popback
            if (i < st->npostfork_hooks - 1) {
                st->postfork_hooks[i] = st->postfork_hooks[st->npostfork_hooks - 1];
            }
            st->npostfork_hooks--;
            return CPB_OK;
        }
    }
    return CPB_NOT_FOUND;
}

int cpb_pcontrol_stop(struct cpb_pcontrol *st) {
    if (cpb_pcontrol_is_master(st)) {
        for (int i=0; i<st->nchildren; i++) {
            kill(st->children[i].pid, SIGTERM);
        }
        for (int i=0; i < st->nchildren; i++) {
            pid_t pid = st->children[i].pid;
            pid_t rpid;
            int state;
            if (waitpid_with_timeout(pid, &rpid, &state, 2000) != 0) {
                fprintf(stderr, "Waiting on processes %d timed out\n", pid);
                kill(pid, SIGKILL);
                waitpid_with_timeout(pid, &rpid, &state, 100);
            }
        }
        st->nchildren = 0;
    }
    st->stop = 1;
}


int cpb_pcontrol_deinit(struct cpb_pcontrol *st) {
    st->nchildren = 0;
    st->npostfork_hooks = 0;
}

int cpb_pcontrol_child_maintain(struct cpb_pcontrol *st) {
    cpb_assert_h(cpb_pcontrol_is_worker(st), "");
    if (getppid() != st->master_pid) {
        //parent died
        fprintf(stderr, "Detected parent died!\n");
        raise(SIGTERM);
    }
}
static void cpb_pcontrol_child_event_loop(struct cpb_event ev) {
    struct cpb_pcontrol  *st    = ev.msg.u.pp.argp1;
    struct cpb_eloop_env *elist = ev.msg.u.pp.argp2;
    struct cpb_eloop *eloop = elist->loops[0].loop;
    cpb_assert_h(!!eloop, "");
    cpb_pcontrol_child_maintain(st);
    cpb_eloop_append_delayed(eloop, ev, 200, 0);
}
//add periodic child maintainance events such as checking if parent died
int cpb_pcontrol_child_setup(struct cpb_pcontrol *st, struct cpb_eloop_env *elist) {
    struct cpb_event new_ev = {.handle = cpb_pcontrol_child_event_loop,
                               .msg.u.pp.argp1 = st,
                               .msg.u.pp.argp2 = elist,};
    cpb_pcontrol_child_event_loop(new_ev);
}
