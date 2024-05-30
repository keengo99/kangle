/*
 * ssl_utils.h
 *
 *  Created on: 2010-8-16
 *      Author: keengo
 */

#ifndef SSL_UTILS_H_
#define SSL_UTILS_H_
#include <memory>
#include "global.h"
#include "KEnvInterface.h"
#include "kgl_ssl.h"
#ifdef KSOCKET_SSL

#define strcEQ(s1,s2)    (strcasecmp(s1,s2)    == 0)
#define strcNE(s1,s2)    (strcasecmp(s1,s2)    != 0)
#define strcEQn(s1,s2,n) (strncasecmp(s1,s2,n) == 0)
#define strcNEn(s1,s2,n) (strncasecmp(s1,s2,n) != 0)
#define EVP_PKEY_key_type(k)              (EVP_PKEY_type(k->type))

#define X509_NAME_get_entries(xs)         (xs->entries)
#define X509_REVOKED_get_serialNumber(xs) (xs->serialNumber)

#define X509_get_signature_algorithm(xs) (xs->cert_info->signature->algorithm)
#define X509_get_key_algorithm(xs)       (xs->cert_info->key->algor->algorithm)

#define X509_NAME_ENTRY_get_data_ptr(xs) (xs->value->data)
#define X509_NAME_ENTRY_get_data_len(xs) (xs->value->length)

#define SSL_CTX_get_extra_certs(ctx)       (ctx->extra_certs)
#define SSL_CTX_set_extra_certs(ctx,value) {ctx->extra_certs = value;}

#define SSL_CIPHER_get_name(s)             (s->name)
#define SSL_CIPHER_get_valid(s)            (s->valid)

#define SSL_SESSION_get_session_id(s)      (s->session_id)
#define SSL_SESSION_get_session_id_length(s) (s->session_id_length)
struct kgl_auto_ssl_free
{
	void operator()(char* t) const noexcept {
		OPENSSL_free(t);
	}
};

using kgl_auto_ssl_var = std::unique_ptr<char, kgl_auto_ssl_free>;

bool make_ssl_env(KEnvInterface *env, SSL *ssl);
kgl_auto_ssl_var ssl_var_lookup(SSL *ssl, const char *var);
kgl_auto_ssl_var ssl_ctx_var_lookup(SSL_CTX *ssl_ctx, const char *var);
#endif
#endif /* SSL_UTILS_H_ */
