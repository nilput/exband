//Author: github.com/nilput
#ifndef VARG_H
#define VARG_H
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#define MAX_ARGS 128

//varg version 0.1.1
/*
 * API:
 *
    example usage:
        int main(int argc, char *argv[]) 
        {
            struct vargstate varg;
            varg_init(&varg, argc, argv);
            const char *usage = "./prog -n <N> -i <input file> [-i <input file 2> ...]\n";

            int n;
            if (varg_get_int(&varg, "-n", &n) != 0) 
                die("expected -n argument\n%s", usage);

            const char *filename = NULL;
            if (varg_get_str(&varg, "-i", &filename) != 0)
                die("expected file name argument\n%s", usage);
            do {
                printf("input file: %s\n", filename);
            } while (varg_get_str(&varg, "-i", &filename) == 0);

            int do_print = varg_get_boolean(&varg, "--print");
            int do_sort  = varg_get_boolean(&varg, "-s");

            const char *remaining;
            while (varg_get_leftover(&varg, &remaining) == 0) {
                printf("extra arg: %s\n", remaining);
            }
            ...
            varg_deinit(&varg);
        }

    note: non of the functions modifiy argv, and currently no allocations are made
    but the functions modify varg, they pop the queried flags

    must be called before anything
        static int varg_init(struct vargstate *varg, int argc, const char *argv[])

    currently does nothing, it's an attempt to future proof the thing and for consistency
        static int varg_deinit(struct vargstate *varg)

    //returns true if a matching argument exists
        static int varg_get_boolean(struct vargstate *varg, const char *pattern)

    //returns 0 if successful, sets *out to the string, the string should not be freed, (it's owned by argv)
    //can be called multiple times for repeated arguments, until it returns non-zero
        static int varg_get_str(struct vargstate *varg, const char *pattern, const char **out)

    //returns 0 if successful, sets *out to the integer
    //can be called multiple times,  until it returns non-zero
        static int varg_get_int(struct vargstate *varg, const char *pattern, int *out)

    //returns true if there are flags that are not handled
        static int varg_has_unhandled_flags(struct vargstate *varg) 

    //sets *out to the first remaining argument if any
    //can be called multiple times until it returns nonzero
        static int varg_get_leftover(struct vargstate *varg, const char **out) 
 */

struct vargstate {
    const char *args[MAX_ARGS];
    int len;
};
enum varg_err {
    VARG_OK = 0,
    VARG_NOT_FOUND = -1, //will remain -1
    VARG_2MANY = -2,
    VARG_INVALID_INT = -3,
    VARG_MISSING_ARG = -4,
    VARG_UNHANDLED_FLAG = -5,
};
static int varg_find_arg(struct vargstate *varg, const char *pattern) {
    for (int i=0; i<varg->len; i++)
        if (strncmp(varg->args[i], pattern, strlen(pattern)) == 0)
            return i;
    return VARG_NOT_FOUND; //-1
}
static void varg_del(struct vargstate *varg, int index) {
    assert(index < varg->len);
    for (int i=index+1; i<varg->len; i++) {
        varg->args[i-1] = varg->args[i];
    }
    varg->len--;
}
static int varg_pop_string(struct vargstate *varg, const char *pattern, const char **out) {
    int argi = varg_find_arg(varg, pattern);
    *out = NULL;
    if (argi < 0) {
        return VARG_NOT_FOUND;
    }
    else if (strlen(varg->args[argi]) > strlen(pattern)) {
        *out = varg->args[argi] + strlen(pattern);
        varg_del(varg, argi);
    }
    else if (argi == varg->len - 1) {
        varg_del(varg, argi);
        return VARG_MISSING_ARG;
    }
    else {
        *out = varg->args[argi+1];
        varg_del(varg, argi+1);
        varg_del(varg, argi);
    }
    return VARG_OK;
}
static int varg_get_boolean(struct vargstate *varg, const char *pattern) {
    int argi = varg_find_arg(varg, pattern);
    if (argi < 0)
        return 0;
    varg_del(varg, argi);
    return 1;
}
static int varg_get_int(struct vargstate *varg, const char *pattern, int *out) {
    const char *begin = NULL;
    char *end = NULL;
    int rv = varg_pop_string(varg, pattern, &begin);
    if (rv != 0)
        return rv;
    errno = 0;
    long v = strtol(begin, &end, 10);
    //we can be more strict, this is being permissive with inputs like "123foo", "foo" being silently discarded
    if (errno != 0 || end == begin) { 
        *out = 0;
        return VARG_INVALID_INT;
    }
    *out = v;
    return VARG_OK;
}
static int varg_get_str(struct vargstate *varg, const char *pattern, const char **out) {
    int rv = varg_pop_string(varg, pattern, out);
    return rv;
}
static int varg_get_leftover(struct vargstate *varg, const char **out) {
    if (varg->len == 0)
        return VARG_MISSING_ARG;
    if (varg->args[0][0] == '-')
        return VARG_UNHANDLED_FLAG;
    *out = varg->args[0];       
    varg_del(varg, 0);
    return VARG_OK;
}
static int varg_has_unhandled_flags(struct vargstate *varg) {
    for (int i=0; i<varg->len; i++) {
        if (varg->args[i][0] == '-')
            return 1;
    }
    return 0;
}
static int varg_init(struct vargstate *varg, int argc, char *argv[]) {
    varg->len = argc - 1;
    if (varg->len > MAX_ARGS) {
        varg->len = 0;
        return VARG_2MANY;
    }
    for (int i=1; i<argc; i++) {
        varg->args[i-1] = argv[i];
    }
    return 0;
}
static void varg_deinit(struct vargstate *varg) {
    varg->len = 0;
}
#endif //VARG_H
