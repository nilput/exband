#ifndef CPB_ASSERT_H
#define CPB_ASSERT_H
#include <assert.h>

#ifdef CPB_NO_ASSERTS
    #define cpb_assert_h(c,m)
    #define cpb_assert_s(c,m)
#else
    #define CPB_ASSERTS
    static void cpb_assert_h(int cond, const char *msg) {
        assert(cond);
    }
    static void cpb_assert_s(int cond, const char *msg) {
        assert(cond);
    }
#endif

#endif// CPB_ASSERT_H
