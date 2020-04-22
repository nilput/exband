#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <stdio.h>
#include <string.h>

#include "cmocka.h"
#include "../../src/exb/exb.h"
#include "../../src/exb/exb_buffer_recycle_list.h

struct test_state {
    struct exb exb;
};

void test_buffer_recycle_list(void **state)
{
    struct test_state *tstate = *state;
    struct exb *exb_ref = &tstate->exb;

    void *buff_a = exb_malloc(exb_ref, 500);
    void *buff_b = exb_malloc(exb_ref, 500);
    void *buff_c = exb_malloc(exb_ref, 256);
    void *buff_d = exb_malloc(exb_ref, 1024);
    struct exb_buffer_recycle_list buff_cyc;
    int rv;
    rv = exb_buffer_recycle_list_init(exb_ref, &buff_cyc);
    assert_true(rv == EXB_OK)
    void *buff;
    size_t buff_sz;

    //push and pop using smaller or equal or greater size
    rv = exb_buffer_recycle_list_push(exb_ref, &buff_cyc, buff_a, 500);
    assert_true(rv == EXB_OK);
    rv = exb_buffer_recycle_list_pop(exb_ref, &buff_cyc, 400, &buff, &buff_sz);
    assert_true(rv == EXB_OK);
    assert_true(buff == buff_a);
    assert_true(buff_sz == 500);
    rv = exb_buffer_recycle_list_push(exb_ref, &buff_cyc, buff_a, 500);
    assert_true(rv == EXB_OK);
    rv = exb_buffer_recycle_list_pop(exb_ref, &buff_cyc, 500, &buff, &buff_sz);
    assert_true(rv == EXB_OK);
    assert_true(buff == buff_a);
    assert_true(buff_sz == 500);
    rv = exb_buffer_recycle_list_push(exb_ref, &buff_cyc, buff_b, 500);
    assert_true(rv == EXB_OK);
    rv = exb_buffer_recycle_list_pop(exb_ref, &buff_cyc, 501, &buff, &buff_sz);
    assert_true(rv != EXB_OK);
    rv = exb_buffer_recycle_list_pop(exb_ref, &buff_cyc, 500, &buff, &buff_sz);
    assert_true(rv == EXB_OK);
    assert_true(buff == buff_b);

    //add multiple buffers
    rv = exb_buffer_recycle_list_push(exb_ref, &buff_cyc, buff_c, 256);
    assert_true(rv == EXB_OK);
    rv = exb_buffer_recycle_list_push(exb_ref, &buff_cyc, buff_a, 500);
    assert_true(rv == EXB_OK);
    rv = exb_buffer_recycle_list_push(exb_ref, &buff_cyc, buff_b, 500);
    assert_true(rv == EXB_OK);
    rv = exb_buffer_recycle_list_push(exb_ref, &buff_cyc, buff_d, 1024);
    assert_true(rv == EXB_OK);

    //state: all added
    rv = exb_buffer_recycle_list_pop(exb_ref, &buff_cyc, 501, &buff, &buff_sz);
    if (rv == EXB_OK) {
        assert_ptr_equal(buff, buff_d);
        assert_int_equal(buff_sz, 1024);
    }
    else {
        rv = exb_buffer_recycle_list_pop(exb_ref, &buff_cyc, 1024, &buff, &buff_sz);    
        assert_true(rv == EXB_OK);
        assert_ptr_equal(buff, buff_d);
        assert_int_equal(buff_sz, 1024);
    }
    //state: d not added
    rv = exb_buffer_recycle_list_pop(exb_ref, &buff_cyc, 501, &buff, &buff_sz);
    assert_true(rv != EXB_OK);
    rv = exb_buffer_recycle_list_push(exb_ref, &buff_cyc, buff_d, 1024);
    assert_true(rv == EXB_OK);
    //state: all added
    rv = exb_buffer_recycle_list_pop(exb_ref, &buff_cyc, 500, &buff, &buff_sz);
    assert_true(rv == EXB_OK);
    assert_true(buff == buff_a || buff == buff_b || buff == buff_d);
    rv = exb_buffer_recycle_list_push(exb_ref, &buff_cyc, buff, buff_sz);
    //state: all added
    
    int seen[4] = {0};
    while ((rv = exb_buffer_recycle_list_pop_eager(exb_ref, &buff_cyc, 1, &buff, &buff_sz)) == EXB_OK) {
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

    rv = exb_buffer_recycle_list_push(exb_ref, &buff_cyc, buff_c, 256);
    assert_true(rv == EXB_OK);
    rv = exb_buffer_recycle_list_push(exb_ref, &buff_cyc, buff_a, 500);
    assert_true(rv == EXB_OK);
    rv = exb_buffer_recycle_list_push(exb_ref, &buff_cyc, buff_b, 500);
    assert_true(rv == EXB_OK);
    rv = exb_buffer_recycle_list_push(exb_ref, &buff_cyc, buff_d, 1024);
    assert_true(rv == EXB_OK);

    exb_buffer_recycle_list_deinit(exb_ref, &buff_cyc);
}


int create_state(void **state) {
    struct test_state *tstate = malloc(sizeof *state);
    assert_true(!!tstate);
    exb_init(&tstate->exb);
    *state = tstate;
    return 0;
}
int destroy_state(void **state) {
    struct test_state *tstate = *state;
    assert_true(!!state);
    
    exb_deinit(&tstate->exb);
    *state = NULL;
    return 0;
}

const struct CMUnitTest decode_tests[] = {
    cmocka_unit_test(test_buffer_recycle_list),
};

int main(void) {
    return cmocka_run_group_tests(decode_tests, create_state, destroy_state);
}
