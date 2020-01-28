/*
 * Author: Turki Alsaleem (github.com/nilput/)
 */
#ifndef DBG_PERF_H
#define DBG_PERF_H
#ifdef __cplusplus
    #include <cstdarg>
extern "C" {
#else
    #include <stdarg.h>
#endif

#ifndef ENABLE_DBGPERF
#define DISABLE_DBGPERF
#endif

#if defined(DISABLE_DBGPERF)
    #define dp_clear()
    #define dp_register_event(x)
    #define dp_end_event(x)
    #define dp_timed_log(x, ...)
    #define dp_timed_logv(x, ap)
    #define dp_dump()
	#define dp_useless(n)
#else  //(enabled)
	//called at top of main
	void dp_clear(void);
	//this pair works like a stack
	void dp_register_event(const char *event);
	void dp_end_event(const char *event);
	//a log with a time and a tid logged to seperate file
	void dp_timed_log(const char *fmt, ...);
	void dp_timed_logv(const char *fmt, va_list ap);
	void dp_useless(int n);
	//called at end of program to write summary to file
	void dp_dump(void);
#endif

#ifdef __cplusplus
} //extern c
#endif

#endif
