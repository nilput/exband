#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <stdio.h>
#include <string.h>

#include "cmocka.h"
#include "../../src/cpb/cpb.h"
#include "../../src/cpb/cpb_buffer_recycle_list.h

struct test_state {
    struct cpb cpb;
};

void test_buffer_recycle_list(void **state)
{
    struct test_state *tstate = *state;
    struct cpb *cpb_ref = &tstate->cpb;

    void *buff_a = cpb_malloc(cpb_ref, 500);
    void *buff_b = cpb_malloc(cpb_ref, 500);
    void *buff_c = cpb_malloc(cpb_ref, 256);
    void *buff_d = cpb_malloc(cpb_ref, 1024);
    struct cpb_buffer_recycle_list buff_cyc;
    int rv;
    rv = cpb_buffer_recycle_list_init(cpb_ref, &buff_cyc);
    assert_true(rv == CPB_OK)
    void *buff;
    size_t buff_sz;

    //push and pop using smaller or equal or greater size
    rv = cpb_buffer_recycle_list_push(cpb_ref, &buff_cyc, buff_a, 500);
    assert_true(rv == CPB_OK);
    rv = cpb_buffer_recycle_list_pop(cpb_ref, &buff_cyc, 400, &buff, &buff_sz);
    assert_true(rv == CPB_OK);
    assert_true(buff == buff_a);
    assert_true(buff_sz == 500);
    rv = cpb_buffer_recycle_list_push(cpb_ref, &buff_cyc, buff_a, 500);
    assert_true(rv == CPB_OK);
    rv = cpb_buffer_recycle_list_pop(cpb_ref, &buff_cyc, 500, &buff, &buff_sz);
    assert_true(rv == CPB_OK);
    assert_true(buff == buff_a);
    assert_true(buff_sz == 500);
    rv = cpb_buffer_recycle_list_push(cpb_ref, &buff_cyc, buff_b, 500);
    assert_true(rv == CPB_OK);
    rv = cpb_buffer_recycle_list_pop(cpb_ref, &buff_cyc, 501, &buff, &buff_sz);
    assert_true(rv != CPB_OK);
    rv = cpb_buffer_recycle_list_pop(cpb_ref, &buff_cyc, 500, &buff, &buff_sz);
    assert_true(rv == CPB_OK);
    assert_true(buff == buff_b);

    //add multiple buffers
    rv = cpb_buffer_recycle_list_push(cpb_ref, &buff_cyc, buff_c, 256);
    assert_true(rv == CPB_OK);
    rv = cpb_buffer_recycle_list_push(cpb_ref, &buff_cyc, buff_a, 500);
    assert_true(rv == CPB_OK);
    rv = cpb_buffer_recycle_list_push(cpb_ref, &buff_cyc, buff_b, 500);
    assert_true(rv == CPB_OK);
    rv = cpb_buffer_recycle_list_push(cpb_ref, &buff_cyc, buff_d, 1024);
    assert_true(rv == CPB_OK);

    //state: all added
    rv = cpb_buffer_recycle_list_pop(cpb_ref, &buff_cyc, 501, &buff, &buff_sz);
    if (rv == CPB_OK) {
        assert_ptr_equal(buff, buff_d);
        assert_int_equal(buff_sz, 1024);
    }
    else {
        rv = cpb_buffer_recycle_list_pop(cpb_ref, &buff_cyc, 1024, &buff, &buff_sz);    
        assert_true(rv == CPB_OK);
        assert_ptr_equal(buff, buff_d);
        assert_int_equal(buff_sz, 1024);
    }
    //state: d not added
    rv = cpb_buffer_recycle_list_pop(cpb_ref, &buff_cyc, 501, &buff, &buff_sz);
    assert_true(rv != CPB_OK);
    rv = cpb_buffer_recycle_list_push(cpb_ref, &buff_cyc, buff_d, 1024);
    assert_true(rv == CPB_OK);
    //state: all added
    rv = cpb_buffer_recycle_list_pop(cpb_ref, &buff_cyc, 500, &buff, &buff_sz);
    assert_true(rv == CPB_OK);
    assert_true(buff == buff_a || buff == buff_b || buff == buff_d);
    rv = cpb_buffer_recycle_list_push(cpb_ref, &buff_cyc, buff, buff_sz);
    //state: all added
    
    int seen[4] = {0};
    while ((rv = cpb_buffer_recycle_list_pop_eager(cpb_ref, &buff_cyc, 1, &buff, &buff_sz)) == CPB_OK) {
        if (buff == buff_a) {
            seen[0]++;
            assert_int_equal(buff_sz, 500);
        }
        else if (buff == buff_b) {
            seen[1]++;
            assert_int_equal(buff_sz, 500);
        }
        else if (buff == buff_c) {
            seen[2]++;
            assert_int_equal(buff_sz, 256);
        }
        else if (buff == buff_d) {
            seen[3]++;
            assert_int_equal(buff_sz, 1024);
        }
    }
    for (int i=0; i<4; i++) {
        assert_int_equal(seen[i], 1);
    }

    rv = cpb_buffer_recycle_list_push(cpb_ref, &buff_cyc, buff_c, 256);
    assert_true(rv == CPB_OK);
    rv = cpb_buffer_recycle_list_push(cpb_ref, &buff_cyc, buff_a, 500);
    assert_true(rv == CPB_OK);
    rv = cpb_buffer_recycle_list_push(cpb_ref, &buff_cyc, buff_b, 500);
    assert_true(rv == CPB_OK);
    rv = cpb_buffer_recycle_list_push(cpb_ref, &buff_cyc, buff_d, 1024);
    assert_true(rv == CPB_OK);

    cpb_buffer_recycle_list_deinit(cpb_ref, &buff_cyc);
}


int create_state(void **state) {
    struct test_state *tstate = malloc(sizeof *state);
    assert_true(!!tstate);
    cpb_init(&tstate->cpb);
    *state = tstate;
    return 0;
}
int destroy_state(void **state) {
    struct test_state *tstate = *state;
    assert_true(!!state);
    
    cpb_deinit(&tstate->cpb);
    *state = NULL;
    return 0;
}

const struct CMUnitTest decode_tests[] = {
    cmocka_unit_test(test_buffer_recycle_list),
};

int main(void) {
    return cmocka_run_group_tests(decode_tests, create_state, destroy_state);
}
