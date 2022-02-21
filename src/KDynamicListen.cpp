#include "KDynamicListen.h"
#include "KVirtualHost.h"
#include "ksocket.h"
#include "KServerListen.h"
#include "KVirtualHostManage.h"
#include "kselector_manager.h"
#include "KPreRequest.h"
#include "lib.h"
#include "WhmContext.h"
#define DEFAULT_IPV4_IP  "0.0.0.0"
#define DEFAULT_IPV6_IP  "::"

static int listen_key_cmp(void *key, void *data)
{
	KListenKey *lk = (KListenKey *)key;
	KListen *listen = (KListen *)data;
	return lk->cmp(listen->key);
}
static void kserver_clean_ctx(void *ctx)
{
	KVirtualHostContainer *vhc = (KVirtualHostContainer *)ctx;
	delete vhc;
}
void kserver_remove_vh(kserver *server, KVirtualHost *vh)
{
	KVirtualHostContainer *vhc = (KVirtualHostContainer *)server->ctx;
#ifndef HTTP_PROXY
	std::list<KSubVirtualHost *>::iterator it2;
	for (it2 = vh->hosts.begin(); it2 != vh->hosts.end(); it2++) {
		vhc->del((*it2)->bind_host,(*it2)->wide,(*it2));
	}
#endif
}
bool kserver_start(kserver *server,const KListenKey *lk, kgl_ssl_ctx *ssl_ctx)
{
	int flag = (lk->ipv4 ? KSOCKET_ONLY_IPV4 : KSOCKET_ONLY_IPV6);
	SET(flag, KSOCKET_FASTOPEN);
	if (!kserver_open(server, lk->ip.c_str(), lk->port, flag, ssl_ctx)) {
		return false;
	}
	return kserver_accept(server);
}
void kserver_bind_vh(kserver *server, KVirtualHost *vh,bool high)
{
	KVirtualHostContainer *vhc = (KVirtualHostContainer *)server->ctx;
	std::list<KSubVirtualHost *>::iterator it2;
	for (it2 = vh->hosts.begin(); it2 != vh->hosts.end(); it2++) {
		vhc->add((*it2)->bind_host, (*it2)->wide, high ? kgl_bind_high : kgl_bind_low,(*it2));
	}
}
void kserver_add_global_vh(kserver *server, KVirtualHost *vh)
{
#ifndef HTTP_PROXY
	if (vh->binds.empty()) {
		kserver_bind_vh(server,vh, false);
	}
#endif
}
void kserver_remove_global_vh(kserver *server, KVirtualHost *vh)
{
#ifndef HTTP_PROXY
	if (vh->binds.empty()) {
		kserver_remove_vh(server,vh);
	}
#endif
}
#ifdef KSOCKET_SSL
kgl_ssl_ctx *kserver_load_ssl(kserver *server, KSslConfig *ssl_config)
{
	server->ssl = 1;
	kgl_ssl_ctx *ssl_ctx = ssl_config->GetSSLCtx(&server->http2);
	if (ssl_ctx) {
		SET(server->flags, WORK_MODEL_SSL);
	}
	return ssl_ctx;
}
#endif
static bool kserver_is_empty(kserver *server)
{
	KVirtualHostContainer *vhc = (KVirtualHostContainer *)server->ctx;
	return vhc->isEmpty();
}
static kserver *kserver_new()
{
	KVirtualHostContainer *vhc = new KVirtualHostContainer();
	kserver *server = kserver_init();
	kserver_bind(server, handle_connection, kserver_clean_ctx, vhc);
	return server;
}
void KListen::SyncFlag()
{
	key->set_work_model(server);
}
void KDynamicListen::Delete(KListenKey *key,KVirtualHost *vh)
{
	krb_node *node = rbtree_find(&listens, key, listen_key_cmp);
	kassert(node);
	if (node == NULL) {
		delete key;
		return;
	}
	KListen *listen = (KListen *)node->data;
	listen->key->ClearFlag(key);
	if (vh) {
		kserver_remove_vh(listen->server, vh);
	}
	if (key->global > 0 && listen->key->global == 0) {
		conf.gvm->UnBindGlobalVirtualHost(listen->server);
	}
	delete key;
	if (listen->IsEmpty()) {
		kassert(kserver_is_empty(listen->server));
		kserver_close(listen->server);		
		rbtree_remove(&listens, node);
		delete listen;
	} else {
		listen->SyncFlag();
#ifdef KSOCKET_SSL
		if (listen->key->ssl == 0 && listen->server->ssl) {
			//remove ssl
			listen->server->ssl = 0;
			CLR(listen->server->flags, WORK_MODEL_SSL);
			kserver_set_ssl_ctx(listen->server,NULL);
		}
#endif
	}
}
kserver *KDynamicListen::Add(KListenKey *key, KSslConfig *ssl_config)
{
	KListen *listen;
	kserver *server = NULL;
	kgl_ssl_ctx *ssl_ctx = NULL;
	int new_flag = 0;
	bool need_load_ssl = false;
	krb_node *node = rbtree_insert(&listens, key, &new_flag, listen_key_cmp);
	if (new_flag) {
		server = kserver_new();
		listen = new KListen(key, server);
		node->data = listen;
		if (key->global>0) {
			conf.gvm->BindGlobalVirtualHost(server);
		}
		need_load_ssl = key->ssl > 0;
	} else {
		listen = (KListen *)node->data;
		server = listen->server;
		if (key->global > 0 && !server->global) {
			conf.gvm->BindGlobalVirtualHost(server);
		}
		listen->key->SetFlag(key);
		if (key->ssl > 0) {
			if (!TEST(listen->server->flags, WORK_MODEL_SSL) || key->global>0) {
				//无ssl或者是全局侦听，要更新ssl
				need_load_ssl = true;
			}
		}
		delete key;
	}
#ifdef KSOCKET_SSL
	if (need_load_ssl) {
		ssl_ctx = kserver_load_ssl(server, ssl_config);
#ifdef ENABLE_HTTP2
		server->http2 = ssl_config->http2;
#endif
#ifdef KSOCKET_SSL
		server->early_data = ssl_config->early_data;
#endif
	}
#endif
	listen->SyncFlag();
	if (!server->started) {
		initListen(listen->key, server, ssl_ctx);
#ifdef KSOCKET_SSL
	} else if (ssl_ctx) {
		kserver_set_ssl_ctx(server, (kgl_ssl_ctx *)ssl_ctx);
#endif
	}
	return server;
}
void KDynamicListen::AddDynamic(const char *str,KVirtualHost *vh)
{
	std::list<KListenKey *> lk;
	parseListen(str,lk);
	if (lk.empty()) {
		return;
	}
	std::list<KListenKey *>::iterator it;
	for (it=lk.begin();it!=lk.end();it++) {
		kserver *server = Add((*it), vh);
		kserver_bind_vh(server,vh,true);
	}
}
void KDynamicListen::RemoveDynamic(const char *str,KVirtualHost *vh)
{
	std::list<KListenKey *> lk;
	parseListen(str,lk);
	if (lk.empty()) {
		return;
	}
	std::list<KListenKey *>::iterator it;
	for (it=lk.begin();it!=lk.end();it++) {
		Delete((*it), vh);
	}
}
void KDynamicListen::RemoveGlobal(KListenHost *lh)
{
	if (!lh->listened) {
		return;
	}
	std::list<KListenKey *> lk;
	parseListen(lh, lk);
	kassert(!lk.empty());
	lh->listened = false;
	std::list<KListenKey *>::iterator it2;
	for (it2 = lk.begin(); it2 != lk.end(); it2++) {
		Delete((*it2),NULL);
	}
}
bool KDynamicListen::AddGlobal(KListenHost *lh)
{
	std::list<KListenKey *> lk;
	kassert(!lh->listened);
	parseListen(lh,lk);
	if (lk.empty()) {
		return false;
	}
	lh->listened = true;
	std::list<KListenKey *>::iterator it2;
	for (it2=lk.begin();it2!=lk.end();it2++) {
		Add((*it2), lh);
	}
	return true;
}
kserver *KDynamicListen::RefsServer(u_short port)
{
	KListenKey key;
	key.port = port;
	int result = 0;
	krb_node *node = rbtree_find2(&listens, &key, listen_key_cmp, &result);
	if (node == NULL) {
		return NULL;
	}
	KListen *listen = (KListen *)node->data;
	if (listen->key->port == port) {
		kserver *server = listen->server;
		kserver_refs(server);
		return server;
	}
	return NULL;
}

