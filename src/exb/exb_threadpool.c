#define _GNU_SOURCE
#include "exb_threadpool.h"
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sched.h>
#include <pthread.h>
int exb_hw_cpu_count() {
    int count = sysconf(_SC_NPROCESSORS_ONLN);
    return count <= 0 ? 1 : count;
}
int exb_hw_bind_to_core(int core_id) {
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (core_id < 0 || core_id >= num_cores)
        return EXB_INVALID_ARG_ERR;

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    pthread_t current_thread = pthread_self();    
    if (pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset) != 0) {
        return EXB_THREAD_ERROR;
    }
    return EXB_OK;
}
int exb_hw_bind_not_to_core(int core_id) {
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (core_id < 0 || core_id >= num_cores)
        return EXB_INVALID_ARG_ERR;

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (int i=0; i<num_cores; i++) {
        if (i != core_id) {
            CPU_SET(core_id, &cpuset);
        }
    }

    pthread_t current_thread = pthread_self();    
    if (pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset) != 0) {
        return EXB_THREAD_ERROR;
    }
    return EXB_OK;
}

//NOTE: the use of setpriority for this is a linux specific incompatibility
//because we are not using the realtime scheduling classes
//on other systems this will set the priority of the entire system
int exb_hw_thread_sched_important() {
    errno = 0;
    int prio = getpriority(PRIO_PROCESS, 0);
    if (prio == -1 && errno != 0)
        return EXB_THREAD_ERROR;
    int new_prio = prio;
    setpriority(PRIO_PROCESS, 0, new_prio);
    return EXB_OK;
}
int exb_hw_thread_sched_background() {
    errno = 0;
    int prio = getpriority(PRIO_PROCESS, 0);
    if (prio == -1 && errno != 0)
        return EXB_THREAD_ERROR;
    
    int new_prio = prio + (19 - prio) / 2;
    fprintf(stderr, "bg PRIO: %d -> %d\n", prio, new_prio);
    //int new_prio = 19;
    //int new_prio = prio + 1 > 19 ? 19 : prio + 1;

    setpriority(PRIO_PROCESS, 0, new_prio);
    return EXB_OK;
}