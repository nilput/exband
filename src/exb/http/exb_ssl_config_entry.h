#ifndef EXB_SSL_CONFIG_ENTRY_H
#define EXB_SSL_CONFIG_ENTRY_H
#include <stdbool.h>
#include "../exb_str.h"

struct exb_ssl_config_entry {
    struct exb_str public_key_path;
    struct exb_str private_key_path;
    struct exb_str ca_path;
    struct exb_str dh_params_path;
    struct exb_str ssl_protocols;
    struct exb_str ssl_ciphers;
    struct exb_str server_name; // a copy, not specified in the json config "ssl" block, but in its parent
    bool is_default;
    int listen_port;
    struct exb_str listen_ip;
};


/*
iterate configuration entries
iter_state: should be point to an integer initialized to 0
entry_out:  shouldn't be freed nor its members, it is to be treated as
            a read only temporary structure, needed members should be copied from it
RETURN VALUE:
    when done iterating the function will return EXB_EOF,
    otherwise a return value other than EXB_OK is to be considered an error
*/
struct exb_server;
extern int exb_ssl_config_entries_iter(struct exb_server *server, int *iter_state, struct exb_ssl_config_entry **entry_out, int *domain_id_out);

static void exb_ssl_config_entry_init(struct exb *exb_ref, struct exb_ssl_config_entry *entry) {
    EXB_UNUSED(exb_ref);
    exb_str_init_empty(&entry->ca_path);
    exb_str_init_empty(&entry->public_key_path);
    exb_str_init_empty(&entry->private_key_path);
    exb_str_init_empty(&entry->dh_params_path);
    exb_str_init_empty(&entry->ssl_protocols);
    exb_str_init_empty(&entry->ssl_ciphers);
    exb_str_init_empty(&entry->server_name);
    exb_str_init_empty(&entry->listen_ip);
    entry->is_default = 0;
    entry->listen_port = 0;
}
static void exb_ssl_config_entry_deinit(struct exb *exb_ref, struct exb_ssl_config_entry *entry) {
    exb_str_deinit(exb_ref, &entry->ca_path);
    exb_str_deinit(exb_ref, &entry->public_key_path);
    exb_str_deinit(exb_ref, &entry->private_key_path);
    exb_str_deinit(exb_ref, &entry->dh_params_path);
    exb_str_deinit(exb_ref, &entry->ssl_protocols);
    exb_str_deinit(exb_ref, &entry->ssl_ciphers);
    exb_str_deinit(exb_ref, &entry->server_name);
    exb_str_deinit(exb_ref, &entry->listen_ip);
}

#endif
