#define _GNU_SOURCE
#include "cpb_threadpool.h"
#include <unistd.h>
#include <sched.h>
#include <pthread.h>
int cpb_hw_cpu_count(int core_id) {
    return sysconf(_SC_NPROCESSORS_ONLN);
}
int cpb_hw_bind_to_core(int core_id) {
   int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
   if (core_id < 0 || core_id >= num_cores)
      return CPB_INVALID_ARG_ERR;

   cpu_set_t cpuset;
   CPU_ZERO(&cpuset);
   CPU_SET(core_id, &cpuset);

   pthread_t current_thread = pthread_self();    
   if (pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset) != 0) {
       return CPB_THREAD_ERROR;
   }
   return CPB_OK;
}