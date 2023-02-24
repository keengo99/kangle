#ifndef KDYNAMICLISTEN_H
#define KDYNAMICLISTEN_H
//#include <map>
#include "kserver.h"
#include "do_config.h"
#include "krbtree.h"
#include "KVirtualHostContainer.h"
#include "KHttp3.h"
/*
* 侦听端口管理。
* 即由virtualhost的<bind>!ip:port</bind>
*/
class WhmContext;
class KVirtualHost;
class KListenKey
{
public:
	KListenKey()
	{
		ssl = 0;
		port = 0;
#ifdef ENABLE_PROXY_PROTOCOL
		proxy = 0;
		ssl_proxy = 0;
#endif
#ifdef ENABLE_TPROXY
		tproxy = 0;
#endif
#ifdef WORK_MODEL_TCP
		tcp = 0;
#endif
		global = 0;
		dynamic = 0;
		manage = 0;
		ipv4 = false;
#ifdef ENABLE_HTTP3
		h3 = 0;
#endif
	}
	int get_socket_flag()
	{
		int flag = (ipv4 ? KSOCKET_ONLY_IPV4 : KSOCKET_ONLY_IPV6);
		KBIT_SET(flag, KSOCKET_FASTOPEN);
		return flag;
	}
	void SetFlag(KListenKey *a)
	{
		this->manage += a->manage;
		this->global += a->global;
		this->dynamic += a->dynamic;
		this->ssl += a->ssl;
#ifdef ENABLE_PROXY_PROTOCOL
		this->proxy += a->proxy;
		this->ssl_proxy += a->ssl_proxy;
#endif
	
#ifdef ENABLE_TPROXY
		this->tproxy += a->tproxy;
#endif
#ifdef WORK_MODEL_TCP
		this->tcp += a->tcp;
#endif
#ifdef ENABLE_HTTP2
		this->h2 += a->h2;
#endif
#ifdef ENABLE_HTTP3
		this->h3 += a->h3;
#endif
	}
	void ClearFlag(KListenKey *a)
	{
		this->manage -= a->manage;
		kassert(this->manage >= 0);
		this->global -= a->global;
		kassert(this->global >= 0);
		this->dynamic -= a->dynamic;
		kassert(this->dynamic >= 0);
		this->ssl -= a->ssl;
		kassert(this->ssl >= 0);
#ifdef ENABLE_PROXY_PROTOCOL
		this->proxy -= a->proxy;
		this->ssl_proxy -= a->ssl_proxy;
		kassert(this->proxy >= 0);
		kassert(this->ssl_proxy >= 0);
#endif
#ifdef ENABLE_TPROXY
		this->tproxy -= a->tproxy;
		kassert(this->tproxy >= 0);
#endif
#ifdef WORK_MODEL_TCP
		this->tcp -= a->tcp;
		kassert(this->tcp >= 0);
#endif
#ifdef ENABLE_HTTP2
		this->h2 -= a->h2;
#endif
#ifdef ENABLE_HTTP3
		this->h3 -= a->h3;
#endif
	}
	int cmp(const KListenKey *a) const
	{
		int ret = port - a->port;
		if (ret > 0) {
			return 1;
		}
		if (ret < 0) {
			return -1;
		}
		return strcmp(ip.c_str(),a->ip.c_str());
	}
	void set_work_model(uint32_t *flags)
	{
		if (manage > 0) {
			KBIT_SET(*flags, WORK_MODEL_MANAGE);
		} else {
			KBIT_CLR(*flags, WORK_MODEL_MANAGE);
		}
#ifdef ENABLE_PROXY_PROTOCOL
		if (proxy) {
			KBIT_SET(*flags, WORK_MODEL_PROXY);
			KBIT_CLR(*flags, WORK_MODEL_SSL_PROXY);
		} else if (ssl_proxy) {
			KBIT_SET(*flags, WORK_MODEL_SSL_PROXY);
			KBIT_CLR(*flags, WORK_MODEL_PROXY);
		} else {
			KBIT_CLR(*flags, WORK_MODEL_PROXY);
			KBIT_CLR(*flags, WORK_MODEL_SSL_PROXY);
		}
#endif
#ifdef WORK_MODEL_TCP
		if (tcp) {
			KBIT_SET(*flags, WORK_MODEL_TCP);
		} else {
			KBIT_CLR(*flags, WORK_MODEL_TCP);
		}
#endif
#ifdef ENABLE_HTTP2
		kgl_set_flag(flags, KGL_SERVER_H2, h2 > 0);
#endif
#ifdef ENABLE_TPROXY
		if (tproxy) {
			KBIT_SET(*flags, WORK_MODEL_TPROXY);
		} else {
			KBIT_CLR(*flags, WORK_MODEL_TPROXY);
		}
#endif

		if (dynamic) {
			KBIT_SET(*flags, KGL_SERVER_DYNAMIC);
		} else {
			KBIT_CLR(*flags, KGL_SERVER_DYNAMIC);
		}
		if (global) {
			KBIT_SET(*flags, KGL_SERVER_GLOBAL);
		} else {
			KBIT_CLR(*flags, KGL_SERVER_GLOBAL);
		}

	}
	std::string ip;
	int port;
	bool ipv4;
	int32_t manage;
	int32_t dynamic;
	int32_t global;
	int32_t ssl;
#ifdef ENABLE_PROXY_PROTOCOL
	int32_t proxy;
	int32_t ssl_proxy;
#endif
#ifdef ENABLE_HTTP2
	int32_t h2 = 0;
#endif
#ifdef ENABLE_HTTP3
	int32_t h3;
#endif
#ifdef ENABLE_TPROXY
	int32_t tproxy;
#endif
#ifdef WORK_MODEL_TCP
	int32_t tcp;
#endif
};
class KListen
{
public:
	KListen(KListenKey *key, kserver *server)
	{
		this->key = key;
		this->server = server;
#ifdef ENABLE_HTTP3
		this->h3_server = nullptr;
#endif
	}
	~KListen()
	{
		if (server) {
			kserver_release(server);
		}
#ifdef ENABLE_HTTP3
		if (h3_server) {
			h3_server->release();
		}
#endif
		kassert(key);
		delete key;
	}
	void sync_flag(kgl_ssl_ctx *ssl_ctx);
	bool IsEmpty()
	{
		return key->dynamic==0 && key->global==0;
	}
	KListenKey *key;
	kserver *server;	
#ifdef ENABLE_HTTP3
	KHttp3Server* h3_server;
#endif
};
/**
* 由virtualHostManager加锁调用
*/
class KDynamicListen
{
public:
	KDynamicListen()
	{
		failedTries = 0;
		rbtree_init(&listens);
	}
	kserver *RefsServer(u_short port);
	void AddDynamic(const char *listen,KVirtualHost *vh);
	void RemoveDynamic(const char *listen,KVirtualHost *vh);
	bool AddGlobal(KListenHost *lh);
	void RemoveGlobal(KListenHost *lh);
	void BindGlobalVirtualHost(KVirtualHost *vh);
	void UnbindGlobalVirtualHost(KVirtualHost *vh);
	void GetListenHtml(std::stringstream &s);
	void GetListenWhm(WhmContext *ctx);
	void QueryDomain(domain_t host,int port, WhmContext *ctx);
	void Close();
	bool IsEmpty()
	{
		return rbtree_is_empty(&listens);
	}
private:
	krb_tree listens;
	//KListen *Find(KListenKey *lk);
	void add(KListenKey *key, KSslConfig *ssl_config, KVirtualHost *vh);
	void Delete(KListenKey *key,KVirtualHost *vh);
	void parse_port(const char *port, KListenKey *lk,KVirtualHost *vh);
	void parseListen(const char *listen,std::list<KListenKey *> &lk, KVirtualHost* vh);
	void parseListen(KListenHost *lh,std::list<KListenKey *> &lk);
	bool init_listen(KListenKey *lk, KListen *listen, kgl_ssl_ctx *ssl_ctx);
	void getListenKey(KListenHost *lh,bool ipv4,std::list<KListenKey *> &lk);
	void getListenKey(KListenHost *lh,const char *port,bool ipv4,std::list<KListenKey *> &lk);
	int failedTries;
};
void kgl_vhc_remove_global_vh(KVirtualHostContainer* vhc, KVirtualHost *vh);
void kgl_vhc_add_global_vh(KVirtualHostContainer* vhc, KVirtualHost *vh);
inline KVirtualHostContainer* get_vh_container(kserver* server)
{
	return (KVirtualHostContainer*)kserver_get_opaque(server);
}
#endif
