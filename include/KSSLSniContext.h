#ifndef KSSLSNICONTEXT_H
#define KSSLSNICONTEXT_H
#include "kgl_ssl.h"
#include "KVirtualHostContainer.h"
#ifdef KSOCKET_SSL
class KSSLSniContext
{
public:
	KSSLSniContext()
	{
		svh = NULL;
		result = query_vh_unknow;
	}
	~KSSLSniContext();
	query_vh_result result;
	KSubVirtualHost *svh;
};
void *kgl_lookup_sni_ssl_ctx(KOPAQUE server_ctx, const char *hostname, SSL_CTX **ssl_ctx);
void kgl_free_ssl_sni(void *sni);
query_vh_result kgl_use_ssl_sni(void *sni, KSubVirtualHost **svh);
#endif
#endif

