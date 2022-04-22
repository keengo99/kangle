/*
 * KSockPoolHelper.cpp
 *
 *  Created on: 2010-6-4
 *      Author: keengo
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */
#include <sstream>
#include "KSockPoolHelper.h"
#include "utils.h"
#include "KAsyncFetchObject.h"
#include "kthread.h"
#include "kaddr.h"
#include "kselector_manager.h"
#include "KHttp2Upstream.h"
#include "KTsUpstream.h"

struct KSockPoolDns
{
	KHttpRequest *rq;
	KAsyncFetchObject *fo;
	KUpstream *socket;
	KSockPoolHelper *sh;
};
using namespace std;
#if 0
static kev_result monitor_connect_result(KOPAQUE data, void *arg, int got)
{
	KUpstream *us = (KUpstream *)arg;
	kconnection *c = us->GetConnection();
	KSockPoolHelper *sph = static_cast<KSockPoolHelper *>(us->container);
	if (got < 0) {
		sph->disable();
	} else {
		if (send(c->st.fd,"H", 1,0) <= 0) {			
			sph->disable();
		} else {
			sph->enable();
		}
	}
	int monitor_tick = (int)(kgl_current_msec - sph->monitor_start_time);
	if (monitor_tick <= 0) {
		monitor_tick = 1;
	}
	if (monitor_tick > 30000) {
		monitor_tick = 30000;
	}	
	if (sph->avg_monitor_tick == 0) {
		sph->avg_monitor_tick = monitor_tick;
	} else {
		sph->avg_monitor_tick = (int)(0.9 * float(sph->avg_monitor_tick) + 0.1 * float(monitor_tick));
	}	
	sph->monitorNextTick();
	us->Destroy();
	return kev_destroy;
}
static kev_result start_monitor_call_back(KOPAQUE data, void *arg,int got)
{
	KSockPoolHelper *sph = (KSockPoolHelper *)arg;
	if (!sph->monitor) {
		sph->release();
		return kev_ok;
	}
	sph->start_monitor_call_back();
	return kev_ok;
}
kev_result next_monitor_call_back(KOPAQUE data, void *arg, int got)
{
	kselector_add_timer(kgl_get_tls_selector(),::start_monitor_call_back, arg, got,NULL);
	return kev_ok;
}
static kev_result sockpool_name_resovled_monitor_call_back(void *arg, struct addrinfo  *addr)
{
	sockaddr_i a;
	KSockPoolDns *spdns = (KSockPoolDns *)arg;
	assert(spdns->socket);
	if (addr==NULL ||
		!kgl_addr_build(addr,(uint16_t)spdns->sh->port, &a) ||
		!spdns->sh->connect_addr(NULL, static_cast<KTcpUpstream *>(spdns->socket), a)) {
		spdns->socket->Destroy();
		spdns->sh->disable();
		spdns->sh->monitorNextTick();
		delete spdns;
		return kev_destroy;
	}
	kev_result ret = spdns->socket->Connect(spdns->socket, monitor_connect_result);
	delete spdns;
	return ret;
}
#endif
void KSockPoolHelper::start_monitor_call_back()
{
#if 0
#ifdef MALLOCDEBUG
	if (quit_program_flag > 0) {
		return;
	}
#endif
	bool need_name_resolved = false;
	KUpstream *us = newConnection(NULL, need_name_resolved);
	if (us == NULL) {
		disable();		
		monitorNextTick();
		return;
	}
	us->BindSelector(kgl_get_tls_selector());
	this->monitor_start_time = kgl_current_msec;
	if (need_name_resolved) {
		KSockPoolDns *spdns = new KSockPoolDns;
		spdns->socket = us;
		spdns->sh = this;
		kgl_find_addr(this->host.c_str(), kgl_addr_ip, sockpool_name_resovled_monitor_call_back, spdns, kgl_get_tls_selector());
		return;
	}
	us->Connect(us, monitor_connect_result);
#endif
}
KSockPoolHelper::KSockPoolHelper() {
	ip = NULL;
	tryTime = 0;
	error_count = 0;
	flags = 0;
	max_error_count = 5;
	hit = 0;
	weight = 1;
	avg_monitor_tick = 0;
#ifdef ENABLE_UPSTREAM_SSL
	//{{ent
#ifdef ENABLE_UPSTREAM_HTTP2
	http2 = false;
#endif//}}
	ssl_ctx = NULL;
#endif
	total_error = 0;
	total_connect = 0;
}
KSockPoolHelper::~KSockPoolHelper() {
#ifdef ENABLE_UPSTREAM_SSL
	if (ssl_ctx) {
		SSL_CTX_free(ssl_ctx);
	}
#endif
	if (ip) {
		free(ip);
	}
}
void KSockPoolHelper::monitorNextTick()
{
#if 0
	if (!monitor) {
		release();
		return;
	}
	kselector_add_timer(kgl_get_tls_selector(),::start_monitor_call_back,this,error_try_time*1000,NULL);
#endif
}
void KSockPoolHelper::startMonitor()
{
#if 0
	if (monitor) {
		return;
	}
	monitor = true;
	addRef();
	selector_manager_add_timer(::start_monitor_call_back, this, rand() % 10000,NULL);
#endif
}

