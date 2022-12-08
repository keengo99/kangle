#include "KSSLSniContext.h"
#include "KConfig.h"
#include "KVirtualHostManage.h"
#ifdef KSOCKET_SSL
KSSLSniContext::~KSSLSniContext()
{
	if (svh) {
		svh->release();
	}
}
void *kgl_create_ssl_sni(SSL *ssl, KOPAQUE server_ctx, const char *hostname)
{
	KSSLSniContext *sni = new KSSLSniContext;
	sni->result = conf.gvm->queryVirtualHost((KVirtualHostContainer *)server_ctx, &sni->svh, hostname, 0);
	if (sni->result != query_vh_success) {
		return sni;
	}
#ifdef ENABLE_SVH_SSL
	if (sni->svh->ssl_ctx) {
		SSL_set_SSL_CTX(ssl, kgl_get_ssl_ctx(sni->svh->ssl_ctx));
		return sni;
	}
#endif
	if (sni->svh->vh && sni->svh->vh->ssl_ctx) {
		SSL_set_SSL_CTX(ssl, kgl_get_ssl_ctx(sni->svh->vh->ssl_ctx));
	}
	return sni;
}
void kgl_free_ssl_sni(void *sni)
{
	KSSLSniContext *s = (KSSLSniContext *)sni;
	delete s;
}
query_vh_result kgl_use_ssl_sni(kconnection *c, KSubVirtualHost **svh)
{
	KSSLSniContext *sni = (KSSLSniContext *)c->sni;
	*svh = sni->svh;
	sni->svh = NULL;
	query_vh_result ret = sni->result;
	delete sni;
	c->sni = NULL;
	return ret;
}
#endif

