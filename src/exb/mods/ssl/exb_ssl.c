/*
This is a builtin module, it differs from other dynamically loaded modules in that it
is statically linked, and is a first class citizen in terms of configuration and loading
*/

#include "../../exb.h"
#include "../../http/http_server_module.h"
#include "../../http/http_server.h"
#include "../../http/http_request.h"
#include "../../exb_build_config.h"
#include "../../http/exb_ssl_config_entry.h"
#include "exb_ssl.h"

//read/recv/write
#include <unistd.h>
#include <errno.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>


enum exb_ssl_state_openssl_flags {
    EXB_SSL_STATE_OPENSSL_ACCEPTED = 1,
};


#if OPENSSL_VERSION_NUMBER < 0x10100000L 
    //reason: older versions require thread synchronization
    #error unsupported version of openssl
#endif

#if defined(EXB_USE_OPENSSL_ECDH) && OPENSSL_VERSION_NUMBER >= 0x0090800fL
#include <openssl/ecdh.h>
#endif

#ifdef EXB_USE_OPENSSL_DH
    #include <openssl/dh.h>
//Source: lighttp
static DH* load_dh_params_4096(void) {
    const unsigned char dh4096_p[] = {
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xC9,0x0F,0xDA,0xA2,
        0x21,0x68,0xC2,0x34,0xC4,0xC6,0x62,0x8B,0x80,0xDC,0x1C,0xD1,
        0x29,0x02,0x4E,0x08,0x8A,0x67,0xCC,0x74,0x02,0x0B,0xBE,0xA6,
        0x3B,0x13,0x9B,0x22,0x51,0x4A,0x08,0x79,0x8E,0x34,0x04,0xDD,
        0xEF,0x95,0x19,0xB3,0xCD,0x3A,0x43,0x1B,0x30,0x2B,0x0A,0x6D,
        0xF2,0x5F,0x14,0x37,0x4F,0xE1,0x35,0x6D,0x6D,0x51,0xC2,0x45,
        0xE4,0x85,0xB5,0x76,0x62,0x5E,0x7E,0xC6,0xF4,0x4C,0x42,0xE9,
        0xA6,0x37,0xED,0x6B,0x0B,0xFF,0x5C,0xB6,0xF4,0x06,0xB7,0xED,
        0xEE,0x38,0x6B,0xFB,0x5A,0x89,0x9F,0xA5,0xAE,0x9F,0x24,0x11,
        0x7C,0x4B,0x1F,0xE6,0x49,0x28,0x66,0x51,0xEC,0xE4,0x5B,0x3D,
        0xC2,0x00,0x7C,0xB8,0xA1,0x63,0xBF,0x05,0x98,0xDA,0x48,0x36,
        0x1C,0x55,0xD3,0x9A,0x69,0x16,0x3F,0xA8,0xFD,0x24,0xCF,0x5F,
        0x83,0x65,0x5D,0x23,0xDC,0xA3,0xAD,0x96,0x1C,0x62,0xF3,0x56,
        0x20,0x85,0x52,0xBB,0x9E,0xD5,0x29,0x07,0x70,0x96,0x96,0x6D,
        0x67,0x0C,0x35,0x4E,0x4A,0xBC,0x98,0x04,0xF1,0x74,0x6C,0x08,
        0xCA,0x18,0x21,0x7C,0x32,0x90,0x5E,0x46,0x2E,0x36,0xCE,0x3B,
        0xE3,0x9E,0x77,0x2C,0x18,0x0E,0x86,0x03,0x9B,0x27,0x83,0xA2,
        0xEC,0x07,0xA2,0x8F,0xB5,0xC5,0x5D,0xF0,0x6F,0x4C,0x52,0xC9,
        0xDE,0x2B,0xCB,0xF6,0x95,0x58,0x17,0x18,0x39,0x95,0x49,0x7C,
        0xEA,0x95,0x6A,0xE5,0x15,0xD2,0x26,0x18,0x98,0xFA,0x05,0x10,
        0x15,0x72,0x8E,0x5A,0x8A,0xAA,0xC4,0x2D,0xAD,0x33,0x17,0x0D,
        0x04,0x50,0x7A,0x33,0xA8,0x55,0x21,0xAB,0xDF,0x1C,0xBA,0x64,
        0xEC,0xFB,0x85,0x04,0x58,0xDB,0xEF,0x0A,0x8A,0xEA,0x71,0x57,
        0x5D,0x06,0x0C,0x7D,0xB3,0x97,0x0F,0x85,0xA6,0xE1,0xE4,0xC7,
        0xAB,0xF5,0xAE,0x8C,0xDB,0x09,0x33,0xD7,0x1E,0x8C,0x94,0xE0,
        0x4A,0x25,0x61,0x9D,0xCE,0xE3,0xD2,0x26,0x1A,0xD2,0xEE,0x6B,
        0xF1,0x2F,0xFA,0x06,0xD9,0x8A,0x08,0x64,0xD8,0x76,0x02,0x73,
        0x3E,0xC8,0x6A,0x64,0x52,0x1F,0x2B,0x18,0x17,0x7B,0x20,0x0C,
        0xBB,0xE1,0x17,0x57,0x7A,0x61,0x5D,0x6C,0x77,0x09,0x88,0xC0,
        0xBA,0xD9,0x46,0xE2,0x08,0xE2,0x4F,0xA0,0x74,0xE5,0xAB,0x31,
        0x43,0xDB,0x5B,0xFC,0xE0,0xFD,0x10,0x8E,0x4B,0x82,0xD1,0x20,
        0xA9,0x21,0x08,0x01,0x1A,0x72,0x3C,0x12,0xA7,0x87,0xE6,0xD7,
        0x88,0x71,0x9A,0x10,0xBD,0xBA,0x5B,0x26,0x99,0xC3,0x27,0x18,
        0x6A,0xF4,0xE2,0x3C,0x1A,0x94,0x68,0x34,0xB6,0x15,0x0B,0xDA,
        0x25,0x83,0xE9,0xCA,0x2A,0xD4,0x4C,0xE8,0xDB,0xBB,0xC2,0xDB,
        0x04,0xDE,0x8E,0xF9,0x2E,0x8E,0xFC,0x14,0x1F,0xBE,0xCA,0xA6,
        0x28,0x7C,0x59,0x47,0x4E,0x6B,0xC0,0x5D,0x99,0xB2,0x96,0x4F,
        0xA0,0x90,0xC3,0xA2,0x23,0x3B,0xA1,0x86,0x51,0x5B,0xE7,0xED,
        0x1F,0x61,0x29,0x70,0xCE,0xE2,0xD7,0xAF,0xB8,0x1B,0xDD,0x76,
        0x21,0x70,0x48,0x1C,0xD0,0x06,0x91,0x27,0xD5,0xB0,0x5A,0xA9,
        0x93,0xB4,0xEA,0x98,0x8D,0x8F,0xDD,0xC1,0x86,0xFF,0xB7,0xDC,
        0x90,0xA6,0xC0,0x8F,0x4D,0xF4,0x35,0xC9,0x34,0x06,0x31,0x99,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        };
    const unsigned char dh4096_g[]={
        0x05,
    };

    
    DH *dh = DH_new();
    if (!dh) 
        return NULL;
    BIGNUM *dh_p = BN_bin2bn(dh4096_p, sizeof(dh4096_p), NULL);
    BIGNUM *dh_g = BN_bin2bn(dh4096_g, sizeof(dh4096_g), NULL);

    if (!dh_p || !dh_g) {
        BN_free(dh_p);
        BN_free(dh_g);
        DH_free(dh);
        return NULL;
    }

    #if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
        dh->p = dh_p;
        dh->g = dh_g;
    #else
        DH_set0_pqg(dh, dh_p, NULL, dh_g);
    #endif

    return dh;
}
#endif