void KSockPoolHelper::checkActive()
{
	if (monitor) {
		return;
	}
	addRef();
	start_monitor_call_back();
}

KUpstream *KSockPoolHelper::GetConnection(KHttpRequest *rq, bool &half, bool &need_name_resolved)
{
	KUpstream *socket = NULL;
	if (!KBIT_TEST(rq->flags, RQ_UPSTREAM_ERROR|RQ_HAS_CONNECTION_UPGRADE)) {
		//如果是发生错误重连或upgrade的连接，则排除连接池
		socket = GetPoolSocket(rq);
		if (socket) {
			half = false;
			return socket;
		}
	}
	half = true;
	return newConnection(rq, need_name_resolved);
}
KUpstream *KSockPoolHelper::newConnection(KHttpRequest *rq,bool &need_name_resolved)
{
	
	KTcpUpstream *socket = new KTcpUpstream(NULL);
	bind(socket);
#ifdef KSOCKET_UNIX
	if (is_unix) {
		struct sockaddr_un addr;
		ksocket_unix_addr(host.c_str(),&addr);
		SOCKET fd = ksocket_half_connect((sockaddr_i *)&addr,NULL,0);
		if (!ksocket_opened(fd)) {
			socket->Destroy();
			return NULL;
		}
		kconnection *cn = socket->GetConnection();
		cn->st.fd = fd;
		return socket;
	}
#endif
	if (!try_numerichost_connect(rq,socket, need_name_resolved) && !need_name_resolved) {
		socket->Destroy();
		return NULL;
	}
	return socket;
}
KUpstream* KSockPoolHelper::GetUpstream(KHttpRequest* rq)
{
	rq->ctx->upstream_sign = this->sign;
	KUpstream* socket = NULL;
	if (!KBIT_TEST(rq->flags, RQ_UPSTREAM_ERROR | RQ_HAS_CONNECTION_UPGRADE)) {
		//如果是发生错误重连或upgrade的连接，则排除连接池
		socket = GetPoolSocket(rq);
		if (socket) {
			return socket;
		}
	}
	sockaddr_i *bind_addr = NULL;
	sockaddr_i bind_tmp_addr;
	const char* bind_ip = ip;
	if (bind_ip == NULL && rq) {
		bind_ip = rq->bind_ip;
	}
	if (bind_ip) {
		bind_addr = &bind_tmp_addr;
		if (!ksocket_getaddr(bind_ip, 0, AF_UNSPEC, AI_NUMERICHOST, bind_addr)) {
			return NULL;
		}
	}

	kgl_addr* addr = NULL;
	if (kfiber_net_getaddr(host.c_str(), &addr) != 0) {
		assert(addr == NULL);
		return NULL;
	}
	int tproxy_mask = 0;
#ifdef ENABLE_TPROXY
	if (rq && KBIT_TEST(rq->filter_flags, RF_TPROXY_UPSTREAM)) {
		tproxy_mask = 9;
	}
#endif
	katom_inc64((void*)&total_connect);
	kconnection* cn = kfiber_net_open2(addr->addr,port);
	kgl_addr_release(addr);
	if (kfiber_net_connect(cn, bind_addr, tproxy_mask) != 0) {
		kfiber_net_close(cn);
		return NULL;
	}
#ifdef ENABLE_UPSTREAM_SSL
	if (this->ssl_ctx) {
		const char* sni_host = NULL;
		if (!this->no_sni) {
			if (rq && !KBIT_TEST(rq->filter_flags, RF_UPSTREAM_NOSNI)) {
				sni_host = rq->url->host;
			}
		}
		if (!kconnection_ssl_connect(cn, ssl_ctx, sni_host)) {
			klog(KLOG_ERR, "cann't bind_fd for ssl socket.\n");
			kfiber_net_close(cn);
			return NULL;
		}
		//{{ent
#ifdef ENABLE_UPSTREAM_HTTP2
#ifdef TLSEXT_TYPE_application_layer_protocol_negotiation
		if (http2 && cn->st.ssl && KBIT_TEST(rq->flags, RQ_HAS_CONNECTION_UPGRADE)) {
			//websocket will turn off http2
			SSL_set_alpn_protos(cn->st.ssl->ssl, (unsigned char*)KGL_HTTP_NPN_ADVERTISE, sizeof(KGL_HTTP_NPN_ADVERTISE) - 1);
		}
#endif
#endif//}}
		if (kfiber_ssl_handshake(cn) != 0) {
			kfiber_net_close(cn);
			return NULL;
		}
		//{{ent
#ifdef ENABLE_UPSTREAM_HTTP2
		const unsigned char* protocol_data = NULL;
		unsigned len = 0;
		kgl_ssl_get_next_proto_negotiated(cn->st.ssl->ssl, &protocol_data, &len);
		if (len == sizeof(KGL_HTTP_V2_NPN_NEGOTIATED) - 1 &&
			memcmp(protocol_data, KGL_HTTP_V2_NPN_NEGOTIATED, len) == 0) {
			KHttp2* http2 = new KHttp2();
			selectable_bind_opaque(&cn->st, http2, kgl_opaque_client_http2);
			KHttp2Upstream* http2_us = http2->client(cn);
			bind(http2_us);
			return http2_us;
		}
#endif//}}
	}
#endif
	socket = new KTcpUpstream(cn);
	bind(socket);
	return socket;
}

