/*
 * ssl_utils.cpp
 *
 *  Created on: 2010-8-16
 *      Author: keengo
 */

#include "ssl_utils.h"
#include "KStringBuf.h"
#include "KHttp2.h"
#ifdef KSOCKET_SSL
#define KGL_HTTP_NPN_ADVERTISE  "\x08http/1.1"

static const char *ssl_vars[] = { "CERT_ISSUER", "CERT_SUBJECT",
		"CERT_KEYSIZE", "CERT_SERIALNUMBER", "CERT_SERVER_ISSUER",
		"CERT_SERVER_SUBJECT", "CERT_SERVER_SERIALNUMBER", NULL };
static kgl_auto_ssl_var ssl_var_lookup_ssl_cert_serial(X509 *xs) {
	char *result;
	BIO *bio;
	int n;
	if ((bio = BIO_new(BIO_s_mem())) == NULL)
		return nullptr;
	i2a_ASN1_INTEGER(bio, X509_get_serialNumber(xs));
	n = (int)BIO_pending(bio);
	result = (char *)malloc(n+1);
	n = BIO_read(bio, result, n);
	result[n] = '\0';
	BIO_free(bio);
	return kgl_auto_ssl_var(result);
}
static void ssl_var_lookup_ssl_cipher_bits(SSL *ssl, int *usekeysize, int *algkeysize) {
	const SSL_CIPHER *cipher;
	*usekeysize = 0;
	*algkeysize = 0;
	if (ssl != NULL) {
		if ((cipher = SSL_get_current_cipher(ssl)) != NULL)
			*usekeysize = SSL_CIPHER_get_bits((SSL_CIPHER*)cipher, algkeysize);
	}
	return;
}
static kgl_auto_ssl_var ssl_var_lookup_ssl_cert(X509 *xs, const char *var) {
	kgl_auto_ssl_var result;
	X509_NAME *xsname;
	ASN1_TIME *tm;
	if (strcEQ(var, "SERIALNUMBER")) {
		result = ssl_var_lookup_ssl_cert_serial(xs);
	} else if (strcEQ(var, "SUBJECT")) {
		xsname = X509_get_subject_name(xs);
		result = kgl_auto_ssl_var(X509_NAME_oneline(xsname, NULL, 0));
	} else if (strcEQ(var, "ISSUER")) {
		xsname = X509_get_issuer_name(xs);
		result = kgl_auto_ssl_var(X509_NAME_oneline(xsname, NULL, 0));
	} else if (strcEQ(var, "NOTBEFORE")) {
		tm = X509_get_notBefore(xs);
		if (tm) {
			result = kgl_auto_ssl_var((char*)OPENSSL_malloc(tm->length + 1));
			if (result) {
				kgl_memcpy(result.get(), tm->data, tm->length);
				result.get()[tm->length] = '\0';
			}
		}
	} else if (strcEQ(var, "NOTAFTER")) {
		tm = X509_get_notAfter(xs);
		if (tm) {
			result = kgl_auto_ssl_var((char*)OPENSSL_malloc(tm->length + 1));
			if (result) {
				kgl_memcpy(result.get(), tm->data, tm->length);
				result.get()[tm->length] = '\0';
			}
		}
	}
	return result;
}
kgl_auto_ssl_var ssl_ctx_var_lookup(SSL_CTX *ssl_ctx, const char *var)
{
#if OPENSSL_VERSION_NUMBER >= 0x10002000L
	X509 *xs;
	if ((xs = SSL_CTX_get0_certificate(ssl_ctx)) != NULL)
			return ssl_var_lookup_ssl_cert(xs, var);
#endif
	return nullptr;
}
kgl_auto_ssl_var ssl_var_lookup(SSL *ssl, const char *var) {
	X509 *xs;
	if (strcEQ(var,"CERT_KEYSIZE")) {
		int user_key_size;
		int alg_key_size;
		ssl_var_lookup_ssl_cipher_bits(ssl, &user_key_size, &alg_key_size);
		kgl_auto_ssl_var result = kgl_auto_ssl_var((char*)OPENSSL_malloc(8));
		int len = snprintf(result.get(), 7, "%d", user_key_size);
		result.get()[len] = '\0';
		return result;
	} else if (strcEQn(var, "CERT_SERVER_", 12)) {
		if ((xs = SSL_get_certificate(ssl)) != NULL)
			return ssl_var_lookup_ssl_cert(xs, var + 12);
#ifdef HEADER_X509_H
	} else if (strcEQn(var, "CERT_", 5)) {
		if ((xs = SSL_get_peer_certificate(ssl)) != NULL) {
			auto result = ssl_var_lookup_ssl_cert(xs, var + 5);
			X509_free(xs);
			return result;
		}
#endif
	}
	return nullptr;
}
bool make_ssl_env(KEnvInterface *env, SSL *ssl) {
	for (int i = 0;; i++) {
		if (ssl_vars[i] == NULL) {
			break;
		}
		auto result = ssl_var_lookup(ssl, ssl_vars[i]);
		if (result) {
			env->addEnv(ssl_vars[i], result.get());
		}
	}
	return true;
}
#endif

