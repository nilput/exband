#ifndef CPB_ASSERT_H
#define CPB_ASSERT_H
#include <assert.h>
static void cpb_assert_h(int cond, const char *msg) {
    assert(cond);
}
static void cpb_assert_s(int cond, const char *msg) {
    assert(cond);
}

#endif// CPB_ASSERT_H