void KDynamicListen::parseListen(KListenHost *lh,std::list<KListenKey *> &lk)
{
	if (strcmp(lh->ip.c_str(),"*")==0) {
		getListenKey(lh,true,lk);
#ifdef KSOCKET_IPV6
		getListenKey(lh,false,lk);
#endif
		return;
	}		
	bool ipv4 = true;
#ifdef KSOCKET_IPV6
	sockaddr_i addr;
	if (ksocket_getaddr(lh->ip.c_str(), 0, AF_UNSPEC, AI_NUMERICHOST,&addr)) {
		if(PF_INET == addr.v4.sin_family){
			ipv4 = true;
		} else {
			ipv4 = false;
		}
	}
#endif
	getListenKey(lh,ipv4,lk);
	return;
}
void KDynamicListen::parseListen(const char *listen,std::list<KListenKey *> &lks)
{
	char *buf = strdup(listen);
	char *p = strrchr(buf,':');
	if (p) {
		*p = '\0';
		p++;
		KListenKey *lk = new KListenKey;
		lk->dynamic = 1;
		parse_port(p, lk);
		if (strcmp(buf,"*")==0) {
			lk->ip = DEFAULT_IPV4_IP;
			lk->ipv4 = true;
			lks.push_back(lk);			
#ifdef KSOCKET_IPV6
			lk = new KListenKey;
			parse_port(p, lk);
			lk->ip = DEFAULT_IPV6_IP;
			lk->ipv4 = false;
			lk->dynamic = 1;
			lks.push_back(lk);
#endif
		} else {		
			lk->ip = buf;
#ifdef KSOCKET_IPV6	
			sockaddr_i addr;
			if (ksocket_getaddr(buf, 0, AF_UNSPEC, AI_NUMERICHOST, &addr)) {
				if(PF_INET == addr.v4.sin_family){
					lk->ipv4 = true;
				} else {
					lk->ipv4 = false;
				}
			}
#else
			lk->ipv4 = true;
#endif
			lks.push_back(lk);
		}
	}
	free(buf);
}
bool KDynamicListen::initListen(const KListenKey *lk,kserver *server, kgl_ssl_ctx *ssl_ctx)
{
	kassert(is_selector_manager_init());
	return kserver_start(server, lk, ssl_ctx);
}
void KDynamicListen::getListenKey(KListenHost *lh,bool ipv4,std::list<KListenKey *> &lk)
{
	char *buf = strdup(lh->port.c_str());
	char *hot = buf;
	while (*hot) {
		char *p = strchr(hot,',');
		if (p) {
			*p++ = '\0';
		}
		if (IS_DIGIT(*hot)) {
			getListenKey(lh, hot, ipv4, lk);
		}
		if (p==NULL) {
			break;
		}
		hot = p;
	}
	free(buf);
}
void KDynamicListen::getListenKey(KListenHost *lh,const char *port,bool ipv4,std::list<KListenKey *> &lk)
{
	KListenKey *key = new KListenKey;
	key->global = 1;
	key->ssl = TEST(lh->model,WORK_MODEL_SSL) > 0;
	key->manage = TEST(lh->model, WORK_MODEL_MANAGE) > 0;
#ifdef WORK_MODEL_TCP
	key->tcp = TEST(lh->model, WORK_MODEL_TCP) > 0;
#endif
	key->ipv4 = ipv4;
	parse_port(port, key);
	if (lh->ip=="*") {
		if (ipv4) {
			key->ip = DEFAULT_IPV4_IP;
		} else {
			key->ip = DEFAULT_IPV6_IP;
		}
	} else {
		key->ip = lh->ip;
	}
	lk.push_back(key);
}
static iterator_ret bind_global_virtual_host_iterator(void *data, void *argv)
{
	kserver *server = ((KListen *)data)->server;
	if (server->global) {
		kserver_add_global_vh(server, (KVirtualHost *)argv);
	}
	return iterator_continue;
}
static iterator_ret unbind_global_virtual_host_iterator(void *data, void *argv)
{
	kserver *server = ((KListen *)data)->server;
	if (server->global) {
		kserver_remove_global_vh(server, (KVirtualHost *)argv);
	}
	return iterator_continue;
}
void KDynamicListen::BindGlobalVirtualHost(KVirtualHost *vh)
{
	rbtree_iterator(&listens, bind_global_virtual_host_iterator, vh);
}
static iterator_ret close_server_iterator(void *data, void *argv)
{
	kserver *server = ((KListen *)data)->server;
	kserver_close(server);
	return iterator_continue;
}
void KDynamicListen::Close()
{
	rbtree_iterator(&listens, close_server_iterator, NULL);
}
void KDynamicListen::parse_port(const char *p, KListenKey *lk)
{
	lk->port = atoi(p);
	if (strchr(p, 's')) {
		lk->ssl = 1;
	}
#ifdef ENABLE_TPROXY
	if (strchr(p, 'x')) {
		lk->tproxy = 1;
	}
#endif
#ifdef WORK_MODEL_TCP
	if (strchr(p, 't')) {
		lk->tcp = 1;
	}
#endif
#ifdef WORK_MODEL_PROXY
	if (strchr(p, 'P')) {
		lk->proxy = 1;
		lk->ssl_proxy = 0;
	} else if(strchr(p, 'p')) {
		lk->ssl_proxy = 1;
		lk->proxy = 0;
	}
#endif
}

