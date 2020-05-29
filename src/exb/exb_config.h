#ifndef EXB_CONFIG_H
#define EXB_CONFIG_H
struct exb;
struct exb_config {
    int tp_threads; //threadpool threads
    int nloops; //number of event loops
    int nproc;  //number of processes
};
static struct exb_config exb_config_default(struct exb *exb_ref) {
    (void) exb_ref;
    struct exb_config conf = {0};
    conf.tp_threads = 4;
    conf.nloops = 1;
    conf.nproc = 1;
    return conf;
}

static void exb_config_deinit(struct exb *exb_ref, struct exb_config *config) {
}
#endif