struct exb_ssl_module {
    struct exb_http_server_module head;
    struct exb *exb_ref;
    struct exb_server *server_ref;
    /*Each set of domain / certifcate pair will have a different context for SNI*/
    SSL_CTX *contexts[EXB_SNI_MAX_DOMAINS]; //Mapped to each domain_id
};

static void exb_ssl_destroy(struct exb_http_server_module *module, struct exb *exb) {
    exb_free(exb, module);
}

static int exb_ssl_openssl_init_domain(struct exb *exb,
                                       struct exb_server *server,
                                       struct exb_ssl_module *mod,
                                       struct exb_ssl_config_entry *entry,
                                       int domain_id) 
{
    int rv = EXB_OK;
	const char *default_ciphers = "HIGH !aNULL !3DES +kEDH +kRSA !kSRP !kPSK";
	const char *default_ecdh_curve = "prime256v1";

	const char *ciphers = entry->ssl_ciphers.len ? entry->ssl_ciphers.str : NULL;
    const char *private_key_file = entry->private_key_path.len ? entry->private_key_path.str : NULL;
    const char *public_key_file = entry->public_key_path.len ? entry->public_key_path.str : NULL;
    const char *ca_file = NULL; 
    const char *dh_params_file = NULL; 
    const char *ecdh_curve = NULL;
	long options = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_CIPHER_SERVER_PREFERENCE | SSL_OP_SINGLE_DH_USE
#ifdef SSL_OP_NO_COMPRESSION
			| SSL_OP_NO_COMPRESSION
#endif
#ifdef EXB_USE_OPENSSL_ECDH
			| SSL_OP_SINGLE_ECDH_USE
#endif
            ;
    
    SSL_CTX *ssl_ctx = SSL_CTX_new(SSLv23_server_method());
    if (ssl_ctx == NULL) {
        exb_log_error(exb, "SSL_CTX_new: %s", ERR_error_string(ERR_get_error(), NULL));
        goto on_error_1;
    }

    if (!SSL_CTX_set_options(ssl_ctx, options)) {
        exb_log_error(exb, "SSL_CTX_set_options(%lx): %s", options, ERR_error_string(ERR_get_error(), NULL));
        goto on_error_1;
    }

    if (ciphers == NULL) 
        ciphers = default_ciphers;
    if (SSL_CTX_set_cipher_list(ssl_ctx, ciphers) != 1) {
        exb_log_error(exb, "SSL_CTX_set_cipher_list('%s'): %s", ciphers, ERR_error_string(ERR_get_error(), NULL));
        goto on_error_1;
    }

#ifdef EXB_USE_OPENSSL_DH
    DH *dh = NULL;
    /* Support for Diffie-Hellman key exchange */
    if (NULL != dh_params_file) {
        /* DH parameters from file */
        BIO *bio = BIO_new_file(dh_params_file, "r");
        if (bio == NULL) {
            exb_log_error(exb, "SSL: BIO_new_file('%s'): unable to open file", dh_params_file);
            goto on_error_1;
        }
        dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
        BIO_free(bio);
        if (!dh) {
            exb_log_error(exb, "SSL: PEM_read_bio_DHparams failed (for file '%s')", dh_params_file);
            goto on_error_1;
        }
    } else {
        dh = load_dh_params_4096();
        if (NULL == dh) {
            exb_log_error(exb, "%s", "SSL: loading default DH parameters failed");
            goto on_error_1;
        }
    }
    SSL_CTX_set_tmp_dh(ssl_ctx, dh);
    DH_free(dh);
#endif

#ifdef EXB_USE_OPENSSL_ECDH
    if (ecdh_curve == NULL) 
        ecdh_curve = default_ecdh_curve;
    int ecdh_nid = OBJ_sn2nid(ecdh_curve);
    if (ecdh_nid == NID_undef) {
        exb_log_error(exb, "SSL: Unknown curve name '%s'", ecdh_curve);
        goto on_error_1;
    }

    EC_KEY *ecdh = EC_KEY_new_by_curve_name(ecdh_nid);
    if (NULL == ecdh) {
        exb_log_error(exb, "SSL: Unable to create curve '%s'", ecdh_curve);
        goto on_error_1;
    }
    SSL_CTX_set_tmp_ecdh(ssl_ctx, ecdh);
    EC_KEY_free(ecdh);
#else
    UNUSED(default_ecdh_curve);
#endif

    if (ca_file != NULL) {
        if (SSL_CTX_load_verify_locations(ssl_ctx, ca_file, NULL) != 1) {
            exb_log_error(exb, "SSL_CTX_load_verify_locations('%s'): %s", ca_file, ERR_error_string(ERR_get_error(), NULL));
            goto on_error_1;
        }
    }


    if (SSL_CTX_use_certificate_file(ssl_ctx, public_key_file, SSL_FILETYPE_PEM) < 0) {
        exb_log_error(exb, "SSL_CTX_use_certificate_file('%s'): %s", public_key_file, ERR_error_string(ERR_get_error(), NULL));
        goto on_error_1;
    }

    if (SSL_CTX_use_PrivateKey_file (ssl_ctx, private_key_file, SSL_FILETYPE_PEM) < 0) {
        exb_log_error(exb, "SSL_CTX_use_PrivateKey_file('%s'): %s", private_key_file, ERR_error_string(ERR_get_error(), NULL));
        goto on_error_1;
    }

    if (SSL_CTX_check_private_key(ssl_ctx) != 1) {
        exb_log_error(exb,
                        "SSL: Private key '%s' does not match the certificate public key, reason: %s",
                        private_key_file,
                        ERR_error_string(ERR_get_error(), NULL));
        goto on_error_1;
    }

    if (SSL_CTX_set_session_id_context(ssl_ctx, (unsigned char *)"exband", 6) != 1) {
        exb_log_error(exb, "SSL_CTX_set_session_id_context(): %s", ERR_error_string(ERR_get_error(), NULL));
        goto on_error_1;
    }


    SSL_CTX_set_default_read_ahead(ssl_ctx, 1);
    SSL_CTX_set_mode(ssl_ctx, SSL_CTX_get_mode(ssl_ctx) | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

    mod->contexts[domain_id] = ssl_ctx;
    return EXB_OK;
on_error_1:
    return rv;
}

static int exb_ssl_openssl_init(struct exb *exb, struct exb_server *server, struct exb_ssl_module *mod) {
    struct exb_ssl_config_entry *entry = NULL;
    int domain_id = -1;
    int istate = 0;
    int rv = EXB_OK;
    while ((rv = exb_ssl_config_entries_iter(server, &istate, &entry, &domain_id)) == EXB_OK) {
        rv = exb_ssl_openssl_init_domain(exb, server, mod, entry, domain_id);
        if (rv != EXB_OK) {
            goto on_error_1;
        }
    }
    
    return EXB_OK;
on_error_1:
    /*TODO: free all contexts*/
    return rv;
}

int exb_ssl_connection_init(struct exb_http_server_module *mod, struct exb_http_multiplexer *mp)
{
    
    struct exb_ssl_module *ssl_mod = (struct exb_ssl_module *) mod;
    struct exb_http_ssl_state *ssl_state = &mp->ssl_state;
    memset(ssl_state, 0, sizeof *ssl_state);
    exb_assert_h(!!ssl_mod->contexts[0], "");
    BIO *rbio = BIO_new(BIO_s_mem());
    BIO *wbio = BIO_new(BIO_s_mem());
    SSL *ssl  = SSL_new(ssl_mod->contexts[ssl_state->ssl_context_id]);
    if (!rbio || !wbio || !ssl) {
        if (rbio)
            BIO_free(rbio);
        if (wbio)
            BIO_free(wbio);
        if (ssl)
            SSL_free(ssl);
        return EXB_SSL_INIT_ERROR;
    }
    SSL_set_accept_state(ssl); /* sets ssl to work in server mode. */
    SSL_set_bio(ssl, rbio, wbio);

    ssl_state->keep_buff = NULL;
    ssl_state->keep_buff_len = 0;
    ssl_state->keep_buff_size = 0;

    ssl_state->ssl_context_id = 0; //temporarily assigned until resolution
    ssl_state->ssl_obj = ssl;
    ssl_state->rbio = rbio;
    ssl_state->wbio = wbio;

    return EXB_OK;
}

void exb_ssl_connection_deinit(struct exb_http_server_module *mod, struct exb_http_multiplexer *mp)
{
    struct exb_http_ssl_state *ssl_state = &mp->ssl_state;
    struct exb_ssl_module *ssl_mod = (struct exb_ssl_module *) mod;
    
    if (ssl_state->keep_buff) {
        exb_evloop_release_buffer(mp->evloop, ssl_state->keep_buff, ssl_state->keep_buff_size);
    }

    if (ssl_state->ssl_obj)
        SSL_free(ssl_state->ssl_obj);
    ssl_state->ssl_obj = NULL;
    ssl_state->rbio = ssl_state->wbio = NULL;
    ssl_state->ssl_context_id = -1;
    memset(ssl_state, 0, sizeof *ssl_state);
}

static struct exb_io_result try_send(int socket_fd,
                                     char *buff,
                                     size_t buff_len) 
{
    ssize_t written = send(socket_fd,
                           buff,
                           buff_len,
                           MSG_DONTWAIT);
    if (written <= 0) {
        if (written == 0) {
            return exb_make_io_result(0, EXB_IO_FLAG_CLIENT_CLOSED);
        }
        else if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return exb_make_io_result(0, 0);
        }
        else {
            return exb_make_io_result(0, EXB_IO_FLAG_IO_ERROR);
        }
    }
    return exb_make_io_result(written, 0);
}

