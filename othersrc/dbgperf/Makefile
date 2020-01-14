.PHONY: all clean
#CFLAGS := -O2 -fpic
CFLAGS := -O2 -Wall -fpic
all: rel
debug: CFLAGS := -O0 -g3 -Wall -fsanitize=address -fpic
debug: libdbgperf.a
rel: libdbgperf.a
libdbgperf.a : dbgperf.o
	ar rcs $@ $^

clean:
	rm *.o *.a 2>/dev/null || true
