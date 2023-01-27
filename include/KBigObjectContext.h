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
	KGL_RESULT close(KGL_RESULT result);
	KGL_RESULT send_data(bool* net_fiber);
	KGL_RESULT stream_write(INT64 offset, const char* buf, int len);
	bool create_send_fiber(kfiber** send_fiber);
	void close_write();
	void obj_dead()
	{
		rq->ctx.cache_hit_part = false;
		KBIT_CLR(rq->sink->data.flags, RQ_CACHE_HIT);
		obj->Dead();
	}
	KSharedBigObject* get_shared_big_obj() {
		return obj->data->sbo;
	}
	bool bigobj_dead = false;
	bool read_fiber_error = false;
	bool verified_object = false;
	KHttpObject* obj;
	KHttpRequest* rq;
	int64_t write_offset = -1;
	int64_t read_offset = 0;
private:
	~KBigObjectContext();
	//return true continue false not continue
	bool verifiy_object(bool match_if_range,KGL_RESULT *result);
	void build_if_range(KHttpRequest* rq);
	KGL_RESULT internal_fetch();
	bool adjust_length() {
		if (!rq->sink->data.range) {
			left_read = obj->index.content_length;
		} else {
			left_read = obj->index.content_length;
			if (!kgl_adjust_range(rq->sink->data.range, &left_read)) {
				return false;
			}
			read_offset = rq->sink->data.range->from;
		}
		return true;
	}

	int64_t left_read = -1;//发送到用户数据剩余
	void tee_output_stream(kgl_output_stream* out);
};
struct KBigObjectReadContext
{
	KBigObjectContext* bo_ctx;
	kfiber* send_fiber;
	int64_t offset;
};
int bigobj_send_fiber(void* arg, int got);
#endif
#endif