static struct exb_io_result send_from_wbio_or_keep(struct exb_ssl_module *ssl_mod,
                                         struct exb_http_multiplexer *mp,
                                         BIO *wbio,
                                         char *tmp_buff,
                                         size_t tmp_buff_sz)
{
    size_t total_sent = 0;
    if (EXB_UNLIKELY(mp->ssl_state.keep_buff && mp->ssl_state.keep_buff_len > 0)) {
        struct exb_io_result pres = try_send(mp->socket_fd,
                                                 mp->ssl_state.keep_buff,
                                                 mp->ssl_state.keep_buff_len);
        if (pres.nbytes > 0 && pres.nbytes < mp->ssl_state.keep_buff_size) {
            //TODO: optimize to use a ring buffer
            memmove(mp->ssl_state.keep_buff, 
                    mp->ssl_state.keep_buff + pres.nbytes,
                    mp->ssl_state.keep_buff_len - pres.nbytes);
        }
        mp->ssl_state.keep_buff_len -= pres.nbytes;
        if (mp->ssl_state.keep_buff_len != 0) {
            return pres;
        }
        if (pres.nbytes == 0 || pres.flags) {
            return pres;
        }
        total_sent += pres.nbytes;
    }
    exb_assert_h(mp->ssl_state.keep_buff || !mp->ssl_state.keep_buff_size, "");
    if (mp->ssl_state.keep_buff && mp->ssl_state.keep_buff_size < tmp_buff_sz) {
        tmp_buff_sz = mp->ssl_state.keep_buff_size;
    }
    int nread = BIO_read(wbio, tmp_buff, tmp_buff_sz);
    if (nread <= 0 && !BIO_should_retry(wbio)) {
        return exb_make_io_result(0, EXB_IO_FLAG_IO_ERROR | EXB_IO_FLAG_CONN_FATAL);
    }
    else if (nread <= 0) {
        return exb_make_io_result(0, 0);
    }
    struct exb_io_result sres = try_send(mp->socket_fd, tmp_buff, nread);
    if (EXB_UNLIKELY(sres.nbytes < nread)) {
        if (!mp->ssl_state.keep_buff) {
            size_t keep_buff_sz = 0;
            exb_assert_h(!!mp->evloop, "");
            int rv = exb_evloop_alloc_buffer(mp->evloop, EXB_SSL_RW_BUFFER_SIZE, &mp->ssl_state.keep_buff, &keep_buff_sz);
            if (rv != EXB_OK) {
                return exb_make_io_result(0, EXB_IO_FLAG_CONN_FATAL);
            }
            mp->ssl_state.keep_buff_size = keep_buff_sz;
        }
        if (EXB_UNLIKELY(sres.nbytes > (mp->ssl_state.keep_buff_size - mp->ssl_state.keep_buff_len))) {
            return exb_make_io_result(0, EXB_IO_FLAG_CONN_FATAL);
        }
        memcpy(mp->ssl_state.keep_buff + mp->ssl_state.keep_buff_len,
              tmp_buff + sres.nbytes,
              nread - sres.nbytes);
        mp->ssl_state.keep_buff_len += nread - sres.nbytes;
    }
    total_sent += sres.nbytes;
    return exb_make_io_result(total_sent, sres.flags);
}

