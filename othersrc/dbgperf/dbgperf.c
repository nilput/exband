#include "dbgperf.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h> //timer works on linux
#include <time.h> //
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <assert.h>
#include <pthread.h>
#include <limits.h>

//this is a mess, i don't know why it works, if it does
//known bugs: DONT CALL DUMP THEN USE AGAIN, CALL DUMP THEN LEAVE! DONT CME BACK

__thread void *dp_data = NULL;

static int dp_tid;
static struct dp_list *dp_thread_list = NULL;

pthread_mutex_t dp_out_mut = PTHREAD_MUTEX_INITIALIZER;

//each thread data gets registered here
struct dp_info;
struct dp_list {
	struct dp_info *dp;
	struct dp_list *next;
};

static void dp_die(const char *s){
	fputs(s, stderr);
	exit(1);
}


struct timer_info{
    struct timespec tstart;
    struct timespec tend;
};

struct dp_event{
	const char *event;
	struct dp_event *next; //relevant for hash table
	struct timer_info tm;
	size_t ncalls;
	double total_time;
	double minus_time;
    int nthreads; //number of threads this occured in (for accumulate)
	bool filled : 1;
	bool dead : 1;
};
struct dp_info {
	//stack
	struct dp_event *ev;
	int ev_len;
	int ev_cap;

	//hashtable
	struct dp_event *ev_set;
	int ev_set_len;
	int ev_set_cap;

	//stats
	int ev_len_max;
	int ev_set_len_max;

	int tid;
	struct timer_info tinit;
	char *log_buff;
	int log_len;
	int log_cap;

};

static void timer_begin(struct timer_info *timer){
	memset(timer, 0, sizeof(struct timer_info));
    clock_gettime(CLOCK_MONOTONIC, &timer->tstart);
}
static void timer_end(struct timer_info *timer){
    clock_gettime(CLOCK_MONOTONIC, &timer->tend);
}
static double timerdt_now(struct timer_info *timer){
    struct timespec tend; 
    clock_gettime(CLOCK_MONOTONIC, &tend);
    return ((double)tend.tv_sec          + 1.0e-9 * tend.tv_nsec) - 
           ((double)timer->tstart.tv_sec + 1.0e-9 * timer->tstart.tv_nsec);
}
static double timerdt(struct timer_info *timer){
    return ((double)timer->tend.tv_sec   + 1.0e-9 * timer->tend.tv_nsec) - 
           ((double)timer->tstart.tv_sec + 1.0e-9 * timer->tstart.tv_nsec);
}

static void timer_fprint(FILE *file, struct timer_info *timer)
{
    fprintf(stderr,"time: %.5f\n", timerdt(timer));
}


static struct dp_info *dp_init(void){
	struct dp_info *dp = (struct dp_info *) dp_data;
	if (!dp){
		dp_data = malloc(sizeof(struct dp_info));
		if (!dp_data)
			dp_die("dp malloc");
		memset(dp_data, 0, sizeof(struct dp_info));
		dp = (struct dp_info *) dp_data;
		//register_thread
		pthread_mutex_lock(&dp_out_mut);
		{
			timer_begin(&dp->tinit);
			dp->tid = dp_tid++; //global
			struct dp_list *tail = dp_thread_list;
			struct dp_list **link = &dp_thread_list;
			if (tail) {
				while(tail->next)
					tail = tail->next;
				link = &tail->next;
			}
			tail = *link = malloc(sizeof(struct dp_list));
			if (!tail)
				dp_die("dp malloc");
			memset(tail, 0, sizeof(struct dp_list));
			tail->dp = dp;
		}
		pthread_mutex_unlock(&dp_out_mut);
	}
	return (struct dp_info *) dp_data;
}



//source: stackoverflow
static int dp_hash(const char *event_name){
	const unsigned char *str = (const unsigned char *) event_name;
    unsigned long hash = 5381;
    int c;
    while (c = *str){
        hash = ((hash << 5U) + hash) + c; /* hash * 33 + c */
        str++;
    }
    int ihash = hash % (INT_MAX + 1U);
    return ihash;
}

static struct dp_event *dp_hash_add_to(struct dp_info *dp, int idx, struct dp_event *ev){
	struct dp_event *dest = &dp->ev_set[idx];
	assert(ev->event);
	if (!dest->event) // empty bucket
		goto cpy;
	while (dest->next){
		assert(dest->event);
		dest = dest->next;
	}

