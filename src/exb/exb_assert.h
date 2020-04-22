#ifndef EXB_ASSERT_H
#define EXB_ASSERT_H
#include <assert.h>

#ifdef EXB_NO_ASSERTS
    #define exb_assert_h(c,m)
    #define exb_assert_s(c,m)
#else
    #define EXB_ASSERTS
    static void exb_assert_h(int cond, const char *msg) {
        assert(cond);
    }
    static void exb_assert_s(int cond, const char *msg) {
        assert(cond);
    }
#endif

#endif// EXB_ASSERT_H