struct exb_io_result read_to_rbio_or_fail(struct exb_ssl_module *ssl_mod,
                                             struct exb_http_multiplexer *mp,
                                             BIO *rbio,
                                             char *tmp_buff,
                                             size_t tmp_buff_sz) 
{
    ssize_t nbytes = recv(mp->socket_fd, tmp_buff, tmp_buff_sz, MSG_DONTWAIT);
    
    if (nbytes < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return exb_make_io_result(0, 0);
        }
        else {
            return exb_make_io_result(0, EXB_IO_FLAG_IO_ERROR);
        }
    }
    else if (nbytes == 0) {
        return exb_make_io_result(0, EXB_IO_FLAG_CLIENT_CLOSED);
    }

    int n = BIO_write(rbio, tmp_buff, nbytes);
    if (n != nbytes) {
        // should do: if (n <= 0)  if (!BIO_sock_should_retry())
        // but in our case, we have no where to store the new bytes
        return exb_make_io_result(0, EXB_IO_FLAG_IO_ERROR | EXB_IO_FLAG_CONN_FATAL);
    }
    return exb_make_io_result(nbytes, 0);
}

struct exb_io_result exb_ssl_connection_read(struct exb_http_server_module *mod,
                                             struct exb_http_multiplexer *mp,
                                             char *output_buffer,
                                             size_t output_buffer_max)
{
    struct exb_ssl_module *ssl_mod = (struct exb_ssl_module *) mod;
    BIO *rbio = mp->ssl_state.rbio;
    BIO *wbio = mp->ssl_state.wbio;
    SSL *ssl  = mp->ssl_state.ssl_obj;
    exb_assert_h(ssl_mod && rbio && wbio && ssl, "");
    char buff[EXB_SSL_RW_BUFFER_SIZE];


    struct exb_io_result rres = read_to_rbio_or_fail(ssl_mod, mp, rbio, buff, sizeof buff);
    if (rres.flags != 0) {
        return exb_make_io_result(0, rres.flags);
    }
    fprintf(stderr, "read_to_rbio read %d bytes\n", (int)rres.nbytes);
    if (!(mp->ssl_state.flags & EXB_SSL_STATE_OPENSSL_ACCEPTED)) {
        for (int i=0; i<3; i++) {
            int rv = SSL_accept(ssl);
            if (rv == 1) {
                fprintf(stderr, "SSL_accept success on socket %d\n", mp->socket_fd);
                mp->ssl_state.flags |= EXB_SSL_STATE_OPENSSL_ACCEPTED;
                break; /*success*/
            }
            int ssl_err = SSL_get_error(ssl, rv);
            if (ssl_err == SSL_ERROR_WANT_READ || ssl_err == SSL_ERROR_WANT_WRITE) {
                struct exb_io_result sres = send_from_wbio_or_keep(ssl_mod,
                                                                    mp,
                                                                    wbio,
                                                                    buff,
                                                                    sizeof buff);
                fprintf(stderr, "send_from_Wbio wrote %d bytes\n", (int)sres.nbytes);
                if (sres.flags)
                    return exb_make_io_result(0, sres.flags); //0 because it's not plaintext bytes
                sres = read_to_rbio_or_fail(ssl_mod, mp, rbio, buff, sizeof buff);
                fprintf(stderr, "read_to_rbio read %d bytes\n", (int)sres.nbytes);
                if (sres.flags)
                    return exb_make_io_result(0, sres.flags); //0 because it's not plaintext bytes
                else if (!sres.nbytes)
                    break;
            }
            else {
                return exb_make_io_result(0, EXB_IO_FLAG_IO_ERROR | EXB_IO_FLAG_CONN_FATAL);
            }
        }
        rres = send_from_wbio_or_keep(ssl_mod,
                                      mp,
                                      wbio,
                                      buff,
                                      sizeof buff);
        fprintf(stderr, "send_from_Wbio wrote %d bytes\n", (int)rres.nbytes);
        if (rres.flags)
            return exb_make_io_result(0, rres.flags); //0 because it's not plaintext bytes
        rres = read_to_rbio_or_fail(ssl_mod, mp, rbio, buff, sizeof buff);
        fprintf(stderr, "read_to_rbio read %d bytes\n", (int)rres.nbytes);
        if (rres.flags)
            return exb_make_io_result(0, rres.flags); //0 because it's not plaintext bytes
    }
    int nread = SSL_read(mp->ssl_state.ssl_obj, output_buffer, output_buffer_max);
    if (nread <= 0) {
        int ssl_err_2 = SSL_get_error(ssl, nread);
        if (ssl_err_2 != SSL_ERROR_WANT_WRITE && ssl_err_2 != SSL_ERROR_WANT_READ) {
            return exb_make_io_result(0, EXB_IO_FLAG_IO_ERROR | EXB_IO_FLAG_CONN_FATAL);
        }
        return exb_make_io_result(0, 0);
    }
    fprintf(stderr, "SSL_read read %d bytes\n", (int)nread);
    return exb_make_io_result(nread, 0);
}
struct exb_io_result exb_ssl_connection_write(struct exb_http_server_module *mod,
                                              struct exb_http_multiplexer *mp,
                                              char *buffer,
                                              size_t buffer_len)
{
    struct exb_ssl_module *ssl_mod = (struct exb_ssl_module *) mod;
    BIO *rbio = mp->ssl_state.rbio;
    BIO *wbio = mp->ssl_state.wbio;
    SSL *ssl  = mp->ssl_state.ssl_obj;
    exb_assert_h(ssl_mod && rbio && wbio && ssl, "");

    char buff[EXB_SSL_RW_BUFFER_SIZE];

    int nbytes = SSL_write(ssl, buffer, buffer_len);
    if (nbytes <= 0) {
        int ssl_error = SSL_get_error(ssl, nbytes);
        if (ssl_error != SSL_ERROR_WANT_READ && ssl_error != SSL_ERROR_WANT_WRITE) {
            return exb_make_io_result(0, EXB_IO_ERR_SSL | EXB_IO_FLAG_CONN_FATAL);
        }
    }
    
    
    struct exb_io_result sres = send_from_wbio_or_keep(ssl_mod,
                                                       mp,
                                                       wbio,
                                                       buff,
                                                       sizeof buff);
    return exb_make_io_result(nbytes, sres.flags);
}