	dest->next = malloc(sizeof(struct dp_event));
	if (!dest->next)
		dp_die("dp malloc");
	dest = dest->next;

cpy:
	memcpy(dest, ev, sizeof(struct dp_event));
	assert(dest->event);
	return dest;
}

void dp_set_realloc(struct dp_info *dp, int new_cap){
		int old_cap = dp->ev_set_cap;
		dp->ev_set_cap = new_cap;
		dp->ev_set = realloc(dp->ev_set, sizeof(struct dp_event) * new_cap);
		if (!dp->ev_set)
			dp_die("dp realloc");
		assert(new_cap > old_cap); 
		if (new_cap > old_cap){
			memset(dp->ev_set + old_cap, 0, sizeof(struct dp_event) * (new_cap - old_cap));
		}

		int i;
		for (i=0; i<old_cap; i++){
			struct dp_event *ev = &dp->ev_set[i];
			struct dp_event *prev = NULL;
			if (!ev->event) //empty bucket
				continue;
			while (ev) {
				assert(ev->event);
				int new_idx = dp_hash(ev->event) % new_cap;
				if (new_idx == i){
					prev = ev;
					ev = ev->next;
					goto next_cell;
				}
				//otherwise we need to move the current node
				struct dp_event *moved = dp_hash_add_to(dp, new_idx, ev);
				moved->next = NULL;
				if (!prev && !ev->next){
					//base array bucket became free
					memset(ev, 0, sizeof(struct dp_event));
					ev = NULL;
					goto next_bucket;
				}
				else if (!prev && ev->next){
					//base array bucket has a place for the next node, so we dont need the next node's storage
					struct dp_event *tmp = ev->next;
					memcpy(ev, tmp, sizeof(struct dp_event)); //remains valid
					free(tmp);
					//ev = ev
				}
				else {
					//connect prev to next, skipping and freeing current
					prev->next = ev->next;
					free(ev);
					ev = prev->next;
					
				}
				next_cell:	;
			}
next_bucket:	;

		}
}

static struct dp_event *dp_get_or_insert(struct dp_info *dp, const char *event_name)
{	
	assert(event_name);
	if (!dp->ev_set){
		dp->ev_set = malloc(sizeof(struct dp_event));
		if (!dp->ev_set)
			dp_die("dp malloc");
		memset(dp->ev_set, 0, sizeof(struct dp_event));
		dp->ev_set_cap = 1;
	}
	else if (dp->ev_set_cap < (dp->ev_set_len * 2)){
		dp_set_realloc(dp, dp->ev_set_cap * 2);
	}
	int idx = dp_hash(event_name) % dp->ev_set_cap;
	struct dp_event *ev = dp->ev_set + idx;
	if (ev->event) //if not empty bucekt
		while (ev){
			assert(ev->event);
			if (strcmp(event_name, ev->event) == 0)
				return ev;
			ev = ev->next;
		}
	//fprintf(stderr, "event: %s\n", event_name);
	//nothing found so add new
	struct dp_event event;
	memset(&event, 0, sizeof(struct dp_event));
	event.event = event_name;
	dp->ev_set_len++;
	//stats
	dp->ev_set_len_max = dp->ev_set_len > dp->ev_set_len_max ? dp->ev_set_len : dp->ev_set_len_max;
	ev = dp_hash_add_to(dp, idx, &event);

	assert(ev && ev->event);
	return ev;
}

static struct dp_event *dp_get_ev_by_idx(int idx, struct dp_info *dp){
	int cur = 0;
	int i;
	for (i=0; i<dp->ev_set_cap; i++){
		struct dp_event *ev = dp->ev_set + i;
		if (!ev->event)
			continue;
		while (ev) {
			if (cur == idx){
				return ev;
			}
			ev = ev->next;
			cur++;
		}
	}
	return NULL;
}

static int in_keys(const char **keys, const char *key){
	while(keys && *keys){
		if (strcmp(*keys, key) == 0)
			return 1;
		keys++;
	}
	return 0;
}
static void insert_key(const char ***keys_p, const char *key){
	const char **keys = *keys_p;
	if (!keys){
		*keys_p = malloc(sizeof(char *) * 1);
		if (!*keys_p)
			dp_die("dp_malloc");
		keys = *keys_p;
		keys[0] = NULL;
	}
	if (key){
		int nkeys = 0;
		while(*keys){
			nkeys++;
			keys++;
		}
		*keys_p = realloc(*keys_p, sizeof(char *) * (nkeys + 2)); //1 for null, 1 for new key
		if (!*keys_p)
			dp_die("dp_realloc");
		keys = *keys_p;
		keys[nkeys] = key;
		keys[nkeys+1] = NULL;
	}
}

