#ifndef kconnection_H
#define kconnection_H
#if 0
#include "kselectable.h"
#include "KVirtualHostContainer.h"
#include "kmalloc.h"
class KHttp2;
class KHttpRequest;
class KSubVirtualHost;
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
#endif
class kconnection
{
public:
	kconnection(kconnection *cn)
	{
		memset(static_cast<KSelectable *>(this), 0, sizeof(KSelectable));
	}

	kgl_pool_t *get_pool()
	{
		if (pool == NULL) {
			pool = kgl_create_pool(4096);
		}
		return pool;
	}
#ifdef KSOCKET_SSL
	void resultSSLShutdown(int got);
	query_vh_result useSniVirtualHost(KHttpRequest *rq);
#endif
	void set_delay()
	{
		ksocket_delay(fd);
	}
	void set_nodelay()
	{
		ksocket_no_delay(fd);
	}
	bool get_self_ip(char *ips, int ips_len);
	void update_remote_addr();
	uint16_t get_self_port();
	int get_self_addr(sockaddr_i *addr);
	bool get_remote_ip(char *ips, int ips_len);
	uint16_t get_remote_port();
	bool HalfConnect(sockaddr_i *addr, sockaddr_i *bind_addr=NULL,int tproxy_mask=0);
	bool HalfConnect(const char *unix_path);
	bool HalfConnect(struct sockaddr *addr, socklen_t addr_size, struct sockaddr *bind_addr=NULL, socklen_t bind_addr_size=0, int tproxy_mask=0);
	void Connect(result_callback result,void *arg);
	bool is_locked(KHttpRequest *rq);
	int Read(char *buf, int len);
	int Write(LPWSABUF buf,int bc);
	bool write_all(const char *buf, int len);
	void delayRead(KHttpRequest *rq,result_callback result,buffer_callback buffer,int msec);
	void delayWrite(KHttpRequest *rq,result_callback result,buffer_callback buffer,int msec);
	void Read(void *rq, result_callback result, buffer_callback buffer);
	void Write(void *rq, result_callback result, buffer_callback buffer);
	void read(KHttpRequest *rq,result_callback result,buffer_callback buffer);
	void write(KHttpRequest *rq,result_callback result,buffer_callback buffer);
	void read_hup(KHttpRequest *rq,result_callback result);
	void Shutdown(KHttpRequest *rq);
	void remove_read_hup(KHttpRequest *rq);
	void AddSync();
	void RemoveSync();
	//return header len
	int start_response(KHttpRequest *rq, INT64 body_len);
	void end_response(KHttpRequest *rq,bool keep_alive);
	
	void real_destroy();
	void release(KHttpRequest *rq);


protected:
	~kconnection();
};
#endif
void request_connection_broken(void *arg, int got);
#endif
