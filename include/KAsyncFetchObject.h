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
	time_t first_body_time;
};
/**
* �첽������չ������֧���첽���õ���չ�Ӹ���̳�
*/
class KAsyncFetchObject : public KFetchObject
{
public:
	KAsyncFetchObject()
	{
		client = NULL;
		badStage = BadStage_Connect;
		buffer = NULL;
		memset(&parser, 0, sizeof(parser));
		memset(&us_buffer, 0, sizeof(us_buffer));
		memset(&pop_header, 0, sizeof(pop_header));
	}
	void Close(KHttpRequest *rq)
	{
		if (client) {
			if (TEST(rq->filter_flags,RF_UPSTREAM_NOKA) 
				|| !rq->ctx->upstream_connection_keep_alive
				|| !rq->ctx->upstream_expected_done) {
				pop_header.keep_alive_time_out = -1;
			}
			client->Gc(pop_header.keep_alive_time_out, pop_header.first_body_time);
			client = NULL;
		}		
		ResetBuffer();		
	}
	virtual ~KAsyncFetchObject()
	{
		assert(client==NULL);	
		if (client) {
			client->Gc(-1,0);
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
		rq->sink->Shutdown();
		int retval;
		kfiber_join(post_fiber, &retval);
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
	KGL_RESULT handleUpstreamError(KHttpRequest *rq,int error,const char *msg,int last_got);
	KGL_RESULT PushHeader(KHttpRequest *rq,const char *attr, int attr_len, const char *val, int val_len, bool request_line);
	ks_buffer us_buffer;
	KSocketBuffer *buffer;
	KUpstream *client;
	kgl_input_stream *in;
	kgl_output_stream *out;
	BadStage badStage;
	friend class KHttp2;
protected:
	KGL_RESULT readHeadSuccess(KHttpRequest *rq,kfiber **post_fiber);
	void BuildChunkHeader();
	void PushStatus(KHttpRequest *rq, int status_code);
	KGL_RESULT PushHeaderFinished(KHttpRequest *rq);
	//��������ͷ��buffer�С�
	virtual void buildHead(KHttpRequest *rq) = 0;
	//����head
	virtual kgl_parse_result ParseHeader(KHttpRequest *rq,char **data, int *len);
	virtual KGL_RESULT ParseBody(KHttpRequest *rq, char **data, int *len);
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