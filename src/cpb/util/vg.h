//Author: github.com/nilput
#ifndef VG_H
#define VG_H
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#define MAX_ARGS 128

//vg version 0.1.1
/*
 * API:
 *
    example usage:
        int main(int argc, char *argv[]) 
        {
            struct vgstate vg;
            vg_init(&vg, argc, argv);
            const char *usage = "./prog -n <N> -i <input file> [-i <input file 2> ...]\n";

            int n;
            if (vg_get_int(&vg, "-n", &n) != 0) 
                die("expected -n argument\n%s", usage);

            const char *filename = NULL;
            if (vg_get_str(&vg, "-i", &filename) != 0)
                die("expected file name argument\n%s", usage);
            do {
                printf("input file: %s\n", filename);
            } while (vg_get_str(&vg, "-i", &filename) == 0);

            int do_print = vg_get_boolean(&vg, "--print");
            int do_sort  = vg_get_boolean(&vg, "-s");

            const char *remaining;
            while (vg_get_leftover(&vg, &remaining) == 0) {
                printf("extra arg: %s\n", remaining);
            }
            ...
            vg_deinit(&vg);
        }

    note: non of the functions modifiy argv, and currently no allocations are made
    but the functions modify vg, they pop the queried flags

    must be called before anything
        static int vg_init(struct vgstate *vg, int argc, const char *argv[])

    currently does nothing, it's an attempt to future proof the thing and for consistency
        static int vg_deinit(struct vgstate *vg)

    //returns true if a matching argument exists
        static int vg_get_boolean(struct vgstate *vg, const char *pattern)

    //returns 0 if successful, sets *out to the string, the string should not be freed, (it's owned by argv)
    //can be called multiple times for repeated arguments, until it returns non-zero
        static int vg_get_str(struct vgstate *vg, const char *pattern, const char **out)

    //returns 0 if successful, sets *out to the integer
    //can be called multiple times,  until it returns non-zero
        static int vg_get_int(struct vgstate *vg, const char *pattern, int *out)

    //returns true if there are flags that are not handled
        static int vg_has_unhandled_flags(struct vgstate *vg) 

    //sets *out to the first remaining argument if any
    //can be called multiple times until it returns nonzero
        static int vg_get_leftover(struct vgstate *vg, const char **out) 
 */

struct vgstate {
    const char *args[MAX_ARGS];
    int len;
};
enum vg_err {
    VG_OK = 0,
    VG_NOT_FOUND = -1, //will remain -1
    VG_2MANY = -2,
    VG_INVALID_INT = -3,
    VG_MISSING_ARG = -4,
    VG_UNHANDLED_FLAG = -5,
};
static int vg_find_arg(struct vgstate *vg, const char *pattern) {
    for (int i=0; i<vg->len; i++)
        if (strncmp(vg->args[i], pattern, strlen(pattern)) == 0)
            return i;
    return VG_NOT_FOUND; //-1
}
static void vg_del(struct vgstate *vg, int index) {
    assert(index < vg->len);
    for (int i=index+1; i<vg->len; i++) {
        vg->args[i-1] = vg->args[i];
    }
    vg->len--;
}
static int vg_pop_string(struct vgstate *vg, const char *pattern, const char **out) {
    int argi = vg_find_arg(vg, pattern);
    *out = NULL;
    if (argi < 0) {
        return VG_NOT_FOUND;
    }
    else if (strlen(vg->args[argi]) > strlen(pattern)) {
        *out = vg->args[argi] + strlen(pattern);
        vg_del(vg, argi);
    }
    else if (argi == vg->len - 1) {
        vg_del(vg, argi);
        return VG_MISSING_ARG;
    }
    else {
        *out = vg->args[argi+1];
        vg_del(vg, argi+1);
        vg_del(vg, argi);
    }
    return VG_OK;
}
static int vg_get_boolean(struct vgstate *vg, const char *pattern) {
    int argi = vg_find_arg(vg, pattern);
    if (argi < 0)
        return 0;
    vg_del(vg, argi);
    return 1;
}
static int vg_get_int(struct vgstate *vg, const char *pattern, int *out) {
    const char *begin = NULL;
    char *end = NULL;
    int rv = vg_pop_string(vg, pattern, &begin);
    if (rv != 0)
        return rv;
    errno = 0;
    long v = strtol(begin, &end, 10);
    //we can be more strict, this is being permissive with inputs like "123foo", "foo" being silently discarded
    if (errno != 0 || end == begin) { 
        *out = 0;
        return VG_INVALID_INT;
    }
    *out = v;
    return VG_OK;
}
static int vg_get_str(struct vgstate *vg, const char *pattern, const char **out) {
    int rv = vg_pop_string(vg, pattern, out);
    return rv;
}
static int vg_get_leftover(struct vgstate *vg, const char **out) {
    if (vg->len == 0)
        return VG_MISSING_ARG;
    if (vg->args[0][0] == '-')
        return VG_UNHANDLED_FLAG;
    *out = vg->args[0];       
    vg_del(vg, 0);
    return VG_OK;
}
static int vg_has_unhandled_flags(struct vgstate *vg) {
    for (int i=0; i<vg->len; i++) {
        if (vg->args[i][0] == '-')
            return 1;
    }
    return 0;
}
static int vg_init(struct vgstate *vg, int argc, char *argv[]) {
    vg->len = argc - 1;
    if (vg->len > MAX_ARGS) {
        vg->len = 0;
        return VG_2MANY;
    }
    for (int i=1; i<argc; i++) {
        vg->args[i-1] = argv[i];
    }
    return 0;
}
static void vg_deinit(struct vgstate *vg) {
    vg->len = 0;
}
#endif //VG_H
