#ifndef KASYNCFETCHOBJECT_H
#define KASYNCFETCHOBJECT_H
#include "KSocketBuffer.h"
#include "KUpstream.h"
#include "KHttpObject.h"
#include "kselector.h"
#include "KHttpResponseParser.h"
#include "KHttpParser.h"
#include "kfiber.h"
#include "KHttpHeaderManager.h"

enum Parse_Result
{
	Parse_Failed,
	Parse_Success,
	Parse_Continue
};
struct kgl_pop_header {
	uint16_t proto;
	uint16_t status_send : 1;
	uint16_t no_body:1;
	uint16_t recved_end_request : 1;
	uint16_t is_100_continue : 1;
	uint16_t is_interim_response : 1;
	uint16_t upstream_is_chunk : 1;
	uint16_t post_is_chunk : 1;
	uint16_t grpc : 1;
	uint16_t post_finished : 1;
	uint16_t post_ended : 1;
	uint16_t has_content_length : 1;
	uint16_t post_canceled : 1;
	uint16_t interim_count : 4;
	int keep_alive_time_out;
	unsigned field_count;
	int64_t response_length;
};
/**
* 异步调用扩展，所以支持异步调用的扩展从该类继承
*/
class KAsyncFetchObject : public KRedirectSource
{
friend int process_post_fiber(void* arg, int got);
public:
	KAsyncFetchObject():KAsyncFetchObject(KGL_UPSTREAM_NEED_QUEUE)
	{		
	}
	KAsyncFetchObject(uint32_t flag) : KRedirectSource(flag) {
		client = NULL;
		buffer = NULL;
		memset(&parser, 0, sizeof(parser));
		memset(&us_buffer, 0, sizeof(us_buffer));
		memset(&pop_header, 0, sizeof(pop_header));
	}
	void Close(KHttpRequest *rq)
	{
		if (client) {
			if (KBIT_TEST(rq->ctx.filter_flags,RF_UPSTREAM_NOKA)
				|| KBIT_TEST(rq->sink->data.flags, RQ_CONNECTION_UPGRADE)
				|| !rq->ctx.upstream_connection_keep_alive
				|| !rq->ctx.upstream_expected_done) {
				pop_header.keep_alive_time_out = -1;
			}
			client->gc(pop_header.keep_alive_time_out);
			client = NULL;
		}
		ResetBuffer();
	}
	virtual ~KAsyncFetchObject()
	{
		free_header_list(early_headers.steal_header());
		assert(client==NULL);	
		if (client) {
			client->gc(-1);
		}
		if (buffer) {
			delete buffer;
		}
		if (us_buffer.buf) {
			xfree(us_buffer.buf);
		}
	}
	//期望中的完成，长连接中用于标识此连接还可用
	virtual void expectDone(KHttpRequest *rq)
	{
		rq->ctx.upstream_expected_done = 1;
	}
	KGL_RESULT Open(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out) override;
public:
	void WaitPostFiber(KHttpRequest* rq, kfiber *post_fiber)
	{
		if (!pop_header.post_finished && !pop_header.post_ended) {
			pop_header.post_canceled = 1;
			/* A tunnel whose origin closed cleanly must end the client stream
			 * (HTTP/2 END_STREAM) after the relayed data, not reset it. */
			if (pop_header.grpc || (KBIT_TEST(rq->sink->data.flags, RQ_CONNECTION_UPGRADE) && rq->ctx.upstream_expected_done)) rq->sink->cancel_read();
			else rq->sink->shutdown();
			if (client) client->shutdown();
		}
		int retval;
		kfiber_join(post_fiber, &retval);
	}
	void on_readhup(KHttpRequest* rq) override;
	bool tunnel_write_end();
	virtual KGL_RESULT read_body_end(KHttpRequest* rq, KGL_RESULT result)
	{
		return result;
	}
	KGL_RESULT ReadBody(KHttpRequest* rq);
	KGL_RESULT ReadHeader(KHttpRequest* rq, kfiber **post_fiber);
	KGL_RESULT SendHeader(KHttpRequest* rq);
	KGL_RESULT ProcessPost(KHttpRequest* rq);
	KGL_RESULT PostResult(KHttpRequest *rq,int got);
	//得到post读缓冲，发送到upstream
	int getPostRBuffer(KHttpRequest *rq,LPWSABUF buf,int bc)
	{
		//新版已经简化流程，之前就把预加载的数据发送到buffer里面了。
		//assert(rq->pre_post_length==0);
		return buffer->getReadBuffer(buf,bc);
	}
	//得到post写缓冲，从client接收post数据
	char *GetPostBuffer(int *len)
	{
		return buffer->getWriteBuffer(len);
	}
	//得到body缓冲,从upstream读
	char *getUpstreamBuffer(int *len)
	{
		return ks_get_write_buffer(&us_buffer, len);
	}
	KUpstream *GetUpstream()
	{
		return client;
	}
	KGL_RESULT upstream_is_error(KHttpRequest *rq,int error,const char *msg, bool transport_error = true);
	KGL_RESULT PushHeader(KHttpRequest *rq,const char *attr, int attr_len, const char *val, int val_len, bool request_line);
	ks_buffer us_buffer;
	KSocketBuffer *buffer;
	KUpstream *client;
	kgl_input_stream *in;
	kgl_output_stream *out;
	kgl_response_body body;
	friend class KHttp2;
protected:
	KGL_RESULT on_read_head_success(KHttpRequest *rq, kfiber **post_fiber);
	void BuildChunkHeader();
	void PushStatus(KHttpRequest *rq, int status_code);
	KGL_RESULT PushHeaderFinished(KHttpRequest *rq);
	//创建发送头到buffer中。
	virtual KGL_RESULT buildHead(KHttpRequest *rq) = 0;
	//解析head
	virtual kgl_parse_result parse_unknow_header(KHttpRequest *rq,char **data, char *end);
	virtual KGL_RESULT ParseBody(KHttpRequest *rq, char **data, char* end);
	//创建post数据到buffer中。
	virtual void buildPost(KHttpRequest *rq)
	{
	}
	/* Allow framed upstream protocols to send their explicit end marker. */
	virtual KGL_RESULT on_post_end()
	{
		return KGL_OK;
	}
	//检查是否还要继续读body,一般长连接需要。
	//如果本身有content-length则不用该函数
	virtual bool checkContinueReadBody(KHttpRequest *rq)
	{
		return true;
	}
	//上游协议是否会自己回应100-continue。不支持的协议(fastcgi/ajp等)需直接转发post,由KSink::read回应100给客户端
	virtual bool upstream_handle_expect()
	{
		return false;
	}
	//长度未知的post是否用chunked编码发送。只有http上游需要,fastcgi/ajp等协议有自己的分包格式
	virtual bool upstream_chunk_post()
	{
		return false;
	}
	KGL_RESULT ParseBody(KHttpRequest* rq);
	khttp_parser parser;
	kgl_pop_header pop_header;
private:
	void ResetBuffer();
	KGL_RESULT SendPost(KHttpRequest *rq);
	void create_post_fiber(KHttpRequest* rq, kfiber** post_fiber);
	void InitUpstreamBuffer()
	{
		if (us_buffer.buf) {
			xfree(us_buffer.buf);
		}
		ks_buffer_init(&us_buffer, 8192);
	}
	KGL_RESULT InternalProcess(KHttpRequest *rq, kfiber** post_fiber);
	KGL_RESULT StoreEarlyHint(const char* name, int name_len, const char* value, int value_len);
	KGL_RESULT ForwardEarlyHints(KHttpRequest* rq);
	KHttpHeaderManager early_headers{};
	size_t early_bytes = 0;
	bool forward_early_hints = false;
	bool early_overflow = false;
};
#endif
