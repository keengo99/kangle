#ifndef KASYNCFETCHOBJECT_H
#define KASYNCFETCHOBJECT_H
#include "KSocketBuffer.h"
#include "KUpstream.h"
#include "KHttpObject.h"
#include "kselector.h"
#include "KHttpResponseParser.h"
#include "KHttpParser.h"
#include "kfiber.h"

enum Parse_Result
{
	Parse_Failed,
	Parse_Success,
	Parse_Continue
};
struct kgl_pop_header {
	Proto_t proto;
	uint16_t status_send : 1;
	uint16_t no_body:1;
	int keep_alive_time_out;
};
/**
* 异步调用扩展，所以支持异步调用的扩展从该类继承
*/
class KAsyncFetchObject : public KFetchObject
{
public:
	KAsyncFetchObject()
	{
		client = NULL;
		buffer = NULL;
		memset(&parser, 0, sizeof(parser));
		memset(&us_buffer, 0, sizeof(us_buffer));
		memset(&pop_header, 0, sizeof(pop_header));
	}
	void Close(KHttpRequest *rq)
	{
		if (client) {
			if (KBIT_TEST(rq->filter_flags,RF_UPSTREAM_NOKA) 
				|| !rq->ctx->upstream_connection_keep_alive
				|| !rq->ctx->upstream_expected_done) {
				pop_header.keep_alive_time_out = -1;
			}
			client->gc(pop_header.keep_alive_time_out);
			client = NULL;
		}
		ResetBuffer();
	}
	virtual ~KAsyncFetchObject()
	{
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
		rq->ctx->upstream_expected_done = 1;
	}
#ifdef ENABLE_REQUEST_QUEUE
	bool needQueue(KHttpRequest *rq)
	{
		return true;
	}
#endif
	KGL_RESULT Open(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out);
public:
	void WaitPostFiber(KHttpRequest* rq, kfiber *post_fiber)
	{
		rq->sink->shutdown();
		int retval;
		kfiber_join(post_fiber, &retval);
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
	KGL_RESULT upstream_is_error(KHttpRequest *rq,int error,const char *msg);
	KGL_RESULT PushHeader(KHttpRequest *rq,const char *attr, int attr_len, const char *val, int val_len, bool request_line);
	ks_buffer us_buffer;
	KSocketBuffer *buffer;
	KUpstream *client;
	kgl_input_stream *in;
	kgl_output_stream *out;
	friend class KHttp2;
protected:
	KGL_RESULT readHeadSuccess(KHttpRequest *rq,kfiber **post_fiber);
	void BuildChunkHeader();
	void PushStatus(KHttpRequest *rq, int status_code);
	KGL_RESULT PushHeaderFinished(KHttpRequest *rq);
	//创建发送头到buffer中。
	virtual KGL_RESULT buildHead(KHttpRequest *rq) = 0;
	//解析head
	virtual kgl_parse_result ParseHeader(KHttpRequest *rq,char **data, char *end);
	virtual KGL_RESULT ParseBody(KHttpRequest *rq, char **data, char* end);
	//创建post数据到buffer中。
	virtual void buildPost(KHttpRequest *rq)
	{
	}
	//检查是否还要继续读body,一般长连接需要。
	//如果本身有content-length则不用该函数
	virtual bool checkContinueReadBody(KHttpRequest *rq)
	{
		return true;
	}
	khttp_parser parser;
	kgl_pop_header pop_header;
private:
	void ResetBuffer();
	KGL_RESULT ParseBody(KHttpRequest* rq);
	KGL_RESULT SendPost(KHttpRequest *rq);
	void CreatePostFiber(KHttpRequest* rq, kfiber** post_fiber);
	void InitUpstreamBuffer()
	{
		if (us_buffer.buf) {
			xfree(us_buffer.buf);
		}
		ks_buffer_init(&us_buffer, 8192);
	}
	KGL_RESULT InternalProcess(KHttpRequest *rq, kfiber** post_fiber);
};
#endif