int exb_ssl_init(struct exb *exb,
                 struct exb_server *server,
                 char *module_args,
                 struct exb_http_server_module **module_out) 
{
    (void) module_args;
    struct exb_ssl_module *mod = exb_malloc(exb, sizeof(struct exb_ssl_module));
    if (!mod)
        return EXB_NOMEM_ERR;
    mod->exb_ref    = exb;
    mod->server_ref = server;
    mod->head.destroy = exb_ssl_destroy;
    int rv = exb_ssl_openssl_init(exb, server, mod);
    if (rv != EXB_OK) {
        exb_ssl_destroy((struct exb_http_server_module *)mod, exb);
        return rv;
    }
    struct exb_ssl_interface ssl_if = { .module = (struct exb_http_server_module *)mod,
                                        .ssl_connection_init = exb_ssl_connection_init,
                                        .ssl_connection_read = exb_ssl_connection_read,
                                        .ssl_connection_write = exb_ssl_connection_write,
                                        .ssl_connection_deinit = exb_ssl_connection_deinit 
                                    };
    rv = exb_server_set_ssl_interface(server, &ssl_if);
    if (rv != EXB_OK) {
        exb_ssl_destroy((struct exb_http_server_module *)mod, exb);
        return rv;
    }
    *module_out = (struct exb_http_server_module*)mod;
    return EXB_OK;
}
