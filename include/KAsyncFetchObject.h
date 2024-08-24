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
	uint16_t proto;
	uint16_t status_send : 1;
	uint16_t no_body:1;
	uint16_t recved_end_request : 1;
	uint16_t is_100_continue : 1;
	uint16_t upstream_is_chunk : 1;
	uint16_t post_is_chunk : 1;
	int keep_alive_time_out;
};
/**
* �첽������չ������֧���첽���õ���չ�Ӹ���̳�
*/
class KAsyncFetchObject : public KRedirectSource
{
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
	//�����е���ɣ������������ڱ�ʶ�����ӻ�����
	virtual void expectDone(KHttpRequest *rq)
	{
		rq->ctx.upstream_expected_done = 1;
	}
	KGL_RESULT Open(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out) override;
public:
	void WaitPostFiber(KHttpRequest* rq, kfiber *post_fiber)
	{
		rq->sink->shutdown();
		int retval;
		kfiber_join(post_fiber, &retval);
	}
	void on_readhup(KHttpRequest* rq) override;
	virtual KGL_RESULT read_body_end(KHttpRequest* rq, KGL_RESULT result)
	{
		return result;
	}
	KGL_RESULT ReadBody(KHttpRequest* rq);
	KGL_RESULT ReadHeader(KHttpRequest* rq, kfiber **post_fiber);
	KGL_RESULT SendHeader(KHttpRequest* rq);
	KGL_RESULT ProcessPost(KHttpRequest* rq);
	KGL_RESULT PostResult(KHttpRequest *rq,int got);
	//�õ�post�����壬���͵�upstream
	int getPostRBuffer(KHttpRequest *rq,LPWSABUF buf,int bc)
	{
		//�°��Ѿ������̣�֮ǰ�Ͱ�Ԥ���ص����ݷ��͵�buffer�����ˡ�
		//assert(rq->pre_post_length==0);
		return buffer->getReadBuffer(buf,bc);
	}
	//�õ�postд���壬��client����post����
	char *GetPostBuffer(int *len)
	{
		return buffer->getWriteBuffer(len);
	}
	//�õ�body����,��upstream��
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
	kgl_response_body body;
	friend class KHttp2;
protected:
	KGL_RESULT on_read_head_success(KHttpRequest *rq, kfiber **post_fiber);
	void BuildChunkHeader();
	void PushStatus(KHttpRequest *rq, int status_code);
	KGL_RESULT PushHeaderFinished(KHttpRequest *rq);
	//��������ͷ��buffer�С�
	virtual KGL_RESULT buildHead(KHttpRequest *rq) = 0;
	//����head
	virtual kgl_parse_result parse_unknow_header(KHttpRequest *rq,char **data, char *end);
	virtual KGL_RESULT ParseBody(KHttpRequest *rq, char **data, char* end);
	//����post���ݵ�buffer�С�
	virtual void buildPost(KHttpRequest *rq)
	{
	}
	//����Ƿ�Ҫ������body,һ�㳤������Ҫ��
	//���������content-length���øú���
	virtual bool checkContinueReadBody(KHttpRequest *rq)
	{
		return true;
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
};
#endif
