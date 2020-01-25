#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <stdio.h>
#include <string.h>

#include "cmocka.h"
#include "../src/cpb/cpb.h"
#include "../src/cpb/cpb_str.h"
#include "../src/cpb/http/http_decode.h"


struct encdec {
        unsigned char *enc;
        int enclen;
        unsigned char *dec;
        int declen;
};
struct keyval {
    unsigned char *key;
    unsigned char *value;
    int keylen;
    int valuelen;
};
struct encdec_keys_values {
        char *enc;
        int enclen;
        struct keyval *keyvalues;
        int nvalues;
};

#include "test_decode_data.h"


struct test_state {
    struct cpb cpb;
};

void test_strappend(void **state)
{
    struct test_state *tstate = *state;
    struct cpb_str foo;
    
    cpb_str_init_const_str(&foo, "hello");
    cpb_str_strappend(&tstate->cpb, &foo, " world");
    assert_string_equal(foo.str, "hello world");
    cpb_str_deinit(&tstate->cpb, &foo);
}


static int vmemcmp(void *a, void *b, int len) {
    int r = memcmp(a, b, len);
    if (r != 0) {
        int idx = 0;
        for (idx=0; idx<len && ((char *)a)[idx] == ((char *)b)[idx]; idx++);
        fprintf(stderr, "0x%x != 0x%x at %d\n", (int)((unsigned char *)a)[idx], (int)((unsigned char *)b)[idx], idx);
    }
    return r;
}

void test_urlencode_decode(void **state)
{
    struct test_state *tstate = *state;
    struct cpb_str dec;
    int sz;
    cpb_str_init(&tstate->cpb, &dec);
    for (int i=0; i<sizeof ed / sizeof ed[0]; i++) {
        cpb_str_ensure_cap(&tstate->cpb, &dec, ed[i].declen);
        cpb_urlencode_decode(dec.str, dec.cap, &sz, ed[i].enc, ed[i].enclen);
        assert_true(sz == ed[i].declen);
        assert_true(sz < dec.cap);
        assert_true(vmemcmp(dec.str, ed[i].dec, ed[i].declen) == 0);
    }
    
    cpb_str_deinit(&tstate->cpb, &dec);
}

int check_encdec_keys_values(struct cpb *cpb, struct encdec_keys_values *ekv) { 
    struct cpb_form_parts fp;
    cpb_urlencode_decode_parts(cpb, &fp, ekv->enc, ekv->enclen);
    assert_int_equal(ekv->nvalues, fp.nparts);
    for (int i=0; i<ekv->nvalues; i++) {
        assert_true(fp.buff.str[fp.keys[i].index + fp.keys[i].len] == 0); //nul terminated
        assert_int_equal(fp.keys[i].len, ekv->keyvalues[i].keylen);
        assert_int_equal(fp.values[i].len, ekv->keyvalues[i].valuelen);
        assert_true(vmemcmp(fp.buff.str + fp.keys[i].index,   ekv->keyvalues[i].key,   ekv->keyvalues[i].keylen) == 0);
        assert_true(vmemcmp(fp.buff.str + fp.values[i].index, ekv->keyvalues[i].value, ekv->keyvalues[i].valuelen) == 0);
    }
    cpb_form_parts_deinit(cpb, &fp);
}

void test_urlencode_decode_parts(void **state)
{
    struct test_state *tstate = *state;
    for (int i=0; i<sizeof eparts / sizeof eparts[0]; i++) {
        check_encdec_keys_values(&tstate->cpb, eparts + i);
    }
}
void test_decode_header_parts(void **state) {
    struct test_state *tstate = *state;
    struct cpb *cpb = &tstate->cpb;
    char *h1 = "multipart/form-data; boundary=foobar";
    assert_true(cpb_content_type_is(h1, strlen(h1), "multipart/form-data"));
    struct cpb_header_params pm;
    cpb_decode_header_params(&tstate->cpb, &pm, h1, strlen(h1));
    assert_true(pm.nparams == 1);
    assert_string_equal(pm.buff.str + pm.value.index, "multipart/form-data");
    assert_string_equal(pm.buff.str + pm.params.keys[0].index, "boundary");
    assert_string_equal(pm.buff.str + pm.params.values[0].index, "foobar");
    cpb_header_params_deinit(&tstate->cpb, &pm);
    char *h2 = "multipart/form-data;boundary=foobar;key2=\"value 2\"  ; key3=\"value \\\"3\"";
    cpb_decode_header_params(&tstate->cpb, &pm, h2, strlen(h2));
    assert_true(pm.nparams == 3);
    assert_string_equal(pm.buff.str + pm.value.index, "multipart/form-data");
    assert_string_equal(pm.buff.str + pm.params.keys[0].index, "boundary");
    assert_string_equal(pm.buff.str + pm.params.values[0].index, "foobar");
    assert_string_equal(pm.buff.str + pm.params.keys[1].index, "key2");
    assert_string_equal(pm.buff.str + pm.params.values[1].index, "value 2");
    assert_string_equal(pm.buff.str + pm.params.keys[2].index, "key3");
    assert_string_equal(pm.buff.str + pm.params.values[2].index, "value \"3");
    
    cpb_header_params_deinit(&tstate->cpb, &pm);
}

