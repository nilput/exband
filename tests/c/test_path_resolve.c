#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <stdio.h>
#include <string.h>

#include "cmocka.h"
#include "../../src/exb/exb.h"
#include "../../src/exb/http/exb_fileserv.h"

struct test_state {
    struct exb exb;
};

void test_path_resolve(void **state)
{
    struct test_state *tstate = *state;
    struct exb *exb_ref = &tstate->exb;
    struct before_after {
        char before[255];
        char after[255];
    };
    struct before_after paths[] = {
        {"/foo/bar/a.text", "/foo/bar/a.text"},
        {"/foo/bar/a..txt", "/foo/bar/a..txt"},
        {"/foo/bar//a.text", "/foo/bar/a.text"},
        {"/foo/bar/", "/foo/bar/"},
        {"/foo/bar/", "/foo/bar/"},
        {"/../foo/bar/", "/foo/bar/"},
        {"/..../foo/bar/", "/..../foo/bar/"},
        {"/./foo/bar/", "/foo/bar/"},
        {"/", "/"},
        {"/.", "/"},
        {"/./", "/"},
        {"/..", "/"},
        {"/../", "/"},
        {"/../a", "/a"},
        {"/../a/", "/a/"},
        {"/a", "/a"},
        {"/a//", "/a/"},
        {"/a/b/", "/a/b/"},
        {"/a/b/", "/a/b/"},
        {"/a../b/", "/a../b/"},
        {"/a/../b/", "/b/"},
        {"/a/../b/c/./d/", "/b/c/d/"},
        {"/a/../b/c/./d", "/b/c/d"},
        {"/../b/c/./d", "/b/c/d"},
        {"/../.././../b/c/./d", "/b/c/d"},
        {"/a../.././../b/c/./d", "/b/c/d"},
        {"/a/../b/c/./d/.", "/b/c/d/"},
        {"/a/../b/c/./d/..", "/b/c/d/"},
        {"/a/../b/c/./d/./", "/b/c/d/"},
        {"/a/../b/c/./d/../", "/b/c/"},
    };
    int paths_len = sizeof paths / sizeof paths[0];
    
    for (int i = 0; i < paths_len; i++) {
        exb_resolve_path(paths[i].before, strlen(paths[i].before));
        if (strcmp(paths[i].before, paths[i].after) != 0) {
            printf("%d: expected '%s': found '%s'\n", i, paths[i].after, paths[i].before);
        }
        assert_string_equal(paths[i].before, paths[i].after); 
    }
    
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

const struct CMUnitTest resolve_tests[] = {
    cmocka_unit_test(test_path_resolve),
};

int main(void) {
    return cmocka_run_group_tests(resolve_tests, create_state, destroy_state);
}