void dp_timed_logv(const char *fmt, va_list ap) {
	dp_register_event("dp_timed_log");
	va_list aq;
	
	struct dp_info *dp = dp_init();	
	if (!dp->log_buff){
		dp->log_cap = 256;
		dp->log_buff = malloc(dp->log_cap);
		if (!dp->log_buff)
				dp_die("dp_realloc");
	}
	int left = 0;
	int len_a[2] = {1, 1};
	while ((len_a[0] + len_a[1]) >= left){
		va_copy(aq, ap);
		left = dp->log_cap - dp->log_len;
		if (left <= (len_a[0] + len_a[1] + 10)){
			dp->log_buff = realloc(dp->log_buff, (dp->log_cap *= 2));
			if (!dp->log_buff)
				dp_die("dp_realloc");
			left = dp->log_cap - dp->log_len;
		}
		len_a[0] = snprintf(dp->log_buff + dp->log_len, left, "\n[%.5f] {%d}: ", timerdt_now(&dp->tinit), dp->tid);
		if (len_a[0] < left){
			len_a[1] = vsnprintf(dp->log_buff + dp->log_len + len_a[0], left - len_a[0], fmt, aq);
		}
		va_end(aq);
	}
	dp->log_len += len_a[0] + len_a[1];
	dp_end_event("dp_timed_log");
}
void dp_timed_log(const char *fmt, ...){
    va_list ap;
    va_start(ap, fmt);
    dp_timed_logv(fmt, ap);
    va_end(ap);
}


//keys must be freed
static void dp_sort_info(struct dp_info *dp, char ***keys_out){
	const char **keys = NULL;
	insert_key(&keys, NULL);

	const char *min_key = NULL;
	double min_val;

	int i;
	struct dp_event *ev;
	while (1){
		for (i=0; (ev = dp_get_ev_by_idx(i, dp)) != NULL; i++){
			if (in_keys(keys, ev->event)){
				continue;
			}
			double exc = ev->total_time - ev->minus_time;
			if (!min_key){
				min_key = ev->event;
				min_val = exc;
			}
			else if (exc < min_val){
				min_key = ev->event;
				min_val = exc;
			}
		}
		if (!min_key)
			break;
		insert_key(&keys, min_key);
		min_key = NULL;
	}
	*keys_out = keys;
}

static void dp_dump_ev(FILE *f, struct dp_event *ev, struct dp_info *accumulate) {
	double exc =  ev->total_time - ev->minus_time;
	fprintf(f, 	"name: '%s'\n"
				"\tinclusive time:     %f.5\n"
				"\texclusive time: %f.5\n"
				"\tnumber of calls: %zd\n"
				"\tavg inc  per call: %f.5\n"
				"\tavg exc. per call: %f.5\n", ev->event, ev->total_time, exc,
										   ev->ncalls, ev->total_time / ev->ncalls, exc / ev->ncalls);
	if (accumulate) {
		struct dp_event *acc_ev = dp_get_or_insert(accumulate, ev->event);
		acc_ev->total_time += ev->total_time;
		acc_ev->minus_time += ev->minus_time;
		acc_ev->ncalls += ev->ncalls;
        acc_ev->nthreads++;
	}
    else {
        //we are printing accumulated info
        assert(ev->nthreads > 0);
        fprintf(f, "\tnthreads:          %d\n"
                   "\tavg inc. per thread:  %f.5\n" 
                   "\tavg exc. per thread:  %f.5\n", ev->nthreads, ev->total_time / (float)ev->nthreads, exc / (float)ev->nthreads);
    }
    fprintf(f, "\n");
}

static void dp_dump_info(FILE *f, struct dp_info *dp, struct dp_info *accumulate){
	int i;
	fprintf(f, "\nTHREAD STATS\n");
	for (i=0; i<dp->ev_len; i++){
		assert(dp->ev[i].filled);
		fprintf(f, "\t%s\t%.5f\n", dp->ev[i].event, timerdt(&dp->ev[i].tm));
	}
	//stats
	fprintf(f,
			"info at dump:\n"
			"ev_len: %d\n"
			"ev_len_max: %d\n"
			"ev_set_len: %d\n"
			"ev_set_len_max: %d\n", dp->ev_len, dp->ev_len_max, dp->ev_set_len, dp->ev_set_len_max);

	for (i=0; i<dp->ev_set_cap; i++){
		struct dp_event *ev = dp->ev_set + i;
		if (!ev->event)
			continue;
		while (ev) {
			assert(ev->event);
			dp_dump_ev(f, ev, accumulate);
			ev = ev->next;
		}
	}
}

