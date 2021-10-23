#ifndef EXB_CONFIG_H
#define EXB_CONFIG_H

struct exb;

enum exb_processing_mode {
    EXB_MODE_MULTITHREADING,
    EXB_MODE_MULTIPROCESSING,
};

struct exb_config {
    enum exb_processing_mode op_mode; //mode of operation, see docs.
    int tp_threads;                   //threadpool threads
    int nloops;                       //number of event loops
    int nprocess;                     //number of processes
};

static struct exb_config exb_config_default(struct exb *exb_ref) {
    (void) exb_ref;
    struct exb_config conf = {0};
    conf.tp_threads = 4;
    conf.nloops = 1;
    conf.nprocess = 1;
    conf.op_mode = EXB_MODE_MULTITHREADING;
    return conf;
}

static void exb_config_deinit(struct exb *exb_ref, struct exb_config *config) {
    (void) exb_ref;
    (void) config;
}

#endif