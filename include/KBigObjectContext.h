#ifndef KBIGOBJECTCONTEXT_H
#define KBIGOBJECTCONTEXT_H
#include "global.h"
#include <list>
#include "krbtree.h"
#include "kfile.h"
#include "KHttpRequest.h"
#include "KSharedBigObject.h"
#include "KStream.h"
#include "kfile.h"
#include "KMutex.h"
#include "KHttpObject.h"
#include "KSendBuffer.h"
#ifdef ENABLE_BIG_OBJECT_206
class KHttpObject;
class KFetchBigObject;

class KBigObjectContext : public kgl_forward_output_stream
{
public:
	KBigObjectContext(KHttpRequest* rq, KHttpObject* obj);
	KGL_RESULT open_cache();
	KGL_RESULT create();

	KGL_RESULT upstream_recv_headed();
	void close();
	KGL_RESULT send_data(bool* net_fiber);
	KGL_RESULT stream_write(INT64 offset, const char* buf, int len);
	bool create_send_fiber(kfiber** send_fiber);
	void close_write(int64_t from);
	KSharedBigObject* get_shared_big_obj() {
		return obj->data->sbo;
	}
	bool bigobj_dead = false;
	bool read_fiber_error = false;

private:
	~KBigObjectContext();
	void build_if_range(KHttpRequest* rq);
	bool adjust_length() {
		if (!rq->sink->data.range) {
			left_read = obj->index.content_length;
		} else {
			left_read = obj->index.content_length;
			if (!kgl_adjust_range(rq->sink->data.range, &left_read)) {
				return false;
			}
			offset = rq->sink->data.range->from;
		}
		return true;
	}
	KHttpObject* obj;
	KHttpRequest* rq;
	int64_t offset = 0;
	int64_t left_read = -1;//发送到用户数据剩余
	void tee_output_stream(kgl_output_stream* out);
};
struct KBigObjectReadContext
{
	KBigObjectContext* bo_ctx;
	kfiber* send_fiber;
	int64_t write_from;
	int64_t offset;
};
int bigobj_send_fiber(void* arg, int got);
#endif
#endif
