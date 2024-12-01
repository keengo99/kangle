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
using simulate_request = KHttpRequest;
simulate_request *kgl_create_simulate_request(kgl_async_http *ctx);
int kgl_simuate_http_request(kgl_async_http* ctx, kfiber** fiber = NULL);
/* will start as new fiber and free rq auto */
int kgl_simuate_start_as_new_fiber(simulate_request*rq, kfiber** fiber = NULL);
/* will start and free rq auto */
int kgl_simuate_start_fiber(simulate_request* rq);
void kgl_simulate_free(simulate_request* rq);

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
		if (header) {
			return header(arg, name, name_len, val, val_len) == 0;
		}
		return true;
	}
	void start(int header_len) override;
	bool response_connection(const char *val, int val_len) override
	{
		return false;
	}
	//返回头长度,-1表示出错
	int internal_start_response_body(int64_t body_size, bool is_100_continue) override
	{
		if (header_finish) {
			header_finish(arg, status_code, body_size);
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
	int write_all(const char *str,int length) override
	{
		if (body) {
			if (body(arg, (char*)str, length) == 0) {
				on_success_response(length);
				return 0;
			}
		}
		return length;
	}
	bool get_self_addr(sockaddr_i *addr) override
	{
		kgl_memcpy(addr, &cn->addr, sizeof(sockaddr_i));
		return true;
	}
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
		return cn->st.base.tmo;
	}
	void flush() override
	{

	}
	void end_request() override {
		
	}
	http_header_hook header;
	http_header_finish_hook header_finish;
	http_post_hook post;
	http_body_hook body;

	KString host;
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
