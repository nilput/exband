LDLIBS := -lcmocka -L ../ -lexb 
LDFLAGS := -L. -Wl,-rpath=\$$ORIGIN/ 
CFLAGS := -Wall -Wno-pointer-sign
debug: CFLAGS += -g3 -O0
debug: all
all: test_decode test_recycle
