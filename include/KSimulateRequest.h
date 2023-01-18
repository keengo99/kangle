#ifndef KSIMULATEREQUEST_H
#define KSIMULATEREQUEST_H
#include "global.h"
#include "ksocket.h"
#include "KHttpHeaderManager.h"
#include "ksapi.h"
#include "kconnection.h"
#include "KHttpStream.h"
#include "KFileStream.h"
#include "KTcpServerSink.h"
#include "kfiber.h"
#ifdef ENABLE_SIMULATE_HTTP
class KHttpRequest;
KHttpRequest *kgl_create_simulate_request(kgl_async_http *ctx);
int kgl_start_simulate_request(KHttpRequest *rq, kfiber** fiber = NULL);
int kgl_simuate_http_request(kgl_async_http *ctx, kfiber** fiber = NULL);
class KSimulateSink : public KSingleConnectionSink
{
public:
	KSimulateSink();
	~KSimulateSink();
	kconnection* get_connection() override
	{
		return cn;
	}
	bool set_transfer_chunked() override
	{
		return false;
	}
	bool internal_response_status(uint16_t status_code) override
	{
		this->status_code = status_code;
		return true;
	}
	bool response_header(const char *name, int name_len, const char *val, int val_len) override
	{
		KHttpHeader *header = new_pool_http_header(name, name_len, val, val_len,(kgl_malloc)kgl_pnalloc, cn->pool);
		if (header) {
			header_manager.append(header);
		}
		return true;
	}
	kev_result read_header() override;
	bool response_connection(const char *val, int val_len) override
	{
		return false;
	}
	//返回头长度,-1表示出错
	int internal_start_response_body(int64_t body_size, bool is_100_continue) override
	{
		if (header) {
			header(arg, status_code, header_manager.header);
		}
		return 0;
	}
	bool is_locked() override
	{
		return false;
	}
	bool readhup(void *arg, result_callback result) override
	{
		return false;
	}
	void remove_readhup() override
	{

	}
	int internal_read(char *buf,int len) override
	{
		if (post) { 
			return post(arg, buf, len);
		}
		return -1;
	}
	bool HasHeaderDataToSend() 
	{
		return false;
	}
	int internal_write(WSABUF *buf, int bc) override
	{
		if (body) {
			if (body(arg, (char*)buf[0].iov_base, buf[0].iov_len) == 0) {
				on_success_response(buf[0].iov_len);
				return buf[0].iov_len;
			}
		}
		return -1;
	}
	bool get_self_addr(sockaddr_i *addr) override
	{
		kgl_memcpy(addr, &cn->addr, sizeof(sockaddr_i));
		return true;
	}
	int end_request() override;
	void shutdown() override
	{

	}
	void set_time_out(int tmo_count) override
	{

	}
	uint32_t get_server_model() override {
		return 0;
	}
	KOPAQUE get_server_opaque()  override {
		return NULL;
	}
	int get_time_out() override
	{
		return cn->st.tmo;
	}
	void flush() override
	{

	}
	http_header_hook header;
	http_post_hook post;
	http_body_hook body;

	KHttpHeaderManager header_manager;
	char *host;
	uint16_t port;
	uint16_t status_code;
	int life_time;
	void *arg;
protected:
	void SetReadDelay(bool delay_flag)
	{

	}
	void SetWriteDelay(bool delay_flag)
	{

	}

};
void async_download(const char* url, const char* file, result_callback cb, void* arg);
int kgl_async_download(const char *url, const char *file, int *status);
bool test_simulate_request();
#endif
#endif