void test_decode_multipart_form(void **state) {
    struct test_state *tstate = *state;
    struct cpb *cpb = &tstate->cpb;
    struct cpb_form_parts fp;
    
    char *h1 = "multipart/form-data; boundary=------------------------3b3fe428898e909a";
    char *body1 = "--------------------------3b3fe428898e909a\r\n"
                  "Content-Disposition: form-data; name=\"f\"\r\n"
                  "\r\n"
                  "12 45 678\r\n"
                  "--------------------------3b3fe428898e909a--";
    cpb_decode_multipart(cpb, &fp,  h1, strlen(h1), body1, strlen(body1));
    assert_int_equal(fp.nparts, 1);
    assert_string_equal(fp.buff.str + fp.keys[0].index, "f");
    assert_int_equal(fp.values[0].len, 9);
    assert_memory_equal(body1 + fp.values[0].index, "12 45 678", 9);
    cpb_form_parts_deinit(cpb, &fp);
    char *h2 = "multipart/form-data; boundary=------------------------9a68d36abfb5d849";
    char *body2 = "--------------------------9a68d36abfb5d849\r\n"
        "Content-Disposition: form-data; name=\"key1\"\r\n"
        "\r\n"
        "value1\r\n"
        "--------------------------9a68d36abfb5d849\r\n"
        "Content-Disposition: form-data; name=\"key2\"\r\n"
        "\r\n"
        "value2\r\n"
        "--------------------------9a68d36abfb5d849\r\n"
        "Content-Disposition: form-data; name=\"repeated\"\r\n"
        "\r\n"
        "value\r\n"
        "--------------------------9a68d36abfb5d849\r\n"
        "Content-Disposition: form-data; name=\"repeated\"\r\n"
        "\r\n"
        "value\r\n"
        "--------------------------9a68d36abfb5d849--\r\n";
    cpb_decode_multipart(cpb, &fp,  h2, strlen(h2), body2, strlen(body2));
    assert_int_equal(fp.nparts, 4);
    assert_string_equal(fp.buff.str + fp.keys[0].index, "key1");
    assert_int_equal(fp.values[0].len, 6);
    assert_memory_equal(body2 + fp.values[0].index, "value1", 6);

    assert_string_equal(fp.buff.str + fp.keys[1].index, "key2");
    assert_int_equal(fp.values[1].len, 6);
    assert_memory_equal(body2 + fp.values[1].index, "value2", 6);

    assert_string_equal(fp.buff.str + fp.keys[2].index, "repeated");
    assert_int_equal(fp.values[2].len, 5);
    assert_memory_equal(body2 + fp.values[2].index, "value", 5);

    assert_string_equal(fp.buff.str + fp.keys[3].index, "repeated");
    assert_int_equal(fp.keys[3].len, strlen("repeated"));
    assert_int_equal(fp.values[3].len, 5);
    assert_memory_equal(body2 + fp.values[3].index, "value", 5);

    cpb_form_parts_deinit(cpb, &fp);
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
    cmocka_unit_test(test_strappend),
    cmocka_unit_test(test_urlencode_decode),
    cmocka_unit_test(test_urlencode_decode_parts),
    cmocka_unit_test(test_decode_header_parts),
    cmocka_unit_test(test_decode_multipart_form),
};

int main(void) {
    return cmocka_run_group_tests(decode_tests, create_state, destroy_state);
}