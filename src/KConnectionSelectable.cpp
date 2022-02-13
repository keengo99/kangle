#include "kconnection.h"
#include "kselector.h"
#include "KHttp2.h"
#include "kserver.h"
#include "KSubVirtualHost.h"
#include "kselector_manager.h"
#if 0
struct kgl_delay_io
{
	kconnection *c;
	KHttpRequest *rq;
	buffer_callback buffer;
	result_callback result;
};
void delay_read(void *arg,int got)
{
	kgl_delay_io *io = (kgl_delay_io *)arg;
	CLR(io->rq->flags, RQ_RTIMER);
	io->c->read(io->rq,io->result,io->buffer);
	delete io;
}
void delay_write(void *arg,int got)
{
	kgl_delay_io *io = (kgl_delay_io *)arg;
	CLR(io->rq->flags, RQ_WTIMER);
	io->c->write(io->rq,io->result,io->buffer);
	delete io;
}
#ifdef KSOCKET_SSL
void resultSSLShutdown(void *arg,int got)
{
	kconnection *c = (kconnection *)arg;
	c->resultSSLShutdown(got);
}
#endif
#ifdef _WIN32
static int kconnection_buffer_addr(void *arg, LPWSABUF buffer, int bc)
{
	sockaddr_i *addr = (sockaddr_i *)arg;
	buffer[0].iov_base = (char *)addr;
	buffer[0].iov_len = addr->get_addr_len();
	return 1;
}
#endif
bool kconnection::get_remote_ip(char *ips, int ips_len)
{
	return ksocket_sockaddr_ip((struct sockaddr *)&addr, addr.get_addr_len(), ips, ips_len);
}
uint16_t kconnection::get_self_port()
{
	sockaddr_i addr;
	if (0 != get_self_addr(&addr)) {
		return 0;
	}
	return addr.get_port();
}
bool kconnection::get_self_ip(char *ips, int ips_len)
{
	sockaddr_i addr;
	if (0 != get_self_addr(&addr)) {
		return false;
	}
	return ksocket_sockaddr_ip((struct sockaddr *)&addr, addr.get_addr_len(), ips, ips_len);
}
uint16_t kconnection::get_remote_port()
{
	return addr.get_port();
}
int kconnection::get_self_addr(sockaddr_i *addr)
{
	int name_len = sizeof(sockaddr_i);
	return getsockname(fd,(struct sockaddr *)addr, &name_len);
}
void kconnection::update_remote_addr()
{
	int addr_len = sizeof(sockaddr_i);
	if (0 == getpeername(fd, (struct sockaddr *) &addr, &addr_len)) {
		kassert(addr_len == addr.get_addr_len());
	}
}
bool kconnection::HalfConnect(sockaddr_i *addr, sockaddr_i *bind_addr,int tproxy_mask)
{
	kgl_memcpy(&this->addr, addr, sizeof(sockaddr_i));
	fd = ksocket_half_connect((struct sockaddr *)addr, addr->get_addr_len(), (struct sockaddr *)bind_addr, bind_addr? bind_addr->get_addr_len():0, tproxy_mask);
	return ksocket_opened(fd);
}
bool kconnection::HalfConnect(const char *unix_path)
{
	return false;
}
bool kconnection::HalfConnect(struct sockaddr *addr, socklen_t addr_size, struct sockaddr *bind_addr, socklen_t bind_addr_size, int tproxy_mask)
{
	kassert(addr_size <= sizeof(sockaddr_i));
	kgl_memcpy(&this->addr, addr, addr_size);
	fd = ksocket_half_connect(addr, addr_size, bind_addr, bind_addr_size, tproxy_mask);
	return ksocket_opened(fd);
}
void kconnection::Connect(result_callback result,void *arg)
{
	//{{ent
#ifdef ENABLE_UPSTREAM_HTTP2
	assert(http2 == NULL && http2_ctx == NULL);
#endif//}}
	//selector->addList(this, KGL_LIST_CONNECT);
#ifdef _WIN32
	e[OP_READ].buffer = kconnection_buffer_addr;
	e[OP_READ].arg = &this->addr;
#endif
	if (!this->selector->connect(this, result, arg)) {
		result(arg, -1);
	}
}
bool kconnection::is_locked(KHttpRequest *rq)
{
	if (TEST(rq->flags, RQ_RTIMER | RQ_WTIMER)) {
		return true;
	}
#ifdef ENABLE_HTTP2
	if (rq->http2_ctx) {
		if (rq->http2_ctx->read_wait) {
			return true;
		}
		if (rq->http2_ctx->write_wait && !IS_WRITE_WAIT_FOR_HUP(rq->http2_ctx->write_wait)) {
			return true;
		}
		return false;
	}
#endif
	return TEST(st_flags, STF_LOCK) > 0;
}
kconnection::~kconnection()
{
	//printf("st=[%p] deleted\n", static_cast<KSelectable *>(this));
#ifdef ENABLE_HTTP2
	if (http2) {
		delete http2;
	}
#endif
	if (ls) {
		ls->release();
	}
#ifdef KSOCKET_SSL
	if (sni) {
		delete sni;
	}
#endif
	if (pool) {
		kgl_destroy_pool(pool);
	}
}
void kconnection::release(KHttpRequest *rq)
{
#ifdef ENABLE_HTTP2
	if (http2) {
		http2->write_end(rq->http2_ctx);
		http2->release(rq->http2_ctx);
		return;
	}
#endif
	real_destroy();
}
void kconnection::Read(void *arg, result_callback result, buffer_callback buffer)
{
	async_read(arg, result, buffer);
}
void kconnection::Write(void *arg, result_callback result, buffer_callback buffer)
{
	async_write(arg, result, buffer);
}
void kconnection::read(KHttpRequest *rq,result_callback result,buffer_callback buffer)
{
#ifdef ENABLE_HTTP2
	if (http2) {
		http2->read(rq->http2_ctx,result,buffer,rq);
		return;
	}
#endif
	async_read(rq,result,buffer);
}
bool kconnection::write_all(const char *buf, int len)
{
	WSABUF b;
	while (len > 0) {
		b.iov_len = len;
		b.iov_base = (char *)buf;
		int got = Write(&b, 1);
		if (got <= 0) {
			return false;
		}
		len -= got;
		buf += got;
	}
	return true;
}
int kconnection::Write(LPWSABUF buf,int bufCount)
{
#ifdef ENABLE_HTTP2
	if (http2) {
		return http2->write(rq->http2_ctx,buf,bufCount);
	}
#endif
#ifdef KSOCKET_SSL
	if (isSSL()) {
		return static_cast<KSSLSocket *>(socket)->ssl_writev(buf, bufCount);
	}
#endif
	return kgl_writev(fd,buf,bufCount);
}
void kconnection::write(KHttpRequest *rq,result_callback result,buffer_callback buffer)
{
	assert(selector->is_same_thread());
#ifdef ENABLE_HTTP2
	if (http2) {
		http2->write(rq->http2_ctx,result,buffer,rq);
		return;
	}
#endif
	async_write(rq,result,buffer);
}
void kconnection::read_hup(KHttpRequest *rq,result_callback result)
{
#ifdef ENABLE_HTTP2
	if (http2) {
		http2->read_hup(rq->http2_ctx, result, rq);
		return;
	}
#endif
#ifdef WORK_MODEL_SIMULATE
	if (TEST(rq->workModel, WORK_MODEL_SIMULATE)) {
		return;
	}
#endif
	selector->read_hup(this,result,NULL,rq);
}
void kconnection::AddSync()
{
	selector->removeSocket(this);
	selector->add_list(this, KGL_LIST_SYNC);	
}
void kconnection::RemoveSync()
{
	selector->remove_list(this);
}
void kconnection::remove_read_hup(KHttpRequest *rq)
{
#ifdef ENABLE_HTTP2
	if (http2) {
		http2->remove_read_hup(rq->http2_ctx);
		return;
	}
#endif
	selector->remove_read_hup(this);
	return;
}
void kconnection::Shutdown(KHttpRequest *rq)
{
#ifdef ENABLE_HTTP2
	if (http2) {
		http2->shutdown(rq->http2_ctx);
		return;
	}
#endif
	shutdown_socket();
}
int kconnection::Read(char *buf,int len)
{
#ifdef ENABLE_HTTP2
	if (http2) {
		return http2->read(rq->http2_ctx,buf,len);
	}
#endif
#ifdef KSOCKET_SSL
	if (isSSL()) {
		return static_cast<KSSLSocket *>(socket)->ssl_read(buf, len);
	}
#endif
	return recv(fd,buf,len,0);
}
void kconnection::delayRead(KHttpRequest *rq,result_callback result,buffer_callback buffer,int msec)
{
	/**
	* delay_read/delay_write在timer之前设置标识，并在timer结束实际运行时，清除相应的标识。
	* 以便在双通道中，检测selectable是否lock时，要同时检测此timer标识。
	* 否则会导致检测了selectable没有lock，并清除相应的selectable，但timer锁定了，结果就出错。
	*/
	kassert(!TEST(rq->flags, RQ_RTIMER));
	SET(rq->flags, RQ_RTIMER);
	assert(selector->is_same_thread());
	kgl_delay_io *io = new kgl_delay_io;
	io->c = this;
	io->rq = rq;
	io->result = result;
	io->buffer = buffer;
	selector->add_timer(delay_read,io,msec, this);
}
void kconnection::delayWrite(KHttpRequest *rq,result_callback result,buffer_callback buffer,int msec)
{
	kassert(!TEST(rq->flags, RQ_WTIMER));
	SET(rq->flags, RQ_WTIMER);
	assert(selector->is_same_thread());
	kgl_delay_io *io = new kgl_delay_io;
	io->c = this;
	io->rq = rq;
	io->result = result;
	io->buffer = buffer;
	selector->add_timer(delay_write,io,msec,this);
}
int kconnection::start_response(KHttpRequest *rq,INT64 body_len)
{
#ifdef ENABLE_HTTP2
	if (http2) {
		assert(rq->http2_ctx);
		if (TEST(rq->flags, RQ_SYNC)) {
			return http2->sync_send_header(rq->http2_ctx, body_len);
		}
		return http2->send_header(rq->http2_ctx, body_len);
	}
#endif
	set_delay();
	return 0;
}
void kconnection::end_response(KHttpRequest *rq,bool keep_alive)
{
#ifdef ENABLE_HTTP2
	if (http2) {
		SET(rq->flags,RQ_CONNECTION_CLOSE);
#ifndef NDEBUG
	/*
		unsigned char md5[16];
		char md5_str[33];
		KMD5Final(md5, &rq->http2_ctx->md5);
		make_digest(md5_str, md5);
		klog(KLOG_WARNING, "%s %s md5=[%s]\n",rq->getClientIp(), rq->url->path, md5_str);
	*/
#endif
		http2->write_end(rq->http2_ctx);
		return;
	}
#endif
	if (keep_alive) {
		set_nodelay();
		SET(rq->workModel,WORK_MODEL_KA);
	}
}
#ifdef KSOCKET_SSL
query_vh_result kconnection::useSniVirtualHost(KHttpRequest *rq)
{
	assert(rq->svh==NULL);
	rq->svh = sni->svh;
	sni->svh = NULL;
	query_vh_result ret = sni->result;
	delete sni;
	sni = NULL;
	return ret;
}
void kconnection::resultSSLShutdown(int got)
{
	if (got<0) {
		delete this;
		return;
	}
	assert(socket);
	KSSLSocket *sslSocket = static_cast<KSSLSocket *>(socket);
	ssl_status status = sslSocket->ssl_shutdown();
#ifdef ENABLE_KSSL_BIO
	if (status != ret_error && BIO_pending(sslSocket->ssl_bio[WRITE_PIPE].bio)>0) {
		async_write(this, ::resultSSLShutdown, NULL);
		return;
	}	
#endif
	switch (status) {
	case SSL_ERROR_WANT_READ:
#ifndef ENABLE_KSSL_BIO
		clear_flag(STF_RREADY);
#endif
		async_read(this, ::resultSSLShutdown, NULL);
		return;
	case SSL_ERROR_WANT_WRITE:
#ifndef ENABLE_KSSL_BIO
		clear_flag(STF_WREADY);
#endif
		async_write(this, ::resultSSLShutdown, NULL);
		return;
	default:
		delete this;
		return;
	}
}
#endif
void kconnection::real_destroy()
{
	if (selector) {
#ifdef KSOCKET_SSL
		if (socket && isSSL()) {
			resultSSLShutdown(0);
			return;
		}
#endif
		assert(queue.next==NULL);
	}
	delete this;
}
#ifdef KSOCKET_SSL
KSSLSniContext::~KSSLSniContext()
{
	if (svh) {
		svh->release();
	}
}
#endif
#endif