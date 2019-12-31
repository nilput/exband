#include "utils.h"
#include <unistd.h>
int cpb_sleep(int ms) {
    return usleep(ms * 1000);
}