void KDynamicListen::UnbindGlobalVirtualHost(KVirtualHost *vh)
{
	rbtree_iterator(&listens, unbind_global_virtual_host_iterator, vh);
}
static iterator_ret listen_whm_iterator(void *data, void *argv)
{
	KListen *listen = (KListen *)data;
	kassert(!listen->IsEmpty());
	WhmContext *ctx = (WhmContext *)argv;
	kserver *server = listen->server;
	if (server->started) {
		std::stringstream s;
		bool unix_socket = false;
#ifdef KSOCKET_UNIX
		if (server->addr.v4.sin_family == AF_UNIX) {
			unix_socket = true;
			s << "<ip>" << ksocket_unix_path(&server->un_addr) << "</ip>";
			s << "<port>-</port>";
		} else {
#endif
			char ip[MAXIPLEN];
			ksocket_sockaddr_ip(&server->addr, ip, sizeof(ip));
			s << "<ip>" << ip << "</ip>";
			s << "<port>" << ksocket_addr_port(&server->addr) << "</port>";
#ifdef KSOCKET_UNIX
		}
#endif
#ifdef WORK_MODEL_PROXY
		if (TEST(server->flags, WORK_MODEL_PROXY)) {
			s << "<proxy>1</proxy>";
		} else if (TEST(server->flags, WORK_MODEL_SSL_PROXY)) {
			s << "<ssl_proxy>1</ssl_proxy>";
		}
#endif
#ifdef WORK_MODEL_TPROXY
		if (TEST(server->flags, WORK_MODEL_TPROXY)) {
			s << "<tproxy>1</tproxy>";
		}
#endif
		s << "<service>" << getWorkModelName(server->flags) << "</service>";
		s << "<tcp_ip>";
		if (unix_socket) {
			s << "unix";
		} else if (server->ss) {
			s << (server->addr.v4.sin_family == PF_INET ? "4" : "6");
		}
		s << "</tcp_ip>";
		s << "<global>" << (int)listen->key->global << "</global>";
		s << "<dynamic>" << (int)listen->key->dynamic << "</dynamic>";
		s << "<multi_bind>" << (is_server_multi_selectable(server) ? 1 : 0) << "</multi_bind>";
		s << "<refs>" << katom_get((void *)&server->refs) << "</refs>";
		ctx->add("listen", s.str().c_str(), false);
	}
	return iterator_continue;
}
void KDynamicListen::GetListenWhm(WhmContext *ctx)
{
	rbtree_iterator(&listens, listen_whm_iterator, ctx);
}
static iterator_ret listen_html_iterator(void *data, void *argv)
{
	KListen *listen = (KListen *)data;
	std::stringstream *s = (std::stringstream *)argv;
	kserver *server = listen->server;
	if (server->started) {
		*s << "<tr>";
		bool unix_socket = false;
#ifdef KSOCKET_UNIX
		if (server->addr.v4.sin_family == AF_UNIX) {
			unix_socket = true;
			*s << "<td>" << ksocket_unix_path(&server->un_addr) << "</td>";
			*s << "<td>-";
		} else {
#endif
			char ip[MAXIPLEN];
			ksocket_sockaddr_ip(&server->addr, ip, sizeof(ip));
			*s << "<td>" << ip << "</td>";
			*s << "<td>" << ksocket_addr_port(&server->addr);
#ifdef KSOCKET_UNIX
		}
#endif
#ifdef WORK_MODEL_PROXY
		if (TEST(server->flags, WORK_MODEL_PROXY)) {
			*s << "P";
		} else 	if (TEST(server->flags, WORK_MODEL_SSL_PROXY)) {
			*s << "p";
		}
#endif
#ifdef WORK_MODEL_TPROXY
		if (TEST(server->flags, WORK_MODEL_TPROXY)) {
			*s << "x";
		}
#endif
		*s << "</td>";
		*s << "<td>" << getWorkModelName(server->flags) << "</td>";
		*s << "<td>";
		if (unix_socket) {
			*s << "unix";
		} else if (server->ss) {
			*s << "tcp/ipv" << (server->addr.v4.sin_family == PF_INET ? "4" : "6");
		}
		*s << "</td>";
		*s << "<td>";
		if (is_server_multi_selectable(server)) {
			*s << "M";
		}
		*s << "</td><td>" << (int)listen->key->global;
		*s << "</td><td>" << (int)listen->key->dynamic;
		*s << "</td><td>" << katom_get((void *)&server->refs);
		*s << "</td>";
		*s << "</tr>";
	}
	return iterator_continue;
}
void KDynamicListen::GetListenHtml(std::stringstream &s)
{
	rbtree_iterator(&listens, listen_html_iterator, &s);
}
struct query_domain_param {
	domain_t host;
	int port;
	WhmContext *ctx;
};
static iterator_ret query_domain_iterator(void *data, void *argv)
{
	query_domain_param *param = (query_domain_param *)argv;
	KListen *listen = (KListen *)data;
	kserver *server = listen->server;
	if (server->closed || !server->started ||
		TEST(server->flags, WORK_MODEL_MANAGE) ||
		(param->port>0 && listen->key->port!=param->port)) {
		return iterator_continue;
	}
	kassert(server->ctx != NULL);
	KSubVirtualHost *svh = (KSubVirtualHost *)((KVirtualHostContainer *)server->ctx)->find(param->host);
	if (svh) {
		KStringBuf s;
		if (!listen->key->ipv4) {
			s << "[";
		}
		s << listen->key->ip;
		if (!listen->key->ipv4) {
			s << "]";
		}
		s << ":" << listen->key->port;
		if (TEST(server->flags,WORK_MODEL_SSL)) {
			s << "s";
		}
		s << "\t";
		if (svh->wide) {
			s << "*";
		}
		s << svh->host << "\t" << svh->vh->name;
		param->ctx->add("vh", s.getString());
	}
	return iterator_continue;
}
void KDynamicListen::QueryDomain(domain_t host, int port, WhmContext *ctx)
{
	query_domain_param param;
	param.host = host;
	param.port = port;
	param.ctx = ctx;
	rbtree_iterator(&listens, query_domain_iterator, &param);
}
