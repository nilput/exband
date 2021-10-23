#ifndef CPB_SERVER_CONFIG_H
#define CPB_SERVER_CONFIG_H
#include "util/varg.h" //argv parsing
int exb_load_configuration(struct vargstate *vg,
                           struct exb *exb_ref,
                           struct exb_config *config_out,
                           struct exb_http_server_config *http_server_config_out);
#endif