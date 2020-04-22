#include <stdio.h>
#include "../ini_reader.h"
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "expected ini name\n");
        return 1;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f) {
        fprintf(stderr, "failed to open \"%s\"\n", argv[1]);
        return 1;
    }
    struct cpb cpb_state;
    cpb_init(&cpb_state);
    struct ini_config *c = ini_parse(&cpb_state, f);
    if (!c) {
        fprintf(stderr, "failed to parse ini\n");
        return 1;
    }
    ini_dump(&cpb_state, c);
    ini_destroy(&cpb_state, c);
}