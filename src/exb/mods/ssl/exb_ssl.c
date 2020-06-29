/*
This is a builtin module, it differs from other dynamically loaded modules in that it
is statically linked, and is a first class citizen in terms of configuration and loading

*/
#include "../../exb.h"
#include "../../http/http_server_module.h"
#include "../../http/http_request.h"
#include "../../exb_build_config.h"
#include "exb_ssl_config_entry.h"

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

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
    /*Each set of domain / certifcate pair will have a different context for SNI*/
    SSL_CTX *contexts[EXB_SNI_MAX_DOMAINS]; //Mapped to each domain
    
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

	const char *ciphers = NULL;
    const char *private_key_file = NULL;
    const char *public_key_file = NULL;
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

    if (SSL_CTX_set_session_id_context(ssl_ctx, "exband", 6) != 1) {
        exb_log_error(exb, "SSL_CTX_set_session_id_context(): %s", ERR_error_string(ERR_get_error(), NULL));
        goto on_error_1;
    }


    SSL_CTX_set_default_read_ahead(ssl_ctx, 1);
    SSL_CTX_set_mode(ssl_ctx, SSL_CTX_get_mode(ssl_ctx) | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
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
int exb_ssl_init(struct exb *exb,
                 struct exb_server *server,
                 char *module_args,
                 struct exb_http_server_module **module_out) 
{
    (void) module_args;
    struct exb_ssl_module *mod = exb_malloc(exb, sizeof(struct exb_ssl_module));
    if (!mod)
        return EXB_NOMEM_ERR;
    mod->exb_ref = exb;
    mod->head.destroy = exb_ssl_destroy;
    int rv = exb_ssl_openssl_init(exb, server, mod);
    if (rv != EXB_OK) {
        exb_ssl_destroy((struct exb_http_server_module *)mod, exb);
        return rv;
    }    
    *module_out = (struct exb_http_server_module*)mod;
    return EXB_OK;
}