bool KSockPoolHelper::connect_addr(KHttpRequest *rq, KTcpUpstream *socket, sockaddr_i &addr)
{
	katom_inc64((void *)&total_connect);
	sockaddr_i *bind_addr = NULL;
	const char *bind_ip = ip;
	if (bind_ip==NULL && rq) {
		bind_ip = rq->bind_ip;
	}
	if (bind_ip) {
		bind_addr = new sockaddr_i;
		if (!ksocket_getaddr(bind_ip, 0, AF_UNSPEC, AI_NUMERICHOST, bind_addr)) {
			delete bind_addr;
			return false;
		}
	}
	kconnection *cn = socket->GetConnection();
	kgl_memcpy(&cn->addr, &addr, sizeof(cn->addr));
	int tproxy_mask = 0;
#ifdef ENABLE_TPROXY
	if (rq && KBIT_TEST(rq->filter_flags, RF_TPROXY_UPSTREAM)) {
		tproxy_mask = 9;
	}
#endif
	bool result = kconnection_half_connect(cn,bind_addr, tproxy_mask);	
	if (bind_addr) {
		delete bind_addr;
	}
#ifdef ENABLE_UPSTREAM_SSL
	if (result && this->ssl_ctx) {
		const char *sni_host = NULL;
		if (!this->no_sni) {
			if (rq && !KBIT_TEST(rq->filter_flags, RF_UPSTREAM_NOSNI)) {
				sni_host = rq->url->host;
			}
		}
		if (!kconnection_ssl_connect(cn, ssl_ctx, sni_host)) {
			klog(KLOG_ERR, "cann't bind_fd for ssl socket.\n");
		}
		//{{ent
#ifdef ENABLE_UPSTREAM_HTTP2
#ifdef TLSEXT_TYPE_application_layer_protocol_negotiation
		if (http2 && cn->st.ssl && KBIT_TEST(rq->flags, RQ_HAS_CONNECTION_UPGRADE)) {
			//websocket will turn off http2
			SSL_set_alpn_protos(cn->st.ssl->ssl, (unsigned char *)KGL_HTTP_NPN_ADVERTISE, sizeof(KGL_HTTP_NPN_ADVERTISE)-1);
		}
#endif
#endif//}}
	}
#endif
	return result;
}
bool KSockPoolHelper::try_numerichost_connect(KHttpRequest *rq,KTcpUpstream *socket, bool &need_name_resolved)
{
	sockaddr_i addr;
	bool is_numerichost = ksocket_getaddr(host.c_str(), port, AF_UNSPEC, AI_NUMERICHOST,&addr);
	need_name_resolved = !is_numerichost;
	if (!is_numerichost) {
		return false;
	}
	return connect_addr(rq, socket, addr);
}
bool KSockPoolHelper::setHostPort(std::string host,int port,const char *ssl)
{
	bool destChanged = false;
	lock.Lock();
	if(this->host != host || this->port!=port){
		destChanged = true;
	}
	this->host = host;
	this->port = port;
#ifdef ENABLE_UPSTREAM_SSL
	char *ssl_buf = NULL;
	char *protocols = NULL;
	char *chiper = NULL;
	if (ssl) {
		this->ssl = ssl;
		ssl_buf = strdup(ssl);
		protocols = strchr(ssl_buf, '/');
		if (protocols) {
			*protocols = '\0';
			protocols++;
			chiper = strchr(protocols, '/');
			if (chiper) {
				*chiper = '\0';
				chiper++;
			}
		}
	} else {
		this->ssl.clear();
	}
	//{{ent
#ifdef ENABLE_UPSTREAM_HTTP2
	this->http2 = false;
	if (ssl_buf && strchr(ssl_buf, 'p')) {
		this->http2 = true;
	}
#endif//}}
	if (ssl_buf && strchr(ssl_buf, 'n')) {
		this->no_sni = 1;
	}
	if (ssl_ctx) {
		SSL_CTX_free(ssl_ctx);
		ssl_ctx = NULL;
	}
	if (ssl) {
		void *ssl_ctx_data = NULL;
		//{{ent
#ifdef ENABLE_UPSTREAM_HTTP2
		ssl_ctx_data = &http2;
#endif//}}
		conf.admin_lock.Lock();
		std::string ssl_client_chiper = cconf ? cconf->ssl_client_chiper : conf.ssl_client_chiper;
		std::string ssl_client_protocols = cconf ? cconf->ssl_client_protocols : conf.ssl_client_protocols;
		std::string ca_path = cconf ? cconf->ca_path : conf.ca_path;
		conf.admin_lock.Unlock();
		ssl_ctx = kgl_ssl_ctx_new_client(IsSslVerify()?ca_path.c_str():NULL, NULL, ssl_ctx_data);
		if (ssl_ctx) {
			kgl_ssl_ctx_set_protocols(ssl_ctx, protocols);
			if (chiper) {
				kgl_ssl_ctx_set_cipher_list(ssl_ctx, chiper);
			}
			if (chiper == NULL || protocols == NULL) {
				if (chiper == NULL && !ssl_client_chiper.empty()) {
					SSL_CTX_set_cipher_list(ssl_ctx, ssl_client_chiper.c_str());
				}
				if (protocols == NULL && !ssl_client_protocols.empty()) {
					kgl_ssl_ctx_set_protocols(ssl_ctx, ssl_client_protocols.c_str());
				}
			}
		}		
	}
#endif
	if (destChanged) {
		//clean();
	}
#ifdef KSOCKET_UNIX
	if(strncasecmp(this->host.c_str(),"unix:",5)==0){
		is_unix = 1;
		this->host = this->host.substr(5);
	}
	if (this->host[0]=='/') {
		is_unix = 1;
	}
#endif
	lock.Unlock();
#ifdef ENABLE_UPSTREAM_SSL
	if (ssl_buf) {
		free(ssl_buf);
	}
#endif
	return true;
}
bool KSockPoolHelper::setHostPort(std::string host, const char *port) {
	const char *ssl = port;
	while (*ssl && IS_DIGIT(*ssl)) {
		ssl++;
	}
	if (*ssl != 's' && *ssl != 'S') {
		ssl = NULL;
	}
	return setHostPort(host,atoi(port),ssl);
}
void KSockPoolHelper::disable() {
	if (error_try_time==0) {
		tryTime = kgl_current_sec + ERROR_RECONNECT_TIME;
	} else {
		tryTime = kgl_current_sec + error_try_time;
	}
}
bool KSockPoolHelper::isEnable() {
	if (tryTime == 0) {
		return true;
	}
	if (tryTime < kgl_current_sec) {
		tryTime += MAX(error_try_time,10);
		checkActive();
		return false;
	}
	return false;
}
void KSockPoolHelper::enable() {
	tryTime = 0;
	error_count = 0;
}
bool KSockPoolHelper::parse(std::map<std::string,std::string> &attr)
{
	setHostPort(attr["host"],attr["port"].c_str());
	setLifeTime(atoi(attr["life_time"].c_str()));
	SetParam(attr["param"].c_str());
//{{ent
#ifdef HTTP_PROXY
	auth_user = attr["auth_user"];
	auth_passwd = attr["auth_passwd"];
#endif//}}
	setIp(attr["self_ip"].c_str());
	sign = (attr["sign"] == "1");
	return true;
}
void KSockPoolHelper::buildXML(std::stringstream &s)
{
	s << " host='" ;
	s << host << "' port='" << port ;
#ifdef ENABLE_UPSTREAM_SSL
	if (ssl.size()>0) {
		s << ssl;
	}
#endif
	s << "' life_time='" << getLifeTime() << "' ";
	kgl_refs_string *str = GetParam();
	if (str) {
		s << "param='";
		s.write(str->str.data, str->str.len);
		s << "' ";
		release_string(str);
	}
//{{ent
#ifdef HTTP_PROXY
	if(auth_user.size()>0){
		s << "auth_user='" << auth_user << "' ";
	}
	if(auth_passwd.size()>0){
		s << "auth_passwd='" << auth_passwd << "' ";
	}
#endif//}}
	if (ip && *ip) {
		s << "self_ip='" << ip << "' ";
	}
	if (sign) {
		s << "sign='1' ";
	}
}
//{{ent
void KSockPoolHelper::addHeader(KHttpRequest *rq,KHttpEnv *s)
{
#ifdef HTTP_PROXY
	lock.Lock();
	if (auth_user.size()>0 && auth_passwd.size()>0) {
		KStringBuf up;
		up << auth_user.c_str() << ":" << auth_passwd.c_str();
		//s << "Proxy-Authorization: Basic " << b64encode((const unsigned char *)up.getString(), up.getSize()) << "\r\n";
	}
	lock.Unlock();
#endif
}
#ifdef HTTP_PROXY
void KSockPoolHelper::buildHttpAuth(KWStream &s)
{
	lock.Lock();
	if(auth_user.size()>0 && auth_passwd.size()>0){
		KStringBuf up;
		up << auth_user.c_str() << ":" << auth_passwd.c_str();
		s << "Proxy-Authorization: Basic " << b64encode((const unsigned char *)up.getString(),up.getSize()) << "\r\n";
	}
	lock.Unlock();
}

void KSockPoolHelper::setProxyAuth(std::string user,std::string passwd)
{
	lock.Lock();
	auth_user = user;
	auth_passwd = passwd;
	lock.Unlock();
}
#endif//}}
