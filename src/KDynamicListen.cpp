#include "KDynamicListen.h"
#include "KVirtualHost.h"
#include "ksocket.h"
#include "KServerListen.h"
#include "KVirtualHostManage.h"
#include "kselector_manager.h"
#include "KPreRequest.h"
#include "KHttpLib.h"
#include "WhmContext.h"
#include "KHttpServer.h"
#define DEFAULT_IPV4_IP  "0.0.0.0"
#define DEFAULT_IPV6_IP  "::"

static int listen_key_cmp(const void *key, const void *data)
{
	KListenKey *lk = (KListenKey *)key;
	KListen *listen = (KListen *)data;
	return lk->cmp(listen->key);
}
static void kgl_release_vh_container(void *ctx)
{
	KVirtualHostContainer *vhc = (KVirtualHostContainer *)ctx;
	vhc->release();
}

static bool kserver_start(kserver *server,KListenKey *lk, kgl_ssl_ctx *ssl_ctx)
{
	if (!kserver_bind(server, lk->ip.c_str(), lk->port, ssl_ctx)) {
		return false;
	}
	return start_http_server(server, lk->get_socket_flag());
}

void kgl_vhc_add_global_vh(KVirtualHostContainer* vhc, KVirtualHost *vh)
{
#ifndef HTTP_PROXY
	vhc->bind_vh(vh, false);
#endif
}
void kgl_vhc_remove_global_vh(KVirtualHostContainer* vhc, KVirtualHost *vh)
{
#ifndef HTTP_PROXY
	vhc->unbind_vh(vh);
#endif
}
#ifdef KSOCKET_SSL
kgl_ssl_ctx *kserver_load_ssl(uint32_t *flags,KSslConfig *ssl_config)
{
	KBIT_SET(*flags, KGL_SERVER_SSL);
	kgl_ssl_ctx *ssl_ctx = ssl_config->refs_ssl_ctx();
	if (ssl_ctx) {
		KBIT_SET(*flags, WORK_MODEL_SSL);
	}
	return ssl_ctx;
}
#endif
static bool kserver_is_empty(kserver *server)
{
	KVirtualHostContainer *vhc = (KVirtualHostContainer *)kserver_get_opaque(server);
	return vhc->empty();
}
static kserver *kserver_new(KVirtualHostContainer* vhc)
{
	kserver *server = kserver_init();
	kserver_set_opaque(server, kgl_release_vh_container, vhc);
	//kserver_bind(server, handle_connection, kserver_clean_ctx, vhc);
	return server;
}
void KListen::sync_flag(kgl_ssl_ctx *ssl_ctx)
{
	key->set_work_model(&server->flags);
#ifdef ENABLE_HTTP3
	if ((key->h3 == 0 || key->ssl==0) && h3_server) {
		h3_server->shutdown();
		h3_server->release();
		h3_server = nullptr;
	} else if (key->h3 > 0 && key->ssl>0 && h3_server == nullptr) {
		if (ssl_ctx == nullptr) {
			ssl_ctx = server->ssl_ctx;
		}
		if (ssl_ctx == nullptr) {
			klog(KLOG_ERR, "h3 only work with https listen\n");
			return;
		}
		KBIT_SET(ssl_ctx->alpn, KGL_ALPN_HTTP3);
		kgl_add_ref_ssl_ctx(ssl_ctx);
		h3_server = kgl_h3_new_server(key->ip.c_str(), key->port, key->get_socket_flag(), ssl_ctx, server->flags);
		if (h3_server == nullptr) {
			kgl_release_ssl_ctx(ssl_ctx);
			klog(KLOG_ERR, "cann't new h3 server on [%s:%d]\n", key->ip.c_str(), key->port);
			return;
		}
		auto vhc = get_vh_container(server);
		if (vhc == nullptr) {
			klog(KLOG_ERR, "panic cann't found vhc\n");
			return;
		}
		vhc->addRef();
		h3_server->bind_data(kgl_release_vh_container, vhc);
		return;
	}
	if (h3_server) {
		key->set_work_model(&h3_server->flags);
	}
#endif
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
		get_vh_container(listen->server)->unbind_vh(vh);
	}
	if (key->global > 0 && listen->key->global == 0) {
		conf.gvm->UnBindGlobalVirtualHost(listen->server);
	}
	delete key;
	if (listen->IsEmpty()) {
		kassert(kserver_is_empty(listen->server));
		kserver_close(listen->server);
#ifdef ENABLE_HTTP3
		if (listen->h3_server) {
			listen->h3_server->shutdown();
		}
#endif
		rbtree_remove(&listens, node);
		delete listen;
	} else {
		listen->sync_flag(nullptr);
#ifdef KSOCKET_SSL
		if (listen->key->ssl == 0 && KBIT_TEST(listen->server->flags, KGL_SERVER_SSL)) {
			//remove ssl
			KBIT_CLR(listen->server->flags, WORK_MODEL_SSL|KGL_SERVER_SSL);
			kserver_set_ssl_ctx(listen->server,NULL);
		}
#endif
	}
}
void KDynamicListen::add(KListenKey *key, KSslConfig *ssl_config, KVirtualHost *vh)
{
	KListen *listen;
	kserver *server = NULL;
	kgl_ssl_ctx *ssl_ctx = NULL;
	int new_flag = 0;
	bool need_load_ssl = false;
	uint32_t flags = 0;
	krb_node *node = rbtree_insert(&listens, key, &new_flag, listen_key_cmp);
	KVirtualHostContainer* vhc = nullptr;
	if (new_flag) {
		vhc = new KVirtualHostContainer();
		server = kserver_new(vhc);
		listen = new KListen(key, server);
		node->data = listen;
		if (key->global>0) {
			assert(conf.gvm != nullptr);
			conf.gvm->BindGlobalVirtualHost(server);
		}
		need_load_ssl = key->ssl > 0;
	} else {
		listen = (KListen *)node->data;
		server = listen->server;
		if (key->global > 0 && !KBIT_TEST(server->flags,KGL_SERVER_GLOBAL)) {
			conf.gvm->BindGlobalVirtualHost(server);
		}
		listen->key->SetFlag(key);
		if (key->ssl > 0) {
			if (!KBIT_TEST(listen->server->flags, WORK_MODEL_SSL) || key->global>0) {
				//无ssl或者是全局侦听，要更新ssl
				need_load_ssl = true;
			}
		}
		delete key;
	}
#ifdef KSOCKET_SSL
	if (need_load_ssl) {
		ssl_ctx = kserver_load_ssl(&server->flags, ssl_config);
	}
#endif
	listen->sync_flag(ssl_ctx);
#ifdef ENABLE_HTTP3
	if (listen->h3_server && !listen->h3_server->is_shutdown() && ssl_ctx) {	
		listen->h3_server->update_ssl_ctx(ssl_ctx);
	}
#endif
	if (!KBIT_TEST(server->flags, KGL_SERVER_START)) {
		init_listen(listen->key, listen, ssl_ctx);
#ifdef KSOCKET_SSL
	} else if (ssl_ctx) {
		kserver_set_ssl_ctx(server, (kgl_ssl_ctx *)ssl_ctx);
#endif
	}
	if (vh) {
		get_vh_container(server)->bind_vh(vh, true);
	}
	return;
}
void KDynamicListen::AddDynamic(const char *str,KVirtualHost *vh)
{
	std::list<KListenKey *> lk;
	parseListen(str,lk,vh);
	if (lk.empty()) {
		return;
	}
	for (auto it=lk.begin();it!=lk.end();++it) {
		add((*it), vh, vh);
	}
}
void KDynamicListen::RemoveDynamic(const char *str,KVirtualHost *vh)
{
	std::list<KListenKey *> lk;
	parseListen(str,lk,vh);
	if (lk.empty()) {
		return;
	}
	for (auto it=lk.begin();it!=lk.end();++it) {
		Delete((*it), vh);
	}
}
void KDynamicListen::RemoveGlobal(KListenHost *lh)
{
	std::list<KListenKey *> lk;
	parseListen(lh, lk);
	for (auto it2 = lk.begin(); it2 != lk.end(); ++it2) {
		Delete((*it2),NULL);
	}
}
bool KDynamicListen::AddGlobal(KListenHost *lh)
{
	std::list<KListenKey *> lk;
	parseListen(lh,lk);
	for (auto it2=lk.begin();it2!=lk.end();++it2) {
		add((*it2), lh, nullptr);
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
void KDynamicListen::parseListen(const char *listen,std::list<KListenKey *> &lks, KVirtualHost* vh)
{
	char *buf = strdup(listen);
	char *p = strrchr(buf,':');
	if (p) {
		*p = '\0';
		p++;
		KListenKey *lk = new KListenKey;
		lk->dynamic = 1;
		parse_port(p, lk, vh);
		if (strcmp(buf,"*")==0) {
			lk->ip = DEFAULT_IPV4_IP;
			lk->ipv4 = true;
			lks.push_back(lk);			
#ifdef KSOCKET_IPV6
			lk = new KListenKey;
			parse_port(p, lk, vh);
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
bool KDynamicListen::init_listen(KListenKey *lk, KListen *listen, kgl_ssl_ctx *ssl_ctx)
{
	kassert(is_selector_manager_init());
	bool result = kserver_start(listen->server, lk, ssl_ctx);
#ifdef ENABLE_HTTP3
	if (result && listen->h3_server) {		
		kgl_set_flag(&listen->server->flags, WORK_MODEL_ALT_H3, listen->h3_server->start());		
	}
#endif
	return result;
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
	key->ssl = KBIT_TEST(lh->model,WORK_MODEL_SSL) > 0;
#ifdef ENABLE_HTTP3
	if (key->ssl > 0) {
		key->h3 = KBIT_TEST(lh->alpn, KGL_ALPN_HTTP3) > 0;
	}
#endif
	key->manage = KBIT_TEST(lh->model, WORK_MODEL_MANAGE) > 0;
#ifdef WORK_MODEL_TCP
	key->tcp = KBIT_TEST(lh->model, WORK_MODEL_TCP) > 0;
#endif
	key->ipv4 = ipv4;
	parse_port(port, key, NULL);
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
	if (KBIT_TEST(server->flags, KGL_SERVER_GLOBAL)) {
		kgl_vhc_add_global_vh(get_vh_container(server), (KVirtualHost *)argv);
	}
	return iterator_continue;
}
static iterator_ret unbind_global_virtual_host_iterator(void *data, void *argv)
{
	kserver *server = ((KListen *)data)->server;
	if (KBIT_TEST(server->flags, KGL_SERVER_GLOBAL)) {
		kgl_vhc_remove_global_vh(get_vh_container(server), (KVirtualHost *)argv);
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
void KDynamicListen::parse_port(const char *p, KListenKey *lk, KVirtualHost *vh)
{
	lk->port = atoi(p);
	if (strchr(p, 's')) {
		lk->ssl = 1;
#ifdef ENABLE_HTTP3
		if (vh && KBIT_TEST(vh->alpn, KGL_ALPN_HTTP3)) {
			lk->h3 = 1;
		}
#endif
	}
#ifdef ENABLE_HTTP2
	if (strchr(p, 'h')) {
		lk->h2 = 1;
	}
#endif

#ifdef ENABLE_HTTP3
	if (strchr(p, 'q')) {
		lk->ssl = 1;
		lk->h3 = 1;
	}
#endif
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
	if (KBIT_TEST(listen->server->flags, KGL_SERVER_START)) {
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
		if (KBIT_TEST(server->flags, WORK_MODEL_PROXY)) {
			s << "<proxy>1</proxy>";
		} else if (KBIT_TEST(server->flags, WORK_MODEL_SSL_PROXY)) {
			s << "<ssl_proxy>1</ssl_proxy>";
		}
#endif
#ifdef WORK_MODEL_TPROXY
		if (KBIT_TEST(server->flags, WORK_MODEL_TPROXY)) {
			s << "<tproxy>1</tproxy>";
		}
#endif
		s << "<service>" << getWorkModelName(server->flags) << "</service>";
		s << "<tcp_ip>";
		if (unix_socket) {
			s << "unix";
		} else if (!klist_empty(&server->ss)) {
			s << (server->addr.v4.sin_family == PF_INET ? "4" : "6");
		}
		s << "</tcp_ip>";
		s << "<global>" << (int)listen->key->global << "</global>";
		s << "<dynamic>" << (int)listen->key->dynamic << "</dynamic>";
#ifdef ENABLE_HTTP3
		s << "<h3>" << (int)listen->key->h3 << "</h3>";
#endif
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
	if (KBIT_TEST(listen->server->flags, KGL_SERVER_START)) {
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
#ifdef ENABLE_HTTP3
		if (KBIT_TEST(server->flags, WORK_MODEL_ALT_H3 | WORK_MODEL_SSL) == (WORK_MODEL_ALT_H3 | WORK_MODEL_SSL)) {
			*s << "q";
		}
#endif
#ifdef WORK_MODEL_PROXY
		if (KBIT_TEST(server->flags, WORK_MODEL_PROXY)) {
			*s << "P";
		} else 	if (KBIT_TEST(server->flags, WORK_MODEL_SSL_PROXY)) {
			*s << "p";
		}
#endif
#ifdef WORK_MODEL_TPROXY
		if (KBIT_TEST(server->flags, WORK_MODEL_TPROXY)) {
			*s << "x";
		}
#endif
		*s << "</td>";
		*s << "<td>" << getWorkModelName(server->flags) << "</td>";
		*s << "<td>";
		if (unix_socket) {
			*s << "unix";
		} else if (!klist_empty(&server->ss)) {
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
	if (!KBIT_TEST(server->flags,KGL_SERVER_START) ||
		KBIT_TEST(server->flags, WORK_MODEL_MANAGE) ||
		(param->port>0 && listen->key->port!=param->port)) {
		return iterator_continue;
	}
	KVirtualHostContainer* vhc = (KVirtualHostContainer*)kserver_get_opaque(server);
	assert(vhc != NULL);
	auto locker = vhc->get_locker();
	KSubVirtualHost *svh = (KSubVirtualHost *)vhc->get_root(locker)->find(param->host);
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
		if (KBIT_TEST(server->flags,WORK_MODEL_SSL)) {
			s << "s";
		}
		s << "\t";
		if (svh->wide) {
			s << "*";
		}
		s << svh->host << "\t" << svh->vh->name;
		param->ctx->add("vh", s.c_str());
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