static void dp_destroy_dp_info(struct dp_info *dp){
	int i;
	free(dp->ev);
	for (i=0; i<dp->ev_set_len; i++){
		struct dp_event *tail = dp->ev_set[i].next;
		while (tail){
			struct dp_event *tmp = tail;
			tail = tail->next;
			free(tmp);
		}
	}
	free(dp->ev_set);
	if (dp->log_buff)
		free(dp->log_buff);
	memset(dp, 0, sizeof(struct dp_info));
}

void dp_clear(void) {
	remove("dplog.txt");
}
//called at end of main
void dp_dump() {
	if (!dp_thread_list)
		return;
	//accumulate all threads
	struct dp_info acc;
	memset(&acc, 0, sizeof(struct dp_info));

	pthread_mutex_lock(&dp_out_mut);
	FILE *f = fopen("dplog.txt", "a");
	if (!f)
		dp_die("cant open log file");
	
	struct dp_list *tail = dp_thread_list;
	while (tail){
		struct dp_info *dp = (struct dp_info *) tail->dp;

		if (dp->log_buff){
			fwrite(dp->log_buff, 1, dp->log_len, f);
		}

		dp_dump_info(f, dp, &acc);
		dp_destroy_dp_info(dp);
		memset(dp, 0, sizeof(struct dp_info));
		free(dp); //THREAD BEOCMES INVALID
		struct dp_list *prev = tail;
		tail = tail->next;
		free(prev);
	}
	
	dp_thread_list = NULL;
	fprintf(f, "\nAccumulated from all threads:\n");
	
	//dp_dump_info(f, &acc, NULL);
	char **keys;
	dp_sort_info(&acc, &keys);
	assert(keys);
	for (int i=0; keys[i] != NULL; i++){
		dp_dump_ev(f, dp_get_or_insert(&acc, keys[i]), NULL);
	}
	free(keys);

	dp_destroy_dp_info(&acc);

	fclose(f);

	pthread_mutex_unlock(&dp_out_mut);
}

void dp_register_event(const char *event) {
	struct dp_info *dp = dp_init();
	//fprintf(stderr, "REG %s\n", event);
	assert(event);
	if (!dp->ev){
		assert(!dp->ev_len);
		dp->ev = malloc(sizeof(struct dp_event));
		if (!dp->ev)
			dp_die("dp malloc");
		dp->ev_cap = 1;
	}
	else if (dp->ev_len == dp->ev_cap){
		dp->ev = realloc(dp->ev, sizeof(struct dp_event) * (dp->ev_cap *= 2));
		if (!dp->ev)
			dp_die("dp realloc");
	}
	memset(&dp->ev[dp->ev_len], 0, sizeof(struct dp_event));
	timer_begin(&dp->ev[dp->ev_len].tm);
	dp->ev[dp->ev_len].event = event;
	dp->ev[dp->ev_len].filled = false;
	dp->ev_len++;

	//stats
	dp->ev_len_max = dp->ev_len > dp->ev_len_max ? dp->ev_len : dp->ev_len_max;
}
void dp_end_event(const char *event) {
	struct dp_info *dp = dp_init();
	assert(dp_data);
	assert(event);
	//fprintf(stderr, "UNREG %s\n", event);

	if (dp->ev_len == 0) {
		fprintf(stderr, "dbgperf Warning: Tried to end Event '%s' but it wasnt registered on the stack\n", event);
		return;
	}
	while (strcmp(event, dp->ev[dp->ev_len - 1].event) != 0) {
		dp->ev_len--;
		fprintf(stderr, "dbgperf Warning: Event '%s' was not terminated\n", dp->ev[dp->ev_len].event);
		if (dp->ev_len == 0) {
			fprintf(stderr, "dbgperf Warning: Tried to end Event '%s' but it wasnt registered on the stack\n", event);
			return;
		}
	}

	timer_end(&dp->ev[dp->ev_len - 1].tm);
	dp->ev[dp->ev_len - 1].filled = true;
	struct dp_event *ev;
	if (event[0] == '-'){
		ev = dp_get_or_insert(dp, event+1);
		assert(ev && ev->event);
		ev->minus_time += timerdt(&dp->ev[dp->ev_len - 1].tm);
	}
	else {
		ev = dp_get_or_insert(dp, event);
		assert(ev && ev->event);
		ev->ncalls++;
		ev->total_time += timerdt(&dp->ev[dp->ev_len - 1].tm);
	}
	
	dp->ev_len--;
}
