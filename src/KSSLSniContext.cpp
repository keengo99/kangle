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
SSL_CTX* kgl_lookup_sni(KOPAQUE server_ctx, const char* hostname, KSSLSniContext *sni)
{
	sni->result = conf.gvm->queryVirtualHost((KVirtualHostContainer*)server_ctx, &sni->svh, hostname, 0);
	if (sni->result != query_vh_success) {
		return nullptr;
	}

#ifdef ENABLE_SVH_SSL
	if (sni->svh->ssl_ctx) {
		return kgl_get_ssl_ctx(sni->svh->ssl_ctx);
	}
#endif
	if (sni->svh->vh && sni->svh->vh->ssl_ctx) {
		return kgl_get_ssl_ctx(sni->svh->vh->ssl_ctx);
	}
	return nullptr;
}
void* kgl_lookup_sni_ssl_ctx(KOPAQUE server_ctx, const char *hostname, SSL_CTX **ssl_ctx)
{
	KSSLSniContext* sni = new KSSLSniContext;
	*ssl_ctx = kgl_lookup_sni(server_ctx, hostname, sni);
	return sni;
}
void kgl_free_ssl_sni(void *sni)
{
	KSSLSniContext *s = (KSSLSniContext *)sni;
	delete s;
}
query_vh_result kgl_use_ssl_sni(void *s, KSubVirtualHost **svh)
{
	KSSLSniContext *sni = (KSSLSniContext *)s;
	*svh = sni->svh;
	sni->svh = NULL;
	query_vh_result ret = sni->result;
	kgl_free_ssl_sni(sni);
	return ret;
}
#endif

